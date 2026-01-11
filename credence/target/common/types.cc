#include "types.h"

namespace credence::target::common::detail {

struct base_stack_pointer::impl
{
    using Offset = Stack_Offset;
};

base_stack_pointer::base_stack_pointer()
    : pimpl{ std::make_unique<impl>() }
{
}
base_stack_pointer::~base_stack_pointer() = default;
base_stack_pointer::base_stack_pointer(base_stack_pointer&&) noexcept = default;
base_stack_pointer& base_stack_pointer::operator=(
    base_stack_pointer&&) noexcept = default;

} // namespace credence::target::common::detail
