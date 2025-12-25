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
#include "assembly.h"       // for Register, make_numeric_immediate, add_in...
#include <credence/error.h> // for assert_equal_impl, credence_assert, cred...
#include <deque>            // for deque
#include <string>           // for basic_string, string
#include <variant>          // for variant

namespace credence::target::x86_64::syscall_ns {

namespace common {

/**
 * @brief Create instructions for a platform-independent exit syscall
 */
// cppcheck-suppress constParameterReference
void exit_syscall(Instructions& instructions, int exit_status)
{
    auto immediate = assembly::make_numeric_immediate(exit_status);
#if defined(CREDENCE_TEST) || defined(__linux__)
    syscall_ns::linux_ns::make_syscall(
        instructions, "exit", { immediate }, nullptr, nullptr);
#elif defined(__APPLE__) || defined(__bsdi__)
    syscall_ns::bsd_ns::make_syscall(
        instructions, "exit", { immediate }, nullptr, nullptr);
#else
    credence_error("Operating system not supported");
#endif
}

/**
 * @brief Get the list of avalable syscalls on the current platform
 */
std::vector<std::string> get_platform_syscall_symbols()
{
    std::vector<std::string> symbols{};
    auto syscall_list = common::get_syscall_list();
    // cppcheck-suppress[useStlAlgorithm,knownEmptyContainer]
    for (auto const& syscall : syscall_list) {
        // cppcheck-suppress[useStlAlgorithm,knownEmptyContainer]
        symbols.emplace_back(syscall.first.data());
    }
    return symbols;
}

} // namespace common

/**
 * @brief General purpsoe register stack in ABI system V order
 */
std::pair<memory::registers::general_purpose,
    memory::registers::general_purpose>
get_argument_general_purpose_registers()
{
    std::deque<Register> qword = { Register::r9,
        Register::r8,
        Register::r10,
        Register::rdx,
        Register::rsi,
        Register::rdi };

    std::deque<assembly::Register> dword = { Register::r9d,
        Register::r8d,
        Register::r10d,
        Register::edx,
        Register::esi,
        Register::edi };

    return std::make_pair(qword, dword);
}

/**
 * @brief NULL-check the memory access, then grab a register
 */
assembly::Register get_storage_register_from_safe_address(
    assembly::Storage argument,
    std::deque<assembly::Register>& qword_registers,
    std::deque<assembly::Register>& dword_registers,
    memory::Stack_Frame* stack_frame,
    memory::Memory_Access* accessor)
{
    Register storage = Register::rdi;
    if (accessor != nullptr) {
        auto accessor_ = *accessor;
        if (accessor_->address_accessor.is_qword_storage_size(
                argument, *stack_frame)) {
            storage = qword_registers.back();
        } else {
            storage = dword_registers.back();
        }
    }
    return storage;
}

/**
 * @brief NULL-check the memory access, then set the signal register
 */
bool set_signal_register_from_safe_address(Instructions& instructions,
    assembly::Register storage,
    memory::Memory_Access* accessor)
{
    if (accessor != nullptr) {
        auto accessor_ = *accessor;
        auto* address_of = accessor_->register_accessor.signal_register;
        if (storage == rr(rsi) and *address_of == Register::rcx) {
            accessor_->set_signal_register(Register::eax);
            asm__src_rs(instructions, movq_, storage, rcx);
            return false;
        }
    }
    return true;
}

/**
 * @brief Prepare the operands for the syscall
 */
void syscall_operands_to_instructions(Instructions& instructions,
    syscall_arguments_t const& arguments,
    std::deque<assembly::Register>& qword_registers,
    std::deque<assembly::Register>& dword_registers,
    memory::Stack_Frame* stack_frame,
    memory::Memory_Access* accessor)
{
    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments[i];
        Register storage = get_storage_register_from_safe_address(
            arg, qword_registers, dword_registers, stack_frame, accessor);

        qword_registers.pop_back();
        dword_registers.pop_back();
        if (is_immediate_rip_address_offset(arg))
            add_asm__as(instructions, lea, storage, arg);
        else if (set_signal_register_from_safe_address(
                     instructions, storage, accessor))
            add_asm__as(instructions, movq_, storage, arg);
    }
}

namespace linux_ns {

/**
 * @brief Create instructions for a linux x86_64 platform syscall
 *    See syscall.h for details
 */
void make_syscall(Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame* stack_frame,
    memory::Memory_Access* accessor)
{
    credence_assert(syscall_list.contains(syscall));
    credence_assert(arguments.size() <= 6);
    auto [number, arg_size] = syscall_list.at(syscall);

    credence_assert_equal(arg_size, arguments.size());

    auto [argument_storage_qword, argument_storage_dword] =
        get_argument_general_purpose_registers();

    assembly::Storage syscall_number = assembly::make_numeric_immediate(number);

    asm__dest_rs(instructions, mov, rax, syscall_number);

    syscall_operands_to_instructions(instructions,
        arguments,
        argument_storage_qword,
        argument_storage_dword,
        stack_frame,
        accessor);

    asm__zero_o(instructions, syscall);
}

} // namespace linux

namespace bsd_ns {

/**
 * @brief Create instructions for a BSD (Darwin) x86_64 platform syscall
 *    See syscall.h for details
 */
void make_syscall(Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame* stack_frame,
    memory::Memory_Access* accessor)
{
    credence_assert(syscall_list.contains(syscall));
    credence_assert(arguments.size() <= 6);
    auto [number, arg_size] = syscall_list.at(syscall);
    credence_assert_equal(arg_size, arguments.size());

    auto [argument_storage_qword, argument_storage_dword] =
        get_argument_general_purpose_registers();

    assembly::Storage syscall_number =
        assembly::make_numeric_immediate(SYSCALL_CLASS_UNIX + number);

    asm__dest_rs(instructions, mov, rax, syscall_number);

    syscall_operands_to_instructions(instructions,
        arguments,
        argument_storage_qword,
        argument_storage_dword,
        stack_frame,
        accessor);

    asm__zero_o(instructions, syscall);
}

} // namespace bsd

namespace common {

/**
 * @brief Create instructions for a platform-independent syscall
 */
void make_syscall(
    // cppcheck-suppress constParameterReference
    Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame* stack_frame,
    memory::Memory_Access* accessor)
{
#if defined(CREDENCE_TEST) || defined(__linux__)
    syscall_ns::linux_ns::make_syscall(
        instructions, syscall, arguments, stack_frame, accessor);
#elif defined(__APPLE__) || defined(__bsdi__)
    syscall_ns::bsd_ns::make_syscall(
        instructions, syscall, arguments, stack_frame, accessor);
#else
    credence_error("Operating system not supported");
#endif
}

} // namespace common
}