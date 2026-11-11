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

#include "inserter.h"

#include "assembly.h"                           // for inserter, Mnemonic
#include "credence/error.h"                     // for credence_assert, thr...
#include "credence/target/common/accessor.h"    // for Buffer_Accessor
#include "credence/target/common/assembly.h"    // for direct_immediate
#include "credence/target/common/memory.h"      // for is_temporary, is_imm...
#include "credence/target/common/runtime.h"     // for is_stdlib_function
#include "credence/target/common/types.h"       // for Immediate, Table_Poi...
#include "credence/util.h"                      // for is_variant, sv, __so...
#include "memory.h"                             // for Memory_Accessor, Ins...
#include "runtime.h"                            // for Library_Call_Inserter
#include "stack.h"                              // for Stack
#include "syscall.h"                            // for syscall_arguments_t
#include "visitor.h"                            // for IR_Instruction_Visitor
#include <credence/object.h>                    // for RValue, Function
#include <credence/target/common/flags.h>       // for Instruction_Flag
#include <credence/target/common/stack_frame.h> // for Stack_Frame, Locals
#include <cstddef>                              // for size_t
#include <fmt/format.h>                         // for format
#include <matchit.h>                            // for App, pattern, app
#include <memory>                               // for shared_ptr
#include <tuple>                                // for get, tuple
#include <variant>                              // for variant, get, operat...

/****************************************************************************
 * x86-64 Instruction Inserters
 *
 * Translates each kind of IR expression into x86-64 instructions. See
 * common/inserter.h for the interfaces. The value of an expression is in the
 * accumulator, eax or rax, and a right-hand-side temporary is in r11d or
 * r11.
 *
 *  Example:
 *
 *    _t2 = y * (2:int:4);
 *    _t3 = x + _t2;
 *    z = _t3;
 *    _t7 = z > (10:int:4);
 *    IF _t7 GOTO _L6;
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rbp - 8]
 *    imul eax, 2
 *    mov r11d, eax                    // _t2 is the right-hand-side
 *    mov eax, dword ptr [rbp - 4]
 *    add eax, r11d
 *    mov dword ptr [rbp - 12], eax
 *    mov eax, dword ptr [rbp - 12]
 *    cmp eax, 10
 *    jg ._L6__main                    // the IF is fused with its comparison
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

namespace credence::target::x86_64 {

namespace {

/**
 * @brief Check if the return value of a function is a qword
 *
 * A pointer, a pointer parameter, or a string is a qword
 */
bool is_qword_return_value(object::Function& frame)
{
    if (!frame.ret.has_value())
        return false;
    auto const& [value, rvalue] = *frame.ret;
    return frame.locals.is_pointer(value) or frame.locals.is_pointer(rvalue) or
           frame.is_pointer_parameter(rvalue) or
           type::is_rvalue_data_type_string(value) or
           type::is_rvalue_data_type_string(rvalue);
}

} // namespace

namespace m = matchit;

/**
 * @brief Get the operand size (word size) of a storage device
 */
assembly::Operand_Size get_operand_size_from_storage(Storage const& storage,
    memory::Stack_Pointer& stack)
{
    m::Id<assembly::Stack::Offset> s;
    m::Id<Immediate> i;
    m::Id<Register> r;
    return m::match(storage)(
        m::pattern | m::as<assembly::Stack::Offset>(s) =
            [&] { return stack->get_operand_size_from_offset(*s); },
        m::pattern | m::as<Immediate>(i) =
            [&] { return assembly::get_operand_size_from_rvalue_datatype(*i); },
        m::pattern | m::as<Register>(r) =
            [&] { return assembly::get_operand_size_from_register(*r); },
        m::pattern | m::_ = [&] { return Operand_Size::Empty; });
}

/**
 * @brief Set up the stack frame of a function at its `BeginFunc`
 *
 * The label of the function is the IR instruction before `BeginFunc`. The
 * kernel passes `argc` at the top of the stack of the entry point, with the
 * pointers of `argv` after it, so `main` keeps its address in r15.
 *
 *  Example:
 *
 *    main(argc, argv) {
 *
 *  Becomes:
 *
 *    lea r15, [rsp]
 *    push rbp
 *    mov rbp, rsp
 */
void Instruction_Inserter::setup_stack_frame_in_function(
    ir::Instructions const& ir_instructions,
    IR_Instruction_Visitor& visitor,
    int index)
{
    auto stack_frame = accessor_->get_frame_in_memory();
    auto symbol = std::get<1>(ir_instructions.at(index - 1));
    auto name = type::get_label_as_human_readable(symbol);
    stack_frame.set_stack_frame(name);
    stack_frame.symbol = name;
    if (name == "main") {
        // setup argc, argv
        auto argc_argv =
            common::runtime::argc_argv_kernel_runtime_access(stack_frame);
        if (argc_argv.first) {
            auto& instructions =
                accessor_->instruction_accessor->get_instructions();
            auto argc_address = direct_immediate("[rsp]");
            x8664_add__asm(instructions, lea, r15, argc_address);
        }
    }
    visitor.from_func_start_ita(name);
}

/**
 * @brief Translate each IR instruction with the visitor
 *
 * The visitor of each instruction is called in order, and a temporary held
 * in the accumulator is released after the instruction that follows it
 */
void Instruction_Inserter::from_ir_instructions(
    ir::Instructions const& ir_instructions)
{
    auto ir_visitor = IR_Instruction_Visitor{ accessor_ };
    for (std::size_t index = 0; index < ir_instructions.size(); index++) {
        auto inst = ir_instructions[index];
        ir_visitor.set_iterator_index(index);
        accessor_->table_accessor.set_ir_iterator_index(index);
        auto held = accessor_->accumulator_temporary;
        ir::Instruction ita_inst = std::get<0>(inst);
        m::match(ita_inst)(
            m::pattern | ir::Instruction::FUNC_START =
                [&] {
                    setup_stack_frame_in_function(
                        ir_instructions, ir_visitor, index);
                },
            m::pattern | ir::Instruction::FUNC_END =
                [&] { ir_visitor.from_func_end_ita(); },
            m::pattern |
                ir::Instruction::MOV = [&] { ir_visitor.from_mov_ita(inst); },
            m::pattern |
                ir::Instruction::PUSH = [&] { ir_visitor.from_push_ita(inst); },
            m::pattern |
                ir::Instruction::POP = [&] { ir_visitor.from_pop_ita(); },
            m::pattern |
                ir::Instruction::CALL = [&] { ir_visitor.from_call_ita(inst); },
            m::pattern | ir::Instruction::JMP_E =
                [&] { ir_visitor.from_jmp_e_ita(inst); },
            m::pattern |
                ir::Instruction::LOCL = [&] { ir_visitor.from_locl_ita(inst); },
            m::pattern |
                ir::Instruction::GOTO = [&] { ir_visitor.from_goto_ita(inst); },
            m::pattern | ir::Instruction::RETURN =
                [&] { ir_visitor.from_return_ita(inst); },
            m::pattern |
                ir::Instruction::LEAVE = [&] { ir_visitor.from_leave_ita(); },
            m::pattern | ir::Instruction::LABEL =
                [&] { ir_visitor.from_label_ita(inst); },
            m::pattern |
                ir::Instruction::IF = [&] { ir_visitor.from_if_ita(inst); }

        );
        // the accumulator holds a temporary until the next instruction
        if (accessor_->accumulator_temporary == held)
            accessor_->accumulator_temporary.reset();
    }
}

/**
 * @brief Resolve the device of each argument of the next call
 *
 * The devices are resolved before any argument is loaded into its
 * register, so an argument that would be overwritten, such as a parameter
 * in an argument register, is stored in an argument slot. The return value
 * of the last call is in eax or rax.
 */
