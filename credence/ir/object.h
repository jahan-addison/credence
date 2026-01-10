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

#include <algorithm>         // for __find, find
#include <array>             // for array
#include <credence/ir/ita.h> // for ITA, Instructions
#include <credence/map.h>    // for Ordered_Map
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for Label, Address, Data_Type, LValue, Labels
#include <credence/util.h>   // for contains
#include <cstddef>           // for size_t
#include <deque>             // for deque
#include <easyjson.h>        // for JSON
#include <fmt/base.h>        // for copy
#include <fmt/compile.h>     // for format, operator""_cf
#include <map>               // for map
#include <memory>            // for shared_ptr
#include <optional>          // for optional
#include <ranges>            // for __fn, end
#include <source_location>   // for source_location
#include <string>            // for basic_string, string, operator==, char_...
#include <string_view>       // for basic_string_view, string_view
#include <utility>           // for pair, move

namespace credence {

using LValue = type::semantic::LValue;
using Type = type::semantic::Type;
using Size = type::semantic::Size;
using RValue = type::semantic::RValue;
using Label = type::semantic::Label;

namespace ir {

using Stack = std::deque<type::semantic::RValue>;

namespace object {

constexpr bool is_vector_lvalue(LValue const& lvalue)
{
    return util::contains(lvalue, "[") and util::contains(lvalue, "]");
}

/**
 * Vector definition map and storage sizes for the object table
 */
struct Vector
{
    using Label = type::semantic::Label;
    using Address = type::semantic::Address;
    using Size = type::semantic::Size;
    using Entry = std::pair<Label, type::Data_Type>;
    using Storage = Ordered_Map<Label, type::Data_Type>;
    using Offset = Ordered_Map<Label, Address>;
    Vector() = delete;
    explicit Vector(Label label, Address size_of)
        : size(size_of)
        , symbol(std::move(label))
    {
    }
    Storage data{};
    Offset offset{};
    constexpr void set_address_offset(Label const& index, Address address)
    {
        offset.insert(index, address);
    }
    int decay_index{ 0 };
    std::size_t size{ 0 };
    Label symbol{};
    static constexpr std::size_t max_size{ 999 };
};

/**
 *  Function and function frame map and storage sizes for the object table
 */
class Function
{
  public:
    explicit Function(type::semantic::Label const& label)
        : symbol(label)
    {
    }
    using Return_RValue = std::optional<std::pair<RValue, RValue>>;
    using Address_Table =
        Symbol_Table<type::semantic::Label, type::semantic::Address>;
    /**
     * @brief Parse ITA function parameters into locals on the frame stack
     *
     * e.g. `__convert(s,v,*k) = (s,v,*k)`
     */
    constexpr void set_parameters_from_symbolic_label(Label const& label)
    {
        auto search =
            std::string_view{ label.begin() + label.find_first_of("(") + 1,
                label.begin() + label.find_first_of(")") };

        if (!search.empty()) {
            std::string parameter;
            for (auto it = search.begin(); it <= search.end(); it++) {
                auto slice = *it;
                if (slice != ',' and it != search.end())
                    parameter += slice;
                else {
                    parameters.emplace_back(parameter);
                    parameter = "";
                }
            }
        }
    }

  public:
    constexpr bool is_pointer_in_stack_frame(RValue const& rvalue)
    {
        return locals.is_pointer(rvalue) or is_pointer_parameter(rvalue);
    }

    constexpr bool is_pointer_parameter(RValue const& parameter)
    {
        using namespace fmt::literals;
        return util::range_contains(
            fmt::format("*{}"_cf, parameter), parameters);
    }
    constexpr bool is_scaler_parameter(RValue const& parameter)
    {
        return util::range_contains(
            type::from_lvalue_offset(parameter), parameters);
    }
    constexpr bool is_parameter(RValue const& parameter)
    {
        return is_scaler_parameter(parameter) or
               is_pointer_parameter(parameter);
    }
    constexpr int get_index_of_parameter(RValue const& parameter)
    {
        for (std::size_t i = 0; i < parameters.size(); i++) {
            if (type::get_unary_rvalue_reference(parameters[i]) ==
                type::from_lvalue_offset(parameter))
                return i;
        }
        return -1;
    }

