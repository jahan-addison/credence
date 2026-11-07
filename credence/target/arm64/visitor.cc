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

#include "visitor.h"

#include "assembly.h"                        // for Register, Mnemonic, arm...
#include "credence/error.h"                  // for credence_assert
#include "credence/map.h"                    // for Ordered_Map
#include "credence/symbol.h"                 // for Symbol_Table
#include "credence/target/common/assembly.h" // for Arch_Type, OS_Type, ali...
#include "credence/target/common/memory.h"   // for is_parameter
#include "credence/target/common/types.h"    // for Table_Pointer
#include "credence/types.h"                  // for from_lvalue_offset, get...
#include "flags.h"                           // for set_alignment_flag
#include "inserter.h"                        // for Expression_Inserter
#include "memory.h"                          // for Memory_Accessor, Instru...
#include "stack.h"                           // for Stack
#include "syscall.h"                         // for exit_syscall
#include <algorithm>                         // for any_of
#include <credence/language/hir/binding.h>   // for Object, Function, Label
#include <credence/target/common/runtime.h>  // for is_stdlib_function, is_...
#include <deque>                             // for deque
#include <matchit.h>                         // for Wildcard, App, Ds, app
#include <memory>                            // for shared_ptr
#include <ranges>                            // for split
#include <string>                            // for basic_string, char_traits
#include <string_view>                       // for basic_string_view
#include <tuple>                             // for get, tuple
#include <utility>                           // for pair
#include <variant>                           // for variant

/****************************************************************************
 * ARM64 IR Visitor
 *
 * Translates each ITA instruction into ARM64 instructions. A function's
 * locals are in registers when its frame makes no calls, and on the stack
 * otherwise, and the value of an expression is in the accumulator, w8 or x8.
 *
 *  Example:
 *
 *    _p4_3 = (1:int:4);
 *    _p5_4 = (2:int:4);
 *    PUSH _p5_4;
 *    PUSH _p4_3;
 *    CALL compute;
 *    POP 16;
 *    _t8 = RET;
 *
 *  Becomes:
 *
 *    mov x0, #1
 *    mov x1, #2
 *    bl compute
 *    mov w8, w0
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

namespace credence::target::arm64 {

namespace m = matchit;

/**
 * @brief IR Instruction Instruction::FUNC_START
 *
 * The frame record, x29 and x30, is stored at the bottom of the frame by a
 * pre-indexed store of the frame size, which is set at the end of the
 * function. The `main` function also saves x19 and sets up `argc` and
 * `argv`: in x0 and x1 on Darwin, and at the top of the stack on Linux.
 *
 *  Example:
 *
 *    main(argc, argv) {
 *
 *  Becomes, on Darwin:
 *
 *    stp x29, x30, [sp, #-32]!
 *    str x19, [sp, #16]
 *    mov x29, sp
 *    str w0, [sp, #20]
 *    mov x19, x1
 */