syscall_ns::syscall_arguments_t
Invocation_Inserter::get_operands_storage_from_argument_stack()
{
    Operand_Inserter operands{ accessor_ };
    syscall_ns::syscall_arguments_t arguments{};
    auto& caller_frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.get_table();
    for (std::size_t index = 0; index < stack_frame_.argument_stack.size();
        index++) {
        auto const& rvalue = stack_frame_.argument_stack.at(index);
        if (index < stack_frame_.argument_slots.size() and
            not stack_frame_.argument_slots.at(index).empty()) {
            arguments.emplace_back(
                accessor_->stack->get(stack_frame_.argument_slots.at(index))
                    .first);
            continue;
        }
        if (rvalue == "RET" and
            common::runtime::is_returning_library_function(
                stack_frame_.tail, common::assembly::Arch_Type::X8664)) {
            arguments.emplace_back(Register::eax);
            continue;
        }
        if (rvalue == "RET") {
            credence_assert(table->functions.contains(stack_frame_.tail));
            auto& tail_frame = table->functions.at(stack_frame_.tail).get();
            if (accessor_->address_accessor.is_lvalue_storage_type(
                    tail_frame.ret->first, "string") or
                caller_frame.is_pointer_in_stack_frame(tail_frame.ret->first) or
                is_qword_return_value(tail_frame))
                arguments.emplace_back(Register::rax);
            else
                arguments.emplace_back(Register::eax);
        } else if (is_global_argument(rvalue)) {
            arguments.emplace_back(insert_global_argument_to_stack(rvalue));
        } else if (caller_frame.is_parameter(rvalue) and
                   stack_frame_.symbol != "main") {
            arguments.emplace_back(insert_parameter_argument_to_stack(rvalue));
        } else {
            arguments.emplace_back(
                operands.get_operand_storage_from_rvalue(rvalue));
        }
    }
    return arguments;
}

/**
 * @brief Check if an argument of a call is a global or a global vector
 * element
 */
bool Invocation_Inserter::is_global_argument(RValue const& rvalue)
{
    auto const& table = accessor_->table_accessor.get_table();
    auto label = type::from_lvalue_offset(rvalue);
    return table->vectors.contains(label) and table->globals.is_pointer(label);
}

/**
 * @brief Store the value of a global argument in an argument slot
 *
 * The value is loaded through r11, since rax may hold the return value of
 * an earlier argument, and each global has its own slot
 *
 *  Example:
 *
 *    printf("%d\n", counter);
 *
 *  Becomes:
 *
 *    mov r11d, dword ptr [rip + counter]
 *    mov dword ptr [rbp - 4], r11d
 *    ...
 *    mov esi, dword ptr [rbp - 4]
 */
common::Stack_Offset Invocation_Inserter::insert_global_argument_to_stack(
    RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto label = type::from_lvalue_offset(rvalue);
    auto key = is_vector_offset(rvalue) ? type::from_decay_offset(rvalue)
                                        : std::string{ "0" };
    auto [offset, size] =
        accessor_->vector_accessor.get_offset_address(rvalue, key);
    auto value = size == Operand_Size::Qword ? Register::r11 : Register::r11d;
    auto address = offset == 0UL ? label : fmt::format("{}+{}", label, offset);
    x8664_add__asm(instructions,
        mov,
        value,
        direct_immediate(fmt::format("{} [rip + {}]",
            memory::storage_prefix_from_operand_size(size),
            address)));
    auto slot = fmt::format("_p_global_{}_{}",
        assembly::get_size_from_operand_size(size),
        ++argument_slot_index);
    accessor_->stack->set_address_from_accumulator(slot, value);
    auto slot_offset = accessor_->stack->get(slot).first;
    x8664_add__asm(instructions, mov, slot_offset, value);
    return slot_offset;
}

/**
 * @brief Store a parameter argument in an argument slot
 *
 * A parameter is in its argument register, which an earlier argument of
 * the call may be loaded into, so it is stored before any argument is
 * loaded
 *
 *  Example:
 *
 *    show(*s) {
 *      printf("%s\n", s);
 *    }
 *
 *  Becomes:
 *
 *    mov qword ptr [rbp - 8], rdi
 *    lea rdi, [rip + ._L_str1__]
 *    mov rsi, qword ptr [rbp - 8]
 *    call printf
 */
common::Stack_Offset Invocation_Inserter::insert_parameter_argument_to_stack(
    RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto storage = operand_inserter.get_operand_storage_from_parameter(rvalue);
    // a parameter of a frame with calls is already in its slot
    if (is_variant(common::Stack_Offset, storage))
        return std::get<common::Stack_Offset>(storage);
    auto value = std::get<Register>(storage);
    auto size = assembly::get_operand_size_from_register(value);
    auto slot = fmt::format("_p_parameter_{}_{}",
        assembly::get_size_from_operand_size(size),
        ++argument_slot_index);
    accessor_->stack->set_address_from_accumulator(slot, value);
    auto slot_offset = accessor_->stack->get(slot).first;
    x8664_add__asm(instructions, mov, slot_offset, value);
    return slot_offset;
}

/**
 * @brief Insert the value of an expression of two immediates
 *
 * The expression is computed at compile time and moved to the accumulator,
 * and the result of a relational expression is passed to the next
 * assignment with the signal register
 */
void Operand_Inserter::insert_from_immediate_rvalues(Immediate const& lhs,
    std::string const& op,
    Immediate const& rhs)
{
    auto instructions_accessor = accessor_->instruction_accessor;
    auto& instructions = instructions_accessor->get_instructions();
    auto& immediate_stack = accessor_->address_accessor.immediate_stack;
    auto accumulator = accessor_->accumulator_accessor;
    m::match(op)(
        m::pattern | m::app(type::is_binary_arithmetic_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_integral_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::get_operand_size_from_rvalue_datatype(lhs));
                x8664_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_relation_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_relational_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    Operand_Size::Byte);
                accessor_->set_signal_register(acc);
                x8664_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(assembly::x8664_is_bitwise_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_bitwise_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    assembly::get_operand_size_from_rvalue_datatype(lhs));
                if (!accessor_->table_accessor.is_ir_instruction_temporary())
                    x8664_add__asm(instructions, mov, acc, imm);
                else
                    immediate_stack.emplace_back(imm);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Get the argument register of a parameter
 *
 * A pointer parameter is a quadword register. `argc` of `main` is at the
 * address in r15, and an element of `argv` is after it.
 *
 *  Examples:
 *
 *    get_operand_storage_from_parameter("argc")      // [r15]
 *    get_operand_storage_from_parameter("argv[1]")   // [r15 + 8 * 2]
 */
Storage Operand_Inserter::get_operand_storage_from_parameter(
    RValue const& rvalue)
{
    auto& frame = stack_frame_.get_stack_frame();
    auto index_of = frame.get_index_of_parameter(rvalue);
    credence_assert_nequal(index_of, -1);
    // a parameter of a frame with calls is in its slot
    auto slot = fmt::format("__parameter_{}",
        type::get_unary_rvalue_reference(
            frame.parameters.at(static_cast<std::size_t>(index_of))));
    if (accessor_->stack->is_allocated(slot))
        return accessor_->stack->get(slot).first;
    // the argc and argv special cases
    if (frame.symbol == "main") {
        if (index_of == 0)
            return direct_immediate("[r15]");
        if (index_of == 1) {
            if (!is_vector_offset(rvalue))
                common::runtime::throw_runtime_error(
                    "invalid argv access, argv is a vector to strings", rvalue);
            auto offset = type::from_decay_offset(rvalue);
            if (!util::is_numeric(offset) and
                not accessor_->address_accessor.is_lvalue_storage_type(
                    offset, "int"))
                common::runtime::throw_runtime_error(
                    fmt::format(
                        "invalid argv access, argv has malformed offset '{}'",
                        offset),
                    rvalue);
            auto offset_integer = type::integral_from_type_ulint(offset) + 1;
            return direct_immediate(
                fmt::format("[r15 + 8 * {}]", offset_integer));
        }
    }
    if (frame.is_pointer_parameter(rvalue))
        return memory::registers::available_qword_register.at(index_of);
    else
        return memory::registers::available_dword_register.at(index_of);
}

/**
 * @brief Get the stack slot of an operand
 */
inline Storage Operand_Inserter::get_operand_storage_from_stack(
    RValue const& rvalue)
{
    auto [operand, operand_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rvalue, accessor_->instruction_accessor->size());
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    assembly::inserter(instructions, operand_inst);
    return operand;
}

/**
 * @brief Get the return register of the last call, eax or rax
 */
inline Storage Operand_Inserter::get_operand_storage_from_return()
{
    auto& tail_call = accessor_->table_accessor.get_table()
                          ->functions.at(stack_frame_.tail)
                          .get();
    if (tail_call.locals.is_pointer(tail_call.ret->first) or
        type::is_rvalue_data_type_string(tail_call.ret->first))
        return Register::rax;
    else
        return Register::eax;
}

/**
 * @brief Get the storage device of a literal operand
 *
 * A string, float, or double is the label of its literal relative to rip,
 * and any other literal is an immediate
 */
Storage Operand_Inserter::get_operand_storage_from_immediate(
    RValue const& rvalue)
{
    assembly::Storage storage{};
    auto immediate = type::get_rvalue_datatype_from_string(rvalue);
    auto type = type::get_type_from_rvalue_data_type(immediate);
    if (type == "string") {
        storage = assembly::make_asciz_immediate(accessor_->address_accessor
                .buffer_accessor.get_string_address_offset(
                    type::get_value_from_rvalue_data_type(immediate)));
        return storage;
    }
    if (type == "float") {
        storage = assembly::make_asciz_immediate(accessor_->address_accessor
                .buffer_accessor.get_float_address_offset(
                    type::get_value_from_rvalue_data_type(immediate)));
        return storage;
    }
    if (type == "double") {
        storage = assembly::make_asciz_immediate(accessor_->address_accessor
                .buffer_accessor.get_double_address_offset(
                    type::get_value_from_rvalue_data_type(immediate)));
        return storage;
    }
    storage = type::get_rvalue_datatype_from_string(rvalue);
    return storage;
}

/**
 * @brief Get the storage device of an rvalue, and insert its address
 * instructions
 *
 * A parameter is its register, a local its stack slot, `RET` the return
 * register, a unary expression is inserted, a literal is an immediate, and
 * a vector element is its address
 */
Storage Operand_Inserter::get_operand_storage_from_rvalue(RValue const& rvalue)
{
    auto& stack = accessor_->stack;
    auto& frame = stack_frame_.get_stack_frame();

    if (frame.is_parameter(rvalue))
        return get_operand_storage_from_parameter(rvalue);

    if (stack->is_allocated(rvalue))
        return get_operand_storage_from_stack(rvalue);

#if defined(__linux__)
    if (rvalue == "RET" and not stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664))
        return get_operand_storage_from_return();

#elif defined(__APPLE__) || defined(__bsdi__)
    if (rvalue == "RET" and not stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664))
        return get_operand_storage_from_return();
