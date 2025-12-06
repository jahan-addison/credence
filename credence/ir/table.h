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
#pragma once

#include <algorithm>         // for __find, find
#include <array>             // for array
#include <credence/error.h>  // for credence_assert
#include <credence/ir/ita.h> // for ITA, Quadruple, Instructions
#include <credence/map.h>    // for Ordered_Map
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for Data_Type, Label, Address, RValue, LValue
#include <credence/util.h>   // for CREDENCE_PRIVATE_UNLESS_TESTED, AST_Node
#include <fmt/format.h>      // for format
#include <functional>        // for bind, placeholders
#include <initializer_list>  // for initializer_list
#include <iosfwd>            // for ostream
#include <map>               // for map
#include <memory>            // for shared_ptr, unique_ptr
#include <optional>          // for optional
#include <ranges>            // for __fn, end
#include <simplejson.h>      // for JSON
#include <source_location>   // for source_location
#include <string>            // for basic_string, operator==, string, char_...
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for get

namespace credence {

namespace ir {

// clang-format off
void emit(std::ostream& os,
util::AST_Node const& symbols,
util::AST_Node const& ast);
// clang-format on

namespace detail {

/**
 * Symbolic vector definition and allocation
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
    static constexpr std::size_t max_size{ 1000 };
};

/**
 * Symbolic function stack frame
 */
struct Function
{
    explicit Function(type::semantic::Label const& label)
        : symbol(label)
    {
    }
    using Address_Table =
        Symbol_Table<type::semantic::Label, type::semantic::Address>;
    void set_parameters_from_symbolic_label(type::semantic::Label const& label);
    type::semantic::Label symbol{};
    type::Labels labels{};
    type::Locals locals{};

    constexpr inline bool is_parameter(type::semantic::RValue const& parameter)
    {
        return std::ranges::find(parameters, parameter) !=
               std::ranges::end(parameters);
    }

    type::semantic::RValue ret{};
    type::Parameters parameters{};
    Ordered_Map<type::semantic::LValue, type::semantic::RValue> temporary{};
    Address_Table label_address{};
    std::array<type::semantic::Address, 2> address_location{};

    static constexpr int max_depth = 1000;
    unsigned int allocation = 0;
};

} // namespace detail

/**
 * @brief
 * Provides instruction location maps, stack frame with all local
 * definitions, type data, and value sizes as a pre-selection pass
 */
class Table
{

  public:
    Table() = delete;
    ~Table() = default;
    explicit Table(ITA::Node const& hoisted_symbols)
        : hoisted_symbols(hoisted_symbols)
    {
    }
    explicit Table(ITA::Node const& hoisted_symbols, Instructions& instructions)
        : instructions(instructions)
        , hoisted_symbols(hoisted_symbols)
    {
    }
    explicit Table(
        ITA::Node const& hoisted_symbols,
        Instructions& instructions,
        Symbol_Table<> globals)
        : instructions(instructions)
        , hoisted_symbols(hoisted_symbols)
        , globals(std::move(globals))
    {
    }

    using Table_PTR = std::unique_ptr<Table>;

  private:
    using LValue = type::semantic::LValue;
    using Type = type::semantic::Type;
    using Size = type::semantic::Size;
    using RValue = type::semantic::RValue;
    using Label = type::semantic::Label;

  public:
    using Function_PTR = std::shared_ptr<detail::Function>;
    using Vector_PTR = std::shared_ptr<detail::Vector>;

  private:
    using Stack_Frame = std::optional<Function_PTR>;
    using Functions = std::map<std::string, Function_PTR>;
    using Vectors = std::map<std::string, Vector_PTR>;

  public:
    Stack_Frame stack_frame;
    Instructions build_from_ita_instructions();
    bool stack_frame_contains_ita_instruction(Label name, Instruction inst);
    void build_symbols_from_vector_lvalues();
    void build_vector_definitions_from_globals();
    RValue from_temporary_lvalue(LValue const& lvalue);
    static Table_PTR build_from_ast(
        ITA::Node const& symbols,
        ITA::Node const& ast);

    /**
     * @brief Get the type from a local in the stack frame
     */
    Type get_type_from_rvalue_data_type(LValue const& lvalue);
    Size get_size_from_local_lvalue(LValue const& lvalue);

  public:
    inline bool is_vector(RValue const& rvalue)
    {
        auto label = util::contains(rvalue, "[")
                         ? type::from_lvalue_offset(rvalue)
                         : rvalue;
        return vectors.contains(label);
    }
    inline bool is_pointer(RValue const& rvalue)
    {
        auto locals = get_stack_frame_symbols();
        return locals.is_pointer(rvalue) or rvalue.starts_with("&");
    }
    inline bool is_vector_or_pointer(RValue const& rvalue)
    {
        return is_vector(rvalue) or is_pointer(rvalue);
    }

  private:
    /** Left-hand-side and right-hand-side type equality check */
    inline bool lhs_rhs_type_is_equal(LValue const& lhs, LValue const& rhs)
    {
        return get_type_from_rvalue_data_type(lhs) ==
               get_type_from_rvalue_data_type(rhs);
    }
    inline bool lhs_rhs_type_is_equal(
        LValue const& lhs,
        type::Data_Type const& rvalue)
    {
        return get_type_from_rvalue_data_type(lhs) == std::get<1>(rvalue);
    }

