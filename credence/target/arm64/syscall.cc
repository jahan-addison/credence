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

#include "syscall.h"
#include "assembly.h"                        // for Mnemonic, arm64_add__asm
#include "memory.h"                          // for Memory_Access, Memory_A...
#include "runtime.h"                         // for get_argument_general_pu...
#include <credence/error.h>                  // for assert_equal_impl, cred...
#include <credence/target/common/assembly.h> // for make_numeric_immediate
#include <credence/target/common/syscall.h>  // for get_syscall_list, sysca...
#include <credence/types.h>                  // for get_value_from_rvalue_d...
#include <fmt/format.h>                      // for format
#include <string>                            // for basic_string, char_traits
#include <utility>                           // for get
#include <variant>                           // for get, variant

/****************************************************************************
 * ARM64 Syscall Interface
 *
 * Syscalls on ARM64 Linux and Darwin. The arguments are in x0 to x5, the
 * syscall number is in x8 on Linux and x16 on Darwin, and the syscall is
 * made with `svc #0` on Linux and `svc #0x80` on Darwin. The `main` function
 * returns with the exit syscall.
 *
 *  Example:
 *
 *    LEAVE;
 *
 *  Becomes, in `main` on Linux:
 *
 *    mov w0, #0
 *    mov x8, #93
 *    svc #0
 *
 *  And on Darwin:
 *
 *    mov w0, #0
 *    mov x16, #1
 *    svc #0x80
 *
 ****************************************************************************/