#elif defined(_WIN32) || defined(_WIN64)
    if (rvalue == "RET" and not stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664))
        return get_operand_storage_from_return();
#endif

    if (type::is_unary_expression(rvalue)) {
        auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
        return unary_inserter.insert_from_unary_operator_rvalue(rvalue);
    }

    if (type::is_rvalue_data_type(rvalue))
        return get_operand_storage_from_immediate(rvalue);

    auto [operand, operand_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rvalue, accessor_->instruction_accessor->size());
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    assembly::inserter(instructions, operand_inst);

    return operand;
}

/**
 * @brief Get the storage of an expression operand, an immediate, a register,
 * or an address
 *
 *  * A temporary is in the accumulator after it's computed, or in its stack
 *    slot
 *
 *  * A parameter is in its argument register
 *
 *  * A dereference, `*p`, loads from the address in the pointer through r10
 */
Storage Operand_Inserter::get_operand_storage_from_expression_operand(
    RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& frame = stack_frame_.get_stack_frame();
    auto const& held = accessor_->accumulator_temporary;
    if (type::is_rvalue_data_type(rvalue))
        return get_operand_storage_from_immediate(rvalue);
    if (held.has_value() and held->first == rvalue)
        return held->second;
    if (frame.is_parameter(rvalue) and
        not type::is_dereference_expression(rvalue))
        return get_operand_storage_from_parameter(rvalue);
    if (type::is_dereference_expression(rvalue)) {
        auto pointer = type::get_unary_rvalue_reference(rvalue);
        auto address = get_operand_storage_from_expression_operand(pointer);
        if (is_variant(Register, address))
            address = assembly::get_qword_register_from_dword(
                std::get<Register>(address));
        x8664_add__asm(instructions, mov, r10, address);
        auto value =
            get_pointee_size(pointer) == 8UL ? Register::r10 : Register::r10d;
        x8664_add__asm(instructions, mov, value, direct_immediate("[r10]"));
        return value;
    }
    if (type::is_temporary(rvalue) and accessor_->stack->is_allocated(rvalue))
        return accessor_->stack->get(rvalue).first;
    if (type::is_temporary(rvalue)) {
        auto expression_inserter = Expression_Inserter{ accessor_ };
        expression_inserter.insert_lvalue_at_temporary_object_address(rvalue);
        return accessor_->accumulator_temporary.has_value()
                   ? accessor_->accumulator_temporary->second
                   : Register::eax;
    }
    auto [operand, operand_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rvalue, instructions.size());
    assembly::inserter(instructions, operand_inst);
    return operand;
}

/**
 * @brief Load an expression operand into the accumulator of its size
 *
 * A byte, the value of a relational expression, is zero-extended
 */
Register Operand_Inserter::insert_operand_into_accumulator(RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto storage = get_operand_storage_from_expression_operand(rvalue);
    auto size = get_operand_size_from_storage(storage, accessor_->stack);
    if (size == Operand_Size::Byte) {
        if (is_variant(Immediate, storage))
            x8664_add__asm(instructions, mov, eax, storage);
        else
            x8664_add__asm(instructions, movzx, eax, storage);
        return Register::eax;
    }
    auto accumulator =
        size == Operand_Size::Qword ? Register::rax : Register::eax;
    if (!is_variant(Register, storage) or
        std::get<Register>(storage) != accumulator)
        x8664_add__asm(instructions, mov, accumulator, storage);
    return accumulator;
}

/**
 * @brief Insert the value in the accumulator into an lvalue
 *
 * The accumulator is narrowed or sign-extended to the size of the lvalue
 */
void Operand_Inserter::insert_from_accumulator_to_lvalue(LValue const& lhs,
    Register accumulator)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& frame = stack_frame_.get_stack_frame();
    // a store through a pointer is at the address in r10
    if (type::is_dereference_expression(lhs)) {
        auto pointer = type::get_unary_rvalue_reference(lhs);
        auto address = get_operand_storage_from_expression_operand(pointer);
        if (is_variant(Register, address))
            address = assembly::get_qword_register_from_dword(
                std::get<Register>(address));
        x8664_add__asm(instructions, mov, r10, address);
        auto value = accumulator;
        if (get_pointee_size(pointer) == 8UL and value == Register::eax) {
            x8664_add__asm(instructions, movsxd, rax, eax);
            value = Register::rax;
        } else if (get_pointee_size(pointer) != 8UL)
            value = assembly::get_dword_register_from_qword(value);
        x8664_add__asm(instructions, mov, direct_immediate("[r10]"), value);
        return;
    }
    Storage destination{};
    if (frame.is_parameter(lhs) and not type::is_dereference_expression(lhs))
        destination = get_operand_storage_from_parameter(lhs);
    else {
        if (!type::is_unary_expression(lhs) and not is_vector_offset(lhs) and
            not accessor_->stack->is_allocated(lhs))
            accessor_->stack->set_address_from_accumulator(lhs, accumulator);
        auto [lhs_storage, storage_inst] =
            accessor_->address_accessor
                .get_lvalue_address_and_insertion_instructions(
                    lhs, instructions.size());
        assembly::inserter(instructions, storage_inst);
        destination = lhs_storage;
    }
    auto size = get_operand_size_from_storage(destination, accessor_->stack);
    auto source = accumulator;
    if (size == Operand_Size::Byte)
        source = Register::al;
    else if (size == Operand_Size::Qword and source == Register::eax) {
        x8664_add__asm(instructions, movsxd, rax, eax);
        source = Register::rax;
    } else if (size == Operand_Size::Dword)
        source = assembly::get_dword_register_from_qword(source);
    x8664_add__asm(instructions, mov, destination, source);
}

/**
 * @brief Get the size of the object a pointer points to
 *
 * A parameter points to a word
 */
Size Operand_Inserter::get_pointee_size(LValue const& pointer)
{
    auto& frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.get_table();
    if (!frame.locals.is_pointer(pointer))
        return 4UL;
    auto data = object::get_rvalue_at_lvalue_object_storage(
        pointer, frame, table->vectors);
    return std::get<2>(data) == 8UL ? 8UL : 4UL;
}