  public:
    Return_RValue ret{};
    Label label_before_reserved{};
    type::Parameters parameters{};
    Ordered_Map<LValue, RValue> temporary{};
    Address_Table label_address{};
    std::array<type::semantic::Address, 2> address_location{};
    type::semantic::Label symbol{};
    type::Labels labels{};
    type::Locals locals{};
    type::RValues tokens{};
    static constexpr int max_depth = 999;
    unsigned int allocation = 0;
};

using Instruction_PTR = std::shared_ptr<ir::Instructions>;
using Function_PTR = std::shared_ptr<Function>;
using Vector_PTR = std::shared_ptr<Vector>;
using Stack_Frame = std::optional<Function_PTR>;
using Functions = std::map<std::string, Function_PTR>;
using Vectors = std::map<std::string, Vector_PTR>;

namespace detail {
/**
 * @brief Vector offset rvalue resolution in the stack frame and global symbols
 *
 */
struct Vector_Offset
{
    Vector_Offset() = delete;
    explicit Vector_Offset(object::Function_PTR& stack_frame,
        object::Vectors& vectors)
        : stack_frame_(stack_frame)
        , vectors_(vectors)
    {
    }
    RValue get_rvalue_offset_of_vector(RValue const& offset);
    bool is_valid_vector_address_offset(LValue const& lvalue);

  private:
    object::Function_PTR& stack_frame_;
    object::Vectors& vectors_;
};

} // namespace detail

type::Data_Type get_rvalue_at_lvalue_object_storage(LValue const& lvalue,
    object::Function_PTR& stack_frame,
    object::Vectors& vectors,
    std::source_location const& location = std::source_location::current());

/**
 * @brief Object table of types, functions, and vectors in a frame
 */
class Object
{
  public:
    bool vector_contains(type::semantic::LValue const& lvalue);
    bool local_contains(type::semantic::LValue const& lvalue);

  public:
    bool stack_frame_contains_call_instruction(Label name,
        ir::Instructions const& instructions);

  public:
    constexpr bool is_stack_frame() { return !stack_frame_symbol.empty(); }
    constexpr void set_stack_frame(Label const& label)
    {
        stack_frame_symbol = label;
    }
    constexpr void reset_stack_frame() { stack_frame_symbol = std::string{}; }
    Function_PTR get_stack_frame();
    Function_PTR get_stack_frame(Label const& label);
    type::Locals& get_stack_frame_symbols();

  private:
    Size get_symbol_size_from_rvalue_data_type(LValue const& lvalue,
        Function_PTR const& stack_frame);

  public:
    RValue lvalue_at_temporary_object_address(LValue const& lvalue,
        Function_PTR const& stack_frame);
    Size lvalue_size_at_temporary_object_address(LValue const& lvalue,
        Function_PTR const& stack_frame);
    Size get_size_of_temporary_binary_rvalue(RValue const& rvalue,
        Function_PTR const& stack_frame);

  public:
    Instruction_PTR ir_instructions{};
    util::AST_Node hoisted_symbols;
    Symbol_Table<> globals{};
    Function::Address_Table address_table{};
    std::string stack_frame_symbol{};
    Stack stack{};
    Functions functions{};
    Vectors vectors{};
    type::RValues strings{};
    type::RValues floats{};
    type::RValues doubles{};
    type::Labels labels{};
};

using Object_PTR = std::shared_ptr<Object>;

void set_stack_frame_return_value(RValue const& rvalue,
    Function_PTR& frame,
    Object_PTR& objects);

} // namespace credence

} // namespace ir

} // namespace object