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
#include "credence/target/arm64/visitor.h"      // for IR_Instruction_Visitor
#include "credence/target/common/accessor.h"    // for Buffer_Accessor
#include "credence/target/common/assembly.h"    // for direct_immediate
#include "credence/target/common/memory.h"      // for is_temporary, is_imm...
#include "credence/target/common/runtime.h"     // for is_stdlib_function
#include "credence/target/common/types.h"       // for Immediate, Table_Poi...
#include "credence/util.h"                      // for is_variant, sv, __so...
#include "flags.h"                              // for Align_Folded
#include "memory.h"                             // for Memory_Accessor, Ins...
#include "runtime.h"                            // for Library_Call_Inserter
#include "stack.h"                              // for Stack
#include "syscall.h"                            // for syscall_arguments_t
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
 * ARM64 Instruction Inserters
 *
 * Translates each kind of IR expression into ARM64 instructions. See
 * common/inserter.h for the interfaces. The value of an expression is in the
 * accumulator, w8 or x8, and the right-hand-side of a binary expression is
 * in w7 or x7.
 *
 *  Example:
 *
 *    _t2 = y * (2:int:4);
 *    _t3 = x + _t2;
 *    z = _t3;
 *    _t7 = z > (10:int:4);
 *    IF _t7 GOTO _L6;
 *
 *  Becomes, where x, y, and z are in w9, w11, and w12:
 *
 *    mov w8, w11
 *    mov w7, #2
 *    mul w8, w8, w7
 *    mov w7, w8           // _t2 is the right-hand-side
 *    mov w8, w9
 *    add w8, w8, w7
 *    mov w12, w8
 *    mov w8, w12
 *    cmp w8, #10
 *    b.gt ._L6__main      // the IF is fused with its comparison
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

namespace {

/**
 * @brief Check if the return value of a function is a doubleword
 *
 * A pointer, a pointer parameter, or a string is a doubleword
 */
bool is_doubleword_return_value(object::Function& frame)
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
 * @brief Set up the stack frame of a function at its `BeginFunc`
 *
 * The label of the function is the IR instruction before `BeginFunc`
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
    accessor_->device_accessor.set_current_frame_symbol(name);
    visitor.from_func_start_ita(name);
}

/**
 * @brief Type check the argument of `print`, and add its length
 *
 * The argument must be a string or a buffer address, and its length is the
 * size of the string in the object table, or the length of the last
 * `read(2)` into the buffer
 *
 *  Example:
 *
 *    print("hello");
 *
 *  Becomes:
 *
 *    operands   // { ._L_str1__, (5:int:4) }
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
    operands.emplace_back(
        common::assembly::make_u32_int_immediate(buffer_size));
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
 * @brief Insert the address of an lvalue into its pointer slot
 *
 * The address is stored in a doubleword slot of the frame, whose offset is
 * resolved from the top of the frame when the frame has no calls
 */
common::Stack_Offset Unary_Operator_Inserter::from_lvalue_address_of_expression(
    RValue const& expr)
{
    auto& table = accessor_->table_accessor.get_table();
    auto& instruction_accessor = accessor_->instruction_accessor;
    auto& address_space = accessor_->address_accessor;

    credence_assert(type::is_unary_expression(expr));

    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    auto offset =
        accessor_->device_accessor.get_device_by_lvalue_reference(rvalue);
    address_space.address_ir_assignment = true;
    accessor_->stack->allocate_pointer_on_stack();
    accessor_->get_frame_in_memory().get_stack_frame().pointers.emplace_back(
        rvalue);

    accessor_->stack->add_address_location_to_stack(rvalue);

    auto stack_frame_has_jump = ir::stack_frame_contains_call_instruction(
        table->get_stack_frame(stack_frame_.symbol),
        accessor_->table_accessor.get_ir_instructions());

    if (!stack_frame_has_jump)
        accessor_->flag_accessor.set_instruction_flag(
            detail::flags::Align_Folded, instruction_accessor->size());

    insert_from_unary_operator_operands(
        op, offset, accessor_->stack->get(rvalue).first);

    if (!stack_frame_has_jump)
        accessor_->flag_accessor.set_instruction_flag(
            detail::flags::Align_SP_Folded, instruction_accessor->size());

    return accessor_->stack->get(rvalue).first;
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
 * @brief Binary operator inserter of expression operands
 *
 * The left-hand-side is loaded into the accumulator, and the operator is
 * applied with the right-hand-side. A right-hand-side temporary in the
 * accumulator is moved to the right-hand-side scratch register, w7, first.
 * An expression of two immediates is folded into one.
 *
 *  Example:
 *
 *    _t3 = x * _t2;
 *
 *  Becomes:
 *
 *    mov w7, w8
 *    mov w8, w9
 *    mul w8, w8, w7
 */
void Binary_Operator_Inserter::from_binary_operator_expression(
    RValue const& rvalue)
{
    credence_assert(type::is_binary_expression(rvalue));
    auto& instructions = accessor_->instruction_accessor->get_instructions();
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
                        Operand_Size::Doubleword
                ? Register::x8
                : Register::w8;
        arm64_add__asm(instructions, mov, accumulator, immediate);
        result = accumulator;
        return;
    }

    Storage rhs_s = assembly::O_NUL;
    auto const& held = accessor_->accumulator_temporary;
    if (held.has_value() and held->first == rhs and lhs != rhs) {
        auto scratch = assembly::is_doubleword_register(held->second)
                           ? Register::x7
                           : Register::w7;
        arm64_add__asm(instructions, mov, scratch, held->second);
        rhs_s = scratch;
    }
    auto accumulator = operand_inserter.insert_operand_into_accumulator(lhs);
    if (rhs_s == assembly::O_NUL)
        rhs_s =
            operand_inserter.get_operand_storage_from_expression_operand(rhs);

    // the operands of an instruction are the same size
    if (is_variant(Register, rhs_s)) {
        auto rhs_r = std::get<Register>(rhs_s);
        if (assembly::is_doubleword_register(rhs_r) and
            not assembly::is_doubleword_register(accumulator)) {
            arm64_add__asm(instructions, sxtw, x8, w8);
            accumulator = Register::x8;
        } else if (assembly::is_doubleword_register(accumulator) and
                   not assembly::is_doubleword_register(rhs_r)) {
            arm64_add__asm(instructions, sxtw, x7, rhs_r);
            rhs_s = Register::x7;
        }
    }

    result = accumulator;
    if (op == "%") {
        insert_from_modulo_operands(accumulator, rhs_s);
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
    } else {
        // the logical immediates of `and`, `orr`, and `eor` are bitmasks
        if (is_variant(Immediate, rhs_s)) {
            auto scratch = assembly::is_doubleword_register(accumulator)
                               ? Register::x7
                               : Register::w7;
            arm64_add__asm(instructions, mov, scratch, rhs_s);
            rhs_s = scratch;
        }
        auto bitwise = Bitwise_Operator_Inserter{ accessor_ };
        assembly::inserter(instructions,
            bitwise
                .from_bitwise_expression_operands(
                    { accumulator, accumulator, rhs_s }, op)
                .second);
    }
}

/**
 * @brief Inserter of the remainder of a signed division
 *
 *  Example:
 *
 *    _t2 = x % 5;
 *
 *  Becomes:
 *
 *    mov w7, #5
 *    sdiv w10, w8, w7
 *    msub w8, w10, w7, w8
 */
void Binary_Operator_Inserter::insert_from_modulo_operands(Register accumulator,
    Storage divisor)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    bool doubleword = assembly::is_doubleword_register(accumulator);
    if (is_variant(Immediate, divisor)) {
        auto scratch = doubleword ? Register::x7 : Register::w7;
        arm64_add__asm(instructions, mov, scratch, divisor);
        divisor = scratch;
    }
    auto quotient = doubleword ? Register::x10 : Register::w10;
    if (std::get<Register>(divisor) == quotient)
        quotient = doubleword ? Register::x6 : Register::w6;
    arm64_add__asm(instructions, sdiv, quotient, accumulator, divisor);
    arm64_add__asm(
        instructions, msub, accumulator, quotient, divisor, accumulator);
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
 *    cmp w8, #5
 *    cset w8, lt
 */
