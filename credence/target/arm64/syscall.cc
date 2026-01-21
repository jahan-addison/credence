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
 *
 * ARM64 System Call Interface
 *
 * Syscall invocation for ARM64 Linux and Darwin. Loads syscall
 * number into x8 (Linux) or x16 (Darwin) and arguments into x0-x7.
 *
 * Example - exit syscall:
 *
 *   B code:    main() { return(0); }
 *
 * Generates (Linux):
 *   mov x8, #93        ; exit syscall number
 *   mov x0, #0         ; exit code
 *   svc #0
 *
 * Generates (Darwin):
 *   mov x16, #1        ; Darwin exit number
 *   mov x0, #0
 *   svc #0x80
 *
 *****************************************************************************/

namespace credence::target::arm64::syscall_ns {

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
 * @brief Set the signal register
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
 * @brief Create instructions for an exit syscall
 */
void Syscall_Invocation_Inserter::exit_syscall(
    assembly::Instructions& instructions,
    int exit_status)
{
    auto immediate = common::assembly::make_numeric_immediate(exit_status);
    auto empty_locals = common::memory::Locals{};
    make_syscall(instructions, "exit", { immediate }, empty_locals);
}

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

#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    auto syscall_list = target::common::syscall_ns::get_syscall_list(
        target::common::assembly::OS_Type::BSD,
        target::common::assembly::Arch_Type::ARM64);
#endif

    credence_assert(syscall_list.contains(syscall));
    credence_assert(arguments.size() <= 6);

    target::common::syscall_ns::syscall_t syscall_entry{ 0, 0 };

    syscall_entry =
        target::common::syscall_ns::arm64::bsd_ns::syscall_list.at(syscall);

    credence_assert_equal(syscall_entry[1], arguments.size());

    syscall_operands_to_instructions(instructions, arguments, argument_stack);

#if defined(__linux__)
    assembly::Storage syscall_number =
        target::common::assembly::make_numeric_immediate(syscall_entry[0]);
    arm64_add__asm(instructions, mov, x8, syscall_number);
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    target::arm64::assembly::Storage syscall_number =
        target::common::assembly::make_numeric_immediate(syscall_entry[0]);
    arm64_add__asm(instructions, mov, x16, syscall_number);
#else
    credence_error("Operating system not supported");
#endif

#if defined(__linux__)
    arm64_add__asm(
        instructions, svc, common::assembly::make_direct_immediate("#0"));
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    arm64_add__asm(
        instructions, svc, common::assembly::make_direct_immediate("#0x80"));
#endif
}

/**
 * @brief Prepare the operands for the syscall
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
            auto imm_1 = direct_immediate(fmt::format("{}@PAGE", immediate));
            arm64_add__asm(instructions, adrp, storage, imm_1);
            auto imm_2 = direct_immediate(fmt::format("{}@PAGEOFF", immediate));
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