/**
 * @brief Insert a syscall
 *
 * The length of the buffer of a `read(2)` is cached for a later `print`
 */
void Invocation_Inserter::insert_from_syscall_function(std::string_view routine,
    assembly::Instructions& instructions)
{
    accessor_->address_accessor.buffer_accessor.set_buffer_size_from_syscall(
        routine, stack_frame_.argument_stack);
    auto operands = get_operands_storage_from_argument_stack();
    syscall_ns::common::make_syscall(
        instructions, routine, operands, &stack_frame_, &accessor_);
}

/**
 * @brief Insert a call to a function of the program
 *
 * Each argument is moved into the next argument register of its size, rdi,
 * r8, r9, rsi, rdx, and rcx. A string is loaded with `lea`, and an element
 * of `argv` is loaded from its slot above r15.
 *
 *  Example:
 *
 *    show(argv[1]);
 *
 *  Becomes:
 *
 *    mov rdi, [r15 + 8 * 2]
 *    call show
 */
void Invocation_Inserter::insert_from_user_defined_function(
    std::string_view routine,
    assembly::Instructions& instructions)
{
    auto operands = get_operands_storage_from_argument_stack();
    for (auto const& operand : operands) {
        auto size = get_operand_size_from_storage(operand, accessor_->stack);
        auto storage = accessor_->register_accessor.get_available_register(
            size, accessor_->stack);
        // an element of `argv` is the address in its slot
        if (assembly::is_immediate_r15_address_offset(operand)) {
            x8664_add__asm(instructions, mov, storage, operand);
            continue;
        }
        // a char on the stack is zero extended into its register
        if (size == Operand_Size::Byte and
            is_variant(common::Stack_Offset, operand)) {
            x8664_add__asm(instructions,
                movzx,
                assembly::get_dword_register_from_qword(
                    std::get<Register>(storage)),
                operand);
            continue;
        }
        if (is_variant(Immediate, operand)) {
            if (type::is_rvalue_data_type_string(
                    std::get<Immediate>(operand))) {
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Load,
                    accessor_->instruction_accessor->size());
            }
        }
        if (size == Operand_Size::Qword)
            accessor_->flag_accessor.set_instruction_flag(
                common::flag::Argument,
                accessor_->instruction_accessor->size());
        x8664_add__asm(instructions, mov, storage, operand);
    }
    auto immediate = direct_immediate(routine);
    x8664_add__asm(instructions, call, immediate);
}

/**
 * @brief Insert a call to a standard library function
 *
 * The arguments of `print` and `printf` are type checked first
 */
void Invocation_Inserter::insert_from_standard_library_function(
    std::string_view routine,
    assembly::Instructions& instructions)
{
    auto operands = get_operands_storage_from_argument_stack();
    auto& argument_stack = stack_frame_.argument_stack;
    m::match(routine)(
        m::pattern | sv("putchar") = [&] {},
        m::pattern | sv("getchar") = [&] {},
        m::pattern | sv("print") =
            [&] {
                insert_type_check_stdlib_print_arguments(
                    argument_stack, operands);
            },
        m::pattern | sv("printf") =
            [&] {
                insert_type_check_stdlib_printf_arguments(
                    argument_stack, operands);
            });
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };

    library_caller.make_library_call(instructions, routine, operands);
}

/**
 * @brief Type check the argument of `print`, and add its length
 *
 * The argument must be a string or a buffer address, and its length is the
 * size of the string in the object table, or the length of the last
 * `read(2)` into the buffer
 */
void Invocation_Inserter::insert_type_check_stdlib_print_arguments(
    common::memory::Locals const& argument_stack,
    syscall_ns::syscall_arguments_t& operands)
{
    auto& address_storage = accessor_->address_accessor;
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };
    if (argument_stack.front() != "RET" and
        not argument_stack.front().starts_with("&"))
        if (!address_storage.is_lvalue_storage_type(
                argument_stack.front(), "string") and
            not library_caller.is_address_device_pointer_to_buffer(
                operands.front()))
            throw_compiletime_error(
                fmt::format("argument '{}' is not a valid buffer address",
                    argument_stack.front()),
                "print",
                __source__,
                "function invocation");
    auto buffer = operands.back();
    auto buffer_size = address_storage.buffer_accessor.has_read_bytes()
                           ? address_storage.buffer_accessor.read_bytes()
                           : address_storage.buffer_accessor
                                 .get_size_of_string_lvalue_buffer_address(
                                     argument_stack.back(), stack_frame_);
    operands.emplace_back(u32_int_immediate(buffer_size));
}

/**
 * @brief Type check the format string of `printf`
 *
 * The format string must be a string or a buffer address
 */
void Invocation_Inserter::insert_type_check_stdlib_printf_arguments(
    common::memory::Locals const& argument_stack,
    syscall_ns::syscall_arguments_t& operands)
{
    auto& address_storage = accessor_->address_accessor;
    auto library_caller =
        runtime::Library_Call_Inserter{ accessor_, stack_frame_ };

    if (argument_stack.front() == "RET" or
        type::is_rvalue_data_type_string(argument_stack.front()))
        return;
    if (!address_storage.is_lvalue_storage_type(
            argument_stack.front(), "string") and
        not library_caller.is_address_device_pointer_to_buffer(
            operands.front()))
        throw_compiletime_error(
            fmt::format("invalid format string '{}'", argument_stack.front()),
            "printf",
            __source__,
            "function invocation");
}

/**
 * @brief Store the address of a string literal, through rcx
 *
 *  Example:
 *
 *    s = ("hello":string:5);
 *
 *  Becomes:
 *
 *    lea rcx, [rip + ._L_str1__]
 *    mov qword ptr [rbp - 8], rcx
 */
void Operand_Inserter::insert_from_string_address_operand(LValue const& lhs,
    Storage const& storage,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto imm = type::get_rvalue_datatype_from_string(rhs);
    expression_inserter.insert_from_string(
        type::get_value_from_rvalue_data_type(imm));
    if (is_variant(assembly::Stack::Offset, storage)) {
        auto offset = std::get<assembly::Stack::Offset>(storage);
        accessor_->stack->set(offset, Operand_Size::Qword);
    }
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::QWord_Dest, instruction_accessor->size());
    accessor_->stack->set_address_from_accumulator(lhs, Register::rcx);
    x8664_add__asm(instructions, mov, storage, rcx);
}

/**
 * @brief Store a float literal from its label, through xmm7
 */
void Operand_Inserter::insert_from_float_address_operand(LValue const& lhs,
    Storage const& storage,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto imm = type::get_rvalue_datatype_from_string(rhs);
    expression_inserter.insert_from_float(
        type::get_value_from_rvalue_data_type(imm));
    if (is_variant(assembly::Stack::Offset, storage)) {
        auto offset = std::get<assembly::Stack::Offset>(storage);
        accessor_->stack->set(offset, Operand_Size::Qword);
    }
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::QWord_Dest, instruction_accessor->size());
    accessor_->stack->set_address_from_accumulator(lhs, Register::xmm7);
    x8664_add__asm(instructions, movsd, storage, Register::xmm7);
}

/**
 * @brief Store a double literal from its label, through xmm7
 */
void Operand_Inserter::insert_from_double_address_operand(LValue const& lhs,
    Storage const& storage,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto imm = type::get_rvalue_datatype_from_string(rhs);
    expression_inserter.insert_from_double(
        type::get_value_from_rvalue_data_type(imm));
    if (is_variant(assembly::Stack::Offset, storage)) {
        auto offset = std::get<assembly::Stack::Offset>(storage);
        accessor_->stack->set(offset, Operand_Size::Qword);
    }
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::QWord_Dest, instruction_accessor->size());
    accessor_->stack->set_address_from_accumulator(lhs, Register::xmm7);
    x8664_add__asm(instructions, movsd, storage, Register::xmm7);
}

/**
 * @brief Insert an assignment of an rvalue to an lvalue
 *
 * The rvalue selects the instructions:
 *
 *  * An assignment through a pointer parameter, `*b = x`, stores through
 *    r10
 *
 *  * A parameter, or a dereference of one, is loaded into the accumulator
 *
 *  * A pointer increment, `p++`, steps over the object it points to
 *
 *  * A literal is moved, or a string, float, or double is its label
 *
 *  * A temporary is the value in the accumulator
 *
 *  * A local is moved through the accumulator
 *
 *  * A unary or binary expression is computed, then stored
 *
 *  Example:
 *
 *    y = x;
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rbp - 4]
 *    mov dword ptr [rbp - 8], eax
 */