void IR_Instruction_Visitor::from_func_start_ita(Label const& name)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& table = accessor_->table_accessor.get_table();
    credence_assert(table->functions.contains(name));
    accessor_->device_accessor.reset_storage_devices();
    stack_frame_.symbol = name;
    stack_frame_.set_stack_frame(name);
    // the frame pointer and link register are saved at the bottom of the
    // frame
    accessor_->stack->allocate(16);
    table->reset_frame_calls();
    set_alignment_flag(Align_SP);
    arm64_add__asm(instructions, stp, x29, x30, alignment__integer());
    // x19 is callee saved, and is saved in the slot above the frame record
    if (name == "main" and
        common::runtime::argc_argv_kernel_runtime_access(stack_frame_).first) {
        accessor_->stack->allocate(8);
        auto argv_imm = direct_immediate("[sp, #16]");
        arm64_add__asm(instructions, str, x19, argv_imm);
    }
    arm64_add__asm(instructions, mov, x29, sp);
    // setup argc and argv for the main function
    if (name == "main") {
        auto argc_argv =
            common::runtime::argc_argv_kernel_runtime_access(stack_frame_);
        if (argc_argv.first) {
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
            // On linux, argc, argv are at the top of the stcak
            // add x19, sp, #32
            set_alignment_flag(Align_S2_Folded);
            arm64_add__asm(instructions, add, x19, sp, 0);
#elif defined(__APPLE__) || defined(__bsdi__)
            // On darwin, argc, argv are in x0 and x1
            accessor_->stack->set_address_from_size("argc");
            arm64_add__asm(
                instructions, str, w0, accessor_->stack->get("argc").first);
            arm64_add__asm(instructions, mov, x19, x1);
#endif
        }
        if (argc_argv.second) {
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
            accessor_->stack->set_address_from_size(
                "argv", Operand_Size::Doubleword);
#endif
        }
        return;
    }
    // a call overwrites the argument registers, so each parameter of a
    // frame with calls is stored in its slot
    auto& frame = table->functions.at(name).get();
    if (!ir::stack_frame_contains_call_instruction(
            frame, accessor_->table_accessor.get_ir_instructions()))
        return;
    for (std::size_t index = 0; index < frame.parameters.size(); index++) {
        auto const& parameter = frame.parameters.at(index);
        auto pointer = parameter.starts_with("*");
        auto slot = fmt::format(
            "__parameter_{}", type::get_unary_rvalue_reference(parameter));
        accessor_->stack->set_address_from_size(
            slot, pointer ? Operand_Size::Doubleword : Operand_Size::Word);
        auto value =
            pointer ? memory::registers::available_doubleword_argument.at(index)
                    : memory::registers::available_word_argument.at(index);
        arm64_add__asm(
            instructions, str, value, accessor_->stack->get(slot).first);
    }
}

/**
 * @brief IR Instruction Instruction::FUNC_END
 *
 * The frame size of the function is recorded for its prologue and
 * epilogue, and the registers and stack are reset for the next function
 */
void IR_Instruction_Visitor::from_func_end_ita()
{
    accessor_->register_accessor.reset_available_registers();
    accessor_->stack->set_stack_frame_allocation_size(stack_frame_.symbol);
    accessor_->stack->clear();
}

/**
 * @brief IR Instruction Instruction::CMP
 *
 * A comparison is a temporary, `_t3 = CMP x`, and the JMP_E that follows it
 * makes the comparison, so there is no CMP instruction to translate
 */
void IR_Instruction_Visitor::from_cmp_ita(
    [[maybe_unused]] ir::Quadruple const& inst)
{
}

/**
 * @brief IR Instruction Instruction::MOV
 *
 * The lvalue and rvalue select the inserter:
 *
 *  * An argument slot, `_p1_1`, of a temporary or dereference is stored on
 *    the stack, since the accumulator is overwritten before the call
 *
 *  * A temporary is the value of its expression in the accumulator
 *
 *  * A dereference, `*b = x`, is a store through the pointer
 *
 *  * An element of a global vector is a load or store from the data section
 *
 *  * Any other assignment is a mov between storage devices
 *
 * A local on the stack is stored after the assignment.
 *
 *  Example:
 *
 *    _t7 = x + (10:int:4);
 *    y = _t7;
 *
 *  Becomes:
 *
 *    mov w8, w9
 *    add w8, w8, #10
 *    mov w10, w8
 */
