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

#include <credence/ir/object.h>

#include <algorithm>         // for __find_if, find_if
#include <array>             // for array
#include <credence/error.h>  // for assert_nequal_impl, credence_assert
#include <credence/ir/ita.h> // for Instructions, Quadruple
#include <credence/map.h>    // for Ordered_Map
#include <credence/types.h>  // for get_value_from_rvalue_data_type, from_l...
#include <credence/util.h>   // for contains
#include <fmt/format.h>      // for format
#include <matchit.h>         // for App, app, pattern, PatternHelper, Patte...
#include <memory>            // for shared_ptr
#include <source_location>   // for source_location
#include <string>            // for basic_string, char_traits, operator==
#include <string_view>       // for basic_string_view
#include <tuple>             // for tuple, get

/****************************************************************************
 * Objects
 *
 * Object table and storage system for the intermediate representation. This
 * tracks vectors, functions, stack frames, and symbol tables throughout
 * the backend.
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

namespace credence::ir::object {

/**
 * @brief Vector PIMPL implementation
 */
struct Vector::Vector_IMPL
{
    Storage data{};
    Offset offset{};
    int decay_index{ 0 };
    std::size_t size{ 0 };
    Label symbol{};
};

Vector::Vector(Label label, Address size_of)
    : pimpl{ std::make_unique<Vector_IMPL>() }
{
    pimpl->size = size_of;
    pimpl->symbol = std::move(label);
}

Vector::~Vector() = default;

Vector::Vector(Vector&&) noexcept = default;

Vector& Vector::operator=(Vector&&) noexcept = default;

Vector::Storage& Vector::get_data()
{
    return pimpl->data;
}
Vector::Storage const& Vector::get_data() const
{
    return pimpl->data;
}
Vector::Offset& Vector::get_offset()
{
    return pimpl->offset;
}
Vector::Offset const& Vector::get_offset() const
{
    return pimpl->offset;
}
void Vector::set_address_offset(Label const& index, Address address)
{
    pimpl->offset.insert(index, address);
}
int& Vector::get_decay_index()
{
    return pimpl->decay_index;
}
int Vector::get_decay_index() const
{
    return pimpl->decay_index;
}
std::size_t& Vector::get_size()
{
    return pimpl->size;
}
std::size_t Vector::get_size() const
{
    return pimpl->size;
}
Vector::Label& Vector::get_symbol()
{
    return pimpl->symbol;
}
Vector::Label const& Vector::get_symbol() const
{
    return pimpl->symbol;
}

/**
 * @brief Function PIMPL implementation
 */
struct Function::Function_IMPL
{
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
    type::Pointers pointers{};
    unsigned int allocation = 0;
};

Function::Function(type::semantic::Label const& label)
    : pimpl{ std::make_unique<Function_IMPL>() }
{
    pimpl->symbol = label;
}

Function::~Function() = default;

Function::Function(Function&&) noexcept = default;

Function& Function::operator=(Function&&) noexcept = default;

/**
 * @brief Parse ITA function parameters into locals on the frame stack
 *
 * e.g. `__convert(s,v,*k) = (s,v,*k)`
 */
void Function::set_parameters_from_symbolic_label(Label const& label)
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
                pimpl->parameters.emplace_back(parameter);
                parameter = "";
            }
        }
    }
}

bool Function::is_pointer_in_stack_frame(RValue const& rvalue)
{
    return pimpl->locals.is_pointer(rvalue) or is_pointer_parameter(rvalue);
}

bool Function::is_pointer_parameter(RValue const& parameter)
{
    using namespace fmt::literals;
    return util::range_contains(
        fmt::format("*{}"_cf, parameter), pimpl->parameters);
}

bool Function::is_scaler_parameter(RValue const& parameter)
{
    return util::range_contains(
        type::from_lvalue_offset(parameter), pimpl->parameters);
}

bool Function::is_parameter(RValue const& parameter)
{
    return is_scaler_parameter(parameter) or is_pointer_parameter(parameter);
}

int Function::get_index_of_parameter(RValue const& parameter)
{
    for (std::size_t i = 0; i < pimpl->parameters.size(); i++) {
        if (type::get_unary_rvalue_reference(pimpl->parameters[i]) ==
            type::from_lvalue_offset(parameter))
            return i;
    }
    return -1;
}