void Binary_Operator_Inserter::insert_from_relational_operands(
    Register accumulator,
    Storage const& rhs,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& ir_instructions = accessor_->table_accessor.get_ir_instructions();
    auto index = accessor_->table_accessor.get_index();
    auto condition = m::match(op)(
        m::pattern | std::string{ "==" } = [] { return std::string{ "eq" }; },
        m::pattern | std::string{ "!=" } = [] { return std::string{ "ne" }; },
        m::pattern | std::string{ "<" } = [] { return std::string{ "lt" }; },
        m::pattern | std::string{ ">" } = [] { return std::string{ "gt" }; },
        m::pattern | std::string{ "<=" } = [] { return std::string{ "le" }; },
        m::pattern | m::_ = [] { return std::string{ "ge" }; });

    arm64_add__asm(instructions, cmp, accumulator, rhs);
    if (index + 1 < ir_instructions.size()) {
        auto const& current = ir_instructions.at(index);
        auto const& next = ir_instructions.at(index + 1);
        if (std::get<0>(next) == ir::Instruction::IF and
            std::get<1>(next) == std::get<1>(current)) {
            auto label = common::assembly::make_direct_immediate(
                assembly::make_label(std::get<3>(next), stack_frame_.symbol));
            if (op == "==")
                arm64_add__asm(instructions, b_eq, label);
            else if (op == "!=")
                arm64_add__asm(instructions, b_ne, label);
            else if (op == "<")
                arm64_add__asm(instructions, b_lt, label);
            else if (op == ">")
                arm64_add__asm(instructions, b_gt, label);
            else if (op == "<=")
                arm64_add__asm(instructions, b_le, label);
            else
                arm64_add__asm(instructions, b_ge, label);
            accessor_->relational_jump = true;
            result = std::nullopt;
            return;
        }
    }
    arm64_add__asm(instructions, cset, w8, direct_immediate(condition));
    result = Register::w8;
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
 *    cmp w8, #0
 *    cset w8, ne
 *    cmp w7, #0
 *    cset w7, ne
 *    orr w8, w8, w7
 */
void Binary_Operator_Inserter::insert_from_logical_operands(
    Register accumulator,
    Storage const& rhs,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    Storage rhs_s = rhs;
    if (is_variant(Immediate, rhs_s)) {
        arm64_add__asm(instructions, mov, w7, rhs_s);
        rhs_s = Register::w7;
    }
    arm64_add__asm(instructions, cmp, accumulator, u32_int_immediate(0));
    arm64_add__asm(instructions, cset, w8, direct_immediate("ne"));
    arm64_add__asm(instructions, cmp, rhs_s, u32_int_immediate(0));
    arm64_add__asm(instructions, cset, w7, direct_immediate("ne"));
    if (op == "&&")
        arm64_add__asm(instructions, and_, w8, w8, w7);
    else
        arm64_add__asm(instructions, orr, w8, w8, w7);
    result = Register::w8;
}

/**
 * @brief Get the operand size of the lvalue of a unary expression
 *
 * A vector element is loaded to find its size, and a literal has the size
 * of its type
 */
Operand_Size Unary_Operator_Inserter::get_operand_size_from_lvalue_reference(
    LValue const& lvalue)
{
    auto& device_accessor = accessor_->device_accessor;
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto is_vector = [&](RValue const& rvalue) {
        return accessor_->table_accessor.get_table()->vectors.contains(
            type::from_lvalue_offset(rvalue));
    };
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->device_accessor.is_lvalue_allocated_in_memory(rvalue);
    };
    return m::match(lvalue)(
        m::pattern | m::app(is_address, true) =
            [&] { return device_accessor.get_word_size_from_lvalue(lvalue); },
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto [vector_s, vector_i] =
                    accessor_->address_accessor
                        .get_arm64_lvalue_and_insertion_instructions(
                            lvalue, instructions.size(), device_accessor);
                assembly::inserter(instructions, vector_i);
                return memory::get_operand_size_from_storage(
                    vector_s, accessor_->stack);
            },
        m::pattern | m::_ =
            [&] {
                auto immediate = type::get_rvalue_datatype_from_string(lvalue);
                return assembly::get_operand_size_from_rvalue_datatype(
                    immediate);
            });
}

/**
 * @brief Get the accumulator of the value of a unary expression
 */
Storage Unary_Operator_Inserter::get_temporary_storage_from_temporary_expansion(
    RValue const& rvalue)
{
    auto size = get_operand_size_from_lvalue_reference(rvalue);
    auto acc = accessor_->get_accumulator_with_rvalue_context(size);
    return acc;
}

/**
 * @brief Insert a unary expression on its operand
 *
 * An operand with a device is updated in place, such as `++x`, and a vector
 * element or literal is loaded into the accumulator first
 *
 *  Example:
 *
 *    x = ++x;
 *
 *  Becomes:
 *
 *    add w9, w9, #1
 */
Storage Unary_Operator_Inserter::insert_from_unary_operator_rvalue(
    RValue const& expr)
{
    credence_assert(type::is_unary_expression(expr));
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& devices = accessor_->device_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& address_space = accessor_->address_accessor;

    Storage storage{};

    auto op = type::get_unary_operator(expr);
    RValue rvalue = type::get_unary_rvalue_reference(expr);
    auto is_vector = [&](RValue const& rvalue) {
        return accessor_->table_accessor.get_table()->vectors.contains(
            type::from_lvalue_offset(rvalue));
    };
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->device_accessor.is_lvalue_allocated_in_memory(rvalue);
    };

    m::match(rvalue)(
        m::pattern | m::app(is_address, true) =
            [&] {
                auto [e_storage, e_inst] =
                    address_space.get_arm64_lvalue_and_insertion_instructions(
                        rvalue, instructions.size(), devices);
                assembly::inserter(instructions, e_inst);
                storage = e_storage;
                insert_from_unary_operator_operands(op, storage, storage);
            },
        m::pattern | m::app(is_vector, true) =
            [&] {
                auto [address, address_inst] =
                    address_space.get_arm64_lvalue_and_insertion_instructions(
                        rvalue, instructions.size(), devices);
                assembly::inserter(instructions, address_inst);
                storage =
                    get_temporary_storage_from_temporary_expansion(rvalue);
                address_space.address_ir_assignment = true;
                accessor_->set_signal_register(Register::x8);
                insert_from_unary_operator_operands(op, storage, address);
            },

        m::pattern | m::_ =
            [&] {
                auto immediate = type::get_rvalue_datatype_from_string(rvalue);
                storage =
                    get_temporary_storage_from_temporary_expansion(rvalue);
                insert_from_unary_operator_operands(op, storage, immediate);
            });

    return storage;
}

/**
 * @brief Insert the instruction of a unary operator
 *
 * The increment and decrement are in place, `&` stores an address, and `*`
 * stores through the address in the destination
 */
void Unary_Operator_Inserter::insert_from_unary_operator_operands(
    std::string const& op,
    Storage const& dest,
    Storage const& src)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& table_accessor = accessor_->table_accessor;
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
                auto size = memory::get_word_size_from_storage(
                    dest, accessor_->stack, accessor_->get_frame_in_memory());
                if (table_accessor.is_ir_instruction_temporary() and
                    table_accessor.next_ir_instruction_is_temporary()) {
                    auto acc =
                        accessor_->get_accumulator_with_rvalue_context(size);
                    assembly::inserter(
                        instructions, assembly::b_not(acc, src).second);
                    accessor_->register_accessor.stack.emplace_back(acc);
                } else
                    assembly::inserter(
                        instructions, assembly::b_not(dest, src).second);
            },
        m::pattern | std::string{ "&" } =
            [&] {
                accessor_->flag_accessor.set_instruction_flag(
                    common::flag::Address, index);
                assembly::inserter(instructions,
                    assembly::store(dest, std::get<common::Stack_Offset>(src))
                        .second);
            },
        m::pattern | std::string{ "*" } =
            [&] {
                auto size = memory::get_word_size_from_storage(
                    dest, accessor_->stack, accessor_->get_frame_in_memory());
                auto acc = accessor_->get_accumulator_with_rvalue_context(size);
                if (!assembly::is_equal_storage_devices(acc, dest)) {
                    arm64_add__asm(instructions, mov, acc, dest);
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::Indirect_Source, index + 1);
                } else {
                    accessor_->flag_accessor.set_instruction_flag(
                        common::flag::Indirect_Source, index);
                }

                arm64_add__asm(instructions, str, acc, src);
            },
        m::pattern | std::string{ "-" } =
            [&] {
                assembly::inserter(
                    instructions, assembly::neg(dest, src).second);
            },
        m::pattern | std::string{ "+" } = [&] {});
}