void IR_Instruction_Visitor::from_mov_ita(ir::Quadruple const& inst)
{
    auto& table = accessor_->table_accessor.get_table();
    auto lhs = ir::get_lvalue_from_mov_qaudruple(inst);
    auto rhs = ir::get_rvalue_from_mov_qaudruple(inst).first;

    auto expression_inserter = Expression_Inserter{ accessor_ };
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto unary_inserter = Unary_Operator_Inserter{ accessor_ };

    auto is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table->vectors.contains(rvalue_reference) and
               table->globals.is_pointer(rvalue_reference);
    };

    m::match(lhs, rhs)(
        // an argument of a computed value is stored before the next
        // instruction, otherwise it's resolved when it's pushed
        m::pattern | m::ds(m::app(is_parameter, true), m::_) =
            [&] {
                auto const& held = accessor_->accumulator_temporary;
                if ((type::is_temporary(rhs) and
                        ((held.has_value() and held->first == rhs) or
                            accessor_->stack->is_allocated(rhs))) or
                    type::is_dereference_expression(rhs))
                    insert_temporary_to_stack(rhs, lhs);
            },
        m::pattern | m::ds(m::app(type::is_temporary, true), m::_) =
            [&] {
                expression_inserter.insert_lvalue_at_temporary_object_address(
                    lhs);
                auto const& held = accessor_->accumulator_temporary;
                if (held.has_value() and held->first == lhs and
                    is_temporary_used_after_next_instruction(lhs))
                    insert_temporary_to_stack(lhs, lhs);
            },
        // an assignment through a pointer, `*b = x`
        m::pattern |
            m::ds(m::app(type::is_dereference_expression, true), m::_) =
            [&] {
                insert_to_pointer(type::get_unary_rvalue_reference(lhs), rhs);
            },
        m::pattern | m::ds(m::app(is_global_vector, true), m::_) =
            [&] {
                expression_inserter.insert_from_global_vector_assignment(
                    lhs, rhs);
            },
        m::pattern | m::ds(m::_, m::app(is_global_vector, true)) =
            [&] {
                expression_inserter.insert_from_global_vector_assignment(
                    lhs, rhs);
            },
        m::pattern | m::_ =
            [&] { operand_inserter.insert_from_mnemonic_operand(lhs, rhs); });

    // the register the lvalue was loaded into is stored back to its slot
    if (!is_parameter(lhs) and not type::is_temporary(lhs) and
        accessor_->stack->is_allocated(lhs)) {
        auto& instructions =
            accessor_->instruction_accessor->get_instructions();
        auto stack_register =
            accessor_->register_accessor.get_available_register(
                accessor_->device_accessor.get_word_size_from_lvalue(lhs));
        auto stack_address = accessor_->stack->get(lhs).first;
        arm64_add__asm(instructions, str, stack_register, stack_address);
    }

    // the scratch registers of an assignment are free after it
    if (!type::is_temporary(lhs) and
        not accessor_->register_accessor.stack.empty())
        accessor_->register_accessor.stack.clear();
}

/**
 * @brief Check if a temporary is an operand after the next instruction
 *
 * The accumulator holds a temporary until the next instruction, so a
 * temporary used after it is stored on the stack. The lvalue of a MOV is
 * its destination, not an operand, so it is skipped.
 *
 *  Example:
 *
 *    _t2 = x;
 *    x = ++x;
 *    y = _t2;
 *
 *  Becomes:
 *
 *    is_temporary_used_after_next_instruction("_t2")   // true
 */
bool IR_Instruction_Visitor::is_temporary_used_after_next_instruction(
    LValue const& temporary)
{
    auto& ir_instructions = accessor_->table_accessor.get_ir_instructions();
    auto is_operand = [&](std::string_view operand) {
        return std::ranges::any_of(
            std::views::split(operand, ' '), [&](auto const& part) {
                return std::string_view{ part.begin(), part.end() } ==
                       temporary;
            });
    };
    for (auto index = iterator_index_ + 2; index < ir_instructions.size();
        index++) {
        auto const& inst = ir_instructions.at(index);
        if (std::get<0>(inst) == ir::Instruction::FUNC_END)
            break;
        if ((std::get<0>(inst) != ir::Instruction::MOV and
                is_operand(std::get<1>(inst))) or
            is_operand(std::get<2>(inst)) or is_operand(std::get<3>(inst)))
            return true;
    }
    return false;
}