namespace credence::target::arm64::syscall_ns {

/**
 * @brief Get the argument register of a syscall argument
 *
 * An address is a doubleword register, a float or double local is a float
 * or double register, and any other argument is a word register
 */
Register Syscall_Invocation_Inserter::get_storage_register_from_safe_address(
    assembly::Storage const& argument,
    common::memory::Locals& argument_stack,
    unsigned int index)
{
    try {
        if (memory::is_doubleword_storage_size(
                argument, accessor_->stack, stack_frame_))
            return dword_registers_.back();
        else {
            if (accessor_->address_accessor.is_lvalue_storage_type(
                    argument_stack.at(index), "float")) {
                return float_registers_.back();
            }
            if (accessor_->address_accessor.is_lvalue_storage_type(
                    argument_stack.at(index), "double")) {
                return double_registers_.back();
            }
        }
    } catch (...) {
        return word_registers_.back();
    }
    return word_registers_.back();
}

/**
 * @brief Move a value in x6 to its argument register
 *
 * A value computed into x6 for a syscall signals that the result is in w0
 * after the syscall, for the assignment of the next lvalue. It returns
 * false when the move is made, so the caller moves any other argument.
 */
bool Syscall_Invocation_Inserter::check_signal_register_from_safe_address(
    Instructions& instructions,
    assembly::Register storage)
{
    auto* address_of = accessor_->register_accessor.signal_register;
    if (storage == arm_rr(x6) and *address_of == Register::x6) {
        accessor_->set_signal_register(Register::w0);
        arm64_add__asm(instructions, mov, storage, arm_rr(x6));
        return false;
    }
    return true;
}

/**
 * @brief Insert the exit syscall with an exit status
 */
void Syscall_Invocation_Inserter::exit_syscall(
    assembly::Instructions& instructions,
    int exit_status)
{
    auto immediate = common::assembly::make_numeric_immediate(exit_status);
    auto empty_locals = common::memory::Locals{};
    make_syscall(instructions, "exit", { immediate }, empty_locals);
}

/**
 * @brief Insert a syscall of the host platform
 *
 * The arguments are loaded into their registers, then the syscall number
 * into x8 or x16, then the supervisor call
 *
 *  Example:
 *
 *    write(1, "hi", 2);
 *
 *  Becomes, on Darwin:
 *
 *    mov w0, #1
 *    adrp x1, ._L_str1__@PAGE
 *    add x1, x1, ._L_str1__@PAGEOFF
 *    mov w2, #2
 *    mov x16, #4
 *    svc #0x80
 */
void Syscall_Invocation_Inserter::make_syscall(
    assembly::Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    common::memory::Locals& argument_stack)
{
#if defined(__linux__)
    auto syscall_list = target::common::syscall_ns::get_syscall_list(
        target::common::assembly::OS_Type::Linux,
        target::common::assembly::Arch_Type::ARM64);
#elif defined(__APPLE__) || defined(__bsdi__)
    auto syscall_list = target::common::syscall_ns::get_syscall_list(
        target::common::assembly::OS_Type::BSD,
        target::common::assembly::Arch_Type::ARM64);
#elif defined(_WIN32) || defined(_WIN64)
    auto syscall_list = target::common::syscall_ns::get_syscall_list(
        target::common::assembly::OS_Type::Linux,
        target::common::assembly::Arch_Type::ARM64);
#endif

    credence_assert(syscall_list.contains(syscall));
    credence_assert(arguments.size() <= 6);

    target::common::syscall_ns::syscall_t syscall_entry{ 0, 0 };

    syscall_entry = syscall_list.at(syscall);

    credence_assert_equal(syscall_entry[1], arguments.size());

    syscall_operands_to_instructions(instructions, arguments, argument_stack);

#if defined(__linux__)
    assembly::Storage syscall_number =
        target::common::assembly::make_numeric_immediate(syscall_entry[0]);
    arm64_add__asm(instructions, mov, x8, syscall_number);
#elif defined(__APPLE__) || defined(__bsdi__)
    target::arm64::assembly::Storage syscall_number =
        target::common::assembly::make_numeric_immediate(syscall_entry[0]);
    arm64_add__asm(instructions, mov, x16, syscall_number);
#elif defined(_WIN32) || defined(_WIN64)
    assembly::Storage syscall_number =
        target::common::assembly::make_numeric_immediate(syscall_entry[0]);
    arm64_add__asm(instructions, mov, x8, syscall_number);
#else
    credence_error("Operating system not supported");
#endif

#if defined(__linux__)
    arm64_add__asm(
        instructions, svc, common::assembly::make_direct_immediate("#0"));
#elif defined(__APPLE__) || defined(__bsdi__)
    arm64_add__asm(
        instructions, svc, common::assembly::make_direct_immediate("#0x80"));
#elif defined(_WIN32) || defined(_WIN64)
    arm64_add__asm(
        instructions, svc, common::assembly::make_direct_immediate("#0"));

#endif
}

/**
 * @brief Load each syscall argument into its register
 *
 * A data section label is loaded from its page address, a stack slot is
 * loaded, and a word register is moved to the word half of the argument
 * register
 */
void Syscall_Invocation_Inserter::syscall_operands_to_instructions(
    assembly::Instructions& instructions,
    syscall_arguments_t const& arguments,
    common::memory::Locals& argument_stack)
{
    for (std::size_t i = 0; i < arguments.size(); i++) {
        Storage arg = arguments[i];

        auto storage =
            get_storage_register_from_safe_address(arg, argument_stack, i);

        word_registers_.pop_back();
        double_registers_.pop_back();
        float_registers_.pop_back();
        dword_registers_.pop_back();

        if (is_immediate_relative_address(arg)) {
            auto immediate =
                type::get_value_from_rvalue_data_type(std::get<Immediate>(arg));
            auto imm_1 = assembly::page_offset_upper_immediate(immediate);
            arm64_add__asm(instructions, adrp, storage, imm_1);
            auto imm_2 = assembly::page_offset_lower_immediate(immediate);
            arm64_add__asm(instructions, add, storage, storage, imm_2);
        } else if (assembly::is_immediate_pc_address_offset(arg)) {
            arm64_add__asm(instructions, ldr, storage, arg);
        } else if (check_signal_register_from_safe_address(
                       instructions, storage)) {
            if (is_variant(common::Stack_Offset, arg))
                arm64_add__asm(instructions, ldr, storage, arg);
            else if (is_variant(Register, arg) and
                     assembly::is_word_register(std::get<Register>(arg))) {
                auto storage_dword =
                    assembly::get_word_register_from_doubleword(storage);
                arm64_add__asm(instructions, mov, storage_dword, arg);
            } else
                arm64_add__asm(instructions, mov, storage, arg);
        }
    }
}

} // namespace credence::target::arm64::syscall_ns