/**
 * @brief Insert the value of a temporary into the accumulator
 *
 * The expression of the temporary is in the object table, and the
 * accumulator holds its value until the next instruction
 *
 *  Example:
 *
 *    _t2 = y * (2:int:4);
 *
 *  Becomes:
 *
 *    mov w8, w11
 *    mov w7, #2
 *    mul w8, w8, w7
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
    insert_from_temporary_rvalue(temporary);
    if (result_.has_value())
        accessor_->accumulator_temporary = { lvalue, *result_ };
}

/**
 * @brief Move the value of a return statement to the return register
 *
 * The return register is x0 for a doubleword, such as a pointer, and w0
 * otherwise
 *
 *  Example:
 *
 *    test(*y) {
 *      return(y);
 *    }
 *
 *  Becomes:
 *
 *    mov x0, x0
 */
void Expression_Inserter::insert_from_return_rvalue(
    object::Function::Return_RValue const& ret)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto immediate =
        operand_inserter.get_operand_storage_from_expression_operand(
            ret->second);
    // a string literal is the address of its label
    if (assembly::is_immediate_relative_address(immediate)) {
        auto label = type::get_value_from_rvalue_data_type(
            std::get<Immediate>(immediate));
        arm64_add__asm(instructions,
            adrp,
            x0,
            assembly::page_offset_upper_immediate(label));
        arm64_add__asm(instructions,
            add,
            x0,
            x0,
            assembly::page_offset_lower_immediate(label));
        return;
    }
    if (memory::is_doubleword_storage_size(
            immediate, accessor_->stack, accessor_->get_frame_in_memory()))
        arm64_add__asm(instructions, mov, x0, immediate);
    else
        arm64_add__asm(instructions, mov, w0, immediate);
}

/**
 * @brief Insert the address of an lvalue into the accumulator
 *
 * A global vector element is its page address plus its offset, and a local
 * vector element or a local on the stack is its offset from the stack
 * pointer
 *
 *  Example:
 *
 *    p = &y;
 *
 *  Becomes, where y is on the stack:
 *
 *    add x8, sp, #16
 */
void Expression_Inserter::insert_from_address_of_rvalue(RValue const& rvalue)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto const& table = accessor_->table_accessor.get_table();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto lvalue = type::get_unary_rvalue_reference(rvalue);
    auto label = type::from_lvalue_offset(lvalue);
    // the address of a global vector element is from its page address
    if (is_vector_offset(lvalue) and table->vectors.contains(label) and
        table->globals.is_pointer(label)) {
        auto offset =
            accessor_->vector_accessor
                .get_offset_address(lvalue, type::from_decay_offset(lvalue))
                .first;
        arm64_add__asm(instructions,
            adrp,
            x8,
            assembly::page_offset_upper_immediate(label));
        arm64_add__asm(instructions,
            add,
            x8,
            x8,
            assembly::page_offset_lower_immediate(label));
        if (offset != 0UL)
            arm64_add__asm(
                instructions, add, x8, x8, u32_int_immediate(offset));
        result_ = Register::x8;
        return;
    }
    // the address of an element of a vector on the stack
    if (operand_inserter.is_local_vector_offset(lvalue)) {
        auto offset = std::get<common::Stack_Offset>(
            operand_inserter.get_operand_storage_from_local_vector_offset(
                lvalue));
        arm64_add__asm(instructions, add, x8, sp, u32_int_immediate(offset));
        result_ = Register::x8;
        return;
    }
    // the address of a local, which is on the stack when its address is
    // taken
    if (accessor_->stack->is_allocated(lvalue)) {
        auto offset = accessor_->stack->get(lvalue).first;
        arm64_add__asm(instructions, add, x8, sp, u32_int_immediate(offset));
        result_ = Register::x8;
        return;
    }
    if (accessor_->table_accessor.next_ir_instruction_is_assignment()) {
        auto assignment =
            std::get<1>(accessor_->table_accessor.get_ir_instructions().at(
                accessor_->table_accessor.get_index() + 1));
        auto lhs_s =
            accessor_->device_accessor.get_device_by_lvalue(assignment);
        accessor_->address_accessor.address_ir_assignment = true;
        accessor_->stack->allocate_pointer_on_stack();
        accessor_->get_frame_in_memory()
            .get_stack_frame()
            .pointers.emplace_back(rvalue);
        accessor_->stack->add_address_location_to_stack(rvalue);
        if (is_variant(common::Stack_Offset, lhs_s)) {
            lhs_s = u32_int_immediate(std::get<common::Stack_Offset>(lhs_s));
            auto rhs_s = assembly::O_NUL;
            auto last_arm64_inst = std::get<1>(instructions.back());
            Storage last_src_one = std::get<1>(last_arm64_inst);
            if (is_variant(Register, last_src_one) and
                (std::get<Register>(last_src_one) == Register::x10 or
                    std::get<Register>(last_src_one) == Register::w10)) {
                rhs_s = std::get<common::Stack_Offset>(
                    std::get<2>(last_arm64_inst));
            } else
                rhs_s = accessor_->stack->get(rvalue).first;
            arm64_add__asm(instructions, add, x6, sp, lhs_s);
            arm64_add__asm(instructions, str, x6, rhs_s);
        } else {
            auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
            auto rhs_s = u32_int_immediate(
                unary_inserter.from_lvalue_address_of_expression(rvalue));
            arm64_add__asm(instructions, add, x6, sp, rhs_s);
        }
    } else {
        auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
        auto rhs_s = u32_int_immediate(
            unary_inserter.from_lvalue_address_of_expression(rvalue));
        arm64_add__asm(instructions, add, x6, sp, rhs_s);
    }
    return;
}

/**
 * @brief Insert the value of the rvalue of a temporary
 *
 * The rvalue is a binary or unary expression, a literal, the operand of a
 * comparison, or the return value of the last call, `RET`. The register
 * that holds the value is the result.
 */