/**
 * @brief Store the value of a temporary in a stack slot
 *
 * The slot is the temporary itself, or the argument slot it is assigned to,
 * and is allocated at the size of the value
 *
 *  Example:
 *
 *    _t2 = x;
 *    x = ++x;
 *    y = _t2;
 *
 *  Becomes:
 *
 *    mov w8, w9
 *    str w8, [sp, #16]
 *    add w9, w9, #1
 *    ldr w10, [sp, #16]
 */
void IR_Instruction_Visitor::insert_temporary_to_stack(LValue const& temporary,
    LValue const& slot)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto value = std::get<Register>(
        operand_inserter.get_operand_storage_from_expression_operand(
            temporary));
    auto size = assembly::is_doubleword_register(value)
                    ? Operand_Size::Doubleword
                    : Operand_Size::Word;
    if (!accessor_->stack->is_allocated(slot))
        accessor_->stack->set_address_from_size(slot, size);
    arm64_add__asm(instructions, str, value, accessor_->stack->get(slot).first);
}

/**
 * @brief Store a value at the address in a pointer
 *
 * The store is the size of the object the pointer points to, so a word is
 * sign extended into a pointer to a doubleword. The pointer is a parameter
 * register, a local register, or a stack slot loaded into x10.
 *
 *  Example:
 *
 *    *b = x;
 *
 *  Becomes:
 *
 *    mov w8, w9
 *    str w8, [x0]
 */
void IR_Instruction_Visitor::insert_to_pointer(LValue const& pointer,
    RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto value = operand_inserter.insert_operand_into_accumulator(rvalue);
    if (operand_inserter.get_pointee_size(pointer) == 8UL) {
        if (!assembly::is_doubleword_register(value)) {
            arm64_add__asm(instructions, sxtw, x8, value);
            value = Register::x8;
        }
    } else
        value = assembly::get_word_register_from_doubleword(value);
    auto address =
        assembly::get_doubleword_register_from_word(std::get<Register>(
            operand_inserter.get_operand_storage_from_expression_operand(
                pointer)));
    arm64_add__asm(instructions,
        str,
        value,
        direct_immediate(fmt::format("[{}]",
            common::assembly::get_storage_as_string<Register>(address))));
}

/**
 * @brief IR Instruction Instruction::PUSH
 *
 * The arguments of a call are pushed in reverse order, so each is added to
 * the front of the argument stack, and the first argument is at the front.
 * An argument stored in a slot before the call is loaded from its slot.
 *
 *  Example:
 *
 *    PUSH _p5_4;
 *    PUSH _p4_3;
 *
 *  Becomes:
 *
 *    argument_stack   // { (1:int:4), (2:int:4) }
 */
void IR_Instruction_Visitor::from_push_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& table = accessor_->table_accessor.get_table();
    auto& frame = stack_frame_.get_stack_frame();
    auto argument = std::get<1>(inst);
    stack_frame_.argument_stack.emplace_front(
        table->lvalue_at_temporary_object_address(argument, frame));
    stack_frame_.argument_slots.emplace_front(
        accessor_->stack->is_allocated(argument) ? argument : "");
}

/**
 * @brief IR Instruction Instruction::RETURN
 *
 * The value of the return statement is moved to the return register, w0 or
 * x0. A function may return from more than one statement.
 *
 *  Example:
 *
 *    RET z ;
 *
 *  Becomes:
 *
 *    mov w0, w11
 */
void IR_Instruction_Visitor::from_return_ita(ir::Quadruple const& inst)
{
    auto inserter = Expression_Inserter{ accessor_ };
    auto& frame = accessor_->table_accessor.get_table()
                      ->functions.at(stack_frame_.symbol)
                      .get();
    if (!frame.ret.has_value())
        return;
    // each return statement returns its own rvalue
    auto rvalue = util::str_trim_ws(std::get<1>(inst));
    inserter.insert_from_return_rvalue(
        std::make_pair(frame.ret->first, rvalue));
}