Function::Return_RValue& Function::get_ret()
{
    return pimpl->ret;
}
Function::Return_RValue const& Function::get_ret() const
{
    return pimpl->ret;
}
Label& Function::get_label_before_reserved()
{
    return pimpl->label_before_reserved;
}
Label const& Function::get_label_before_reserved() const
{
    return pimpl->label_before_reserved;
}
type::Parameters& Function::get_parameters()
{
    return pimpl->parameters;
}
type::Parameters const& Function::get_parameters() const
{
    return pimpl->parameters;
}
Ordered_Map<LValue, RValue>& Function::get_temporary()
{
    return pimpl->temporary;
}
Ordered_Map<LValue, RValue> const& Function::get_temporary() const
{
    return pimpl->temporary;
}
Function::Address_Table& Function::get_label_address()
{
    return pimpl->label_address;
}
Function::Address_Table const& Function::get_label_address() const
{
    return pimpl->label_address;
}
std::array<type::semantic::Address, 2>& Function::get_address_location()
{
    return pimpl->address_location;
}
std::array<type::semantic::Address, 2> const& Function::get_address_location()
    const
{
    return pimpl->address_location;
}
type::semantic::Label& Function::get_symbol()
{
    return pimpl->symbol;
}
type::semantic::Label const& Function::get_symbol() const
{
    return pimpl->symbol;
}
type::Labels& Function::get_labels()
{
    return pimpl->labels;
}
type::Labels const& Function::get_labels() const
{
    return pimpl->labels;
}
type::Locals& Function::get_locals()
{
    return pimpl->locals;
}
type::Locals const& Function::get_locals() const
{
    return pimpl->locals;
}
type::RValues& Function::get_tokens()
{
    return pimpl->tokens;
}
type::RValues const& Function::get_tokens() const
{
    return pimpl->tokens;
}

type::Pointers& Function::get_pointers()
{
    return pimpl->pointers;
}

type::Pointers const& Function::get_pointers() const
{
    return pimpl->pointers;
}

unsigned int& Function::get_allocation()
{
    return pimpl->allocation;
}
unsigned int Function::get_allocation() const
{
    return pimpl->allocation;
}

/**
 * @brief Object PIMPL implementation
 */
struct Object::Object_IMPL
{
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

Object::Object()
    : pimpl{ std::make_unique<Object_IMPL>() }
{
}

Object::~Object() = default;

Object::Object(Object&&) noexcept = default;

Object& Object::operator=(Object&&) noexcept = default;

bool Object::is_stack_frame()
{
    return !pimpl->stack_frame_symbol.empty();
}
void Object::set_stack_frame(Label const& label)
{
    pimpl->stack_frame_symbol = label;
}
void Object::reset_stack_frame()
{
    pimpl->stack_frame_symbol = std::string{};
}

Instruction_PTR& Object::get_ir_instructions()
{
    return pimpl->ir_instructions;
}
Instruction_PTR const& Object::get_ir_instructions() const
{
    return pimpl->ir_instructions;
}
util::AST_Node& Object::get_hoisted_symbols()
{
    return pimpl->hoisted_symbols;
}
util::AST_Node const& Object::get_hoisted_symbols() const
{
    return pimpl->hoisted_symbols;
}
Symbol_Table<>& Object::get_globals()
{
    return pimpl->globals;
}
Symbol_Table<> const& Object::get_globals() const
{
    return pimpl->globals;
}
Function::Address_Table& Object::get_address_table()
{
    return pimpl->address_table;
}
Function::Address_Table const& Object::get_address_table() const
{
    return pimpl->address_table;
}
std::string& Object::get_stack_frame_symbol()
{
    return pimpl->stack_frame_symbol;
}
std::string const& Object::get_stack_frame_symbol() const
{
    return pimpl->stack_frame_symbol;
}
Stack& Object::get_stack()
{
    return pimpl->stack;
}
Stack const& Object::get_stack() const
{
    return pimpl->stack;
}
Functions& Object::get_functions()
{
    return pimpl->functions;
}
Functions const& Object::get_functions() const
{
    return pimpl->functions;
}
Vectors& Object::get_vectors()
{
    return pimpl->vectors;
}
Vectors const& Object::get_vectors() const
{
    return pimpl->vectors;
}
type::RValues& Object::get_strings()
{
    return pimpl->strings;
}
type::RValues const& Object::get_strings() const
{
    return pimpl->strings;
}
type::RValues& Object::get_floats()
{
    return pimpl->floats;
}
type::RValues const& Object::get_floats() const
{
    return pimpl->floats;
}
type::RValues& Object::get_doubles()
{
    return pimpl->doubles;
}
type::RValues const& Object::get_doubles() const
{
    return pimpl->doubles;
}
type::Labels& Object::get_labels()
{
    return pimpl->labels;
}
type::Labels const& Object::get_labels() const
{
    return pimpl->labels;
}

/**
 * @brief Vector_Offset PIMPL implementation
 */
namespace detail {

struct Vector_Offset::Vector_Offset_IMPL
{
    object::Function_PTR& stack_frame_;
    object::Vectors& vectors_;