void Expression_Inserter::insert_from_temporary_rvalue(RValue const& rvalue)
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

    m::match(rvalue)(
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] {
                binary_inserter.from_binary_operator_expression(rvalue);
                result_ = binary_inserter.result;
            },
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                if (type::is_address_of_expression(rvalue)) {
                    insert_from_address_of_rvalue(rvalue);
                    return;
                }
                result_ = insert_from_unary_value(rvalue);
            },
        m::pattern | m::app(type::is_rvalue_data_type, true) =
            [&] {
                Storage immediate =
                    operand_inserter.get_operand_storage_from_rvalue(rvalue);
                auto acc = accessor_->accumulator_accessor
                               .get_accumulator_register_from_storage(
                                   immediate, accessor_->stack);
                arm64_add__asm(instructions, mov, acc, immediate);
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
                    insert_from_temporary_rvalue(operand);
                else
                    result_ = operand_inserter.insert_operand_into_accumulator(
                        operand);
            },
        m::pattern | RValue{ "RET" } =
            [&] {
                // `getchar` and a syscall return a word in w0
                if (common::runtime::is_returning_library_function(
                        stack_frame_.tail,
                        common::assembly::Arch_Type::ARM64)) {
                    arm64_add__asm(instructions, mov, w8, w0);
                    result_ = Register::w8;
                    return;
                }
                if (is_stdlib_function(stack_frame_.tail))
                    return;
                credence_assert(table->functions.contains(stack_frame_.tail));
                auto& frame = table->functions.at(stack_frame_.tail).get();
                credence_assert(frame.ret.has_value());
                if (is_doubleword_return_value(frame)) {
                    arm64_add__asm(instructions, mov, x8, x0);
                    result_ = Register::x8;
                } else {
                    arm64_add__asm(instructions, mov, w8, w0);
                    result_ = Register::w8;
                }
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
 *    mov w8, w9
 *    neg w8, w8
 */
std::optional<Register> Expression_Inserter::insert_from_unary_value(
    RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto op = type::get_unary_operator(rvalue);
    if (op == "*") {
        auto operand_inserter = Operand_Inserter{ accessor_ };
        return operand_inserter.insert_operand_into_accumulator(
            fmt::format("*{}", type::get_unary_rvalue_reference(rvalue)));
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
            [&] {
                arm64_add__asm(instructions, neg, accumulator, accumulator);
            },
        m::pattern | std::string{ "~" } =
            [&] {
                arm64_add__asm(instructions, mvn, accumulator, accumulator);
            },
        m::pattern | std::string{ "!" } =
            [&] {
                arm64_add__asm(
                    instructions, cmp, accumulator, u32_int_immediate(0));
                arm64_add__asm(instructions, cset, w8, direct_immediate("eq"));
                accumulator = Register::w8;
            },
        m::pattern | m::_ = [&] {});
    return accumulator;
}

/**
 * @brief Insert an assignment to or from a global vector element
 *
 *  Example:
 *
 *    x = mess[1];
 *
 *  Becomes:
 *
 *    adrp x6, mess@PAGE
 *    add x6, x6, mess@PAGEOFF
 *    ldr x9, [x6, #8]
 */
void Expression_Inserter::insert_from_global_vector_assignment(
    LValue const& lhs,
    LValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto [lhs_storage, lhs_storage_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            lhs, instructions.size(), accessor_->device_accessor);
    assembly::inserter(instructions, lhs_storage_inst);
    auto [rhs_storage, rhs_storage_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rhs, instructions.size(), accessor_->device_accessor);
    assembly::inserter(instructions, rhs_storage_inst);
    accessor_->flag_accessor.set_instruction_flag(
        common::flag::Indirect_Source, instructions.size());
    arm64_add__asm(instructions, ldr, lhs_storage, rhs_storage);
}

/**
 * @brief Insert an arithmetic or relational expression of two operands
 *
 * An immediate left-hand-side is swapped to the right-hand-side, since the
 * first operand of an ARM64 instruction is a register, and a relational
 * expression jumps to the label of the IF that follows it
 */
void Operand_Inserter::insert_from_binary_operands(
    assembly::Assignment_Operands& operands,
    std::string const& op)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    if (is_variant(Immediate, operands.first) and
        not assembly::is_immediate_x1_address_offset(operands.first) and
        not assembly::is_immediate_relative_address(operands.first)) {
        std::swap(operands.first, operands.second);
    }

    m::match(op)(
        m::pattern | m::app(type::is_binary_arithmetic_operator, true) =
            [&] {
                auto arithmetic = Arithemtic_Operator_Inserter{ accessor_ };
                assembly::inserter(instructions,
                    arithmetic.from_arithmetic_expression_operands(operands, op)
                        .second);
            },
        m::pattern | m::app(type::is_relation_binary_operator, true) =
            [&] {
                auto relational = Relational_Operator_Inserter{ accessor_ };
                auto& ir_instructions =
                    accessor_->table_accessor.get_ir_instructions();
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
            },
        m::pattern | m::app(type::is_binary_arithmetic_operator, true) =
            [&] {
                auto relational = Relational_Operator_Inserter{ accessor_ };
                auto& ir_instructions =
                    accessor_->table_accessor.get_ir_instructions();
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
            },
        m::pattern | m::_ =
            [&, op] {
                credence_error(fmt::format("unreachable: operator '{}'", op));
            });
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
                arm64_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_relation_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_relational_expression(lhs, op, rhs);
                auto acc = accumulator.get_accumulator_register_from_size(
                    Operand_Size::Byte);
                accessor_->set_signal_register(acc);
                arm64_add__asm(instructions, mov, acc, imm);
            },
        m::pattern | m::app(type::is_bitwise_binary_operator, true) =
            [&] {
                auto imm = common::assembly::
                    get_result_from_trivial_bitwise_expression(lhs, op, rhs);
                auto acc = accessor_->get_accumulator_with_rvalue_context(imm);
                if (!accessor_->table_accessor.is_ir_instruction_temporary())
                    arm64_add__asm(instructions, mov, acc, imm);
                else
                    immediate_stack.emplace_back(imm);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); });
}

/**
 * @brief Store the return value of the last call in an lvalue
 *
 * The return register is x0 or w0 by the return value of the callee. A
 * standard library function has no return value, except `getchar`, which
 * returns its character in w0.
 *
 *  Example:
 *
 *  auto x = getchar();
 *  putchar(x);
 *
 */
void Expression_Inserter::insert_lvalue_from_return_rvalue(LValue const& lvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& table = accessor_->table_accessor.get_table();
    auto operand_inserter = Operand_Inserter{ accessor_ };
#if defined(__linux__)
    if (common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64) and
        not common::runtime::is_returning_library_function(
            stack_frame_.tail, common::assembly::Arch_Type::ARM64))
        return;
#elif defined(__APPLE__) || defined(__bsdi__)
    if (common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64) and
        not common::runtime::is_returning_library_function(
            stack_frame_.tail, common::assembly::Arch_Type::ARM64))
        return;
#elif defined(_WIN32) || defined(_WIN64)
    if (common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64) and
        not common::runtime::is_returning_library_function(
            stack_frame_.tail, common::assembly::Arch_Type::ARM64))
        return;
#endif
    auto returns_word = common::runtime::is_returning_library_function(
        stack_frame_.tail, common::assembly::Arch_Type::ARM64);
    if (!returns_word and not table->functions.contains(stack_frame_.tail))
        return;
    auto [lhs_s, lhs_i] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            lvalue, instructions.size(), accessor_->device_accessor);
    assembly::inserter(instructions, lhs_i);
    if (is_variant(common::Stack_Offset, lhs_s)) {
        auto lhs_r = accessor_->register_accessor.get_available_register(
            accessor_->device_accessor.get_word_size_from_lvalue(lvalue));
        arm64_add__asm(instructions, str, lhs_r, lhs_s);
        lhs_s = lhs_r;
    }
    if (returns_word)
        arm64_add__asm(instructions, mov, lhs_s, w0);
    else {
        auto& frame = table->functions.at(stack_frame_.tail).get();
        Storage return_s =
            is_doubleword_return_value(frame) ? Register::x0 : Register::w0;
        arm64_add__asm(instructions, mov, lhs_s, return_s);
    }
}

/**
 * @brief Insert an assignment of an rvalue to an lvalue
 *
 * The rvalue selects the instructions:
 *
 *  * A dereference, `b = *a`, loads through the pointer
 *
 *  * A pointer increment, `p++`, steps over the object it points to
 *
 *  * A parameter is copied from its argument register
 *
 *  * A literal is moved, or a string, float, or double is its label
 *
 *  * A binary or unary expression is computed, then stored
 *
 *  * A temporary is the value in the accumulator, or the return value of
 *    the last call
 *
 *  * A local is moved through the accumulator
 *
 *  Example:
 *
 *    y = x;
 *
 *  Becomes:
 *
 *    mov w8, w9
 *    mov w11, w8
 */
