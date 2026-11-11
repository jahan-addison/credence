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

#include "assembly.h"                        // for Register, is_immediate_...
#include "credence/target/common/flags.h"    // for Flag_Accessor, Instruct...
#include "credence/target/common/memory.h"   // for is_vector_offset
#include "credence/target/common/types.h"    // for Stack_Offset
#include "stack.h"                           // for Stack
#include <credence/error.h>                  // for throw_compiletime_error
#include <credence/object.h>                 // for Object, LValue, RValue
#include <credence/symbol.h>                 // for Symbol_Table
#include <credence/target/common/assembly.h> // for make_direct_immediate
#include <credence/types.h>                  // for from_decay_offset, from...
#include <credence/util.h>                   // for is_numeric, overload
#include <deque>                             // for deque
#include <fmt/format.h>                      // for format
#include <matchit.h>                         // for matchit
#include <string>                            // for basic_string, char_traits
#include <string_view>                       // for basic_string_view
#include <utility>                           // for pair
#include <variant>                           // for variant, monostate, visit

/****************************************************************************
 * x86-64 Memory Accessor
 *
 * The memory accessor facade of the x86-64 target, and the address of each
 * lvalue. A local is on the stack below rbp, a global is relative to rip,
 * and a vector element is an offset in its vector.
 *
 *  Example:
 *
 *    main() {
 *      auto x, arr[5];
 *      extrn value;
 *      x = value;
 *      arr[2] = 42;
 *    }
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rip + value]   // a global, from rip
 *    mov dword ptr [rbp - 4], eax       // x
 *    mov dword ptr [rbp - 12], 42       // arr[2]
 *
 ****************************************************************************/

/****************************************************************************
 * Register selection table:
 *
 *   rax      = The default "accumulator" register for expression expansion,
 *              the return value, and the number of a syscall
 *   rdi, r8, r9, rsi, rdx, rcx
 *            = The arguments of a call to a function of the program
 *   rdi, rsi, rdx, rcx, r8, r9
 *            = The arguments of a call to the standard library, System V
 *   rdi, rsi, rdx, r10, r8, r9
 *            = The arguments of a syscall
 *   xmm0 - xmm7
 *            = The float and double arguments of the standard library
 *   xmm7     = The store register of a float or double literal
 *   rcx      = The address register of the address-of operator, and the
 *              shift count of a shift by a register, in cl
 *   rdx      = The high half of the dividend and the remainder of `idiv`
 *   r10      = The address register of a dereference
 *   r11      = The scratch register of the right-hand-side of a binary
 *              expression, the divisor of `idiv`, and the value of a
 *              global argument
 *   r15      = The address of argc, argv on the stack
 *   rbp      = The frame pointer
 *
 *   Locals, parameters, vectors, and vector offsets are always on the
 *   stack below rbp
 *
 *****************************************************************************/

