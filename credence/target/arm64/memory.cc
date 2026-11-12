/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#include "memory.h"

#include "assembly.h"                        // for Register, Operand_Size
#include "credence/target/common/flags.h"    // for Flag_Accessor, Instruct...
#include "credence/target/common/memory.h"   // for align_up_to, is_vector_...
#include "credence/target/common/types.h"    // for Stack_Offset, Table_Poi...
#include "flags.h"                           // for flags
#include "stack.h"                           // for Stack
#include <credence/error.h>                  // for credence_assert, creden...
#include <credence/object.h>                 // for LValue, Object, get_rva...
#include <credence/symbol.h>                 // for Symbol_Table
#include <credence/target/common/assembly.h> // for direct_immediate, make_...
#include <credence/types.h>                  // for get_size_from_rvalue_da...
#include <credence/util.h>                   // for INLINE_DEBUG, is_variant
#include <deque>                             // for deque
#include <fmt/format.h>                      // for format
#include <matchit.h>                         // for App, pattern, app, Patt...
#include <string>                            // for basic_string, char_traits
#include <string_view>                       // for basic_string_view
#include <tuple>                             // for get
#include <utility>                           // for pair
#include <variant>                           // for get, variant, monostate

/****************************************************************************
 * ARM64 Memory Accessor
 *
 * The memory accessor facade of the ARM64 target, and the device of each
 * local: a register in a frame with no calls, or a stack slot. A local whose
 * address is taken is always on the stack, since a register has no address.
 *
 *  Example:
 *
 *    main() {
 *      auto x, *p, y;
 *      x = 1;
 *      y = 2;
 *      p = &y;
 *    }
 *
 *  Becomes:
 *
 *    mov w9, #1             // x is in w9
 *    ldr w10, [sp, #16]     // y is on the stack, loaded into x10
 *    mov w10, #2
 *    str w10, [sp, #16]
 *    add x8, sp, #16        // the address of y
 *    mov x11, x8            // p is in x11
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   x0 - x7  = The arguments of a call, and x0 the return value
 *   x6       = The data section address register, and the second operand
 *              of a binary expression in a temporary
 *      s6    = floating point
 *      d6    = double
 *   x7       = The scratch register of the right-hand-side of a binary
 *              expression
 *   x8       = The default "accumulator" register for expression expansion
 *   x9, x11 - x14, x16, x17
 *            = If there are no function calls in a stack frame, local scope
 *              variables are stored in these registers, after which the
 *              stack is used
 *   x10      = The stack move register, and the address register of a
 *              pointer on the stack
 *   x15      = The address of a vector element on the stack
 *   x18      = The platform register on Darwin, never used
 *   x19      = The argc, argv scratch register
 *
 *   A local whose address is taken, vectors, and vector offsets are always
 *   on the stack
 *
 *****************************************************************************/