void Operand_Inserter::insert_from_mnemonic_operand(LValue const& lhs,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& address_storage = accessor_->address_accessor;
    auto accumulator = accessor_->accumulator_accessor;

    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto binary_inserter = Binary_Operator_Inserter{ accessor_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };

    auto is_address = [&](RValue const& rvalue) {
        return stack->is_allocated(rvalue);
    };

    auto& frame = stack_frame_.get_stack_frame();
    // an assignment through a pointer parameter, `*b = x`
    if (type::is_dereference_expression(lhs) and
        frame.is_parameter(type::get_unary_rvalue_reference(lhs))) {
        auto value = insert_operand_into_accumulator(rhs);
        auto address = get_operand_storage_from_parameter(
            type::get_unary_rvalue_reference(lhs));
        if (is_variant(Register, address))
            address = assembly::get_qword_register_from_dword(
                std::get<Register>(address));
        x8664_add__asm(instructions, mov, r10, address);
        x8664_add__asm(instructions, mov, direct_immediate("[r10]"), value);
        return;
    }
    // a copy of a parameter, `b = a`, or a dereference of a pointer
    // parameter, `b = *a`
    if ((frame.is_parameter(rhs) and
            not type::is_dereference_expression(rhs)) or
        (type::is_dereference_expression(rhs) and
            frame.is_parameter(type::get_unary_rvalue_reference(rhs)))) {
        insert_from_accumulator_to_lvalue(
            lhs, insert_operand_into_accumulator(rhs));
        return;
    }
    // a pointer increment or decrement steps over the object it points to
    if (type::is_unary_expression(rhs) and
        (type::get_unary_operator(rhs) == "++" or
            type::get_unary_operator(rhs) == "--") and
        (frame.locals.is_pointer(lhs) or frame.is_pointer_parameter(lhs))) {
        auto pointer = insert_operand_into_accumulator(lhs);
        auto stride = u32_int_immediate(get_pointee_size(lhs));
        if (type::get_unary_operator(rhs) == "++")
            x8664_add__asm(instructions, add, pointer, stride);
        else
            x8664_add__asm(instructions, sub, pointer, stride);
        insert_from_accumulator_to_lvalue(lhs, pointer);
        return;
    }

    m::match(rhs)(
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                auto [lhs_storage, storage_inst] =
                    address_storage
                        .get_lvalue_address_and_insertion_instructions(
                            lhs, instruction_accessor->size());
                assembly::inserter(instructions, storage_inst);
                if (type::get_type_from_rvalue_data_type(imm) == "string")
                    insert_from_string_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "float")
                    insert_from_float_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "double")
                    insert_from_double_address_operand(lhs, lhs_storage, rhs);
                else
                    x8664_add__asm(instructions, mov, lhs_storage, imm);
            },
        // the value of a temporary is in the accumulator
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_storage.address_ir_assignment) {
                    address_storage.address_ir_assignment = false;
                    Storage lhs_storage = stack->get(lhs).first;
                    x8664_add__asm(
                        instructions, mov, lhs_storage, Register::rcx);
                } else {
                    auto value =
                        get_operand_storage_from_expression_operand(rhs);
                    auto size = get_operand_size_from_storage(value, stack);
                    auto acc = size == Operand_Size::Qword ? Register::rax
                                                           : Register::eax;
                    if (size == Operand_Size::Byte)
                        x8664_add__asm(instructions, movzx, eax, value);
                    else if (!is_variant(Register, value) or
                             std::get<Register>(value) != acc)
                        x8664_add__asm(instructions, mov, acc, value);
                    insert_from_accumulator_to_lvalue(lhs, acc);
                }
            },
        m::pattern | m::app(is_address, true) =
            [&] {
                credence_assert(stack->get(rhs).second != Operand_Size::Empty);
                Storage lhs_storage = stack->get(lhs).first;
                Storage rhs_storage = stack->get(rhs).first;
                auto acc = accumulator.get_accumulator_register_from_size(
                    stack->get(rhs).second);
                x8664_add__asm(instructions, mov, acc, rhs_storage);
                x8664_add__asm(instructions, mov, lhs_storage, acc);
            },
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                auto [lhs_storage, storage_inst] =
                    address_storage
                        .get_lvalue_address_and_insertion_instructions(
                            lhs, instruction_accessor->size());
                assembly::inserter(instructions, storage_inst);
                auto unary_op = type::get_unary_operator(rhs);
                unary_inserter.insert_from_unary_operator_operands(
                    unary_op, lhs_storage);
            },
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] {
                binary_inserter.from_binary_operator_expression(rhs);
                if (binary_inserter.result.has_value())
                    insert_from_accumulator_to_lvalue(
                        lhs, *binary_inserter.result);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Insert a unary expression on its operand
 *
 * See ir/temporary.h for the temporaries of unary expressions
 *
 *  Example:
 *
 *    x = ++x;
 *
 *  Becomes:
 *
 *    inc dword ptr [rbp - 4]
 */
Storage Unary_Operator_Inserter::insert_from_unary_operator_rvalue(
    RValue const& expr)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto& address_space = accessor_->address_accessor;
    auto& table_accessor = accessor_->table_accessor;
    auto& register_accessor = accessor_->register_accessor;

    credence_assert(type::is_unary_expression(expr));

    Storage storage{};

    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    auto is_vector = [&](RValue const& rvalue) {
        return table_accessor.get_table()->vectors.contains(
            type::from_lvalue_offset(rvalue));
    };

    if (stack->contains(rvalue)) {
        // This is the address-of operator, use a qword size register
        if (op == "&") {
            address_space.address_ir_assignment = true;
            insert_from_unary_operator_operands(op, stack->get(rvalue).first);
            accessor_->set_signal_register(Register::rcx);
            return Register::rcx;
        }
        auto size = stack->get(rvalue).second;
        storage = table_accessor.next_ir_instruction_is_temporary() and
                          not table_accessor.last_ir_instruction_is_assignment()
                      ? register_accessor.get_second_register_from_size(size)
                      : accessor_->accumulator_accessor
                            .get_accumulator_register_from_size(size);
        x8664_add__asm(instructions, mov, storage, stack->get(rvalue).first);
        insert_from_unary_operator_operands(op, storage);
    } else if (is_vector(rvalue)) {
        auto [address, address_inst] =
            address_space.get_lvalue_address_and_insertion_instructions(
                rvalue, false);
        assembly::inserter(instructions, address_inst);
        auto size = get_operand_size_from_storage(address, stack);
        storage = table_accessor.next_ir_instruction_is_temporary() and
                          not table_accessor.last_ir_instruction_is_assignment()
                      ? register_accessor.get_second_register_from_size(size)
                      : accessor_->accumulator_accessor
                            .get_accumulator_register_from_size(size);
        address_space.address_ir_assignment = true;
        accessor_->set_signal_register(Register::rax);
        insert_from_unary_operator_operands(op, storage, address);
    } else {
        auto immediate = type::get_rvalue_datatype_from_string(rvalue);
        auto size = assembly::get_operand_size_from_rvalue_datatype(immediate);
        storage = table_accessor.next_ir_instruction_is_temporary() and
                          not table_accessor.last_ir_instruction_is_assignment()
                      ? register_accessor.get_second_register_from_size(size)
                      : accessor_->accumulator_accessor
                            .get_accumulator_register_from_size(size);
        x8664_add__asm(instructions, mov, storage, immediate);
        insert_from_unary_operator_operands(op, storage);
    }
    return storage;
}

/**
 * @brief Load the address of a string literal into rcx
 */
void Expression_Inserter::insert_from_string(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_string(str));
    auto location = assembly::make_asciz_immediate(
        accessor_->address_accessor.buffer_accessor.get_string_address_offset(
            str));
    x8664_add__asm(instructions, lea, rcx, location);
}

/**
 * @brief Load a float literal from its label into xmm7
 */

void Expression_Inserter::insert_from_float(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_float(str));
    auto location = assembly::make_asciz_immediate(
        accessor_->address_accessor.buffer_accessor.get_float_address_offset(
            str));
    x8664_add__asm(instructions, movsd, xmm7, location);
}