    Vector_Offset_IMPL(object::Function_PTR& stack_frame,
        object::Vectors& vectors)
        : stack_frame_(stack_frame)
        , vectors_(vectors)
    {
    }
};

Vector_Offset::Vector_Offset(object::Function_PTR& stack_frame,
    object::Vectors& vectors)
    : pimpl{ std::make_unique<Vector_Offset_IMPL>(stack_frame, vectors) }
{
}

Vector_Offset::~Vector_Offset() = default;

Vector_Offset::Vector_Offset(Vector_Offset&&) noexcept = default;

Vector_Offset& Vector_Offset::operator=(Vector_Offset&&) noexcept = default;

/**
 * @brief Get the rvalue at the address of an offset in memory
 */
RValue Vector_Offset::get_rvalue_offset_of_vector(RValue const& offset)
{
    // clang-format off
    return pimpl->stack_frame_->get_locals().is_defined(offset)
    ? type::get_value_from_rvalue_data_type(
        get_rvalue_at_lvalue_object_storage(
            offset, pimpl->stack_frame_, pimpl->vectors_))
    : offset;
    // clang-format on
}

/**
 * @brief Check that the offset rvalue is a valid address in the vector
 */
bool Vector_Offset::is_valid_vector_address_offset(LValue const& lvalue)
{
    auto lvalue_reference = type::get_unary_rvalue_reference(lvalue);
    auto address = type::from_lvalue_offset(lvalue_reference);
    auto offset = type::from_decay_offset(lvalue_reference);
    if (pimpl->stack_frame_->is_parameter(offset)) {
        return true;
    }
    return pimpl->vectors_.at(address)->get_data().contains(
        get_rvalue_offset_of_vector(offset));
}

} // namespace detail

/**
 * @brief Pattern matching helpers
 */
bool Object::vector_contains(type::semantic::LValue const& lvalue)
{
    return pimpl->vectors.contains(lvalue);
}
bool Object::local_contains(type::semantic::LValue const& lvalue)
{
    const auto& locals = get_stack_frame_symbols();
    return locals.is_defined(lvalue) and not is_vector_lvalue(lvalue);
}

/**
 * @brief Stack frame helpers
 */
Function_PTR Object::get_stack_frame()
{
    credence_assert(pimpl->functions.contains(pimpl->stack_frame_symbol));
    return pimpl->functions.at(pimpl->stack_frame_symbol);
}
Function_PTR Object::get_stack_frame(Label const& label)
{
    credence_assert(pimpl->functions.contains(label));
    return pimpl->functions.at(label);
}
type::Locals& Object::get_stack_frame_symbols()
{
    return get_stack_frame()->get_locals();
}

/**
 * @brief Resolve the rvalue of a pointer in the table object and stack frame
 */
type::Data_Type get_rvalue_at_lvalue_object_storage(LValue const& lvalue,
    object::Function_PTR& stack_frame,
    object::Vectors& vectors,
    std::source_location const& location)
{
    auto& locals = stack_frame->get_locals();
    auto lvalue_reference = type::get_unary_rvalue_reference(lvalue);

    if (lvalue_reference == "RET")
        return type::NULL_RVALUE_LITERAL;
    if (locals.is_pointer(lvalue_reference)) {
        auto address_at =
            locals.get_pointer_by_name(lvalue_reference, location);
        if (address_at == "NULL")
            return type::NULL_RVALUE_LITERAL;
        return get_rvalue_at_lvalue_object_storage(
            address_at, stack_frame, vectors, location);
    }
    if (vectors.contains(type::from_lvalue_offset(lvalue_reference))) {
        auto vector_offset = detail::Vector_Offset{ stack_frame, vectors };
        auto address = type::from_lvalue_offset(lvalue_reference);
        auto offset = type::from_decay_offset(lvalue_reference);
        auto offset_rvalue = vector_offset.get_rvalue_offset_of_vector(offset);
        if (stack_frame->is_parameter(offset)) {
            return type::Data_Type{ lvalue, "word", 8UL };
        }
        if (!vector_offset.is_valid_vector_address_offset(lvalue))
            throw_compiletime_error(
                fmt::format("lvalue '{}' is not a vector with offset '{}' with "
                            "and storage of '{}'",
                    address,
                    offset,
                    offset_rvalue),
                lvalue,
                location);
        return vectors.at(address)->get_data()[offset_rvalue];
    }
    if (type::is_rvalue_data_type(lvalue))
        return type::get_rvalue_datatype_from_string(lvalue);

    return locals.get_symbol_by_name(lvalue_reference, location);
}

/**
 * @brief Resolve the rvalue at a temporary storage address in the object table
 */
RValue Object::lvalue_at_temporary_object_address(LValue const& lvalue,
    Function_PTR const& stack_frame)
{
    auto rvalue = type::is_temporary(lvalue) or util::contains(lvalue, "_p")
                      ? stack_frame->get_temporary()[lvalue]
                      : lvalue;
    if (util::contains(rvalue, "_t") and util::contains(rvalue, " ")) {
        return rvalue;
    } else {
        if (util::contains(rvalue, "_t") or util::contains(lvalue, "_p"))
            return lvalue_at_temporary_object_address(rvalue, stack_frame);
        else
            return rvalue;
    }
}

/**
 * @brief Resolve the size of an rvalue data type that is NOT a word literal
 */
Size Object::get_symbol_size_from_rvalue_data_type(LValue const& lvalue,
    Function_PTR const& stack_frame)
{
    auto& locals = stack_frame->get_locals();
    credence_assert(locals.is_defined(lvalue));
    auto datatype = locals.get_symbol_by_name(lvalue);
    if (type::is_rvalue_data_type_word(datatype))
        return lvalue_size_at_temporary_object_address(
            type::get_value_from_rvalue_data_type(datatype), stack_frame);
    else
        return type::get_size_from_rvalue_data_type(
            locals.get_symbol_by_name(lvalue));
}

/**
 * @brief Resolve the size at a temporary storage address in the object table
 */
Size Object::lvalue_size_at_temporary_object_address(LValue const& lvalue,
    Function_PTR const& stack_frame)
{
    auto rvalue = lvalue_at_temporary_object_address(lvalue, stack_frame);
    auto& locals = stack_frame->get_locals();

    if (type::is_rvalue_data_type(rvalue) and
        not type::is_rvalue_data_type_word(rvalue))
        return type::get_size_from_rvalue_data_type(rvalue);
    if (type::is_unary_expression(rvalue))
        return lvalue_size_at_temporary_object_address(
            type::get_unary_rvalue_reference(rvalue), stack_frame);
    if (type::is_binary_expression(rvalue)) {
        auto [left, right, op] = type::from_rvalue_binary_expression(rvalue);
        if (type::is_rvalue_data_type(left) and
            not type::is_rvalue_data_type_word(left))
            return type::get_size_from_rvalue_data_type(left);
        if (type::is_rvalue_data_type(right) and
            not type::is_rvalue_data_type_word(right))
            return type::get_size_from_rvalue_data_type(right);
        if (locals.is_defined(left) and not locals.is_pointer(left)) {
            auto datatype = locals.get_symbol_by_name(left);
            if (type::is_rvalue_data_type_word(datatype))
                return lvalue_size_at_temporary_object_address(
                    type::get_value_from_rvalue_data_type(datatype),
                    stack_frame);
            else
                return type::get_size_from_rvalue_data_type(
                    locals.get_symbol_by_name(left));
        }
        if (locals.is_defined(right) and not locals.is_pointer(right)) {
            auto datatype = locals.get_symbol_by_name(right);
            if (type::is_rvalue_data_type_word(datatype))
                return lvalue_size_at_temporary_object_address(
                    type::get_value_from_rvalue_data_type(datatype),
                    stack_frame);
            else
                return type::get_size_from_rvalue_data_type(
                    locals.get_symbol_by_name(right));
        }
    }
    if (type::is_temporary_datatype_binary_expression(rvalue)) {
        auto [left, right, op] = type::from_rvalue_binary_expression(rvalue);
        return lvalue_size_at_temporary_object_address(left, stack_frame);
    }
    if (locals.is_defined(rvalue)) {
        auto datatype = locals.get_symbol_by_name(rvalue);
        if (type::is_rvalue_data_type_word(datatype))
            return lvalue_size_at_temporary_object_address(
                type::get_value_from_rvalue_data_type(datatype), stack_frame);
        else
            return type::get_size_from_rvalue_data_type(
                locals.get_symbol_by_name(rvalue));
    }
    credence_error("unreachable");
    return 0UL;
}

Size Object::get_size_of_temporary_binary_rvalue(RValue const& rvalue,
    Function_PTR const& stack_frame)
{
    Size size = 0UL;
    auto temp_side = type::is_temporary_operand_binary_expression(rvalue);
    auto [left, right, _] = type::from_rvalue_binary_expression(rvalue);
    if (type::is_temporary(left) and type::is_temporary(right))
        return lvalue_size_at_temporary_object_address(left, stack_frame);
    if (temp_side == "left")
        if (!type::is_rvalue_data_type(right))
            size = lvalue_size_at_temporary_object_address(right, stack_frame);
        else
            size = type::get_size_from_rvalue_data_type(right);
    else {
        if (!type::is_rvalue_data_type(left))
            size = lvalue_size_at_temporary_object_address(left, stack_frame);
        else
            size = type::get_size_from_rvalue_data_type(left);
    }
    credence_assert_nequal(size, 0UL);
    return size;
}

/**
 * @brief Search the ir instructions in a stack frame for a instruction
 */
bool Object::stack_frame_contains_call_instruction(Label name,
    ir::Instructions const& instructions)
{
    credence_assert(pimpl->functions.contains(name));
    auto frame = pimpl->functions.at(name);
    auto search = std::ranges::find_if(
        instructions.begin() + frame->get_address_location()[0],
        instructions.begin() + frame->get_address_location()[1],
        [&](ir::Quadruple const& quad) {
            return std::get<0>(quad) == Instruction::CALL;
        });
    return search != instructions.begin() + frame->get_address_location()[1];
}

/**
 * @brief Set the stack frame return value in the table
 */
void set_stack_frame_return_value(RValue const& rvalue,
    Function_PTR& frame,
    Object_PTR& objects)
{
    auto is_vector_lvalue = [&](LValue const& lvalue) {
        return util::contains(lvalue, "[") and util::contains(lvalue, "]");
    };
    auto local_contains = [&](LValue const& lvalue) {
        const auto& locals = frame->get_locals();
        return locals.is_defined(lvalue) and not is_vector_lvalue(lvalue);
    };
    auto is_pointer_ = [&](RValue const& value) {
        return frame->get_locals().is_pointer(value);
    };
    auto is_parameter = [&](RValue const& value) {
        return frame->is_parameter(value);
    };
    namespace m = matchit;
    m::match(rvalue)(
        m::pattern | m::app(is_pointer_, true) =
            [&] {
                frame->get_ret() = std::make_pair(
                    frame->get_locals().get_pointer_by_name(rvalue), rvalue);
            },
        m::pattern | m::app(is_parameter, true) =
            [&] {
                if (objects->get_stack().empty()) {
                    frame->get_ret() = std::make_pair("NULL", rvalue);
                    return;
                }
                auto index_of = frame->get_index_of_parameter(rvalue);
                credence_assert_nequal(index_of, -1);
                frame->get_ret() =
                    std::make_pair(objects->get_stack().at(index_of), rvalue);
            },
        m::pattern | m::app(local_contains, true) =
            [&] {
                auto value_at = type::get_value_from_rvalue_data_type(
                    frame->get_locals().get_symbol_by_name(rvalue));
                frame->get_ret() = std::make_pair(value_at, rvalue);
            },
        m::pattern | m::app(type::is_rvalue_data_type, true) =
            [&] {
                auto datatype = type::get_rvalue_datatype_from_string(rvalue);
                auto value_at = type::get_value_from_rvalue_data_type(datatype);
                frame->get_ret() = std::make_pair(value_at, rvalue);
            },
        m::pattern | m::app(is_vector_lvalue, true) =
            [&] {
                auto value_at = get_rvalue_at_lvalue_object_storage(
                    rvalue, frame, objects->get_vectors(), __source__);
                frame->get_ret() = std::make_pair(
                    type::get_value_from_rvalue_data_type(value_at), rvalue);
            },
        m::pattern | m::app(type::is_temporary, true) =
            [&] {
                frame->get_ret() =
                    std::make_pair(frame->get_temporary().at(rvalue), rvalue);
            },
        m::pattern | m::_ = [&] { credence_error("unreachable"); }

    );
}
}
