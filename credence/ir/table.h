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

#include <algorithm>         // for remove_if, __any_of, __find_if, any_of
#include <array>             // for array
#include <cctype>            // for isspace
#include <credence/assert.h> // for CREDENCE_ASSERT
#include <credence/ir/ita.h> // for ITA
#include <credence/map.h>    // for Ordered_Map
#include <credence/symbol.h> // for Symbol_Table
#include <credence/types.h>  // for LValue, RValue, Data_Type, ...
#include <credence/util.h>   // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <cstddef>           // for size_t
#include <deque>             // for deque
#include <initializer_list>  // for initializer_list
#include <memory>            // for shared_ptr, unique_ptr
#include <optional>          // for nullopt, nullopt_t, optional
#include <set>               // for set
#include <string>            // for basic_string, string
#include <string_view>       // for basic_string_view, string_view
#include <tuple>             // for tuple
#include <utility>           // for pair
#include <variant>           // for variant

namespace credence {

namespace ir {

// clang-format off
void emit(std::ostream& os,
util::AST_Node const& symbols,
util::AST_Node const& ast);
// clang-format on

namespace detail {

/**
 * Symbolic ITA vector definition and allocation
 */
struct Vector
{
    using Storage = Ordered_Map<std::string, type::Data_Type>;
    Vector() = delete;
    explicit Vector(type::semantic::Address size_of)
        : size(size_of)
    {
    }
    Storage data{};
    int decay_index{ 0 };
    unsigned long size{ 0 };
    static constexpr int max_size{ 1000 };
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
 * Provides instruction location maps, the stack frame and stack
 * allocation sizes as a pass for platform code generation:
 *
 *      * Instruction address table to functions in the ITA
 *         * Symbolic stack for Instruction::CALL
 *      * Function definition map of local labels,
 *         * stack allocation, and depth size
 *      * Vector definition map of vectors
 *         * (arrays) in the global scope
 *      * A set of labels for Instruction::GOTO jumps
 */
class Table
{

  public:
    Table() = delete;
    ~Table() = default;
    explicit Table(ITA::Node const& hoisted_symbols)
        : hoisted_symbols_(hoisted_symbols)
    {
    }
    explicit Table(ITA::Node const& hoisted_symbols, Instructions& instructions)
        : instructions(instructions)
        , hoisted_symbols_(hoisted_symbols)
    {
    }

    using Table_PTR = std::unique_ptr<Table>;

  private:
    using LValue = type::semantic::LValue;
    using Type = type::semantic::Type;
    using Size = type::semantic::Size;
    using RValue = type::semantic::RValue;
    using Label = type::semantic::Label;

  private:
    using Function_PTR = std::shared_ptr<detail::Function>;
    using Stack_Frame = std::optional<Function_PTR>;
    using Vector_PTR = std::shared_ptr<detail::Vector>;
    using Functions = std::map<std::string, Function_PTR>;
    using Vectors = std::map<std::string, Vector_PTR>;

  public:
    Stack_Frame stack_frame;
    Instructions build_from_ita_instructions();
    bool stack_frame_contains_ita_instruction(Label name, Instruction inst);
    void build_symbols_from_vector_lvalues();
    void build_vector_definitions_from_globals(Symbol_Table<>& globals);
    RValue from_temporary_lvalue(LValue const& lvalue);
    static Table_PTR build_from_ast(
        ITA::Node const& symbols,
        ITA::Node const& ast);

    /**
     * @brief Get the type from a local in the stack frame
     */
    Type get_type_from_local_lvalue(LValue const& lvalue);
    Size get_size_from_local_lvalue(LValue const& lvalue);

  public:
    inline bool is_vector_or_pointer(RValue const& rvalue)
    {
        auto locals = get_stack_frame_symbols();
        return vectors.contains(rvalue) or util::contains(rvalue, "[") or
               locals.is_pointer(rvalue) or rvalue.starts_with("&");
    }

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    /** Left-hand-side and right-hand-side type equality check */
    inline bool lhs_rhs_type_is_equal(LValue const& lhs, LValue const& rhs)
    {
        return get_type_from_local_lvalue(lhs) ==
               get_type_from_local_lvalue(rhs);
    }
    inline bool lhs_rhs_type_is_equal(
        LValue const& lhs,
        type::Data_Type const& rvalue)
    {
        return get_type_from_local_lvalue(lhs) == std::get<1>(rvalue);
    }

    /**
     * @brief Check that the type of an lvalue is an integral type
     */
    inline void assert_integral_unary_expression(
        LValue const& lvalue,
        RValue const& rvalue,
        Type const& type)
    {
        if (std::ranges::find(type::integral_unary_types, type) ==
            type::integral_unary_types.end())
            construct_error(
                std::format(
                    "invalid numeric unary expression on lvalue, lvalue "
                    "type "
                    "\"{}\" is not a numeric type",
                    lvalue),
                rvalue);
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
        CREDENCE_ASSERT(is_stack_frame());
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
    void from_label_ita_instruction(Quadruple const& instruction);
    void from_mov_ita_instruction(Quadruple const& instruction);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::Data_Type from_rvalue_unary_expression(
        LValue const& lvalue,
        RValue const& rvalue,
        type::RValue_Reference unary_operator);
    void from_scaler_symbol_assignment(
        LValue const& lhs,
        LValue const& rhs);
    void from_pointer_or_vector_assignment(
        LValue const& lhs,
        LValue& rhs,
        bool indirection = false);
    void is_boundary_out_of_range(RValue const& rvalue);
    void type_invalid_assignment_check(
        LValue const& lvalue,
        RValue const& rvalue);
    void type_invalid_assignment_check(
        LValue const& lvalue,
        type::Data_Type const& rvalue);
    void from_trivial_vector_assignment(
        LValue const& lhs,
        type::Data_Type const& rvalue);
    void reassign_valid_pointers_or_vectors(
        LValue const& lvalue,
        type::RValue_Reference_Type const& rvalue,
        bool indirection = false);
    void from_temporary_reassignment(
        LValue const& lhs,
        LValue const& rhs);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    type::semantic::Address instruction_index{ 0 };
    // clang-format on
  private:
    void construct_error(
        type::RValue_Reference message,
        type::RValue_Reference symbol = "");

  public:
    Instructions instructions{};

  private:
    type::Globals globals{};
    ITA::Node hoisted_symbols_;

  public:
    detail::Function::Address_Table address_table{};
    type::Stack stack{};
    Functions functions{};
    Vectors vectors{};
    type::Labels labels{};
};

} // namespace ir

} // namespace credence