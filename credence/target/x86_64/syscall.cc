/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
    Register E_ADDRESS = Register::eax;
    auto immediate = assembly::make_numeric_immediate(exit_status);
#if defined(CREDENCE_TEST) || defined(__linux__)
    syscall_ns::linux_ns::make_syscall(
        instructions, "exit", { immediate }, &E_ADDRESS);
#elif defined(__APPLE__) || defined(__bsdi__)
    syscall_ns::bsd_ns::make_syscall(
        instructions, "exit", { immediate }, &E_ADDRESS);
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

namespace linux_ns {

/**
 * @brief Create instructions for a linux x86_64 platform syscall
 *    See syscall.h for details
 */
void make_syscall(Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    Register* address_of)
{
    credence_assert(syscall_list.contains(syscall));
    credence_assert(arguments.size() <= 6);
    auto [number, arg_size] = syscall_list.at(syscall);
    credence_assert_equal(arg_size, arguments.size());

    std::deque<Register> argument_storage = { Register::r9,
        Register::r8,
        Register::r10,
        Register::rdx,
        Register::rsi,
        Register::rdi };

    assembly::Storage syscall_number = assembly::make_numeric_immediate(number);
    asm__dest_rs(instructions, mov, rax, syscall_number);

    for (auto const& arg : arguments) {
        auto storage = argument_storage.back();
        argument_storage.pop_back();
        if (is_immediate_rip_address_offset(arg))
            add_asm__as(instructions, lea, storage, arg);
        else {
            if (storage == rr(rsi) and *address_of == Register::rcx) {
                *address_of = Register::eax;
                asm__src_rs(instructions, movq_, storage, rcx);
                continue;
            }
            add_asm__as(instructions, movq_, storage, arg);
        }
    }

    asm__zero_o(instructions, syscall);
}
/**
 * @brief Create instructions for a linux x86_64 platform syscall
 *    In additional, this overload checks allocated pointers in a frame
 */
void make_syscall(Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame& stack_frame,
    Register* address_of)
{
    credence_assert(syscall_list.contains(syscall));
    credence_assert(arguments.size() <= 6);
    auto [number, arg_size] = syscall_list.at(syscall);
    credence_assert_equal(arg_size, arguments.size());

    std::deque<Register> argument_storage = { Register::r9,
        Register::r8,
        Register::r10,
        Register::rdx,
        Register::rsi,
        Register::rdi };

    assembly::Storage syscall_number = assembly::make_numeric_immediate(number);
    asm__dest_rs(instructions, mov, rax, syscall_number);
    auto frame = stack_frame.get_stack_frame();
    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments[i];
        auto argument_rvalue =
            type::get_unary_rvalue_reference(stack_frame.argument_stack.at(i));
        auto storage = argument_storage.back();
        argument_storage.pop_back();
        if (is_immediate_rip_address_offset(arg) or
            frame->locals.is_pointer(argument_rvalue) or
            frame->is_pointer_parameter(argument_rvalue))
            add_asm__as(instructions, lea, storage, arg);
        else {
            if (storage == rr(rsi) and *address_of == Register::rcx) {
                *address_of = Register::eax;
                asm__src_rs(instructions, movq_, storage, rcx);
                continue;
            }
            add_asm__as(instructions, movq_, storage, arg);
        }
    }
}

} // namespace linux

namespace bsd_ns {

/**
 * @brief Create instructions for a BSD (Darwin) x86_64 platform syscall
 *    In additional, this overload checks allocated pointers in a frame
 */
void make_syscall(Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame& stack_frame,
    Register* address_of)
{
    credence_assert(syscall_list.contains(syscall));
    credence_assert(arguments.size() <= 6);
    auto [number, arg_size] = syscall_list.at(syscall);
    credence_assert_equal(arg_size, arguments.size());
    // clang-format off
    std::deque<Register> argument_storage = {
        Register::r9, Register::r8, Register::r10,
        Register::rdx, Register::rsi, Register::rdi
    };
    // clang-format on
    auto frame = stack_frame.get_stack_frame();
    assembly::Storage syscall_number =
        assembly::make_numeric_immediate(SYSCALL_CLASS_UNIX + number);
    asm__dest_rs(instructions, mov, rax, syscall_number);
    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments[i];
        auto argument_rvalue =
            type::get_unary_rvalue_reference(stack_frame.argument_stack.at(i));
        auto storage = argument_storage.back();
        argument_storage.pop_back();
        if (is_immediate_rip_address_offset(arg) or
            frame->locals.is_pointer(argument_rvalue) or
            frame->is_pointer_parameter(argument_rvalue))
            add_asm__as(instructions, lea, storage, arg);
        else {
            if (storage == rr(rsi) and *address_of == Register::rcx) {
                *address_of = Register::eax;
                asm__src_rs(instructions, mov, storage, rcx);
                continue;
            }
            add_asm__as(instructions, mov, storage, arg);
        }
    }
    asm__zero_o(instructions, syscall);
}

/**
 * @brief Create instructions for a BSD (Darwin) x86_64 platform syscall
 *    See syscall.h for details
 */
void make_syscall(Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    Register* address_of)
{
    credence_assert(syscall_list.contains(syscall));
    credence_assert(arguments.size() <= 6);
    auto [number, arg_size] = syscall_list.at(syscall);
    credence_assert_equal(arg_size, arguments.size());
    // clang-format off
    std::deque<Register> argument_storage = {
        Register::r9, Register::r8, Register::r10,
        Register::rdx, Register::rsi, Register::rdi
    };
    // clang-format on
    assembly::Storage syscall_number =
        assembly::make_numeric_immediate(SYSCALL_CLASS_UNIX + number);
    asm__dest_rs(instructions, mov, rax, syscall_number);
    for (std::size_t i = 0; i < arguments.size(); i++) {
        auto arg = arguments[i];
        auto storage = argument_storage.back();
        argument_storage.pop_back();
        if (is_immediate_rip_address_offset(arg))
            add_asm__as(instructions, lea, storage, arg);
        else {
            if (storage == rr(rsi) and *address_of == Register::rcx) {
                *address_of = Register::eax;
                asm__src_rs(instructions, mov, storage, rcx);
                continue;
            }
            add_asm__as(instructions, mov, storage, arg);
        }
    }
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
    Register* address_of)
{
#if defined(CREDENCE_TEST) || defined(__linux__)
    syscall_ns::linux_ns::make_syscall(
        instructions, syscall, arguments, address_of);
#elif defined(__APPLE__) || defined(__bsdi__)
    syscall_ns::bsd_ns::make_syscall(
        instructions, syscall, arguments, address_of);
#else
    credence_error("Operating system not supported");
#endif
}

void make_syscall(
    // cppcheck-suppress constParameterReference
    Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments,
    memory::Stack_Frame& stack_frame,
    Register* address_of)
{
#if defined(CREDENCE_TEST) || defined(__linux__)
    syscall_ns::linux_ns::make_syscall(
        instructions, syscall, arguments, stack_frame, address_of);
#elif defined(__APPLE__) || defined(__bsdi__)
    syscall_ns::bsd_ns::make_syscall(
        instructions, syscall, arguments, stack_frame, address_of);
#else
    credence_error("Operating system not supported");
#endif
}

} // namespace common
}