/**
 * @brief Load a double literal from its label into xmm7
 */
void Expression_Inserter::insert_from_double(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_double(str));
    auto location = assembly::make_asciz_immediate(
        accessor_->address_accessor.buffer_accessor.get_double_address_offset(
            str));
    x8664_add__asm(instructions, movsd, xmm7, location);
}

/**
 * @brief Binary operator inserter of expression operands
 *
 * The left-hand-side is loaded into the accumulator, and the operator is
 * applied with the right-hand-side. A right-hand-side temporary in the
 * accumulator is moved to the r11 scratch register first.
 *
 *  Example:
 *
 *    _t3 = x * _t2;
 *
 *  Becomes:
 *
 *    mov r11d, eax
 *    mov eax, dword ptr [rbp - 4]
 *    imul eax, r11d
 */
void Binary_Operator_Inserter::from_binary_operator_expression(
    RValue const& rvalue)
{
    credence_assert(type::is_binary_expression(rvalue));
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& stack = accessor_->stack;
    auto [lhs, rhs, op] = type::from_rvalue_binary_expression(rvalue);
    Operand_Inserter operand_inserter{ accessor_ };

    // the value of an expression of two immediates is folded
    if (is_immediate(lhs) and is_immediate(rhs)) {
        auto [lhs_i, rhs_i] = get_rvalue_pair_as_immediate(lhs, rhs);
        auto immediate =
            type::is_relation_binary_operator(op)
                ? common::assembly::
                      get_result_from_trivial_relational_expression(
                          lhs_i, op, rhs_i)
            : type::is_bitwise_binary_operator(op)
                ? common::assembly::get_result_from_trivial_bitwise_expression(
                      lhs_i, op, rhs_i)
                : common::assembly::get_result_from_trivial_integral_expression(
                      lhs_i, op, rhs_i);
        auto accumulator =
            !type::is_relation_binary_operator(op) and
                    assembly::get_operand_size_from_rvalue_datatype(lhs_i) ==
                        Operand_Size::Qword
                ? Register::rax
                : Register::eax;
        x8664_add__asm(instructions, mov, accumulator, immediate);
        result = accumulator;
        return;
    }

    Storage rhs_s = assembly::O_NUL;
    auto const& held = accessor_->accumulator_temporary;
    if (held.has_value() and held->first == rhs and lhs != rhs) {
        auto scratch = assembly::is_qword_register(held->second)
                           ? Register::r11
                           : Register::r11d;
        x8664_add__asm(instructions, mov, scratch, held->second);
        rhs_s = scratch;
    }
    auto accumulator = operand_inserter.insert_operand_into_accumulator(lhs);
    if (rhs_s == assembly::O_NUL)
        rhs_s =
            operand_inserter.get_operand_storage_from_expression_operand(rhs);

    // the operands of an instruction are the same size, and an immediate
    // byte is already a dword immediate
    auto rhs_size = get_operand_size_from_storage(rhs_s, stack);
    if (rhs_size == Operand_Size::Byte and not is_variant(Immediate, rhs_s)) {
        x8664_add__asm(instructions, movzx, r11d, rhs_s);
        rhs_s = Register::r11d;
        rhs_size = Operand_Size::Dword;
    }
    if (!is_variant(Immediate, rhs_s) or
        not assembly::is_immediate_rip_address_offset(rhs_s)) {
        if (rhs_size == Operand_Size::Qword and accumulator == Register::eax) {
            x8664_add__asm(instructions, movsxd, rax, eax);
            accumulator = Register::rax;
        } else if (rhs_size == Operand_Size::Dword and
                   accumulator == Register::rax and
                   not is_variant(Immediate, rhs_s)) {
            x8664_add__asm(instructions, movsxd, r11, rhs_s);
            rhs_s = Register::r11;
        }
    }

    result = accumulator;
    if (op == "/" or op == "%") {
        insert_from_division_operands(accumulator, rhs_s, op);
    } else if (type::is_binary_arithmetic_operator(op)) {
        auto arithmetic = Arithemtic_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            arithmetic
                .from_arithmetic_expression_operands({ accumulator, rhs_s }, op)
                .second);
    } else if (op == "&&" or op == "||") {
        insert_from_logical_operands(accumulator, rhs_s, op);
    } else if (type::is_relation_binary_operator(op)) {
        insert_from_relational_operands(accumulator, rhs_s, op);
    } else if (op == "<<" or op == ">>") {
        insert_from_shift_operands(accumulator, rhs_s, op);
    } else {
        auto bitwise = Bitwise_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            bitwise.from_bitwise_expression_operands({ accumulator, rhs_s }, op)
                .second);
    }
}

/**
 * @brief Inserter of a signed division or its remainder
 *
 *  Example:
 *
 *    _t2 = x % 5;
 *
 *  Becomes:
 *
 *    mov r11d, 5
 *    cdq
 *    idiv r11d
 *    mov eax, edx
 */
void Binary_Operator_Inserter::insert_from_division_operands(
    Register accumulator,
    Storage divisor,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    bool qword = accumulator == Register::rax;
    if (is_variant(Immediate, divisor)) {
        auto scratch = qword ? Register::r11 : Register::r11d;
        x8664_add__asm(instructions, mov, scratch, divisor);
        divisor = scratch;
    }
    if (qword)
        x8664_add__asm(instructions, cqo);
    else
        x8664_add__asm(instructions, cdq);
    x8664_add__asm(instructions, idiv, divisor);
    if (op == "%") {
        if (qword)
            x8664_add__asm(instructions, mov, rax, rdx);
        else
            x8664_add__asm(instructions, mov, eax, edx);
    }
}

/**
 * @brief Inserter of a relational expression
 *
 * The comparison jumps to the label of the next IF instruction when it is
 * the condition, otherwise its value is 0 or 1 in the accumulator
 *
 *  Example:
 *
 *    _t2 = x < 5;
 *    y = _t2;
 *
 *  Becomes:
 *
 *    cmp eax, 5
 *    setl al
 *    movzx eax, al
 */
void Binary_Operator_Inserter::insert_from_relational_operands(
    Register accumulator,
    Storage const& rhs,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& ir_instructions = accessor_->table_accessor.get_ir_instructions();
    auto index = accessor_->table_accessor.get_index();

    x8664_add__asm(instructions, cmp, accumulator, rhs);
    if (index + 1 < ir_instructions.size()) {
        auto const& current = ir_instructions.at(index);
        auto const& next = ir_instructions.at(index + 1);
        if (std::get<0>(next) == ir::Instruction::IF and
            std::get<1>(next) == std::get<1>(current)) {
            auto label = direct_immediate(
                assembly::make_label(std::get<3>(next), stack_frame_.symbol));
            if (op == "==")
                x8664_add__asm(instructions, je, label);
            else if (op == "!=")
                x8664_add__asm(instructions, jne, label);
            else if (op == "<")
                x8664_add__asm(instructions, jl, label);
            else if (op == ">")
                x8664_add__asm(instructions, jg, label);
            else if (op == "<=")
                x8664_add__asm(instructions, jle, label);
            else
                x8664_add__asm(instructions, jge, label);
            accessor_->relational_jump = true;
            result = std::nullopt;
            return;
        }
    }
    m::match(op)(
        m::pattern | std::string{ "==" } =
            [&] { x8664_add__asm(instructions, sete, al); },
        m::pattern | std::string{ "!=" } =
            [&] { x8664_add__asm(instructions, setne, al); },
        m::pattern | std::string{ "<" } =
            [&] { x8664_add__asm(instructions, setl, al); },
        m::pattern | std::string{ ">" } =
            [&] { x8664_add__asm(instructions, setg, al); },
        m::pattern | std::string{ "<=" } =
            [&] { x8664_add__asm(instructions, setle, al); },
        m::pattern | m::_ = [&] { x8664_add__asm(instructions, setge, al); });
    x8664_add__asm(instructions, movzx, eax, al);
    result = Register::eax;
}

/**
 * @brief Inserter of a logical "and" or "or" expression as 0 or 1
 *
 *  Example:
 *
 *    _t3 = _t1 || _t2;
 *
 *  Becomes:
 *
 *    cmp eax, 0
 *    setne al
 *    movzx r10d, al
 *    cmp r11d, 0
 *    setne al
 *    movzx eax, al
 *    or eax, r10d
 */
