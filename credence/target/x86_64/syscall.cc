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
#include "instructions.h"   // for Register, make_numeric_immediate, Mnemonic
#include <credence/error.h> // for assert_equal_impl, credence_assert, cred...
#include <deque>            // for deque
#include <string>           // for basic_string
#include <variant>          // for variant
#include <vector>           // for vector

namespace credence::target::x86_64::syscall {

namespace common {
// cppcheck-suppress constParameterReference
void exit_syscall(Instructions& instructions, int exit_status)
{
    auto immediate = detail::make_numeric_immediate(exit_status);
#if defined(CREDENCE_TEST) || defined(__linux__)
    syscall::linux_ns::make_syscall(instructions, "exit", { immediate });
#elif defined(__APPLE__) || defined(__bsdi__)
    syscall::bsd_ns::make_syscall(instructions, "exit", { immediate });
#else
    credence_error("Operating system not supported");
#endif
}

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

void make_syscall(
    Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments)
{
    using namespace detail;
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
    Storage syscall_number = make_numeric_immediate(number);
    add_inst_ll(instructions, mov, rax, syscall_number);
    for (auto const& arg : arguments) {
        auto storage = argument_storage.back();
        argument_storage.pop_back();
        if (is_immediate_rip_address_offset(arg))
            add_inst_as(instructions, lea, storage, arg);
        else
            add_inst_as(instructions, mov, storage, arg);
    }
    add_inst_ee(instructions, detail::Mnemonic::syscall);
}
} // namespace linux

namespace bsd_ns {

void make_syscall(
    Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments)
{
    using namespace detail;
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
    Storage syscall_number =
        make_numeric_immediate(SYSCALL_CLASS_UNIX + number);
    add_inst_ll(instructions, mov, rax, syscall_number);
    for (auto const& arg : arguments) {
        auto storage = argument_storage.back();
        argument_storage.pop_back();
        if (is_immediate_rip_address_offset(arg))
            add_inst_as(instructions, lea, storage, arg);
        else
            add_inst_as(instructions, mov, storage, arg);
    }
    add_inst_ee(instructions, detail::Mnemonic::syscall);
}

} // namespace bsd

namespace common {
void make_syscall(
    // cppcheck-suppress constParameterReference
    Instructions& instructions,
    std::string_view syscall,
    syscall_arguments_t const& arguments)
{
#if defined(CREDENCE_TEST) || defined(__linux__)
    syscall::linux_ns::make_syscall(instructions, syscall, arguments);
#elif defined(__APPLE__) || defined(__bsdi__)
    syscall::bsd_ns::make_syscall(instructions, syscall, arguments);
#else
    credence_error("Operating system not supported");
#endif
}
} // namespace common

}