/**
 * @brief IR Instruction Instruction::LEAVE
 *
 * The frame record is restored by a post-indexed load of the frame size,
 * which is set at the end of the function. A function returns to its
 * caller, and `main` exits with the exit syscall.
 *
 *  Example:
 *
 *    LEAVE;
 *
 *  Becomes, in `main` on Darwin:
 *
 *    ldp x29, x30, [sp], #32
 *    mov w0, #0
 *    mov x16, #1
 *    svc #0x80
 */
void IR_Instruction_Visitor::from_leave_ita()
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();

    if (stack_frame_.symbol == "main") {
        auto argv_imm = direct_immediate("[sp, #16]");
        auto argc_argv =
            common::runtime::argc_argv_kernel_runtime_access(stack_frame_);
        if (argc_argv.first)
            arm64_add__asm(instructions, ldr, x19, argv_imm);
    }

    auto sp_imm = direct_immediate("[sp]");
    // replaced safely in the code generator
    auto code_imm = direct_immediate("0x0");
    arm64_add__asm(instructions, ldp, x29, x30, sp_imm, code_imm);

    if (stack_frame_.symbol == "main") {
        auto syscall_inserter =
            syscall_ns::Syscall_Invocation_Inserter{ accessor_, stack_frame_ };
        syscall_inserter.exit_syscall(instructions, 0);
    } else
        arm64_add__asm(instructions, ret);
}

/**
 * @brief IR Instruction Instruction::LABEL
 *
 * A label is local to its function, `_L4` in `main` is `._L4__main`
 */
void IR_Instruction_Visitor::from_label_ita(ir::Quadruple const& inst)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    instructions.emplace_back(
        type::get_label_as_human_readable(std::get<1>(inst)));
}

/**
 * @brief IR Instruction Instruction::POP
 *
 * The arguments of the call are in registers, so the argument stack and
 * slots are cleared and nothing is emitted
 */
void IR_Instruction_Visitor::from_pop_ita()
{
    stack_frame_.size = 0;
    stack_frame_.argument_stack.clear();
    stack_frame_.argument_slots.clear();
    if (!stack_frame_.call_stack.empty())
        stack_frame_.call_stack.pop_back();
}

/**
 * @brief IR Instruction Instruction::CALL
 *
 * A call is to a syscall, a function of the standard library, or a
 * function of the program, and its arguments are loaded from the argument
 * stack into the argument registers
 *
 *  Example:
 *
 *    CALL compute;
 *
 *  Becomes:
 *
 *    mov x0, #1
 *    mov x1, #2
 *    bl compute
 */
void IR_Instruction_Visitor::from_call_ita(ir::Quadruple const& inst)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto inserter = Invocation_Inserter{ accessor_ };

    auto function_name = type::get_label_as_human_readable(std::get<1>(inst));

    auto is_syscall_function = [&](Label const& label) {
#if defined(__linux__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64);
#elif defined(_WIN32) || defined(_WIN64)
        return common::runtime::is_syscall_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64);
#endif
    };
    auto is_stdlib_function = [&](Label const& label) {
#if defined(__linux__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64);

#elif defined(__APPLE__) || defined(__bsdi__)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64);
#elif defined(_WIN32) || defined(_WIN64)
        return common::runtime::is_stdlib_function(label,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64);
#endif
    };
    m::match(function_name)(
        m::pattern | m::app(is_syscall_function, true) =
            [&] {
                inserter.insert_from_syscall_function(
                    function_name, instruction_accessor->get_instructions());
            },
        m::pattern | m::app(is_stdlib_function, true) =
            [&] {
                inserter.insert_from_standard_library_function(
                    function_name, instruction_accessor->get_instructions());
            },
        m::pattern | m::_ =
            [&] {
                inserter.insert_from_user_defined_function(
                    function_name, instruction_accessor->get_instructions());
            });

    stack_frame_.call_stack.emplace_back(
        accessor_->get_frame_in_memory().symbol);
    stack_frame_.tail = function_name;
}

/**
 * @brief IR Instruction Instruction::GOTO
 *
 *  Example:
 *
 *    GOTO _L4;
 *
 *  Becomes:
 *
 *    b ._L4__main
 */
