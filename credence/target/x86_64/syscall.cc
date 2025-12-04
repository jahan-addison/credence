#include "syscall.h"
#include "instructions.h"   // for Register, make_numeric_immediate, Mnemonic
#include <credence/error.h> // for assert_equal_impl, credence_assert, cred...
#include <deque>            // for deque
#include <string>           // for basic_string
#include <variant>          // for variant

namespace credence::target::x86_64::syscall {

namespace common {
// cppcheck-suppress constParameterReference
void exit_syscall(Instructions& instructions, int exit_status)
{
    auto immediate = detail::make_numeric_immediate(exit_status);
#if defined(CREDENCE_TEST) || defined(__linux__)
    syscall::linux::make_syscall(instructions, "exit", { immediate });
#elif defined(__APPLE__) || defined(__bsdi__)
    syscall::bsd::make_syscall(instructions, "exit", { immediate });
#else
    credence_error("Operating system not supported");
#endif
}

} // namespace common

namespace linux {

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
        add_inst_as(instructions, mov, storage, arg);
    }
    add_inst_ee(instructions, detail::Mnemonic::syscall);
}
} // namespace linux

namespace bsd {

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
        add_inst_as(instructions, mov, storage, arg);
    }
    add_inst_ee(instructions, detail::Mnemonic::syscall);
}
} // namespace bsd

}