namespace credence::target::arm64::memory {

namespace m = matchit;

/**
 * @brief Get the second operand register of a binary expression, w6 or x6
 */
Register get_second_register_for_binary_operand(assembly::Operand_Size size)
{
    if (size == assembly::Operand_Size::Doubleword)
        return Register::x6;
    else
        return Register::w6;
}

/**
 * @brief Get the scratch register of a size, w8 or x8
 */
Register get_scratch_register(assembly::Operand_Size size)
{
    if (size == assembly::Operand_Size::Doubleword)
        return Register::x8;
    else
        return Register::w8;
}

/**
 * @brief Get the operand size of a storage device
 *
 * A stack offset has the size of its slot, an immediate the size of its
 * type, and a register its width. A page relative address is a doubleword.
 */
assembly::Operand_Size get_operand_size_from_storage(Storage const& storage,
    memory::Stack_Pointer& stack)
{
    namespace m = matchit;
    m::Id<assembly::Stack::Offset> s;
    m::Id<Immediate> i;
    m::Id<Register> r;
    if (assembly::is_immediate_relative_address(storage))
        return assembly::Operand_Size::Doubleword;
    return m::match(storage)(
        m::pattern | m::as<assembly::Stack::Offset>(s) =
            [&] { return stack->get_operand_size_from_offset(*s); },
        m::pattern | m::as<Immediate>(i) =
            [&] { return assembly::get_operand_size_from_rvalue_datatype(*i); },
        m::pattern | m::as<Register>(r) =
            [&] { return assembly::get_operand_size_from_register(*r); },
        m::pattern | m::_ = [&] { return assembly::Operand_Size::Empty; });
}

/**
 * @brief Check if a storage device is a doubleword
 *
 * A string or a page relative address is an address, so it is a doubleword
 */
bool is_doubleword_storage_size(assembly::Storage const& storage,
    Stack_Pointer& stack,
    [[maybe_unused]] Stack_Frame& stack_frame)
{
    auto result{ false };
    std::visit(util::overload{
                   [&](std::monostate) {},
                   [&](common::Stack_Offset const& s) {
                       result = stack->get_operand_size_from_offset(s) ==
                                assembly::Operand_Size::Doubleword;
                   },
                   [&](Register const& s) {
                       result = assembly::is_doubleword_register(s);
                   },
                   [&](Immediate const& s) {
                       result = type::is_rvalue_data_type_string(s) or
                                assembly::is_immediate_relative_address(s);
                   },
               },
        storage);
    return result;
}

/**
 * @brief Get the accumulator of a value, or the second operand register
 *
 * A value computed for the temporary of the next instruction, which is not
 * the operand of an assignment, is the second operand of a binary expression,
 * so it is in w6 or x6
 */
Register Memory_Accessor::get_accumulator_with_rvalue_context(
    Storage const& device)
{
    auto size =
        get_word_size_from_storage(device, stack, get_frame_in_memory());
    return table_accessor.next_ir_instruction_is_temporary() and
                   not table_accessor.last_ir_instruction_is_assignment()
               ? get_second_register_for_binary_operand(size)
               : accumulator_accessor.get_accumulator_register_from_size(size);
}

/**
 * @brief Get the accumulator of a size, or the second operand register
 */
Register Memory_Accessor::get_accumulator_with_rvalue_context(
    assembly::Operand_Size size)
{
    return table_accessor.next_ir_instruction_is_temporary() and
                   not table_accessor.last_ir_instruction_is_assignment()
               ? get_second_register_for_binary_operand(size)
               : accumulator_accessor.get_accumulator_register_from_size(size);
}

namespace detail {

/**
 * @brief Check if an immediate is a binary expression of two literals
 *
 *  Example:
 *
 *    ((2:int:4) + (3:int:4):word:8)   // true
 */
constexpr bool arm64_is_binary_datatype_expression(Immediate const& immediate)
{
    auto rvalue = type::get_value_from_rvalue_data_type(immediate);

    if (type::get_unquoted_space_offsets(rvalue).size() != 2)
        return false;
    auto test = type::from_rvalue_binary_expression(rvalue);
    if (!type::is_rvalue_data_type(std::get<0>(test)))
        return false;
    if (!type::is_rvalue_data_type(std::get<1>(test)))
        return false;
    return true;
}

/**
 * @brief Check if an immediate is a binary expression of temporaries
 */
constexpr bool arm64_is_temporary_datatype_binary_expression(
    Immediate const& immediate)
{
    return type::is_temporary_datatype_binary_expression(
        type::get_value_from_rvalue_data_type(immediate));
}

/**
 * @brief Get the accumulator register of a size
 *
 * A byte or halfword is in a word register, since ARM64 has no narrower
 * general purpose registers. A signal register is returned once in place
 * of the accumulator.
 */
Register Accumulator_Accessor::get_accumulator_register_from_size(
    assembly::Operand_Size size)
{
    namespace m = matchit;
    if (*signal_register_ != Register::wzr) {
        auto designated = *signal_register_;
        *signal_register_ = Register::wzr;
        return designated;
    }
    return m::match(size)(
        m::pattern |
            assembly::Operand_Size::Doubleword = [&] { return Register::x8; },
        m::pattern | assembly::Operand_Size::Halfword =
            [&] { return Register::w8; }, // No 16-bit direct
        m::pattern | assembly::Operand_Size::Byte =
            [&] { return Register::w8; }, // No 8-bit direct
        m::pattern | m::_ = [&] { return Register::w8; });
}

/**
 * @brief Get the storage device of an operand of a binary expression
 *
 * A literal is an immediate, a local is its device, and a temporary is the
 * accumulator of its size
 */
Device_Accessor::Device Device_Accessor::get_operand_rvalue_device(
    RValue const& rvalue)
{
    if (type::is_rvalue_data_type(rvalue))
        return type::get_rvalue_datatype_from_string(rvalue);
    else if (is_lvalue_allocated_in_memory(rvalue))
        return get_device_by_lvalue(rvalue);
    else {
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
        if (rvalue == "argc")
            return Register::x19;
#endif
        auto& frame = stack_frame_.get_stack_frame();
        auto size = assembly::get_operand_size_from_size(
            object::lvalue_size_at_temporary_object_address(rvalue, frame));
        if (size == Operand_Size::Doubleword)
            return Register::x8;
        else
            return Register::w8;
    }
}

/**
 * @brief Get the address and instructions of a vector element
 *
 * A global vector element is an offset from the page address of the vector
 * in x6, and a local vector element is its address from the stack pointer
 * in x15.
 *
 *  Example:
 *
 *    x = mess[1];
 *
 *  Becomes, where mess is global:
 *
 *    adrp x6, mess@PAGE
 *    add x6, x6, mess@PAGEOFF
 *    ldr x9, [x6, #8]
 */
Address_Accessor::Address
Address_Accessor::get_lvalue_address_and_from_unary_and_vectors(
    Address& instructions,
    LValue const& lvalue,
    std::size_t instruction_index)
{
    auto vector_accessor = Vector_Accessor{ table_, stack_frame_ };
    auto lhs = type::from_lvalue_offset(lvalue);
    auto offset = type::from_decay_offset(lvalue);
    auto& inst = instructions.second;
    m::match(lvalue)(
        m::pattern | m::app(is_global_vector, true) =
            [&] {
                credence_assert(table_->vectors.contains(lhs));
                if (table_->globals.is_pointer(lhs)) {
                    auto offset_storage =
                        vector_accessor.get_offset_address(lvalue, offset);
                    auto global_offset =
                        assembly::page_offset_upper_immediate(lhs);
                    auto global_offset_page =
                        assembly::page_offset_lower_immediate(lhs);
                    auto vector_offset = direct_immediate(
                        fmt::format("[x6, #{}]", offset_storage.first));
                    auto address_offset = offset_storage.first == 0UL
                                              ? direct_immediate("[x6]")
                                              : vector_offset;
                    arm64_add__asm(inst, adrp, x6, global_offset);
                    arm64_add__asm(inst, add, x6, x6, global_offset_page);
                    instructions.first = address_offset;
                }
            },
        m::pattern | m::app(is_vector_offset, true) =
            [&] {
                credence_assert(table_->vectors.contains(lhs));
                auto& vector = table_->vectors.at(lhs).get();
                auto vector_s =
                    stack_->get_stack_offset_from_table_vector_index(
                        lhs, offset, vector);
                auto offset_immediate = u32_int_immediate(vector_s);
                auto vector_type = type::get_type_from_rvalue_data_type(
                    vector.data.at(offset));
                auto size = vector_type == "string"
                                ? assembly::Operand_Size::Doubleword
                                : assembly::Operand_Size::Word;
                stack_->set(vector_s, size);
                if (stack_->get_aad_local_size() == 0UL) {
                    arm64_add__asm(inst, add, x15, sp, offset_immediate);
                    instructions.first = Register::x15;
                } else
                    instructions.first = vector_s;
                flag_accessor_.set_instruction_flag(
                    arm64::detail::flags::Vector_Storage,
                    instruction_index + 1);
            });

    return instructions;
}

/**
 * @brief Get the address of an lvalue and the instructions that load it
 *
 * A signal register is the address, and is used once. A local on the stack
 * is loaded into the stack move register, so the assignment is a register
 * operation followed by a store, and the load is only for an operation that
 * reads the lvalue, such as `++x`.
 */
Address_Accessor::Address
Address_Accessor::get_arm64_lvalue_and_insertion_instructions(
    LValue const& lvalue,
    std::size_t instruction_index,
    Device_Accessor& device_accessor)
{
    Address instructions{ assembly::O_NUL, {} };
    auto* signal_register = register_accessor_.signal_register;
    if (*signal_register != Register::wzr) {
        Storage rr = *signal_register;
        *signal_register = Register::wzr;
        instructions.first = rr;
        return instructions;
    }
    get_lvalue_address_and_from_unary_and_vectors(
        instructions, lvalue, instruction_index);
    if (instructions.first == assembly::O_NUL)
        instructions.first = device_accessor.get_device_by_lvalue(lvalue);
    if (is_variant(common::Stack_Offset, instructions.first)) {
        auto str_r = register_accessor_.get_available_register(
            device_accessor.get_word_size_from_lvalue(lvalue));
        // the load is needed by a read-modify-write, such as `++x`, and the
        // register is stored back after the assignment
        arm64_add__asm(instructions.second, ldr, str_r, instructions.first);
        instructions.first = str_r;
    }
    return instructions;
}

/**
 * @brief Get the total size of the devices of the locals
 */
Size Device_Accessor::get_size_of_address_table()
{
    auto size = 0UL;
    for (auto const& device : address_table) {
        size += static_cast<std::size_t>(
            get_word_size_from_storage(device.second, stack_, stack_frame_));
    }
    return size;
}

/**
 * @brief Take the next local register for an unnamed value
 */
Register Device_Accessor::get_available_storage_register(
    assembly::Operand_Size size)
{
    auto registers = register_accessor_.get_register_list_by_size(size);
    register_id.insert(id_index);
    credence_assert(registers.size() > id_index);
    Register storage = registers.at(id_index);
    address_table.insert("_", storage);
    id_index++;
    return storage;
}

/**
 * @brief Take the next local register for an lvalue
 */
void Device_Accessor::set_word_or_doubleword_register(LValue const& lvalue,
    assembly::Operand_Size size)
{
    auto registers = register_accessor_.get_register_list_by_size(size);
    register_id.insert(id_index);
    credence_assert(registers.size() > id_index);
    Register storage = registers.at(id_index);
    address_table.insert(lvalue, storage);
    id_index++;
}

/**
 * @brief Get the storage device of an lvalue
 *
 * `argc` is in x19, a local on the stack is its offset, and any other local
 * is its register
 */
Device_Accessor::Device Device_Accessor::get_device_by_lvalue(
    LValue const& lvalue)
{
    if (lvalue == "argc")
        return Register::x19;
    else if (stack_->is_allocated(lvalue))
        return stack_->get(lvalue).first;
    else
        return address_table.at(lvalue);
}

/**
 * @brief Get an available register storage device, use the stack if none
 * available
 */
assembly::Storage Register_Accessor::get_available_register(
    assembly::Operand_Size size)
{
    if (*signal_register != Register::wzr) {
        auto rr = *signal_register;
        *signal_register = Register::wzr;
        return rr;
    }
    return size == assembly::Operand_Size::Doubleword ? Register::x10
                                                      : Register::w10;
}

/**
 * @brief Allocate a register or stack slot for an lvalue of a size
 *
 * A char is a word, since every load and store of a local is a word or
 * doubleword register. A word or doubleword is on the stack when the frame
 * makes a call, no local register is left, or its address is taken.
 */
void Device_Accessor::insert_lvalue_to_device(LValue const& lvalue,
    Operand_Size size)
{
    if (is_lvalue_allocated_in_memory(lvalue))
        return;
    auto& table_ = address_accessor_.table_;
    auto stack_frame_has_jump = ir::stack_frame_contains_call_instruction(
        table_->get_stack_frame(stack_frame_.symbol),
        table_accessor_.get_ir_instructions());
    switch (size) {
        case assembly::Operand_Size::Empty:
            stack_->set_address_from_size(lvalue, size);
            break;
        // a char is loaded and stored with a word register, so it is a word
        case assembly::Operand_Size::Byte:
        case assembly::Operand_Size::Halfword:
            size = assembly::Operand_Size::Word;
            [[fallthrough]];
        case assembly::Operand_Size::Word:
        case assembly::Operand_Size::Doubleword: {
            if (stack_frame_has_jump or
                register_id.size() == registers::available_word.size() or
                is_address_taken(lvalue)) {
                stack_->set_address_from_size(lvalue, size);
                address_table.insert(lvalue, stack_->get(lvalue).first);
            } else
                set_word_or_doubleword_register(lvalue, size);
        } break;
    }
}

/**
 * @brief Allocate a register or stack slot for an lvalue of its type
 *
 * The size is the size of the value of the lvalue in the object table
 */
void Device_Accessor::insert_lvalue_to_device(LValue const& lvalue,
    INLINE_DEBUG)
{
    auto& table_ = address_accessor_.table_;
    auto& frame = stack_frame_.get_stack_frame();
    credence_assert(frame.locals.is_defined(lvalue));

    if (is_lvalue_allocated_in_memory(lvalue))
        return;

    auto stack_frame_has_jump = ir::stack_frame_contains_call_instruction(
        table_->get_stack_frame(stack_frame_.symbol),
        table_accessor_.get_ir_instructions());
    auto rvalue = object::get_rvalue_at_lvalue_object_storage(
        lvalue, frame, table_->vectors, DEBUG_SOURCE);
    auto operand = assembly::get_operand_size_from_rvalue_datatype(rvalue);

    switch (operand) {
        case assembly::Operand_Size::Empty:
            stack_->set_address_from_size(lvalue, operand);
            break;
        // a char is loaded and stored with a word register, so it is a word
        case assembly::Operand_Size::Byte:
        case assembly::Operand_Size::Halfword:
            operand = assembly::Operand_Size::Word;
            [[fallthrough]];
        case assembly::Operand_Size::Word:
        case assembly::Operand_Size::Doubleword: {
            if (stack_frame_has_jump or
                register_id.size() == registers::available_word.size() or
                is_address_taken(lvalue)) {
                stack_->set_address_from_size(lvalue, operand);
                address_table.insert(lvalue, stack_->get(lvalue).first);
            } else
                set_word_or_doubleword_register(lvalue, operand);
        } break;
    }
}

/**
 * @brief Check if the address of an lvalue is taken in its stack frame
 *
 * A register has no address, so the lvalue is allocated on the stack. The
 * IR of the frame is scanned for an address-of the lvalue.
 *
 *  Example:
 *
 *    p = &y;
 *
 *  Becomes:
 *
 *    is_address_taken("y")   // true
 */
bool Device_Accessor::is_address_taken(LValue const& lvalue)
{
    auto const& frame =
        address_accessor_.table_->get_stack_frame(stack_frame_.symbol);
    auto& instructions = table_accessor_.get_ir_instructions();
    auto end = std::min(static_cast<std::size_t>(frame.address_location[1]) + 1,
        instructions.size());
    for (auto index = static_cast<std::size_t>(frame.address_location[0]);
        index < end;
        index++) {
        auto const& inst = instructions.at(index);
        if (std::get<0>(inst) != ir::Instruction::MOV)
            continue;
        auto rvalue = ir::get_rvalue_from_mov_qaudruple(inst).first;
        if (type::is_address_of_expression(rvalue) and
            type::get_unary_rvalue_reference(rvalue) == lvalue)
            return true;
    }
    return false;
}

/**
 * @brief Allocate a device for the offset of a vector element
 */
void Device_Accessor::set_vector_offset_to_storage_space(LValue const& lvalue)
{
    auto offset_lvalue =
        fmt::format("__{}_vector_offset_{}", lvalue, ++vector_index);
    insert_lvalue_to_device(offset_lvalue);
}

/**
 * @brief Get the size of an rvalue from its type or its device
 *
 * A `word` has no size of its own, so its size is the size of its device
 */
Size Device_Accessor::get_size_from_rvalue_reference(RValue const& rvalue)
{
    if (!type::is_rvalue_data_type_a_type(rvalue, "word"))
        return type::get_size_from_rvalue_data_type(rvalue);
    if (is_lvalue_allocated_in_memory(rvalue)) {
        auto lvalue_reference = get_device_by_lvalue(rvalue);
        if (is_variant(Register, lvalue_reference))
            return assembly::get_size_from_register(
                std::get<Register>(lvalue_reference));
        if (is_variant(Immediate, lvalue_reference))
            return get_size_from_rvalue_data_type(
                rvalue, std::get<Immediate>(lvalue_reference));
        if (is_variant(common::Stack_Offset, lvalue_reference))
            return assembly::get_size_from_operand_size(
                stack_->get_operand_size_from_offset(
                    std::get<common::Stack_Offset>(lvalue_reference)));
    }
    credence_error("unreachable");
    return 0UL;
}

/**
 * @brief Get the size of a `word` from the expression it was computed from
 *
 * A binary expression of temporaries has the size of its temporaries, a
 * binary expression of literals the size of its left-hand-side, and a unary
 * expression the size of its operand
 */
Size Device_Accessor::get_size_from_rvalue_data_type(LValue const& lvalue,
    Immediate const& rvalue)
{
    auto& table_ = address_accessor_.table_;
    auto& frame = stack_frame_.get_stack_frame();

    if (!type::is_rvalue_data_type_a_type(rvalue, "word"))
        return type::get_size_from_rvalue_data_type(rvalue);

    return m::match(rvalue)(
        m::pattern |
            m::app(arm64_is_temporary_datatype_binary_expression, true) =
            [&] {
                return object::get_size_of_temporary_binary_rvalue(
                    type::get_value_from_rvalue_data_type(rvalue), frame);
            },
        m::pattern | m::app(arm64_is_binary_datatype_expression, true) =
            [&] {
                auto [left, right, _] =
                    type::from_rvalue_binary_expression(rvalue);
                return type::get_size_from_rvalue_data_type(left);
            },
        m::pattern | m::app(type::is_unary_data_type_expression, true) =
            [&] {
                auto lvalue_reference = type::get_unary_rvalue_reference(
                    type::get_value_from_rvalue_data_type(rvalue));
                if (is_lvalue_allocated_in_memory(lvalue_reference))
                    return assembly::get_size_from_operand_size(
                        get_word_size_from_lvalue(lvalue_reference));
                else
                    return type::get_size_from_rvalue_data_type(
                        object::get_rvalue_at_lvalue_object_storage(
                            lvalue_reference, frame, table_->vectors));
            },
        m::pattern | m::_ =
            [&] {
                auto immediate = object::get_rvalue_at_lvalue_object_storage(
                    lvalue, frame, table_->vectors);
                return type::get_size_from_rvalue_data_type(immediate);
            }

    );
}

/**
 * @brief Check if an lvalue has a register or stack slot
 */
bool Device_Accessor::is_lvalue_allocated_in_memory(LValue const& lvalue)
{
    return address_table.contains(lvalue) or stack_->is_allocated(lvalue);
}

} // namespace detail

} // namespace credence::target::arm64::memory