void Binary_Operator_Inserter::insert_from_logical_operands(
    Register accumulator,
    Storage rhs,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto zero = u32_int_immediate(0);
    if (is_variant(Immediate, rhs)) {
        x8664_add__asm(instructions, mov, r11d, rhs);
        rhs = Register::r11d;
    }
    x8664_add__asm(instructions, cmp, accumulator, zero);
    x8664_add__asm(instructions, setne, al);
    x8664_add__asm(instructions, movzx, r10d, al);
    x8664_add__asm(instructions, cmp, rhs, zero);
    x8664_add__asm(instructions, setne, al);
    x8664_add__asm(instructions, movzx, eax, al);
    if (op == "&&")
        x8664_add__asm(instructions, and_, eax, r10d);
    else
        x8664_add__asm(instructions, or_, eax, r10d);
    result = Register::eax;
}

/**
 * @brief Inserter of a shift, where a count that is not an immediate is in cl
 */
void Binary_Operator_Inserter::insert_from_shift_operands(Register accumulator,
    Storage const& rhs,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    if (is_variant(Immediate, rhs)) {
        if (op == "<<")
            x8664_add__asm(instructions, shl, accumulator, rhs);
        else
            x8664_add__asm(instructions, shr, accumulator, rhs);
        return;
    }
    x8664_add__asm(instructions, push, rcx);
    x8664_add__asm(instructions, mov, ecx, rhs);
    if (op == "<<")
        x8664_add__asm(instructions, shl, accumulator, cl);
    else
        x8664_add__asm(instructions, shr, accumulator, cl);
    x8664_add__asm(instructions, pop, rcx);
}

/**
 * @brief Insert an arithmetic, relational, bitwise, or unary expression of
 * two operands
 *
 * An immediate left-hand-side is swapped to the right-hand-side, since the
 * destination of an x86-64 instruction is not an immediate, and a
 * relational expression jumps to the label of the IF that follows it
 */
void Operand_Inserter::insert_from_binary_operands(
    assembly::Binary_Operands& operands,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    if (is_variant(Immediate, operands.first) and
        not assembly::is_immediate_r15_address_offset(operands.first) and
        not assembly::is_immediate_rip_address_offset(operands.first))
        std::swap(operands.first, operands.second);
    if (type::is_binary_arithmetic_operator(op)) {
        auto arithmetic = Arithemtic_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            arithmetic.from_arithmetic_expression_operands(operands, op)
                .second);
    } else if (type::is_relation_binary_operator(op)) {
        auto relational = Relational_Operator_Inserter{ accessor_ };
        auto& ir_instructions = accessor_->table_accessor.get_ir_instructions();
        auto ir_index = accessor_->table_accessor.get_index();
        if (ir_instructions.size() > ir_index and
            std::get<0>(ir_instructions.at(ir_index + 1)) ==
                ir::Instruction::IF) {
            auto label = assembly::make_label(
                std::get<3>(ir_instructions.at(ir_index + 1)),
                stack_frame_.symbol);
            assembly::inserter(instructions,
                relational.from_relational_expression_operands(
                    operands, op, label));
        }
    } else if (type::is_bitwise_binary_operator(op)) {
        auto bitwise = Bitwise_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            bitwise.from_bitwise_expression_operands(operands, op).second);
    } else if (type::is_unary_expression(op)) {
        auto unary = Unary_Operator_Inserter{ accessor_ };
        unary.insert_from_unary_operator_operands(op, operands.first);
    } else {
        credence_error(fmt::format("unreachable: operator '{}'", op));
    }
}

/**
 * @brief Insert the value of a temporary into the accumulator
 *
 * The expression of the temporary is in the object table, and the
 * accumulator holds its value until the next instruction
 */
void Expression_Inserter::insert_lvalue_at_temporary_object_address(
    LValue const& lvalue)
{
    auto& frame = stack_frame_.get_stack_frame();
    auto& table = accessor_->table_accessor.get_table();
    // a copy of a temporary, e.g. `_t3 = _t2;`, is the value of `_t2`
    // when it was computed, not its expression computed again
    if (frame.temporary.contains(lvalue) and
        type::is_temporary(frame.temporary.at(lvalue))) {
        auto operand_inserter = Operand_Inserter{ accessor_ };
        auto accumulator = operand_inserter.insert_operand_into_accumulator(
            frame.temporary.at(lvalue));
        accessor_->accumulator_temporary = { lvalue, accumulator };
        return;
    }
    auto temporary = table->lvalue_at_temporary_object_address(lvalue, frame);
    result_.reset();
    insert_from_rvalue(temporary);
    if (result_.has_value())
        accessor_->accumulator_temporary = { lvalue, *result_ };
}

/**
 * @brief Insert the instruction of a unary operator
 *
 * The increment and decrement are in place, and the other operators are
 * applied to the destination
 */
void Unary_Operator_Inserter::insert_from_unary_operator_operands(
    std::string const& op,
    Storage const& dest,
    Storage const& src)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto index = accessor_->instruction_accessor->size();
    m::match(op)(
        m::pattern | std::string{ "++" } =
            [&] {
                assembly::inserter(instructions, assembly::inc(dest).second);
            },
        m::pattern | std::string{ "--" } =
            [&] {
                assembly::inserter(instructions, assembly::dec(dest).second);
            },
        m::pattern | std::string{ "~" } =
            [&] {
                assembly::inserter(instructions, assembly::b_not(dest).second);
            },
        m::pattern | std::string{ "&" } =
            [&] {
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Address, index);
                auto acc = Register::rcx;
                if (src != assembly::O_NUL)
                    assembly::inserter(
                        instructions, assembly::lea(dest, src).second);
                else
                    assembly::inserter(
                        instructions, assembly::lea(acc, dest).second);
            },
        m::pattern | std::string{ "*" } =
            [&] {
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   dest, accessor_->stack);
                x8664_add__asm(instructions, mov, acc, dest);
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Indirect, index);
                x8664_add__asm(instructions, mov, acc, src);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                assembly::inserter(instructions, assembly::neg(dest).second);
            },
        m::pattern | std::string{ "+" } = [&] {});
}

/**
 * @brief Insert the value of the rvalue of a temporary
 *
 * The rvalue is a binary or unary expression, a literal, the operand of a
 * comparison, or the return value of the last call, `RET`. The register
 * that holds the value is the result.
 */
void Expression_Inserter::insert_from_rvalue(RValue const& rvalue)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.get_table();
    auto& instructions = instruction_accessor->get_instructions();

    auto binary_inserter = Binary_Operator_Inserter{ accessor_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
    auto operand_inserter = Operand_Inserter{ accessor_ };

    auto is_comparator = [](RValue const& rvalue) {
        return rvalue.starts_with("CMP");
    };

    auto is_stdlib_function = [&](Label const& label) {
#if defined(__linux__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::X8664);
#elif defined(_WIN32) || defined(_WIN64)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::X8664);
#endif
    };

    m::match(rvalue)(
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] {
                binary_inserter.from_binary_operator_expression(rvalue);
                result_ = binary_inserter.result;
            },
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] { result_ = insert_from_unary_value(rvalue); },
        m::pattern | m::app(type::is_rvalue_data_type, true) =
            [&] {
                Storage immediate =
                    operand_inserter.get_operand_storage_from_rvalue(rvalue);
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   immediate, accessor_->stack);
                x8664_add__asm(instructions, mov, acc, immediate);
                auto type = type::get_type_from_rvalue_data_type(rvalue);
                if (type == "string")
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::Address, instruction_accessor->size());
                result_ = acc;
            },
        // the truth value of a comparator is the value of its operand
        m::pattern | m::app(is_comparator, true) =
            [&] {
                auto operand = util::str_trim_ws(rvalue.substr(4));
                if (operand == "RET")
                    insert_from_rvalue(operand);
                else
                    result_ = operand_inserter.insert_operand_into_accumulator(
                        operand);
            },
        // the return value of a call is in the accumulator
        m::pattern | RValue{ "RET" } =
            [&] {
                // `getchar` and a syscall return a word in eax
                if (common::runtime::is_returning_library_function(
                        stack_frame_.tail,
                        common::assembly::Arch_Type::X8664)) {
                    result_ = Register::eax;
                    return;
                }
                if (is_stdlib_function(stack_frame_.tail))
                    return;
                credence_assert(table->functions.contains(stack_frame_.tail));
                auto& frame = table->functions.at(stack_frame_.tail).get();
                credence_assert(frame.ret.has_value());
                if (is_qword_return_value(frame)) {
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::QWord_Dest, instruction_accessor->size());
                    accessor_->set_signal_register(Register::rax);
                    result_ = Register::rax;
                } else
                    result_ = Register::eax;
            },
        m::pattern | m::_ =
            [&] {
                result_ =
                    operand_inserter.insert_operand_into_accumulator(rvalue);
            });
}