    inline bool lhs_rhs_type_is_equal(
        type::Data_Type const& lhs,
        type::Data_Type const& rhs)
    {
        return type::get_type_from_rvalue_data_type(lhs) ==
               type::get_type_from_rvalue_data_type(rhs);
    }

    /**
     * @brief Check that the type of an lvalue is an integral type
     */
    inline void assert_integral_unary_expression(
        RValue const& rvalue,
        Type const& type,
        std::source_location const& location = std::source_location::current())
    {
        if (std::ranges::find(type::integral_unary_types, type) ==
            type::integral_unary_types.end())
            table_compiletime_error(
                fmt::format(
                    "invalid numeric unary expression on lvalue, lvalue "
                    "type "
                    "\"{}\" is not a numeric type",
                    type),
                rvalue,
                location);
    }

    /**
     * @brief Either lhs or rhs are trivial vector assignments
     */
    inline bool is_trivial_vector_assignment(
        LValue const& lhs,
        LValue const& rhs)
    {
        return (
            (vectors.contains(lhs) and vectors[lhs]->data.size() == 1) or
            (vectors.contains(rhs) and vectors[rhs]->data.size() == 1));
    }

  public:
    constexpr inline bool is_stack_frame() { return stack_frame.has_value(); }
    inline std::shared_ptr<detail::Function> get_stack_frame()
    {
        credence_assert(is_stack_frame());
        return stack_frame.value();
    }

    inline type::Locals& get_stack_frame_symbols()
    {
        return get_stack_frame()->locals;
    }

    void set_stack_frame(Label const& label);

  public:
    type::Data_Type from_integral_unary_expression(RValue const& lvalue);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void from_func_start_ita_instruction(Label const& label);
    void from_func_end_ita_instruction();
    void from_call_ita_instruction(Label const& label);
    void from_globl_ita_instruction(Label const& label);
    void from_locl_ita_instruction(Quadruple const& instruction);
    void from_push_instruction(Quadruple const& instruction);
    void from_pop_instruction(Quadruple const& instruction);
    void from_return_instruction(Quadruple const& instruction);
    void from_label_ita_instruction(Quadruple const& instruction);
    void from_mov_ita_instruction(Quadruple const& instruction);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::semantic::Address instruction_index{ 0 };

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void from_temporary_reassignment(LValue const& lhs, LValue const& rhs);
    type::Data_Type from_rvalue_unary_expression(
        LValue const& lvalue,
        RValue const& rvalue,
        type::RValue_Reference unary_operator);
    void from_scaler_symbol_assignment(LValue const& lhs, LValue const& rhs);
    void from_pointer_or_vector_assignment(
        LValue const& lhs,
        LValue const& rhs,
        bool indirection = false);
    void is_boundary_out_of_range(RValue const& rvalue);

    // clang-format on

  private:
    void type_invalid_assignment_check(
        LValue const& lvalue,
        RValue const& rvalue);
    void type_invalid_assignment_check(
        LValue const& lvalue,
        type::Data_Type const& rvalue);
    void type_invalid_assignment_check(
        LValue const& lvalue,
        Vector_PTR const& vector,
        RValue const& index);
    void type_invalid_assignment_check(
        Vector_PTR const& vector_lhs,
        Vector_PTR const& vector_rhs,
        RValue const& index);
    void type_invalid_assignment_check(
        Vector_PTR const& vector_lhs,
        Vector_PTR const& vector_rhs,
        RValue const& index_lhs,
        RValue const& index_rhs);

  private:
    void type_safe_assign_pointer(
        LValue const& lvalue,
        RValue const& rvalue,
        bool indirection = false);
    void type_safe_assign_trivial_vector(
        LValue const& lvalue,
        RValue const& rvalue);
    void type_safe_assign_dereference(
        LValue const& lvalue,
        RValue const& rvalue);
    void type_safe_assign_vector(LValue const& lvalue, RValue const& rvalue);
    void type_safe_assign_pointer_or_vector_lvalue(
        LValue const& lvalue,
        type::RValue_Reference_Type const& rvalue,
        bool indirection = false);

  private:
    void from_trivial_vector_assignment(
        LValue const& lhs,
        type::Data_Type const& rvalue);

  public:
    type::Data_Type get_rvalue_data_type_at_pointer(LValue const& lvalue);

  private:
    using Type_Check_Lambda = std::function<bool(type::semantic::LValue)>;
    bool vector_contains_(type::semantic::LValue const& lvalue)
    {
        return vectors.contains(lvalue);
    }
    bool local_contains_(type::semantic::LValue const& lvalue)
    {
        const auto& locals = get_stack_frame_symbols();
        return locals.is_defined(lvalue) and not is_vector_lvalue(lvalue);
    }
    Type_Check_Lambda is_vector_lvalue = [&](LValue const& lvalue) {
        return util::contains(lvalue, "[") and util::contains(lvalue, "]");
    };

  public:
    void table_compiletime_error(
        std::string_view message,
        type::RValue_Reference symbol,
        std::source_location const& location = std::source_location::current());

  public:
    Instructions instructions{};

  public:
    ITA::Node hoisted_symbols;
    Symbol_Table<> globals{};
    detail::Function::Address_Table address_table{};
    type::Stack stack{};
    Functions functions{};
    Vectors vectors{};
    type::Strings strings{};
    type::Labels labels{};
};

} // namespace ir

} // namespace credence