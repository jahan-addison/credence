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

#pragma once

#include <credence/ir/object.h>
#include <credence/ir/table.h>
#include <memory>
#include <string>

/****************************************************************************
 *
 * Stack frame
 *
 * Local variables, parameters, and call stack for each function.
 * Manages stack offsets for variable access during code translation.
 *
 * Example - function with locals:
 *
 *   compute(a, b) {
 *     auto x, y, z;
 *     x = a + b;
 *     y = x * 2;
 *     z = y - a;
 *     return(z);
 *   }
 *
 * Stack frame layout:
 *   [rbp + 16] parameter 'b'
 *   [rbp + 8]  parameter 'a'
 *   [rbp + 0]  saved base pointer
 *   [rbp - 4]  local 'x'
 *   [rbp - 8] local 'y'
 *   [rbp - 12] local 'z'
 *
 ****************************************************************************/

namespace credence::target::common::memory {

using Locals = ir::Stack;

struct Stack_Frame
{
    explicit Stack_Frame(std::shared_ptr<ir::object::Object> objects)
        : objects_(objects)
    {
    }

    using Frame = ir::object::Function_PTR;
    using Label = std::string;

    inline void set_stack_frame(Label const& name)
    {
        objects_->set_stack_frame(name);
    }
    inline Frame get_stack_frame(Label const& name) const
    {
        return objects_->get_functions().at(name);
    }
    inline Frame get_stack_frame() const
    {
        return objects_->get_functions().at(symbol);
    }

    Locals argument_stack{};
    Locals call_stack{ "main" };
    Label symbol{ "main" };
    Label tail{};
    std::size_t size{};

  protected:
    std::shared_ptr<ir::object::Object> objects_;
};

} // namespace target::common::memory
