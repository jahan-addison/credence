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

#include "types.h"           // for RValue, LValue, ...
#include <array>             // for array
#include <credence/map.h>    // for Ordered_Map
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for Parameters, Address, Data_Type, Label
#include <credence/util.h>   // for contains, AST_Node
#include <cstddef>           // for size_t
#include <deque>             // for deque
#include <functional>        // for std::reference_wrapper
#include <map>               // for map
#include <optional>          // for optional
#include <source_location>   // for source_location
#include <string>            // for basic_string, string
#include <utility>           // for pair, move

/****************************************************************************
 * Name Resolution and Object Binding
 *
 * Provides an object storage interface from the IR for target platform passes.
 * Represents vectors, functions, stack frames, and symbol tables as queryable
 * lvalues and rvalues for backend code generation.
 *
 * Example:
 *
 *  main() {
 *    auto array[10];
 *    array[5] = 42;
 *  }
 *
 *  Creates an Object table with:
 *    - Vector "array" with size 10
 *    - Function frame "main" with locals
 *    - Storage for array[5] = (42:int:4)
 *
 ****************************************************************************/

namespace credence::language::hir::binding {

constexpr bool is_vector_lvalue(LValue const& lvalue)
{
    return util::contains(lvalue, "[") and util::contains(lvalue, "]");
}

/**
 * @brief Vector definition map and storage sizes for the object table
 */
struct Vector
{
    using Label = type::semantic::Label;
    using Address = type::semantic::Address;
    using Size = type::semantic::Size;
    using Entry = std::pair<Label, type::Data_Type>;
    using Storage = Ordered_Map<Label, type::Data_Type>;
    using Offset = Ordered_Map<Label, Address>;

    static constexpr std::size_t max_size{ 999 };

  public:
    Vector() = delete;
    explicit Vector(Label label, Address size_of)
        : size(size_of)
        , symbol(std::move(label))
    {
    }
    Vector(Vector const&) = delete;
    Vector& operator=(Vector const&) = delete;
    Vector(Vector&&) = default;
    Vector& operator=(Vector&&) = default;

  public:
    constexpr void set_address_offset(Label const& index, Address address)
    {
        offset.insert(index, address);
    }

  public:
    Storage data{};
    Offset offset{};
    int decay_index{ 0 };
    std::size_t size{ 0 };
    Label symbol{};
};

/**
 * @brief Function and function frame storage data for the object table
 */
class Function
{
  public:
    using Return_RValue = std::optional<std::pair<RValue, RValue>>;
    using Address_Table = Symbol_Table<Label, Address>;

    static constexpr int max_depth = 999;

  public:
    explicit Function(Label const& label)
        : symbol(label)
    {
    }
    Function(Function const&) = delete;
    Function& operator=(Function const&) = delete;
    Function(Function&&) = default;
    Function& operator=(Function&&) = default;

  public:
    void set_parameters_from_symbolic_label(Label const& label);
    bool is_pointer_in_stack_frame(RValue const& rvalue);
    bool is_pointer_parameter(RValue const& parameter);
    bool is_scaler_parameter(RValue const& parameter);
    bool is_parameter(RValue const& parameter);
    int get_index_of_parameter(RValue const& parameter);

  public:
    constexpr void reset_calls() { calls = 0; }

  public:
    Return_RValue ret{};
    Label label_before_reserved{};
    Parameters parameters{};
    Ordered_Map<LValue, RValue> temporary{};
    Address_Table label_address{};
    std::array<Address, 2> address_location{};
    Label symbol{};
    Labels labels{};
    Locals locals{};
    RValues tokens{};
    Pointers pointers{};
    unsigned int calls{ 0 };
    unsigned int allocation{ 0 };
};

using Functions = Ordered_Map<std::string, std::reference_wrapper<Function>>;
using Vectors = Ordered_Map<std::string, std::reference_wrapper<Vector>>;

namespace detail {
/**
 * @brief Vector offset rvalue resolution in the stack frame and global symbols
 */
struct Vector_Offset
{
  public:
    Vector_Offset() = delete;
    explicit Vector_Offset(Function& stack_frame, Vectors& vectors)
        : stack_frame_(stack_frame)
        , vectors_(vectors)
    {
    }

  public:
    RValue get_rvalue_offset_of_vector(RValue const& offset);
    bool is_valid_vector_address_offset(LValue const& lvalue);

  private:
    Function& stack_frame_;
    Vectors& vectors_;
};

} // namespace detail

type::Data_Type get_rvalue_at_lvalue_object_storage(LValue const& lvalue,
    Function& stack_frame,
    Vectors& vectors,
    std::source_location const& location = std::source_location::current());

using Argument_Stack = std::map<Label, std::deque<type::Parameters>>;

/**
 * @brief Object_Table table of types, functions, and vectors in a frame
 */
class Object_Table
{
  public:
    explicit Object_Table() = default;
    Object_Table(Object_Table const&) = delete;
    Object_Table& operator=(Object_Table const&) = delete;
    Object_Table(Object_Table&&) = default;
    Object_Table& operator=(Object_Table&&) = default;

  public:
    Function& set_function_definition(Label const& label);
    Vector& set_vector_definition(Label const& label, Address size_of);

  public:
    bool vector_contains(LValue const& lvalue);
    bool function_contains(Label const& label);
    bool local_contains(LValue const& lvalue);

  public:
    void set_ir_parameters(Label const& label, type::Parameters& parameters);
    std::deque<type::Parameters>& get_ir_parameters(Label const& label);

  public:
    constexpr bool is_stack_frame() { return !stack_frame_symbol.empty(); }
    constexpr void set_stack_frame(Label const& label)
    {
        stack_frame_symbol = label;
    }
    constexpr void reset_stack_frame() { stack_frame_symbol = std::string{}; }
    void reset_frame_calls();
    Function& get_stack_frame();
    Function& get_stack_frame(Label const& label);
    Locals& get_stack_frame_symbols();

  public:
    RValue lvalue_at_temporary_object_address(LValue const& lvalue,
        Function& stack_frame);
    Size lvalue_size_at_temporary_object_address(LValue const& lvalue,
        Function& stack_frame);
    Size get_size_of_temporary_binary_rvalue(RValue const& rvalue,
        Function& stack_frame);

  public:
    bool is_vector(RValue const& rvalue);
    bool is_pointer(RValue const& rvalue);
    bool is_vector_or_pointer(RValue const& rvalue);
    bool is_null_symbol(LValue const& lvalue);

  public:
    Type get_type_from_object_data_type(LValue const& lvalue);
    Size get_size_from_object_name(LValue const& lvalue);

  private:
    Size get_symbol_size_from_rvalue_data_type(LValue const& lvalue,
        Function& stack_frame);

  private:
    std::deque<Function> function_storage_{};
    std::deque<Vector> vector_storage_{};

  public:
    util::AST_Node hoisted_symbols;
    Symbol_Table<> globals{};
    Function::Address_Table address_table{};
    std::string stack_frame_symbol{};
    Stack stack{};
    Argument_Stack ir_parameters{};
    Functions functions{};
    Vectors vectors{};
    RValues strings{};
    RValues floats{};
    RValues doubles{};
    Labels labels{};
};

void set_stack_frame_return_value(RValue const& rvalue,
    Function& frame,
    Object_Table& objects);

} // namespace credence::language::hir::binding