void Operand_Inserter::insert_from_mnemonic_operand(LValue const& lhs,
    RValue const& rhs)
{
    auto instruction_accessor = accessor_->instruction_accessor;
    auto& devices = accessor_->device_accessor;
    auto& instructions = instruction_accessor->get_instructions();
    auto& address_storage = accessor_->address_accessor;
    auto accumulator = accessor_->accumulator_accessor;
    auto is_address = [&](RValue const& rvalue) {
        return accessor_->device_accessor.is_lvalue_allocated_in_memory(rvalue);
    };

    // a dereference, `b = *a`
    if (type::is_dereference_expression(rhs)) {
        insert_from_accumulator_to_lvalue(
            lhs, insert_operand_into_accumulator(rhs));
        return;
    }
    // a pointer increment or decrement steps over the object it points to
    if (type::is_unary_expression(rhs) and
        (type::get_unary_operator(rhs) == "++" or
            type::get_unary_operator(rhs) == "--") and
        (stack_frame_.get_stack_frame().locals.is_pointer(lhs) or
            stack_frame_.get_stack_frame().is_pointer_parameter(lhs))) {
        auto pointer = insert_operand_into_accumulator(lhs);
        auto stride = u32_int_immediate(get_pointer_stride(lhs));
        if (type::get_unary_operator(rhs) == "++")
            arm64_add__asm(instructions, add, pointer, pointer, stride);
        else
            arm64_add__asm(instructions, sub, pointer, pointer, stride);
        insert_from_accumulator_to_lvalue(lhs, pointer);
        return;
    }
    // a copy of a parameter, `b = a`, is from its argument register, or the
    // stack for `argc`
    if (stack_frame_.get_stack_frame().is_parameter(rhs)) {
        insert_from_accumulator_to_lvalue(
            lhs, insert_operand_into_accumulator(rhs));
        return;
    }

    m::match(rhs)(
        m::pattern | m::app(is_immediate, true) =
            [&] {
                auto imm = type::get_rvalue_datatype_from_string(rhs);
                auto [lhs_storage, storage_inst] =
                    accessor_->address_accessor
                        .get_arm64_lvalue_and_insertion_instructions(
                            lhs, instructions.size(), devices);
                if (is_variant(common::Stack_Offset, lhs_storage)) {
                    auto lhs_r =
                        accessor_->register_accessor.get_available_register(
                            devices.get_word_size_from_lvalue(rhs));
                    arm64_add__asm(instructions, str, lhs_r, lhs_storage);
                    lhs_storage = lhs_r;
                }
                assembly::inserter(instructions, storage_inst);
                auto expression_inserter = Expression_Inserter{ accessor_ };
                if (type::get_type_from_rvalue_data_type(imm) == "string")
                    insert_from_string_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "float")
                    insert_from_float_address_operand(lhs, lhs_storage, rhs);
                else if (type::get_type_from_rvalue_data_type(imm) == "double")
                    insert_from_double_address_operand(lhs, lhs_storage, rhs);
                else
                    arm64_add__asm(instructions, mov, lhs_storage, imm);
            },
        m::pattern | m::app(type::is_binary_expression, true) =
            [&] {
                auto binary_inserter = Binary_Operator_Inserter{ accessor_ };
                binary_inserter.from_binary_operator_expression(rhs);
                if (binary_inserter.result.has_value())
                    insert_from_accumulator_to_lvalue(
                        lhs, *binary_inserter.result);
            },
        m::pattern | m::app(type::is_unary_expression, true) =
            [&] {
                auto unary_inserter = Unary_Operator_Inserter{ accessor_ };

                auto [lhs_storage, storage_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        lhs, instructions.size(), devices);
                if (is_variant(common::Stack_Offset, lhs_storage)) {
                    auto lhs_r =
                        accessor_->register_accessor.get_available_register(
                            devices.get_word_size_from_lvalue(rhs));
                    arm64_add__asm(instructions, str, lhs_r, lhs_storage);
                    lhs_storage = lhs_r;
                }
                assembly::inserter(instructions, storage_inst);
                auto unary_op = type::get_unary_operator(rhs);
                unary_inserter.insert_from_unary_operator_operands(
                    unary_op, lhs_storage);
            },
        m::pattern | m::app(is_temporary, true) =
            [&] {
                if (address_storage.address_ir_assignment) {
                    address_storage.address_ir_assignment = false;
                    auto [lhs_storage, storage_inst] =
                        address_storage
                            .get_arm64_lvalue_and_insertion_instructions(
                                lhs, instructions.size(), devices);
                    if (is_variant(common::Stack_Offset, lhs_storage)) {
                        auto lhs_r =
                            accessor_->register_accessor.get_available_register(
                                devices.get_word_size_from_lvalue(rhs));
                        arm64_add__asm(instructions, str, lhs_r, lhs_storage);
                        lhs_storage = lhs_r;
                    }
                    arm64_add__asm(instructions, mov, lhs_storage, x6);
                } else {
                    auto& frame = stack_frame_.get_stack_frame();
                    auto rvalue =
                        accessor_->table_accessor.get_table()
                            ->lvalue_at_temporary_object_address(rhs, frame);
                    if (rvalue == "RET") {
                        auto expression_inserter =
                            Expression_Inserter{ accessor_ };
                        expression_inserter.insert_lvalue_from_return_rvalue(
                            lhs);
                        return;
                    }
                    if (!type::is_unary_expression(lhs))
                        accessor_->device_accessor.insert_lvalue_to_device(lhs);
                    auto value =
                        get_operand_storage_from_expression_operand(rhs);
                    auto size = assembly::is_doubleword_register(
                                    std::get<Register>(value))
                                    ? Operand_Size::Doubleword
                                    : Operand_Size::Word;
                    auto accumulator = size == Operand_Size::Doubleword
                                           ? Register::x8
                                           : Register::w8;
                    if (std::get<Register>(value) != accumulator)
                        arm64_add__asm(instructions, mov, accumulator, value);
                    insert_from_accumulator_to_lvalue(lhs, accumulator);
                }
            },
        m::pattern | m::app(is_address, true) =
            [&] {
                auto [lhs_storage, lhs_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        lhs, instructions.size(), devices);
                assembly::inserter(instructions, lhs_inst);
                auto [rhs_storage, rhs_inst] =
                    address_storage.get_arm64_lvalue_and_insertion_instructions(
                        rhs, instructions.size(), devices);
                assembly::inserter(instructions, rhs_inst);
                auto acc = accumulator.get_accumulator_register_from_size(
                    devices.get_word_size_from_lvalue(rhs));

                arm64_add__asm(instructions, mov, acc, rhs_storage);
                arm64_add__asm(instructions, mov, lhs_storage, acc);
            },
        m::pattern | m::_ = [&] {});
}

/**
 * @brief Insert the value in the accumulator into an lvalue
 *
 * The accumulator is narrowed or sign-extended to the size of the lvalue,
 * and an lvalue on the stack is stored from the stack move register after
 * the instruction
 */
void Operand_Inserter::insert_from_accumulator_to_lvalue(LValue const& lhs,
    Register accumulator)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto& devices = accessor_->device_accessor;
    auto& frame = stack_frame_.get_stack_frame();
    auto insert_from_accumulator = [&](Register destination) {
        auto source = accumulator;
        if (assembly::is_doubleword_register(destination) and
            not assembly::is_doubleword_register(source)) {
            auto wide = assembly::get_doubleword_register_from_word(source);
            arm64_add__asm(instructions, sxtw, wide, source);
            source = wide;
        } else if (!assembly::is_doubleword_register(destination))
            source = assembly::get_word_register_from_doubleword(source);
        arm64_add__asm(instructions, mov, destination, source);
    };
    if (frame.is_parameter(lhs)) {
        auto parameter = get_operand_storage_from_parameter(lhs);
        // a parameter in its slot is stored from the stack move register
        if (is_variant(common::Stack_Offset, parameter)) {
            auto size = memory::get_operand_size_from_storage(
                parameter, accessor_->stack);
            auto value = size == Operand_Size::Doubleword ? Register::x10
                                                          : Register::w10;
            insert_from_accumulator(value);
            arm64_add__asm(instructions, str, value, parameter);
            return;
        }
        insert_from_accumulator(std::get<Register>(parameter));
        return;
    }
    if (!type::is_unary_expression(lhs))
        devices.insert_lvalue_to_device(lhs);
    auto [lhs_storage, storage_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            lhs, instructions.size(), devices);
    if (is_variant(common::Stack_Offset, lhs_storage)) {
        auto size = memory::get_operand_size_from_storage(
            lhs_storage, accessor_->stack);
        lhs_storage =
            size == Operand_Size::Doubleword ? Register::x10 : Register::w10;
    } else
        assembly::inserter(instructions, storage_inst);
    insert_from_accumulator(std::get<Register>(lhs_storage));
}

/**
 * @brief Get the return register of the last call, x0 or w0
 */
inline Storage Operand_Inserter::get_operand_storage_from_return()
{
    auto& tail_call = accessor_->table_accessor.get_table()
                          ->functions.at(stack_frame_.tail)
                          .get();
    if (tail_call.locals.is_pointer(tail_call.ret->first) or
        type::is_rvalue_data_type_string(tail_call.ret->first))
        return Register::x0;
    else
        return Register::w0;
}

/**
 * @brief Get the storage device of a literal operand
 *
 * A string, float, or double is the label of its literal in the data
 * section, and any other literal is an immediate
 */
