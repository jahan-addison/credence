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
    using RValue_Data_Type = std::tuple<RValue, Type, Size>;
    using RValue_Reference = std::string_view;
    using RValue_Reference_Type = std::variant<RValue, RValue_Data_Type>;
    using ITA_Table = std::unique_ptr<Table>;
    using Address_Table = Symbol_Table<Label, Address>;
    using Symbolic_Stack = std::deque<RValue>;
    using Locals = Symbol_Table<RValue_Data_Type, LValue>;
    using Temporary = std::pair<LValue, RValue>;
    using Parameters = std::vector<std::string>;
    using Labels = std::set<Label>;
    using Globals = type::Value_Pointer;

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
            RValue_Reference label)
        {
            return Label{ label.begin() + 2,
                          label.begin() + label.find_first_of("(") };
        }
        constexpr inline bool is_parameter(RValue parameter)
        {
            return std::ranges::find(parameters, parameter) !=
                   std::ranges::end(parameters);
        }

        std::shared_ptr<Locals> locals = std::make_shared<Locals>();

        Label symbol{};
        Labels labels{};
        Parameters parameters{};
        Address_Table label_address{};
        static constexpr int max_depth{ 1000 };
        std::array<Address, 2> address_location{};
        unsigned int allocation{ 0 };
        std::map<LValue, RValue> temporary{};
    };

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    /**
     * Symbolic ITA vector definition and allocation
     */
    struct Vector
    {
        using Storage = std::map<std::string, RValue_Data_Type>;
        Vector() = delete;
        explicit Vector(Address size_of)
            : size(size_of)
        {
        }
        Storage data{};
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

  public:
    ITA::Instructions build_from_ita_instructions();
    void build_symbols_from_vector_lvalues();
    void build_vector_definitions_from_globals(Symbol_Table<>& globals);
    RValue from_temporary_lvalue(LValue const& lvalue);
    static ITA_Table build_from_ast(
        ITA::Node const& symbols,
        ITA::Node const& ast);

  public:
    constexpr static auto UNARY_TYPES = { "++", "--", "*", "&", "-",
                                          "+",  "~",  "!", "~" };

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    static constexpr RValue_Data_Type NULL_RVALUE_LITERAL =
        RValue_Data_Type{ "NULL", "null", sizeof(void*) };
    using Binary_Expression = std::tuple<std::string, std::string, std::string>;

    /**
     * @brief is ITA unary
     */
    constexpr inline bool is_unary(RValue_Reference rvalue)
    {
        return std::ranges::any_of(UNARY_TYPES, [&](std::string_view x) {
            return rvalue.starts_with(x) or rvalue.ends_with(x);
        });
    }
    /**
     * @brief is vector, pointer, or address-of rvalue
     */
    /**
     * @brief Get unary operator from ITA rvalue string
     */
    constexpr inline RValue_Reference get_unary(RValue_Reference rvalue)
    {
        auto it = std::ranges::find_if(UNARY_TYPES, [&](std::string_view op) {
            return rvalue.find(op) != std::string_view::npos;
        });
        if (it == UNARY_TYPES.end())
            return "";
        return *it;
    } /**
       * @brief Get unary lvalue from ITA rvalue string
       */
    constexpr inline LValue get_unary_rvalue_reference(RValue_Reference rvalue, std::string_view unary_chracters = "+-*&+~!")
    {
        auto lvalue = std::string{ rvalue.begin(), rvalue.end() };
        lvalue.erase(
            std::remove_if(
                lvalue.begin(),
                lvalue.end(),
                [&](char ch) {
                    return ::isspace(ch) or
                           unary_chracters.find(ch) != std::string_view::npos;
                }),
            lvalue.end());
        return lvalue;
    }
    // clang-format on
    inline bool is_vector_or_pointer(RValue const& rvalue)
    {
        auto locals = get_stack_frame_symbols();
        return vectors.contains(rvalue) or util::contains(rvalue, "[") or
               locals->is_pointer(rvalue) or rvalue.starts_with("&");
    }
    /**
     * @brief Get the type from a local in the stack frame
     */
    Type get_type_from_local_symbol(LValue const& lvalue);
    inline Type get_type_from_local_symbol(RValue_Data_Type const& rvalue)
    {
        return std::get<1>(rvalue);
    }
    /** Left-hand-side and right-hand-side type equality check */
    inline bool lhs_rhs_type_is_equal(LValue const& lhs, LValue const& rhs)
    {
        return get_type_from_local_symbol(lhs) ==
               get_type_from_local_symbol(rhs);
    }
    inline bool lhs_rhs_type_is_equal(
        LValue const& lhs,
        RValue_Data_Type const& rvalue)
    {
        return get_type_from_local_symbol(lhs) == std::get<1>(rvalue);
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
    /**
     * @brief Get the integer or rvalue reference offset
     *   * v[20]        = 20
     *   * sidno[errno] = errno
     */
    constexpr inline RValue from_pointer_offset(RValue_Reference rvalue)
    {
        return std::string{ rvalue.begin() + rvalue.find_first_of("[") + 1,
                            rvalue.begin() + rvalue.find_first_of("]") };
    }
    /**
     * @brief Get the lvalue from a vector or pointer offset
     *
     *   * v[19]        = v
     *   * sidno[errno] = sidno
     *
     */
    constexpr inline RValue from_lvalue_offset(RValue_Reference rvalue)
    {
        return std::string{ rvalue.begin(),
                            rvalue.begin() + rvalue.find_first_of("[") };
    }

    constexpr inline bool is_stack_frame() { return stack_frame.has_value(); }
    inline Function_PTR& get_stack_frame()
    {
        CREDENCE_ASSERT(is_stack_frame());
        return stack_frame.value();
    }
    inline std::shared_ptr<Locals> get_stack_frame_symbols()
    {
        return get_stack_frame()->locals;
    }

  private:
    void construct_error(
        RValue_Reference message,
        RValue_Reference symbol = "");

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
    void from_variable_ita_instruction(ITA::Quadruple const& instruction);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    RValue_Data_Type from_rvalue_unary_expression(
        LValue const& lvalue,
        RValue const& rvalue,
        RValue_Reference unary_operator);
    void from_scaler_symbol_assignment(LValue const& lhs, LValue const& rhs);
    void from_pointer_or_vector_assignment(
        LValue const& lhs,
        LValue& rhs,
        bool indirection = false);
    void is_boundary_out_of_range(RValue const& rvalue);
    void from_type_invalid_assignment(LValue const& lvalue, RValue const& rvalue);
    void from_type_invalid_assignment(
        LValue const& lvalue,
        RValue_Data_Type const& rvalue);
    void from_trivial_vector_assignment(
        LValue const& lhs,
        RValue_Data_Type const& rvalue);
    void safely_reassign_pointers_or_vectors(
        LValue const& lvalue,
        RValue_Reference_Type const& rvalue,
        bool indirection = false);
    Binary_Expression from_rvalue_binary_expression(RValue const& rvalue);
    void from_temporary_reassignment(LValue const& lhs, LValue const& rhs);
    RValue_Data_Type from_integral_unary_expression(RValue const& lvalue);
    std::pair<std::string, std::string> get_rvalue_from_variable_instruction(
        ITA::Quadruple const& instruction);
    RValue_Data_Type static get_rvalue_symbol_type_size(
        std::string const& datatype);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Address instruction_index{ 0 };
    // clang-format on

  public:
    ITA::Instructions instructions{};

  private:
    Globals globals{};
    Stack_Frame stack_frame{ std::nullopt };
    ITA::Node hoisted_symbols_;

  public:
    Address_Table address_table{};
    Symbolic_Stack stack{};
    Functions functions{};
    Vectors vectors{};
    Labels labels{};
};

} // namespace ir

} // namespace credence