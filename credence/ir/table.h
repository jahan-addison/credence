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

#include <algorithm>           // for remove_if, __any_of, __find_if, any_of
#include <array>               // for array
#include <cctype>              // for isspace
#include <credence/assert.h>   // for CREDENCE_ASSERT
#include <credence/ir/ita.h>   // for ITA
#include <credence/map.h>      // for Ordered_Map
#include <credence/symbol.h>   // for Symbol_Table
#include <credence/typeinfo.h> // for LValue, RValue, Data_Type, ...
#include <credence/util.h>     // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <cstddef>             // for size_t
#include <deque>               // for deque
#include <initializer_list>    // for initializer_list
#include <memory>              // for shared_ptr, unique_ptr
#include <optional>            // for nullopt, nullopt_t, optional
#include <set>                 // for set
#include <string>              // for basic_string, string
#include <string_view>         // for basic_string_view, string_view
#include <tuple>               // for tuple
#include <utility>             // for pair
#include <variant>             // for variant

namespace credence {

namespace ir {

// clang-format off
void emit(std::ostream& os,
util::AST_Node const& symbols,
util::AST_Node const& ast);
// clang-format on

std::pair<std::string, std::string> get_rvalue_from_mov_qaudruple(
    ITA::Quadruple const& instruction);

namespace detail {

/**
 * Symbolic ITA vector definition and allocation
 */
struct Vector
{
    using Storage = Ordered_Map<std::string, typeinfo::Data_Type>;
    Vector() = delete;
    explicit Vector(typeinfo::semantic::Address size_of)
        : size(size_of)
    {
    }
    Storage data{};
    int decay_index{ 0 };
    unsigned long size{ 0 };
    static constexpr int max_size{ 1000 };
};

/**
 * Symbolic ITA function stack frame allocation
 */
struct Function
{
    Function() = delete;
    explicit Function(typeinfo::semantic::Label const& label)
        : symbol(label)
    {
    }
    using Address_Table =
        Symbol_Table<typeinfo::semantic::Label, typeinfo::semantic::Address>;
    void set_parameters_from_symbolic_label(
        typeinfo::semantic::Label const& label);
    typeinfo::semantic::Label symbol{};
    typeinfo::Labels labels{};
    typeinfo::Locals locals{};

    constexpr inline bool is_parameter(
        typeinfo::semantic::RValue const& parameter)
    {
        return std::ranges::find(parameters, parameter) !=
               std::ranges::end(parameters);
    }

    typeinfo::Parameters parameters{};
    Ordered_Map<typeinfo::semantic::LValue, typeinfo::semantic::RValue>
        temporary{};
    Address_Table label_address{};
    std::array<typeinfo::semantic::Address, 2> address_location{};

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
 *        * Symbolic stack for ITA::Instruction::CALL
 *      * Function definition map of local labels,
 *        * stack allocation, and depth size
 *      * Vector definition map of vectors
 *        * (arrays) in the global scope
 *      * A set of labels for ITA::Instruction::GOTO jumps
 *
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
    explicit Table(
        ITA::Node const& hoisted_symbols,
        ITA::Instructions& instructions)
        : instructions(instructions)
        , hoisted_symbols_(hoisted_symbols)
    {
    }

    using Table_PTR = std::unique_ptr<Table>;

  private:
    using LValue = typeinfo::semantic::LValue;
    using Type = typeinfo::semantic::Type;
    using RValue = typeinfo::semantic::RValue;
    using Label = typeinfo::semantic::Label;

  private:
    using Function_PTR = std::shared_ptr<detail::Function>;
    using Stack_Frame = std::optional<Function_PTR>;
    using Vector_PTR = std::shared_ptr<detail::Vector>;
    using Functions = std::map<std::string, Function_PTR>;
    using Vectors = std::map<std::string, Vector_PTR>;

  public:
    Stack_Frame stack_frame;
    ITA::Instructions build_from_ita_instructions();
    bool stack_frame_contains_ita_instruction(
        Label name,
        ITA::Instruction inst);
    void build_symbols_from_vector_lvalues();
    void build_vector_definitions_from_globals(Symbol_Table<>& globals);
    RValue from_temporary_lvalue(LValue const& lvalue);
    static Table_PTR build_from_ast(
        ITA::Node const& symbols,
        ITA::Node const& ast);

    /**
     * @brief Get the type from a local in the stack frame
     */
    Type get_type_from_rvalue_data_type(LValue const& lvalue);

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
        return get_type_from_rvalue_data_type(lhs) ==
               get_type_from_rvalue_data_type(rhs);
    }
    inline bool lhs_rhs_type_is_equal(
        LValue const& lhs,
        typeinfo::Data_Type const& rvalue)
    {
        return get_type_from_rvalue_data_type(lhs) == std::get<1>(rvalue);
    }

    /**
     * @brief Check that the type of an lvalue is an integral type
     */
    inline void assert_integral_unary_expression(
        LValue const& lvalue,
        RValue const& rvalue,
        Type const& type)
    {
        if (std::ranges::find(typeinfo::integral_unary, type) ==
            typeinfo::integral_unary.end())
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

    inline typeinfo::Locals& get_stack_frame_symbols()
    {
        return get_stack_frame()->locals;
    }

    void set_stack_frame(Label const& label);

  public:
    typeinfo::Data_Type from_integral_unary_expression(RValue const& lvalue);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    void from_func_start_ita_instruction(Label const& label);
    void from_func_end_ita_instruction();
    void from_call_ita_instruction(Label const& label);
    void from_globl_ita_instruction(Label const& label);
    void from_locl_ita_instruction(ITA::Quadruple const& instruction);
    void from_push_instruction(ITA::Quadruple const& instruction);
    void from_pop_instruction(ITA::Quadruple const& instruction);
    void from_label_ita_instruction(ITA::Quadruple const& instruction);
    void from_mov_ita_instruction(ITA::Quadruple const& instruction);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    typeinfo::Data_Type from_rvalue_unary_expression(
        LValue const& lvalue,
        RValue const& rvalue,
        typeinfo::RValue_Reference unary_operator);
    void from_scaler_symbol_assignment(
        LValue const& lhs,
        LValue const& rhs);
    void from_pointer_or_vector_assignment(
        LValue const& lhs,
        LValue& rhs,
        bool indirection = false);
    void is_boundary_out_of_range(RValue const& rvalue);
    void from_type_invalid_assignment(
        LValue const& lvalue,
        RValue const& rvalue);
    void from_type_invalid_assignment(
        LValue const& lvalue,
        typeinfo::Data_Type const& rvalue);
    void from_trivial_vector_assignment(
        LValue const& lhs,
        typeinfo::Data_Type const& rvalue);
    void safely_reassign_pointers_or_vectors(
        LValue const& lvalue,
        typeinfo::RValue_Reference_Type const& rvalue,
        bool indirection = false);
    void from_temporary_reassignment(
        LValue const& lhs,
        LValue const& rhs);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    typeinfo::semantic::Address instruction_index{ 0 };
    // clang-format on
  private:
    void construct_error(
        typeinfo::RValue_Reference message,
        typeinfo::RValue_Reference symbol = "");

  public:
    ITA::Instructions instructions{};

  private:
    typeinfo::Globals globals{};
    ITA::Node hoisted_symbols_;

  public:
    detail::Function::Address_Table address_table{};
    typeinfo::Stack stack{};
    Functions functions{};
    Vectors vectors{};
    typeinfo::Labels labels{};
};

} // namespace ir

} // namespace credence