void IR_Instruction_Visitor::from_goto_ita(ir::Quadruple const& inst)
{
    auto label = common::assembly::make_direct_immediate(
        assembly::make_label(std::get<1>(inst), stack_frame_.symbol));
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    arm64_add__asm(instructions, b, label);
}

/**
 * @brief IR Instruction Instruction::LOCL
 *
 * A pointer is a doubleword and a vector is allocated on the stack, and any
 * other local is a register or stack slot of its size
 *
 *  Example:
 *
 *    LOCL x;
 *    LOCL *p;
 *
 *  Becomes, in a frame with no calls:
 *
 *    x  -> w9
 *    p  -> x10
 */
void IR_Instruction_Visitor::from_locl_ita(ir::Quadruple const& inst)
{
    auto locl_lvalue = std::get<1>(inst);
    auto& table = accessor_->table_accessor.get_table();
    auto& stack = accessor_->stack;
    auto is_vector = [&](RValue const& rvalue) {
        return table->vectors.contains(type::from_lvalue_offset(rvalue));
    };
    m::match(locl_lvalue)(
        m::pattern | m::app(type::is_dereference_expression, true) =
            [&] {
                auto lvalue =
                    type::get_unary_rvalue_reference(std::get<1>(inst));
                accessor_->device_accessor.insert_lvalue_to_device(
                    lvalue, Operand_Size::Doubleword);
            },
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto& vector = table->vectors.at(locl_lvalue).get();
                auto size = stack->get_stack_size_from_table_vector(vector);
                stack->set_address_from_size(locl_lvalue, size);
            },
        m::pattern | m::_ =
            [&] {
                accessor_->device_accessor.insert_lvalue_to_device(locl_lvalue);
            }

    );
}

/**
 * @brief IR Instruction Instruction::JMP_E
 *
 * A case of a switch statement, the jump is taken when the operand of its
 * CMP temporary is equal to the case
 *
 *  Example:
 *
 *    _t3 = CMP x;
 *    JMP_E _t3 (1:int:4) _L5;
 *
 *  Becomes:
 *
 *    mov w8, w9
 *    cmp w8, #1
 *    b.eq ._L5__main
 */
void IR_Instruction_Visitor::from_jmp_e_ita(ir::Quadruple const& inst)
{
    auto [_, of, with, jump] = inst;
    auto& frame = stack_frame_.get_stack_frame();
    auto of_comparator = frame.temporary.at(of).substr(4);
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto of_rvalue_storage =
        accessor_->address_accessor
            .get_arm64_lvalue_and_insertion_instructions(
                of_comparator, instructions.size(), accessor_->device_accessor)
            .first;
    auto with_rvalue_storage = type::get_rvalue_datatype_from_string(with);
    auto jump_label = assembly::make_label(jump, stack_frame_.symbol);
    auto comparator_instructions = assembly::r_eq(
        of_rvalue_storage, with_rvalue_storage, jump_label, Register::w8);
    assembly::inserter(instructions, comparator_instructions);
}

/**
 * @brief IR Instruction Instruction::IF
 *
 * A relational condition inserted its own conditional jump, otherwise the
 * jump is taken when the condition is not zero
 */
void IR_Instruction_Visitor::from_if_ita(ir::Quadruple const& inst)
{
    if (accessor_->relational_jump) {
        accessor_->relational_jump = false;
        return;
    }
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto condition =
        operand_inserter.get_operand_storage_from_expression_operand(
            std::get<1>(inst));
    if (is_variant(Immediate, condition)) {
        arm64_add__asm(instructions, mov, w7, condition);
        condition = Register::w7;
    }
    auto label = common::assembly::make_direct_immediate(
        assembly::make_label(std::get<3>(inst), stack_frame_.symbol));
    arm64_add__asm(instructions, cmp, condition, u32_int_immediate(0));
    arm64_add__asm(instructions, b_ne, label);
}
}