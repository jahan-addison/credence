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
#include "assembly.h"                        // for Instructions, arm_add_a...
#include "memory.h"                          // for Memory_Access, Memory_A...
#include "runtime.h"                         // for get_argument_general_pu..
#include <credence/error.h>                  // for assert_equal_impl, cred...
#include <credence/target/common/assembly.h> // for make_numeric_immediate
#include <credence/target/common/syscall.h>  // for get_syscall_list
#include <string>                            // for basic_string
#include <utility>                           // for get
#include <variant>                           // for variant

namespace credence::target::arm64::syscall_ns {
/**
 * @brief Create instructions for a platform-independent exit syscall
 */
// cppcheck-suppress constParameterReference
void exit_syscall(assembly::Instructions& instructions, int exit_status)
{
    auto immediate = common::assembly::make_numeric_immediate(exit_status);
    syscall_ns::make_syscall(instructions, "exit", { immediate }, nullptr);
}

void make_syscall(assembly::Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Memory_Access* accessor)
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

    auto [doubleword_storage, word_storage] =
        target::arm64::runtime::get_argument_general_purpose_registers();

    target::arm64::syscall_ns::syscall_operands_to_instructions(
        instructions, arguments, doubleword_storage, word_storage, accessor);

#if defined(__linux__)
    assembly::Storage syscall_number =
        target::common::assembly::make_numeric_immediate(syscall_entry[0]);
    arm64_add__asm(instructions, mov, x8, syscall_number);
#elif defined(CREDENCE_TEST) || defined(__APPLE__) || defined(__bsdi__)
    // load syscall number into x16 (darwin uses x16 instead of x8)
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
 * @brief NULL-check the memory access, then set the signal register
 */
bool check_signal_register_from_safe_address(Instructions& instructions,
    assembly::Register storage,
    memory::Memory_Access* accessor)
{
    if (accessor != nullptr) {
        auto accessor_ = *accessor;
        auto* address_of = accessor_->register_accessor.signal_register;
        if (storage == arm_rr(x26) and *address_of == Register::x26) {
            accessor_->set_signal_register(Register::w0);
            arm64_add__asm(instructions, mov, storage, arm_rr(x26));
            return false;
        }
    }
    return true;
}
/**
 * @brief Prepare the operands for the syscall
 */

void syscall_operands_to_instructions(assembly::Instructions& instructions,
    syscall_arguments_t const& arguments,
    memory::registers::general_purpose& w_registers,
    memory::registers::general_purpose& d_registers,
    memory::Memory_Access* accessor)
{
    for (std::size_t i = 0; i < arguments.size(); i++) {
        Storage arg = arguments[i];
        auto storage = get_storage_register_from_safe_address(
            arg, w_registers, d_registers, accessor);

        w_registers.pop_back();
        d_registers.pop_back();

        if (is_immediate_relative_address(arg)) {
            auto immediate =
                type::get_value_from_rvalue_data_type(std::get<Immediate>(arg));
            auto imm_1 = direct_immediate(fmt::format("{}@PAGE", immediate));
            arm64_add__asm(instructions, adrp, storage, imm_1);
            auto imm_2 = direct_immediate(fmt::format("{}@PAGEOFF", immediate));
            arm64_add__asm(instructions, add, storage, storage, imm_2);
        } else if (check_signal_register_from_safe_address(
                       instructions, storage, accessor))
            arm64_add__asm(instructions, mov, storage, arg);
    }
}

} // namespace credence::target::arm64::syscall_ns
