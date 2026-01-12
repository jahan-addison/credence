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

/****************************************************************************
 *  Object table and type system for the intermediate representation. This
 *  tracks vectors, functions, stack frames, and symbol tables throughout
 *  the compilation process.
 *
 *  Example:
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
 * @brief Vector definition map and storage sizes for the object table
 *
 * Uses the PIMPL idiom to reduce build times
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
    // Constructors and destructor
    Vector() = delete;
    explicit Vector(Label label, Address size_of);
    ~Vector();
    Vector(Vector&&) noexcept;
    Vector& operator=(Vector&&) noexcept;
    Vector(Vector const&) = delete;
    Vector& operator=(Vector const&) = delete;

    // Getters
    Storage& get_data();
    Storage const& get_data() const;
    Offset& get_offset();
    Offset const& get_offset() const;
    int& get_decay_index();
    int get_decay_index() const;
    std::size_t& get_size();
    std::size_t get_size() const;
    Label& get_symbol();
    Label const& get_symbol() const;

    // Setters
    void set_address_offset(Label const& index, Address address);

  private:
    struct Vector_IMPL;
    std::unique_ptr<Vector_IMPL> pimpl;
};

/**
 * @brief Function and function frame storage data for the object table
 *
 * Uses the PIMPL idiom to reduce build times
 */
class Function
{
  public:
    using Return_RValue = std::optional<std::pair<RValue, RValue>>;
    using Address_Table =
        Symbol_Table<type::semantic::Label, type::semantic::Address>;

    static constexpr int max_depth = 999;

  public:
    // Constructors and destructor
    explicit Function(type::semantic::Label const& label);
    ~Function();
    Function(Function&&) noexcept;
    Function& operator=(Function&&) noexcept;
    Function(Function const&) = delete;
    Function& operator=(Function const&) = delete;

    // Parameter utilities
    void set_parameters_from_symbolic_label(Label const& label);
    bool is_pointer_in_stack_frame(RValue const& rvalue);
    bool is_pointer_parameter(RValue const& parameter);
    bool is_scaler_parameter(RValue const& parameter);
    bool is_parameter(RValue const& parameter);
    int get_index_of_parameter(RValue const& parameter);

    // Getters
  public:
    Label& get_label_before_reserved();
    Label const& get_label_before_reserved() const;
    type::Parameters& get_parameters();
    type::Parameters const& get_parameters() const;
    Ordered_Map<LValue, RValue>& get_temporary();
    Ordered_Map<LValue, RValue> const& get_temporary() const;
    Address_Table& get_label_address();
    Address_Table const& get_label_address() const;
    std::array<type::semantic::Address, 2>& get_address_location();
    std::array<type::semantic::Address, 2> const& get_address_location() const;
    type::semantic::Label& get_symbol();
    type::semantic::Label const& get_symbol() const;

  public:
    Return_RValue& get_ret();
    Return_RValue const& get_ret() const;
    type::Labels& get_labels();
    type::Labels const& get_labels() const;
    type::Locals& get_locals();
    type::Locals const& get_locals() const;
    type::RValues& get_tokens();
    type::RValues const& get_tokens() const;
    type::Pointers& get_pointers();
    type::Pointers const& get_pointers() const;
    unsigned int& get_allocation();
    unsigned int get_allocation() const;

  private:
    struct Function_IMPL;
    std::unique_ptr<Function_IMPL> pimpl;
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
 * Uses the PIMPL idiom to reduce build times
 */
struct Vector_Offset
{
  public:
    // Constructors and destructor
    Vector_Offset() = delete;
    explicit Vector_Offset(object::Function_PTR& stack_frame,
        object::Vectors& vectors);
    ~Vector_Offset();
    Vector_Offset(Vector_Offset&&) noexcept;
    Vector_Offset& operator=(Vector_Offset&&) noexcept;
    Vector_Offset(Vector_Offset const&) = delete;
    Vector_Offset& operator=(Vector_Offset const&) = delete;

    // Utilities
    RValue get_rvalue_offset_of_vector(RValue const& offset);
    bool is_valid_vector_address_offset(LValue const& lvalue);

  private:
    struct Vector_Offset_IMPL;
    std::unique_ptr<Vector_Offset_IMPL> pimpl;
};

} // namespace detail

type::Data_Type get_rvalue_at_lvalue_object_storage(LValue const& lvalue,
    object::Function_PTR& stack_frame,
    object::Vectors& vectors,
    std::source_location const& location = std::source_location::current());

/**
 * @brief Object table of types, functions, and vectors in a frame
 *
 * Uses the PIMPL idiom to reduce build times
 */
class Object
{
  public:
    // Constructors and destructor
    explicit Object();
    ~Object();
    Object(Object&&) noexcept;
    Object& operator=(Object&&) noexcept;
    Object(Object const&) = delete;
    Object& operator=(Object const&) = delete;

    // Container queries
    bool vector_contains(type::semantic::LValue const& lvalue);
    bool local_contains(type::semantic::LValue const& lvalue);
    bool stack_frame_contains_call_instruction(Label name,
        ir::Instructions const& instructions);

    // Stack frame management
    bool is_stack_frame();
    void set_stack_frame(Label const& label);
    void reset_stack_frame();
    Function_PTR get_stack_frame();
    Function_PTR get_stack_frame(Label const& label);
    type::Locals& get_stack_frame_symbols();

    // Temporary object utilities
    RValue lvalue_at_temporary_object_address(LValue const& lvalue,
        Function_PTR const& stack_frame);
    Size lvalue_size_at_temporary_object_address(LValue const& lvalue,
        Function_PTR const& stack_frame);
    Size get_size_of_temporary_binary_rvalue(RValue const& rvalue,
        Function_PTR const& stack_frame);

    // Getters: IR and symbol tables
    Instruction_PTR& get_ir_instructions();
    Instruction_PTR const& get_ir_instructions() const;
    util::AST_Node& get_hoisted_symbols();
    util::AST_Node const& get_hoisted_symbols() const;
    Symbol_Table<>& get_globals();
    Symbol_Table<> const& get_globals() const;
    Function::Address_Table& get_address_table();
    Function::Address_Table const& get_address_table() const;

    // Getters: Stack frame state
    std::string& get_stack_frame_symbol();
    std::string const& get_stack_frame_symbol() const;
    Stack& get_stack();
    Stack const& get_stack() const;

    // Getters: Functions and vectors
    Functions& get_functions();
    Functions const& get_functions() const;
    Vectors& get_vectors();
    Vectors const& get_vectors() const;

    // Getters: Literal storage
    type::RValues& get_strings();
    type::RValues const& get_strings() const;
    type::RValues& get_floats();
    type::RValues const& get_floats() const;
    type::RValues& get_doubles();
    type::RValues const& get_doubles() const;
    type::Labels& get_labels();
    type::Labels const& get_labels() const;

  private:
    Size get_symbol_size_from_rvalue_data_type(LValue const& lvalue,
        Function_PTR const& stack_frame);

  private:
    struct Object_IMPL;
    std::unique_ptr<Object_IMPL> pimpl;
};

using Object_PTR = std::shared_ptr<Object>;

void set_stack_frame_return_value(RValue const& rvalue,
    Function_PTR& frame,
    Object_PTR& objects);

} // namespace credence

} // namespace ir

} // namespace object