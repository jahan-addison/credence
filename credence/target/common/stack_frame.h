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

#include <credence/ir/table.h>
#include <credence/object.h>
#include <memory>
#include <string>

/****************************************************************************
 * Stack Frame
 *
 * The function under translation, its call stack, and the arguments of the
 * next call. The frame of each function is in the object table, and the
 * backends switch frames at each `BeginFunc`.
 *
 *  Example:
 *
 *    compute(a, b) {
 *      auto x, y, z;
 *      x = a + b;
 *      y = x * 2;
 *      z = y - a;
 *      return(z);
 *    }
 *
 *  Becomes, on x86-64, where a parameter is in its argument register:
 *
 *    edi           parameter 'a'
 *    r8d           parameter 'b'
 *    [rbp - 4]     local 'x'
 *    [rbp - 8]     local 'y'
 *    [rbp - 12]    local 'z'
 *
 *  And on arm64, where a frame with no calls keeps its locals in registers:
 *
 *    w0, w1        parameters 'a' and 'b'
 *    w9, w10, w11  locals 'x', 'y', and 'z'
 *
 ****************************************************************************/

namespace credence::target::common::memory {

using Locals = type::Stack;

/**
 * @brief The function under translation and the arguments of its next call
 *
 *  * argument_stack - the arguments of the next call, pushed by PUSH in
 *    reverse order, so the front is the first argument
 *
 *  * argument_slots - the slot of each argument stored before its call
 *
 *  * call_stack - the caller of each call in progress, pushed at CALL and
 *    popped at POP, so its back is the caller of `tail`
 *
 *  * symbol - the name of the function under translation
 *
 *  * tail - the last function called, whose return value is in the return
 *    register
 */
struct Stack_Frame
{
    explicit Stack_Frame(ir::Object_Table_PTR objects)
        : objects_(objects)
    {
    }

    using Frame = object::Function;
    using Label = std::string;

    inline void set_stack_frame(Label const& name) { symbol = name; }
    inline Frame& get_stack_frame(Label const& name) const
    {
        return objects_->functions.at(name).get();
    }
    inline Frame& get_stack_frame() const
    {
        return objects_->functions.at(symbol).get();
    }

    Locals argument_stack{};
    // the stack slot of each argument that was stored before its call, or
    // empty when the argument is resolved from the argument stack
    Locals argument_slots{};
    Locals call_stack{ "main" };
    Label symbol{ "main" };
    Label tail{};
    std::size_t size{};

  protected:
    ir::Object_Table_PTR objects_;
};

} // namespace target::common::memory