Storage Operand_Inserter::get_operand_storage_from_immediate(
    RValue const& rvalue)
{
    Storage storage{};
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
 * @brief Get the argument register of a parameter
 *
 * A pointer parameter is a doubleword register. `argc` of `main` is on the
 * stack on Darwin and its address is in x19 on Linux, and an element of
 * `argv` must have an integer index.
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
        if (index_of == 0) {
#if defined(__linux__) || defined(_WIN32) || defined(_WIN64)
            return Register::x19;
#elif defined(__APPLE__) || defined(__bsdi__)
            return accessor_->stack->get("argc").first;
#endif
        }
        if (index_of == 1) {
            if (!is_vector_offset(rvalue))
                common::runtime::throw_runtime_error(
                    "invalid argv access, argv is a vector to strings", rvalue);
            auto offset = type::from_decay_offset(rvalue);
            if (!util::is_numeric(offset) and
                not accessor_->address_accessor.is_lvalue_storage_type(
                    offset, "int"))
                common::runtime::throw_runtime_error(
                    fmt::format("invalid argv access, argv has malformed "
                                "offset '{}'",
                        offset),
                    rvalue);
        }
    }
    if (frame.is_pointer_parameter(rvalue))
        return memory::registers::available_doubleword_argument.at(index_of);
    else
        return memory::registers::available_word_argument.at(index_of);
}

/**
 * @brief Get the storage device of an rvalue, and insert its address
 * instructions
 *
 * A parameter is its register, `RET` the return register, a unary
 * expression is inserted, a literal is an immediate, and an lvalue is
 * loaded from its address
 */