namespace credence::target::x86_64::memory {

namespace m = matchit;

namespace detail {

/**
 * @brief Get the storage device of an operand of a binary expression
 *
 * `argc` is at the address in r15 and an element of `argv` is after it, a
 * literal is an immediate, a local is its stack slot, and a temporary is in
 * rax
 *
 *  Examples:
 *
 *    get_register_for_binary_operator("argc", stack)      // [r15]
 *    get_register_for_binary_operator("argv[1]", stack)   // [r15 + 8 * 2]
 */
assembly::Storage Register_Accessor::get_register_for_binary_operator(
    RValue const& rvalue,
    Stack_Pointer& stack)
{
    // the kernel passes argc on the stack, then the pointers of argv
    if (rvalue == "argc")
        return common::assembly::make_direct_immediate("[r15]");
    if (type::from_lvalue_offset(rvalue) == "argv") {
        auto offset = type::from_decay_offset(rvalue);
        if (!util::is_numeric(offset) and
            not address_accessor.is_lvalue_storage_type(offset, "int"))
            throw_compiletime_error(
                fmt::format(
                    "invalid argv access, argv has malformed offset '{}'",
                    offset),
                rvalue);
        auto offset_integer = type::integral_from_type_ulint(offset) + 1;
        return common::assembly::make_direct_immediate(
            fmt::format("[r15 + 8 * {}]", offset_integer));
    }
    if (type::is_rvalue_data_type(rvalue))
        return type::get_rvalue_datatype_from_string(rvalue);

    if (stack->contains(rvalue))
        return stack->get(rvalue).first;

    return assembly::Register::rax;
}

/**
 * @brief Take the next argument register of a size, or a stack slot when
 * none is left
 *
 * The quadword and doubleword lists are the same registers, so both are
 * taken together, and the next argument after `rdi` is `r8d`, not `edi`
 */
assembly::Storage Register_Accessor::get_available_register(
    assembly::Operand_Size size,
    Stack_Pointer& stack)
{
    if (available_qword.empty() or available_dword.empty())
        return stack->allocate(size);
    Storage storage = size == Operand_Size::Qword ? available_qword.front()
                                                  : available_dword.front();
    available_qword.pop_front();
    available_dword.pop_front();
    return storage;
}

/**
 * @brief Get the address of an lvalue and the instructions that load it
 *
 * A dereference loads the pointer into rax and flags the next instruction
 * as indirect, a global vector element is relative to rip, a local vector
 * element is its offset in the vector, and a local is its stack slot. The
 * size prefix of a global is left out for an address, such as `lea`.
 */
Address_Accessor::Address
Address_Accessor::get_lvalue_address_and_insertion_instructions(
    LValue const& lvalue,
    std::size_t instruction_index,
    bool use_prefix)
{
    auto vector_accessor = Vector_Accessor{ table_ };
    assembly::Instruction_Pair instructions{ Register::eax, {} };
    auto lhs = type::from_lvalue_offset(lvalue);
    auto offset = type::from_decay_offset(lvalue);

    if (type::is_dereference_expression(lvalue)) {
        auto storage =
            stack_->get(type::get_unary_rvalue_reference(lvalue)).first;
        x8664_add__asm(instructions.second, mov, Register::rax, storage);
        flag_accessor_.set_instruction_flag(
            common::flag::Indirect, instruction_index + 1);
        instructions.first = Register::rax;
    } else if (is_global_vector(lhs)) {
        if (table_->globals.is_pointer(lhs)) {
            auto rip_storage =
                vector_accessor.get_offset_address(lvalue, offset);
            auto prefix = storage_prefix_from_operand_size(rip_storage.second);
            auto rip_arithmetic =
                rip_storage.first == 0UL
                    ? lhs
                    : fmt::format("{}+{}", lhs, rip_storage.first);
            rip_arithmetic =
                use_prefix
                    ? fmt::format("{} [rip + {}]", prefix, rip_arithmetic)
                    : fmt::format("[rip + {}]", rip_arithmetic);
            instructions.first =
                common::assembly::make_array_immediate(rip_arithmetic);
        }
    } else if (is_vector_offset(lvalue)) {
        credence_assert(table_->vectors.contains(lhs));
        auto& vector = table_->vectors.at(lhs).get();
        instructions.first = stack_->get_stack_offset_from_table_vector_index(
            lhs, offset, vector);

    } else if (stack_->is_allocated(lvalue)) {
        instructions.first = stack_->get(lvalue).first;
    }
    return instructions;
}

/**
 * @brief Check if a storage device is a quadword
 *
 * A string, an address relative to rip, or an element of `argv` is an
 * address, so it is a quadword
 */
bool Address_Accessor::is_qword_storage_size(
    assembly::Storage const& storage) const
{
    auto result{ false };

    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](common::Stack_Offset const& s) {
                result = stack_->get_operand_size_from_offset(s) ==
                         Operand_Size::Qword;
            },
            [&](Register const& s) { result = assembly::is_qword_register(s); },
            [&](Immediate const& s) {
                result = type::is_rvalue_data_type_string(s) or
                         assembly::is_immediate_r15_address_offset(s) or
                         assembly::is_immediate_rip_address_offset(s);
            },
        },
        storage);

    return result;
}

} // namespace detail

} // namespace memory