/**
 * @brief Inserter of the value of a unary expression into the accumulator
 *
 * The increment, decrement, and dereference operators are inserted by the
 * unary operator inserter, so there is no value to return
 *
 *  Example:
 *
 *    _t2 = - x;
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rbp - 4]
 *    neg eax
 */
std::optional<Register> Expression_Inserter::insert_from_unary_value(
    RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto op = type::get_unary_operator(rvalue);
    if (op == "&") {
        if (auto address = insert_from_address_of_value(rvalue))
            return address;
    }
    if (op != "-" and op != "+" and op != "~" and op != "!") {
        auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
        unary_inserter.insert_from_unary_operator_rvalue(rvalue);
        return std::nullopt;
    }
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto accumulator = operand_inserter.insert_operand_into_accumulator(
        type::get_unary_rvalue_reference(rvalue));
    m::match(op)(
        m::pattern | std::string{ "-" } =
            [&] { x8664_add__asm(instructions, neg, accumulator); },
        m::pattern | std::string{ "~" } =
            [&] { x8664_add__asm(instructions, not_, accumulator); },
        m::pattern | std::string{ "!" } =
            [&] {
                x8664_add__asm(
                    instructions, cmp, accumulator, u32_int_immediate(0));
                x8664_add__asm(instructions, sete, al);
                x8664_add__asm(instructions, movzx, eax, al);
                accumulator = Register::eax;
            },
        m::pattern | m::_ = [&] {});
    return accumulator;
}

/**
 * @brief Inserter of the address of a vector element, or of a local that is
 * an argument, into the accumulator
 *
 * The address of a local assigned to a pointer is from the unary operator
 * inserter
 */
std::optional<Register> Expression_Inserter::insert_from_address_of_value(
    RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto const& table = accessor_->table_accessor.get_table();
    auto& ir_instructions = accessor_->table_accessor.get_ir_instructions();
    auto index = accessor_->table_accessor.get_index();
    auto lvalue = type::get_unary_rvalue_reference(rvalue);
    auto label = type::from_lvalue_offset(lvalue);
    auto is_argument = index + 1 < ir_instructions.size() and
                       is_parameter(std::get<1>(ir_instructions.at(index + 1)));
    if ((is_vector_offset(lvalue) and table->vectors.contains(label)) or
        (is_argument and accessor_->stack->is_allocated(lvalue))) {
        auto [address, address_inst] =
            accessor_->address_accessor
                .get_lvalue_address_and_insertion_instructions(
                    lvalue, instructions.size(), false);
        assembly::inserter(instructions, address_inst);
        x8664_add__asm(instructions, lea, rax, address);
        return Register::rax;
    }
    return std::nullopt;
}

/**
 * @brief Move the value of a return statement to the return register
 *
 * The return register is rax for a quadword, such as a pointer, and eax
 * otherwise. A byte, the value of a relational expression, is zero
 * extended.
 *
 *  Example:
 *
 *    test(*y) {
 *      return(y);
 *    }
 *
 *  Becomes:
 *
 *    mov rax, rdi
 */
void Expression_Inserter::insert_from_return_rvalue(
    object::Function::Return_RValue const& ret)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto immediate =
        operand_inserter.get_operand_storage_from_expression_operand(
            ret->second);
    auto size = get_operand_size_from_storage(immediate, accessor_->stack);
    // a string literal is the address of its label
    if (is_variant(Immediate, immediate) and
        assembly::is_immediate_rip_address_offset(immediate) and
        type::is_rvalue_data_type_string(ret->second)) {
        x8664_add__asm(instructions, lea, rax, immediate);
        return;
    }
    if (size == Operand_Size::Byte)
        x8664_add__asm(instructions, movzx, eax, immediate);
    else if (size == Operand_Size::Qword)
        x8664_add__asm(instructions, mov, rax, immediate);
    else
        x8664_add__asm(instructions, mov, eax, immediate);
}

/**
 * @brief Insert an assignment to or from a global vector element
 *
 *  Example:
 *
 *    x = value;
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rip + value]
 *    mov dword ptr [rbp - 4], eax
 */
void Expression_Inserter::insert_from_global_vector_assignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto [lhs_storage, lhs_storage_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                lhs, instruction_accessor->size());
    assembly::inserter(instructions, lhs_storage_inst);
    auto [rhs_storage, rhs_storage_inst] =
        accessor_->address_accessor
            .get_lvalue_address_and_insertion_instructions(
                rhs, instruction_accessor->size());
    assembly::inserter(instructions, rhs_storage_inst);
    auto acc =
        accessor_->accumulator_accessor.get_accumulator_register_from_storage(
            lhs_storage, accessor_->stack);
    x8664_add__asm(instructions, mov, acc, rhs_storage);
    x8664_add__asm(instructions, mov, lhs_storage, acc);
}

/**
 * @brief Get the instruction of an arithmetic operator on two operands
 */
Instruction_Pair
Arithemtic_Operator_Inserter::from_arithmetic_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "*" } =
            [&] {
                instructions = assembly::mul(operands.first, operands.second);
            },
        m::pattern | std::string{ "/" } =
            [&] {
                instructions = assembly::div(operands.first, operands.second);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                instructions = assembly::sub(operands.first, operands.second);
            },
        m::pattern | std::string{ "+" } =
            [&] {
                instructions = assembly::add(operands.first, operands.second);
            },
        m::pattern | std::string{ "%" } =
            [&] {
                instructions = assembly::mod(operands.first, operands.second);
            });
    return instructions;
}

/**
 * @brief Get the instruction of a bitwise operator on two operands
 */
Instruction_Pair Bitwise_Operator_Inserter::from_bitwise_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::eax, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "<<" } =
            [&] {
                instructions =
                    assembly::lshift(operands.first, operands.second);
            },
        m::pattern | std::string{ ">>" } =
            [&] {
                instructions =
                    assembly::rshift(operands.first, operands.second);
            },
        m::pattern | std::string{ "^" } =
            [&] {
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   operands.first, accessor_->stack);
                instructions = assembly::b_xor(acc, operands.second);
            },
        m::pattern | std::string{ "&" } =
            [&] {
                instructions = assembly::b_and(operands.first, operands.second);
            },
        m::pattern | std::string{ "|" } =
            [&] {
                instructions = assembly::b_or(operands.first, operands.second);
            });
    return instructions;
}

/**
 * @brief Get the comparison and conditional jump of a relational operator
 *
 *  Example:
 *
 *    from_relational_expression_operands({ dword ptr [rbp - 12], 10 }, ">",
 *        "._L6__main")
 *
 *  Becomes:
 *
 *    mov eax, dword ptr [rbp - 12]
 *    cmp eax, 10
 *    jg ._L6__main
 */
assembly::Instructions
Relational_Operator_Inserter::from_relational_expression_operands(
    assembly::Binary_Operands const& operands,
    std::string const& binary_op,
    Label const& jump_label)
{
    auto register_storage = Register::eax;
    if (accessor_->address_accessor.is_qword_storage_size(operands.first) or
        accessor_->address_accessor.is_qword_storage_size(operands.second)) {
        register_storage = Register::rax;
    }

    return m::match(binary_op)(
        m::pattern | std::string{ "==" } =
            [&] {
                return assembly::r_eq(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ "!=" } =
            [&] {
                return assembly::r_neq(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ "<" } =
            [&] {
                return assembly::r_lt(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ ">" } =
            [&] {
                return assembly::r_gt(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ "<=" } =
            [&] {
                return assembly::r_le(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | std::string{ ">=" } =
            [&] {
                return assembly::r_ge(operands.first,
                    operands.second,
                    jump_label,
                    register_storage);
            },
        m::pattern | m::_ = [&] { return assembly::Instructions{}; });
}

}