Storage Operand_Inserter::get_operand_storage_from_rvalue(RValue const& rvalue)
{
    auto& frame = stack_frame_.get_stack_frame();

    if (frame.is_parameter(rvalue)) {
        return get_operand_storage_from_parameter(rvalue);
    }

#if defined(__linux__)
    if (rvalue == "RET" and not stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#elif defined(__APPLE__) || defined(__bsdi__)
    if (rvalue == "RET" and not stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#endif

    if (type::is_unary_expression(rvalue)) {
        auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
        return unary_inserter.insert_from_unary_operator_rvalue(rvalue);
    }

    if (type::is_rvalue_data_type(rvalue))
        return get_operand_storage_from_immediate(rvalue);

    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto [operand, operand_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rvalue, instructions.size(), accessor_->device_accessor);
    assembly::inserter(instructions, operand_inst);
    return operand;
}

/**
 * @brief Get the storage device of an rvalue, without its address
 * instructions
 *
 * The device of an argument is resolved before it is loaded into its
 * register, so the address instructions are left out
 */
Storage Operand_Inserter::get_operand_storage_from_rvalue_no_instructions(
    RValue const& rvalue)
{
    auto& frame = stack_frame_.get_stack_frame();

    if (frame.is_parameter(rvalue)) {
        return get_operand_storage_from_parameter(rvalue);
    }
#if defined(__linux__)
    if (rvalue == "RET" and not stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::Linux,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#elif defined(__APPLE__) || defined(__bsdi__)
    if (rvalue == "RET" and not stack_frame_.tail.empty() and
        not common::runtime::is_stdlib_function(stack_frame_.tail,
            common::assembly::OS_Type::BSD,
            common::assembly::Arch_Type::ARM64))
        return get_operand_storage_from_return();

#endif

    if (type::is_unary_expression(rvalue)) {
        auto unary_inserter = Unary_Operator_Inserter{ accessor_ };
        return unary_inserter.insert_from_unary_operator_rvalue(rvalue);
    }

    if (type::is_rvalue_data_type(rvalue))
        return get_operand_storage_from_immediate(rvalue);

    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto [operand, operand_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rvalue, instructions.size(), accessor_->device_accessor);
    return operand;
}

/**
 * @brief Get the storage of an expression operand, an immediate or a register
 *
 *  * A temporary is in the accumulator after it's computed, or in its stack
 *    slot
 *
 *  * A parameter is in its argument register
 *
 *  * A dereference, `*p`, loads from the address in the pointer
 *
 *  * An lvalue on the stack is loaded into the stack move register
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
        not type::is_dereference_expression(rvalue)) {
        // `argc` is on the stack on Darwin, and x19 is its address on Linux
        auto parameter = get_operand_storage_from_parameter(rvalue);
        if (is_variant(common::Stack_Offset, parameter)) {
            auto value = memory::get_operand_size_from_storage(parameter,
                             accessor_->stack) == Operand_Size::Doubleword
                             ? Register::x10
                             : Register::w10;
            arm64_add__asm(instructions, ldr, value, parameter);
            return value;
        }
        if (frame.symbol == "main" and
            frame.get_index_of_parameter(rvalue) == 0 and
            std::get<Register>(parameter) == Register::x19) {
            arm64_add__asm(instructions, ldr, w10, direct_immediate("[x19]"));
            return Register::w10;
        }
        return parameter;
    }
    if (type::is_dereference_expression(rvalue)) {
        auto pointer = get_operand_storage_from_expression_operand(
            type::get_unary_rvalue_reference(rvalue));
        auto address = assembly::get_doubleword_register_from_word(
            std::get<Register>(pointer));
        auto value =
            get_pointee_size(type::get_unary_rvalue_reference(rvalue)) == 8UL
                ? Register::x10
                : Register::w10;
        arm64_add__asm(instructions,
            ldr,
            value,
            direct_immediate(fmt::format("[{}]",
                common::assembly::get_storage_as_string<Register>(address))));
        return value;
    }
    if (is_local_vector_offset(rvalue)) {
        auto offset = get_operand_storage_from_local_vector_offset(rvalue);
        auto size =
            memory::get_operand_size_from_storage(offset, accessor_->stack);
        auto value =
            size == Operand_Size::Doubleword ? Register::x10 : Register::w10;
        arm64_add__asm(instructions, ldr, value, offset);
        return value;
    }
    if (type::is_temporary(rvalue) and accessor_->stack->is_allocated(rvalue)) {
        auto [offset, size] = accessor_->stack->get(rvalue);
        auto value =
            size == Operand_Size::Doubleword ? Register::x10 : Register::w10;
        arm64_add__asm(instructions, ldr, value, offset);
        return value;
    }
    if (type::is_temporary(rvalue)) {
        auto expression_inserter = Expression_Inserter{ accessor_ };
        expression_inserter.insert_lvalue_at_temporary_object_address(rvalue);
        return accessor_->accumulator_temporary.has_value()
                   ? accessor_->accumulator_temporary->second
                   : Register::w8;
    }
    auto [operand, operand_inst] =
        accessor_->address_accessor.get_arm64_lvalue_and_insertion_instructions(
            rvalue, instructions.size(), accessor_->device_accessor);
    assembly::inserter(instructions, operand_inst);
    if (is_variant(common::Stack_Offset, operand)) {
        auto size =
            memory::get_operand_size_from_storage(operand, accessor_->stack);
        auto value =
            size == Operand_Size::Doubleword ? Register::x10 : Register::w10;
        arm64_add__asm(instructions, ldr, value, operand);
        return value;
    }
    return operand;
}

/**
 * @brief Check if an rvalue is an offset of a vector on the stack
 */
bool Operand_Inserter::is_local_vector_offset(RValue const& rvalue)
{
    auto const& table = accessor_->table_accessor.get_table();
    auto label = type::from_lvalue_offset(rvalue);
    return is_vector_offset(rvalue) and table->vectors.contains(label) and
           not table->globals.is_pointer(label) and
           accessor_->stack->is_allocated(label);
}

/**
 * @brief Get the stack offset of an element of a vector on the stack
 *
 * The address accessor flags the next instruction as a vector store, so
 * an element that's read is addressed here instead
 */
Storage Operand_Inserter::get_operand_storage_from_local_vector_offset(
    RValue const& rvalue)
{
    auto& table = accessor_->table_accessor.get_table();
    auto label = type::from_lvalue_offset(rvalue);
    auto offset = type::from_decay_offset(rvalue);
    auto& vector = table->vectors.at(label).get();
    auto vector_s = accessor_->stack->get_stack_offset_from_table_vector_index(
        label, offset, vector);
    auto type =
        vector.data.contains(offset)
            ? type::get_type_from_rvalue_data_type(vector.data.at(offset))
            : std::string{ "word" };
    accessor_->stack->set(vector_s,
        type == "string" ? Operand_Size::Doubleword : Operand_Size::Word);
    return vector_s;
}

/**
 * @brief Load an expression operand into the accumulator of its size
 */
Register Operand_Inserter::insert_operand_into_accumulator(RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto storage = get_operand_storage_from_expression_operand(rvalue);
    auto doubleword =
        is_variant(Immediate, storage)
            ? assembly::get_operand_size_from_rvalue_datatype(
                  std::get<Immediate>(storage)) == Operand_Size::Doubleword
            : assembly::is_doubleword_register(std::get<Register>(storage));
    auto accumulator = doubleword ? Register::x8 : Register::w8;
    if (!is_variant(Register, storage) or
        std::get<Register>(storage) != accumulator)
        arm64_add__asm(instructions, mov, accumulator, storage);
    return accumulator;
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
 * @brief Get the byte size a pointer increment steps over
 *
 * The elements of a vector on the stack are in 8 byte slots
 */
Size Operand_Inserter::get_pointer_stride(LValue const& pointer)
{
    auto& frame = stack_frame_.get_stack_frame();
    if (frame.locals.is_pointer(pointer)) {
        auto target = type::get_unary_rvalue_reference(
            frame.locals.get_pointer_by_name(pointer));
        if (is_local_vector_offset(target))
            return 8UL;
    }
    return get_pointee_size(pointer);
}

/**
 * @brief Resolve the device of each argument of the next call
 *
 * The devices are resolved before any argument is loaded into its
 * register, so an argument that would be overwritten, such as a global in
 * x6 or a parameter in an argument register, is stored in an argument slot.
 * The return value of the last call is in x0 or w0.
 */
Invocation_Inserter::arguments_t
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
                stack_frame_.tail, common::assembly::Arch_Type::ARM64)) {
            arguments.emplace_back(Register::w0);
            continue;
        }
        if (rvalue == "RET") {
            credence_assert(table->functions.contains(stack_frame_.tail));
            auto& tail_frame = table->functions.at(stack_frame_.tail).get();
            if (accessor_->address_accessor.is_lvalue_storage_type(
                    tail_frame.ret->first, "string") or
                caller_frame.is_pointer_in_stack_frame(tail_frame.ret->first) or
                is_doubleword_return_value(tail_frame))
                arguments.emplace_back(Register::x0);
            else
                arguments.emplace_back(Register::w0);
        } else {
            auto operand = assembly::O_NUL;
            if (accessor_->stack->is_allocated(rvalue)) {
                arguments.emplace_back(accessor_->stack->get(rvalue).first);
                continue;
            }
            if (operands.is_local_vector_offset(rvalue)) {
                arguments.emplace_back(
                    operands.get_operand_storage_from_local_vector_offset(
                        rvalue));
                continue;
            }
            if (is_global_argument(rvalue)) {
                arguments.emplace_back(insert_global_argument_to_stack(rvalue));
                continue;
            }
            if (caller_frame.is_parameter(rvalue) and
                not is_vector_offset(rvalue)) {
                arguments.emplace_back(
                    insert_parameter_argument_to_stack(rvalue));
                continue;
            }
            if (is_vector_offset(rvalue))
                operand = operands.get_operand_storage_from_rvalue(rvalue);
            else
                operand =
                    operands.get_operand_storage_from_rvalue_no_instructions(
                        rvalue);
            if (is_variant(Register, operand) and
                std::get<Register>(operand) == Register::x15) {
                auto lhs = type::from_lvalue_offset(rvalue);
                auto offset = type::from_decay_offset(rvalue);
                auto& vector = table->vectors.at(lhs).get();
                auto vector_s =
                    accessor_->stack->get_stack_offset_from_table_vector_index(
                        lhs, offset, vector);
                arguments.emplace_back(vector_s);
            } else
                arguments.emplace_back(operand);
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
 * A global is addressed from its page in x6, and each argument of a call is
 * loaded after all of them are resolved, so the value of each global is
 * stored in its own slot
 *
 *  Example:
 *
 *    printf("%d\n", counter);
 *
 *  Becomes:
 *
 *    adrp x6, counter@PAGE
 *    add x6, x6, counter@PAGEOFF
 *    ldr w10, [x6]
 *    str w10, [sp, #16]
 *    ...
 *    ldr w1, [sp, #16]
 */
common::Stack_Offset Invocation_Inserter::insert_global_argument_to_stack(
    RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto const& table = accessor_->table_accessor.get_table();
    auto label = type::from_lvalue_offset(rvalue);
    auto key = is_vector_offset(rvalue) ? type::from_decay_offset(rvalue)
                                        : std::string{ "0" };
    auto offset =
        accessor_->vector_accessor.get_offset_address(rvalue, key).first;
    // a string element is the address of the string
    auto type = type::get_type_from_rvalue_data_type(
        object::get_rvalue_at_lvalue_object_storage(
            rvalue, stack_frame_.get_stack_frame(), table->vectors));
    auto size = type == "string" or type == "long" or type == "double"
                    ? Operand_Size::Doubleword
                    : Operand_Size::Word;
    auto value =
        size == Operand_Size::Doubleword ? Register::x10 : Register::w10;
    arm64_add__asm(
        instructions, adrp, x6, assembly::page_offset_upper_immediate(label));
    arm64_add__asm(instructions,
        add,
        x6,
        x6,
        assembly::page_offset_lower_immediate(label));
    arm64_add__asm(instructions,
        ldr,
        value,
        direct_immediate(fmt::format("[x6, #{}]", offset)));
    // the `_p` prefix is an argument slot, which is a buffer when it is a
    // doubleword, and a slot keeps its size across calls
    auto slot = fmt::format("_p_global_{}_{}",
        assembly::get_size_from_operand_size(size),
        ++global_argument_index);
    accessor_->stack->set_address_from_size(slot, size);
    auto slot_offset = accessor_->stack->get(slot).first;
    arm64_add__asm(instructions, str, value, slot_offset);
    return slot_offset;
}

/**
 * @brief Store a parameter argument in an argument slot
 *
 * A parameter is in its argument register, which an earlier argument of
 * the call may be loaded into, so a parameter in an argument register is
 * stored before any argument is loaded. `argc` is on the stack or in x19,
 * so it is not stored.
 *
 *  Example:
 *
 *    show(s) {
 *      printf("%s\n", s);
 *    }
 *
 *  Becomes:
 *
 *    str x0, [sp, #16]
 *    adrp x0, ._L_str1__@PAGE
 *    add x0, x0, ._L_str1__@PAGEOFF
 *    ldr x1, [sp, #16]
 *    bl _printf
 */
Storage Invocation_Inserter::insert_parameter_argument_to_stack(
    RValue const& rvalue)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto operand_inserter = Operand_Inserter{ accessor_ };
    auto storage = operand_inserter.get_operand_storage_from_parameter(rvalue);
    if (!is_variant(Register, storage) or
        std::get<Register>(storage) == Register::x19)
        return storage;
    auto value = std::get<Register>(storage);
    auto size = assembly::is_doubleword_register(value)
                    ? Operand_Size::Doubleword
                    : Operand_Size::Word;
    auto slot = fmt::format("_p_parameter_{}_{}",
        assembly::get_size_from_operand_size(size),
        ++global_argument_index);
    accessor_->stack->set_address_from_size(slot, size);
    auto slot_offset = accessor_->stack->get(slot).first;
    arm64_add__asm(instructions, str, value, slot_offset);
    return slot_offset;
}

/**
 * @brief Insert a syscall
 *
 * The length of the buffer of a `read(2)` is cached for a later `print`
 */
void Invocation_Inserter::insert_from_syscall_function(std::string_view routine,
    Instructions& instructions)
{
    auto syscall_inserter =
        syscall_ns::Syscall_Invocation_Inserter{ accessor_, stack_frame_ };
    accessor_->address_accessor.buffer_accessor.set_buffer_size_from_syscall(
        routine, stack_frame_.argument_stack);

    auto operands = this->get_operands_storage_from_argument_stack();

    syscall_inserter.make_syscall(
        instructions, routine, operands, stack_frame_.argument_stack);
}

/**
 * @brief Insert a call to a function of the program
 *
 * Each argument is loaded into its argument register: a string from its
 * page address, a float or double from its label, an element of `argv`
 * from x19, and any other argument from its device
 *
 *  Example:
 *
 *    show(argv[1]);
 *
 *  Becomes:
 *
 *    ldr x0, [x19, #8]
 *    bl show
 */
void Invocation_Inserter::insert_from_user_defined_function(
    std::string_view routine,
    Instructions& instructions)
{
    auto operands = get_operands_storage_from_argument_stack();
    auto expression_inserter = Expression_Inserter{ accessor_ };
    for (std::size_t i = 0; i < operands.size(); i++) {
        auto const& operand = operands[i];
        auto argument = stack_frame_.argument_stack.at(i);
        // the type of a literal is only its storage while it is a label
        auto arg_type = is_variant(Immediate, operand)
                            ? type::get_type_from_rvalue_data_type(argument)
                            : std::string{};
        auto arg_register = assembly::get_register_from_integer_argument(i);
        // an element of `argv` is a doubleword from x19
        if (stack_frame_.symbol == "main" and
            type::from_lvalue_offset(argument) == "argv") {
            auto offset_integer = type::integral_from_type_ulint(
                type::from_decay_offset(argument));
            auto argv_offset =
                assembly::make_argv_offset_immediate(offset_integer);
            arm64_add__asm(instructions,
                ldr,
                assembly::get_doubleword_register_from_word(arg_register),
                argv_offset);
        } else
            m::match(arg_type)(
                m::pattern | sv("string") =
                    [&] {
                        auto immediate = type::get_value_from_rvalue_data_type(
                            std::get<Immediate>(operand));
                        auto imm_1 =
                            assembly::page_offset_upper_immediate(immediate);
                        arm64_add__asm(instructions, adrp, arg_register, imm_1);
                        auto imm_2 =
                            assembly::page_offset_lower_immediate(immediate);
                        arm64_add__asm(instructions,
                            add,
                            arg_register,
                            arg_register,
                            imm_2);
                    },
                m::pattern | m::or_(sv("float"), sv("double")) =
                    [&] {
                        auto immediate = type::get_value_from_rvalue_data_type(
                            std::get<Immediate>(operand));
                        auto imm =
                            direct_immediate(fmt::format("={}", immediate));
                        arm64_add__asm(instructions, ldr, arg_register, imm);
                    },
                m::pattern | m::_ =
                    [&] {
                        accessor_->flag_accessor.set_instruction_flag(
                            common::flag::Argument, instructions.size());
                        if (is_variant(common::Stack_Offset, operand) or
                            assembly::is_immediate_pc_address_offset(operand))
                            arm64_add__asm(
                                instructions, ldr, arg_register, operand);
                        else if (is_variant(Register, operand) and
                                 assembly::is_word_register(
                                     std::get<Register>(operand))) {
                            auto storage_dword =
                                assembly::get_word_register_from_doubleword(
                                    arg_register);
                            arm64_add__asm(
                                instructions, mov, storage_dword, operand);
                        } else
                            arm64_add__asm(
                                instructions, mov, arg_register, operand);
                    });
    }
    arm64_add__asm(instructions, bl, direct_immediate(routine));
}

/**
 * @brief Insert a call to a standard library function
 *
 * The arguments of `print` and `printf` are type checked first
 */
void Invocation_Inserter::insert_from_standard_library_function(
    std::string_view routine,
    Instructions& instructions)
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
 * @brief Get the instruction of an arithmetic operator on two operands
 */
Instruction_Pair
Arithemtic_Operator_Inserter::from_arithmetic_expression_operands(
    assembly::Assignment_Operands const& operands,
    std::string const& binary_op)
{
    Instruction_Pair instructions{ Register::w8, {} };
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
 * @brief Get the instruction of a bitwise operator on three operands
 *
 *  Example:
 *
 *    from_bitwise_expression_operands({ w8, w8, w7 }, "&")
 *    // and w8, w8, w7
 */
Instruction_Pair Bitwise_Operator_Inserter::from_bitwise_expression_operands(
    assembly::Ternary_Operands const& operands,
    std::string const& binary_op)
{
    auto [s0, s1, s2] = operands;
    Instruction_Pair instructions{ Register::wzr, {} };
    m::match(binary_op)(
        m::pattern | std::string{ "<<" } =
            [&] { instructions = assembly::lshift(s0, s1, s2); },
        m::pattern | std::string{ ">>" } =
            [&] { instructions = assembly::rshift(s0, s1, s2); },
        m::pattern | std::string{ "^" } =
            [&] { instructions = assembly::b_xor(s0, s1, s2); },
        m::pattern | std::string{ "~" } =
            [&] { instructions = assembly::b_not(s0, s1, s1); },
        m::pattern | std::string{ "&" } =
            [&] { instructions = assembly::b_and(s0, s1, s2); },
        m::pattern | std::string{ "|" } =
            [&] { instructions = assembly::b_or(s0, s1, s2); });
    return instructions;
}

/**
 * @brief Get the comparison and conditional jump of a relational operator
 *
 * The comparison is made in a doubleword register when either operand is
 * a doubleword
 *
 *  Example:
 *
 *    from_relational_expression_operands({ w9, #10 }, ">", "._L6__main")
 *
 *  Becomes:
 *
 *    mov w8, w9
 *    cmp w8, #10
 *    b.gt ._L6__main
 */
Instructions Relational_Operator_Inserter::from_relational_expression_operands(
    assembly::Assignment_Operands const& operands,
    std::string const& binary_op,
    Label const& jump_label)
{
    auto register_storage = Register::w8;
    if (memory::is_doubleword_storage_size(operands.first,
            accessor_->stack,
            accessor_->get_frame_in_memory()) or
        memory::is_doubleword_storage_size(operands.second,
            accessor_->stack,
            accessor_->get_frame_in_memory())) {
        register_storage = Register::x8;
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
        m::pattern | m::_ = [&] { return Instructions{}; });
}

/**
 * @brief Insert the label of a float or double literal into a device
 *
 * A float or double is loaded from its label by the argument that uses it,
 * so an assignment of one emits nothing
 */
void Operand_Inserter::insert_from_float_address_operand(
    [[maybe_unused]] LValue const& lhs,
    [[maybe_unused]] Storage const& storage,
    [[maybe_unused]] RValue const& rhs)
{
}

void Operand_Inserter::insert_from_double_address_operand(
    [[maybe_unused]] LValue const& lhs,
    [[maybe_unused]] Storage const& storage,
    [[maybe_unused]] RValue const& rhs)
{
}

/**
 * @brief Insert the page address of a string literal into a device
 */
void Operand_Inserter::insert_from_string_address_operand(
    [[maybe_unused]] LValue const& lhs,
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
        accessor_->stack->set(offset, Operand_Size::Doubleword);
    }
    arm64_add__asm(instructions, mov, storage, x6);
}

/**
 * @brief Insert the page address of a string literal into x6
 *
 *  Example:
 *
 *    s = ("hello":string:5);
 *
 *  Becomes:
 *
 *    adrp x6, ._L_str1__@PAGE
 *    add x6, x6, ._L_str1__@PAGEOFF
 */
void Expression_Inserter::insert_from_string(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    credence_assert(
        accessor_->address_accessor.buffer_accessor.is_allocated_string(str));
    auto immediate =
        accessor_->address_accessor.buffer_accessor.get_string_address_offset(
            str);
    auto imm_1 = assembly::page_offset_upper_immediate(immediate);
    arm64_add__asm(instructions, adrp, x6, imm_1);
    auto imm_2 = assembly::page_offset_lower_immediate(immediate);
    arm64_add__asm(instructions, add, x6, x6, imm_2);
}

/**
 * @brief Load a float or double literal from the literal pool into s6 or d6
 */
void Expression_Inserter::insert_from_float(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto immediate = direct_immediate(fmt::format("={}", str));
    arm64_add__asm(instructions, ldr, s6, immediate);
}

void Expression_Inserter::insert_from_double(RValue const& str)
{
    auto& instructions = accessor_->instruction_accessor->get_instructions();
    auto immediate = direct_immediate(fmt::format("={}", str));
    arm64_add__asm(instructions, ldr, d6, immediate);
}

} // namespace credence::target::arm64