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
#include "assembly.h"                        // for Register, Storage, is_i...
#include <credence/error.h>                  // for assert_equal_impl, cred...
#include <credence/target/common/assembly.h> // for make_numeric_immediate
#include <credence/target/common/syscall.h>  // for syscall_list, SYSCALL_C...
#include <credence/target/x86_64/memory.h>   // for Memory_Access, Stack_Frame
#include <deque>                             // for deque
#include <string>                            // for basic_string
#include <utility>                           // for get, make_pair, pair
#include <variant>                           // for variant

/****************************************************************************
 * x86-64 Syscall Interface
 *
 * Syscalls on x86-64 Linux and Darwin. The syscall number is in rax and the
 * arguments are in rdi, rsi, rdx, r10, r8, and r9, then the `syscall`
 * instruction is executed. A Darwin syscall number is in the Unix syscall
 * class, 0x2000000. The `main` function returns with the exit syscall.
 *
 *  Example:
 *
 *    LEAVE;
 *
 *  Becomes, in `main` on Linux:
 *
 *    mov rax, 60
 *    mov rdi, 0
 *    syscall
 *
 *  And on Darwin:
 *
 *    mov rax, 33554433     // 0x2000000 | 1
 *    mov rdi, 0
 *    syscall
 *
 ****************************************************************************/

namespace credence::target::x86_64::syscall_ns {

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
 * @brief Get the syscall register of an argument
 *
 * An address is a quadword register and any other argument a doubleword
 * register. With no memory accessor, such as the exit syscall of `main`,
 * the argument is in rdi.
 */
assembly::Register get_storage_register_from_safe_address(
    assembly::Storage argument,
    std::deque<assembly::Register>& qword_registers,
    std::deque<assembly::Register>& dword_registers,
    [[maybe_unused]] memory::Stack_Frame* stack_frame,
    memory::Memory_Access* accessor)
{
    Register storage = Register::rdi;
    if (accessor != nullptr) {
        auto accessor_ = *accessor;
        if (accessor_->address_accessor.is_qword_storage_size(argument)) {
            storage = qword_registers.back();
        } else {
            storage = dword_registers.back();
        }
    }
    return storage;
}

/**
 * @brief Move a value in rcx to its syscall register
 *
 * A value computed into rcx for a syscall signals that the result is in
 * eax after the syscall, for the assignment of the next lvalue. It returns
 * false when the move is made, so the caller moves any other argument.
 */
bool check_signal_register_from_safe_address(Instructions& instructions,
    assembly::Register storage,
    memory::Memory_Access* accessor)
{
    if (accessor != nullptr) {
        auto accessor_ = *accessor;
        auto* address_of = accessor_->register_accessor.signal_register;
        if (storage == x64_rr(rsi) and *address_of == Register::rcx) {
            accessor_->set_signal_register(Register::eax);
            x8664_add__asm(instructions, movq_, storage, rcx);
            return false;
        }
    }
    return true;
}

/**
 * @brief Load each syscall argument into its register
 *
 * An address relative to rip is loaded with `lea`, and any other argument
 * is moved
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
            x8664_add__asm(instructions, lea, storage, arg);
        else if (check_signal_register_from_safe_address(
                     instructions, storage, accessor))
            x8664_add__asm(instructions, movq_, storage, arg);
    }
}

namespace common {

/**
 * @brief Insert the exit syscall with an exit status
 */
// cppcheck-suppress constParameterReference
void exit_syscall(Instructions& instructions, int exit_status)
{
    auto immediate =
        target::common::assembly::make_numeric_immediate(exit_status);
    make_syscall(instructions, "exit", { immediate }, nullptr, nullptr);
}

/**
 * @brief Insert a syscall of the host platform
 *
 * The syscall number is moved to rax, in the Unix syscall class on Darwin,
 * then the arguments are loaded into rdi, rsi, rdx, r10, r8, and r9
 *
 *  Example:
 *
 *    write(1, "hi", 2);
 *
 *  Becomes, on Darwin:
 *
 *    mov rax, 33554436
 *    mov edi, 1
 *    lea rsi, [rip + ._L_str1__]
 *    mov edx, 2
 *    syscall
 */
void make_syscall(
    // cppcheck-suppress constParameterReference
    Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame* stack_frame,
    memory::Memory_Access* accessor)
{

#if defined(__linux__)
    auto syscall_list = target::common::syscall_ns::get_syscall_list(
        target::common::assembly::OS_Type::Linux,
        target::common::assembly::Arch_Type::X8664);
#elif defined(__APPLE__) || defined(__bsdi__)
    auto syscall_list = target::common::syscall_ns::get_syscall_list(
        target::common::assembly::OS_Type::BSD,
        target::common::assembly::Arch_Type::X8664);
#elif defined(_WIN32) || defined(_WIN64)
    auto syscall_list = target::common::syscall_ns::get_syscall_list(
        target::common::assembly::OS_Type::Linux,
        target::common::assembly::Arch_Type::X8664);
#endif

    credence_assert(syscall_list.contains(syscall));

    credence_assert(arguments.size() <= 6);
    target::common::syscall_ns::syscall_t syscall_entry{ 0, 0 };

    syscall_entry = syscall_list.at(syscall);

    credence_assert_equal(syscall_entry[1], arguments.size());

    // the kernel takes the fourth argument in r10, since `syscall` writes
    // its return address to rcx
    std::deque<Register> argument_storage_qword = { Register::r9,
        Register::r8,
        Register::r10,
        Register::rdx,
        Register::rsi,
        Register::rdi };
    std::deque<Register> argument_storage_dword = { Register::r9d,
        Register::r8d,
        Register::r10d,
        Register::edx,
        Register::esi,
        Register::edi };

#if defined(__linux__)
    auto syscall_number =
        target::common::assembly::make_numeric_immediate(syscall_entry[0]);
#elif defined(__APPLE__) || defined(__bsdi__)
    auto syscall_number = target::common::assembly::make_numeric_immediate(
        target::common::syscall_ns::x86_64::bsd_ns::SYSCALL_CLASS_UNIX +
        syscall_entry[0]);
#elif defined(_WIN32) || defined(_WIN64)
    auto syscall_number =
        target::common::assembly::make_numeric_immediate(syscall_entry[0]);
#endif

    x8664_add__asm(instructions, mov, rax, syscall_number);

    syscall_operands_to_instructions(instructions,
        arguments,
        argument_storage_qword,
        argument_storage_dword,
        stack_frame,
        accessor);

    x8664_add__asm(instructions, syscall);
}
}

} // namespace common