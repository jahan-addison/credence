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
#include <credence/symbol.h> // for Symbol_Table
#include <credence/util.h>   // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <cstddef>           // for size_t
#include <deque>             // for deque
#include <initializer_list>  // for initializer_list
#include <map>               // for map
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

/**
 * @brief
 * A function, symbolic label, and address table for ITA instruction
 * sets. Provides instruction location maps, stack frame and stack allocation
 * sizes as a pre-selection pass for platform code generation
 *
 *  - Table::Address_Table address_table
 *      * Instruction address table to functions in the ITA
 *
 *  - Table::Symbolic_Stack stack
 *      * Symbolic stack for ITA::Instruction::CALL
 *
 *  - Table::Functions functions
 *      * Function definition map of local labels,
 *      * stack allocation, and depth size
 *
 *  - Table::Vectors vectors
 *      * Vector definition map of vectors
 *      * (arrays) in the global scope
 *
 *  - Table::Labels labels
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

  public:
    using Label = std::string;
    using Type = std::string;
    using Address = std::size_t;
    using Size = std::size_t;
    using LValue = std::string;
    using RValue = std::string;
    using ITA_Table = std::unique_ptr<Table>;
    using Address_Table = Symbol_Table<Label, Address>;
    using Symbolic_Stack = std::deque<RValue>;
    using Temporary = std::pair<LValue, RValue>;
    using Parameters = std::vector<std::string>;
    using Labels = std::set<Label>;
    using Locals = std::set<std::string>;

  private:
    /**
     * Symbolic ITA function stack frame allocation
     */
    struct Function
    {
        explicit Function(Label const& label)
            : symbol(label)
        {
        }
        void set_parameters_from_symbolic_label(Label const& label);
        static constexpr Label get_label_as_human_readable(
            std::string_view label)
        {
            return Label{ label.begin() + 2,
                          label.begin() + label.find_first_of("(") };
        }
        constexpr inline bool is_parameter(RValue parameter)
        {
            return std::ranges::find(parameters, parameter) !=
                   std::ranges::end(parameters);
        }
        Label symbol{};
        Labels labels{};
        Locals locals{};
        Parameters parameters{};
        Address_Table label_address{};
        static constexpr int max_depth{ 1000 };
        std::array<Address, 2> address_location{};
        unsigned int allocation{ 0 };
        std::map<LValue, RValue> temporary{};
    };

  private:
    /**
     * Symbolic ITA vector definition and allocation
     */
    struct Vector
    {
        Vector() = delete;
        explicit Vector(Address size_of)
            : size(size_of)
        {
        }
        std::map<Address, RValue> data{};
        int decay_index{ 0 };
        unsigned long size{ 0 };
        static constexpr int max_size{ 1000 };
    };

  private:
    using Function_PTR = std::shared_ptr<Function>;
    using Vector_PTR = std::shared_ptr<Vector>;
    using Functions = std::map<std::string, Function_PTR>;
    using Vectors = std::map<std::string, Vector_PTR>;
    using Stack_Frame = std::optional<Function_PTR>;
    using Binary_Expression = std::tuple<std::string, std::string, std::string>;
    using RValue_Data_Type = std::tuple<RValue, Type, Size>;

  public:
    ITA::Instructions from_ita_instructions();
    static ITA_Table from_ast(ITA::Node const& symbols, ITA::Node const& ast);

  public:
    constexpr static auto UNARY_TYPES = { "++", "--", "*", "&", "-",
                                          "+",  "~",  "!", "~" };

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    constexpr bool is_unary(std::string_view rvalue);
    constexpr std::string_view get_unary(std::string_view rvalue);
    constexpr LValue get_unary_lvalue(std::string& lvalue);

    void construct_error(std::string_view message, std::string_view symbol);

    constexpr inline bool is_stack_frame() { return stack_frame.has_value(); }

    inline Function_PTR get_stack_frame()
    {
        CREDENCE_ASSERT(is_stack_frame());
        return stack_frame.value();
    }

    private:
    void build_symbols_from_vector_definitions();

  CREDENCE_PRIVATE_UNLESS_TESTED:
    void from_func_start_ita_instruction(ITA::Instructions const& instructions);
    void from_func_end_ita_instruction();
    void from_push_instruction(ITA::Quadruple const& instruction);
    void from_pop_instruction(ITA::Quadruple const& instruction);
    void from_label_ita_instruction(ITA::Quadruple const& instruction);
    void from_variable_ita_instruction(ITA::Quadruple const& instruction);

 CREDENCE_PRIVATE_UNLESS_TESTED:
    RValue_Data_Type from_rvalue_unary_expression(
        LValue const& lvalue,
        RValue& rvalue,
        std::string_view unary_operator);
    void from_symbol_reassignment(LValue const& lhs, LValue const& rhs);
    void from_pointer_assignment(LValue const& lhs, LValue const& rhs);
    void vector_out_of_range_check(RValue const& rvalue);
    RValue from_temporary(LValue const& lvalue);
    RValue from_lvalue_offset(RValue const& rvalue);
    RValue from_pointer_offset(RValue const& rvalue);
    Binary_Expression from_rvalue_binary_expression(RValue const& rvalue);
    void from_temporary_assignment(LValue const& lhs, LValue const& rhs);
    RValue_Data_Type from_integral_unary_expression(RValue const& lvalue);
    RValue_Data_Type static get_rvalue_symbol_type_size(
        std::string const& datatype);

   CREDENCE_PRIVATE_UNLESS_TESTED:
    Symbol_Table<RValue_Data_Type, LValue> symbols_{};
    // clang-format on

  private:
    ITA::Instructions instructions{};
    Stack_Frame stack_frame{ std::nullopt };
    Address instruction_index{ 0 };
    ITA::Node hoisted_symbols_;

  public:
    Address_Table address_table{};
    Symbolic_Stack stack{};
    Functions functions{};
    Vectors vectors{};
    Labels labels{};
};

inline void emit_ita_instructions_from_ast(
    std::ostream& os,
    ITA::Node const& symbols,
    ITA::Node const& ast)
{
    ITA::emit(os, Table::from_ast(symbols, ast)->from_ita_instructions());
}

} // namespace ir

} // namespace credence