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

#include "accessor.h"
#include "types.h"                              // for RValue, Table_Pointer
#include <credence/error.h>                     // for credence_assert, thr...
#include <credence/ir/ita.h>                    // for Instruction
#include <credence/language/literal.h>          // for is_integer_string
#include <credence/object.h>                    // for Object, Function
#include <credence/target/common/memory.h>      // for is_vector_offset
#include <credence/target/common/stack_frame.h> // for Stack_Frame, Locals
#include <credence/types.h>                     // for get_size_from_rvalue...
#include <credence/util.h>                      // for is_numeric, __source__
#include <easyjson.h>                           // for JSON
#include <fmt/format.h>                         // for format
#include <map>                                  // for map
#include <matchit.h>                            // for Or, match, or_, pattern
#include <string>                               // for basic_string, char_t...
#include <tuple>                                // for get, tuple

/****************************************************************************
 * Memory Accessors
 *
 * The platform-agnostic base types of the Memory_Accessor facade. Each target
 * derives its own accessors from them, and the inserters reach the object
 * table, the IR instruction under translation, the stack, the registers, and
 * the literals of the data section through one accessor object.
 *
 *  Example:
 *
 *    main() {
 *      auto x;
 *      x = 5;
 *      print("hello");
 *    }
 *
 *  Becomes, on arm64:
 *
 *    ldr w10, [sp, #16]               // x is on the stack of the device
 *    mov w10, #5
 *    str w10, [sp, #16]
 *    adrp x0, ._L_str1__@PAGE         // the label of the buffer accessor
 *    add x0, x0, ._L_str1__@PAGEOFF
 *    mov w1, #5                       // the size of the buffer accessor
 *    bl _print
 *
 ****************************************************************************/

namespace credence::target::x86_64::assembly {
enum class Operand_Size : std::size_t;
}

namespace credence::target::common::memory {

struct Memory_Accessor::impl
{
    explicit impl(std::shared_ptr<object::Object_Table> obj)
        : objects(std::move(obj))
        , stack_frame(objects)
    {
    }
    impl(const impl& other)
        : objects(other.objects)
        , stack_frame(other.objects)
    {
    }
    std::shared_ptr<object::Object_Table> objects;
    Stack_Frame stack_frame;
};

Memory_Accessor::Memory_Accessor(std::shared_ptr<object::Object_Table> objects)
    : pimpl(std::make_unique<impl>(std::move(objects)))
{
}

Memory_Accessor::~Memory_Accessor() = default;

Stack_Frame& Memory_Accessor::get_frame_in_memory()
{
    return pimpl->stack_frame;
}
const Stack_Frame& Memory_Accessor::get_frame_in_memory() const
{
    return pimpl->stack_frame;
}

struct Buffer_Accessor::impl
{
    impl(Table_Pointer& table)
        : table(table)
        , constant_size_index(0)
    {
    }
    Table_Pointer& table;
    std::size_t constant_size_index;
    std::map<RValue, Label> string_literals;
    std::map<RValue, Label> float_literals;
    std::map<RValue, Label> double_literals;
    std::size_t read_bytes_cache_{ 0 };
};

Buffer_Accessor::Buffer_Accessor(Table_Pointer& table)
    : pimpl(std::make_unique<impl>(table))
{
}

Buffer_Accessor::~Buffer_Accessor() = default;

struct Table_Accessor::impl
{
    impl(Table_Pointer& table, ir::Instructions_PTR& instructions)
        : table_(table)
        , instructions_(instructions)
    {
    }
    Table_Pointer& table_;
    ir::Instructions_PTR& instructions_;
    unsigned int index{};
};

Table_Accessor::Table_Accessor(Table_Pointer& table,
    ir::Instructions_PTR& instructions)
    : pimpl(std::make_unique<impl>(table, instructions))
{
}

Table_Accessor::~Table_Accessor() = default;

/**
 * @brief Get the size of the value returned by the last call
 *
 * A function that returns its parameter returns the argument of the caller,
 * so the size is from the caller's local, or the pointee of the local when
 * it is a pointer
 *
 *  Example:
 *
 *    id(s) { return(s); }
 *    main() { auto x; x = "hello"; print(id(x)); }
 *
 *  Becomes:
 *
 *    get_size_of_return_string(frame)   // 5, the size of "hello"
 */
Size Buffer_Accessor::get_size_of_return_string(Stack_Frame const& stack_frame)
{
    auto& table_ = pimpl->table;
    credence_assert(table_->functions.contains(stack_frame.tail));
    auto& tail_frame = table_->functions.at(stack_frame.tail).get();
    auto return_rvalue = tail_frame.ret->second;

    if (tail_frame.is_parameter(return_rvalue)) {
        credence_assert(
            table_->functions.contains(stack_frame.call_stack.back()));
        auto& last_stack_frame =
            table_->functions.at(stack_frame.call_stack.back()).get();
        if (last_stack_frame.locals.is_pointer(tail_frame.ret->first)) {
            return type::get_size_from_rvalue_data_type(
                type::get_rvalue_datatype_from_string(
                    last_stack_frame.locals.get_pointer_by_name(
                        tail_frame.ret->first)));
        }
        if (type::is_rvalue_data_type(tail_frame.ret->first))
            return type::get_size_from_rvalue_data_type(
                type::get_rvalue_datatype_from_string(tail_frame.ret->first));
        return type::get_size_from_rvalue_data_type(
            last_stack_frame.locals.get_symbol_by_name(tail_frame.ret->first));
    }
    return type::get_size_from_rvalue_data_type(
        type::get_rvalue_datatype_from_string(tail_frame.ret->first));
}

/**
 * @brief Get the size of the value of a local
 *
 * The size of a pointer is the size of its pointee, and a local assigned the
 * return value of a call, `RET`, is the size of the value the callee returns
 */
Size Buffer_Accessor::get_size_in_local_address(LValue const& lvalue,
    Stack_Frame const& stack_frame)
{
    auto& frame = stack_frame.get_stack_frame();
    auto& locals = frame.locals;
    if (locals.is_pointer(lvalue) and
        type::is_rvalue_data_type_string(locals.get_pointer_by_name(lvalue))) {
        return type::get_size_from_rvalue_data_type(
            type::get_rvalue_datatype_from_string(
                locals.get_pointer_by_name(lvalue)));
    }
    if (locals.is_pointer(lvalue)) {
        auto rvalue_address = object::get_rvalue_at_lvalue_object_storage(
            lvalue, frame, pimpl->table->vectors, __source__);
        return type::get_size_from_rvalue_data_type(rvalue_address);
    }
    auto local_symbol = locals.get_symbol_by_name(lvalue);
    auto local_rvalue = type::get_value_from_rvalue_data_type(local_symbol);

    if (local_rvalue == "RET") {
        credence_assert(pimpl->table->functions.contains(stack_frame.tail));
        auto& tail_frame = pimpl->table->functions.at(stack_frame.tail).get();
        return type::get_size_from_rvalue_data_type(
            type::get_rvalue_datatype_from_string(tail_frame.ret->first));
    }
    return type::get_size_from_rvalue_data_type(local_symbol);
}

/**
 * @brief Get the size of a string in the object table at compile-time
 *
 * The size is the length argument of a buffer passed to a library call or
 * syscall. It is resolved from where the string is: the return value of the
 * last call, a local, a dereferenced pointer, an element of a global or
 * local vector, or a string literal.
 *
 *  Example:
 *
 *    mess [2] "too bad", "tough luck";
 *    print(mess[1]);
 *
 *  Becomes:
 *
 *    get_size_of_string_lvalue_buffer_address("mess[1]", frame)   // 10
 */
Size Buffer_Accessor::get_size_of_string_lvalue_buffer_address(
    LValue const& lvalue,
    Stack_Frame const& stack_frame)
{
    auto lhs = type::from_lvalue_offset(lvalue);
    auto offset = type::from_decay_offset(lvalue);
    auto& vectors = pimpl->table->vectors;
    auto& frame = stack_frame.get_stack_frame();

    Operand_Lambda is_global_vector = [&](RValue const& rvalue) {
        auto& table = pimpl->table;
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table->vectors.contains(rvalue_reference) and
               table->globals.is_pointer(rvalue_reference);
    };

    if (lvalue == "RET")
        return get_size_of_return_string(stack_frame);

    if (frame.locals.is_defined(lvalue))
        return get_size_in_local_address(lvalue, stack_frame);

    if (type::is_dereference_expression(lvalue)) {
        return type::get_size_from_rvalue_data_type(
            object::get_rvalue_at_lvalue_object_storage(
                type::get_unary_rvalue_reference(lvalue), frame, vectors));
    }

    if (is_global_vector(lhs)) {
        std::string key{};
        auto& vector = pimpl->table->vectors.at(lhs).get();
        if (util::is_numeric(offset)) {
            key = offset;
        } else {
            auto index = object::get_rvalue_at_lvalue_object_storage(
                offset, frame, vectors, __source__);
            key = std::string{ type::get_value_from_rvalue_data_type(index) };
        }
        return type::get_size_from_rvalue_data_type(vector.data.at(key));
    }

    if (is_vector_offset(lvalue)) {
        std::string key{};
        auto& vector = pimpl->table->vectors.at(lhs).get();
        if (util::is_numeric(offset)) {
            key = offset;
        } else {
            auto index = object::get_rvalue_at_lvalue_object_storage(
                offset, frame, vectors, __source__);
            key = std::string{ type::get_value_from_rvalue_data_type(index) };
        }
        return type::get_size_from_rvalue_data_type(vector.data.at(key));
    }

    if (is_immediate(lvalue))
        return type::get_size_from_rvalue_data_type(
            type::get_rvalue_datatype_from_string(lvalue));
    else
        return type::get_size_from_rvalue_data_type(
            object::get_rvalue_at_lvalue_object_storage(
                lvalue, frame, vectors, __source__));
}

/**
 * @brief Cache the length of the buffer filled by a `read(2)` syscall
 *
 * The buffer of `read(2)` is filled at runtime, so the length of a later
 * `print` of the buffer is the length argument of the read
 *
 *  Example:
 *
 *    read(0, buffer, 10);
 *    print(buffer);
 *
 *  Becomes:
 *
 *    read_bytes()   // 10
 */
void Buffer_Accessor::set_buffer_size_from_syscall(std::string_view routine,
    memory::Locals& argument_stack)
{
    // a syscall with no arguments, such as `getpid`, has no buffer
    if (argument_stack.empty())
        return;
    namespace m = matchit;
    m::match(routine)(m::pattern | m::or_(sv("read")) = [&] {
        auto argument = argument_stack.back();
        if (credence::util::is_numeric(argument))
            pimpl->read_bytes_cache_ = std::stoul(argument);
        else if (type::is_rvalue_data_type_a_type(argument, "int"))
            pimpl->read_bytes_cache_ =
                std::stoul(type::get_value_from_rvalue_data_type(argument));
    });
}

/**
 * @brief Check if a `read(2)` length is cached
 *
 * When it is, the size of the next buffer is the cached length from
 * read_bytes(), otherwise it is from the object table
 */
bool Buffer_Accessor::has_read_bytes()
{
    return pimpl->read_bytes_cache_ != 0UL;
}

/**
 * @brief Take the cached `read(2)` length, which clears it
 */
std::size_t Buffer_Accessor::read_bytes()
{
    auto read_bytes = pimpl->read_bytes_cache_;
    pimpl->read_bytes_cache_ = 0;
    return read_bytes;
}
/**
 * @brief Map a literal to the label of its directive in the data section
 *
 * The data emitter inserts each string, float, and double literal once, and
 * an operand of the literal is loaded from its label
 */
void Buffer_Accessor::insert_string_literal(RValue const& key,
    Label const& asciz_address)
{
    pimpl->string_literals.insert_or_assign(key, asciz_address);
}
void Buffer_Accessor::insert_float_literal(RValue const& key,
    Label const& floatz_address)
{
    pimpl->float_literals.insert_or_assign(key, floatz_address);
}
void Buffer_Accessor::insert_double_literal(RValue const& key,
    Label const& doublez_address)
{
    pimpl->double_literals.insert_or_assign(key, doublez_address);
}
/**
 * @brief Get the label of a literal in the data section
 */
RValue Buffer_Accessor::get_string_address_offset(RValue const& string)
{
    credence_assert(is_allocated_string(string));
    return pimpl->string_literals.at(string);
}
RValue Buffer_Accessor::get_float_address_offset(RValue const& string)
{
    credence_assert(is_allocated_float(string));
    return pimpl->float_literals.at(string);
}
RValue Buffer_Accessor::get_double_address_offset(RValue const& string)
{
    credence_assert(is_allocated_double(string));
    return pimpl->double_literals.at(string);
}
bool Buffer_Accessor::is_allocated_string(RValue const& rvalue)
{
    return pimpl->string_literals.contains(rvalue);
}
bool Buffer_Accessor::is_allocated_float(RValue const& rvalue)
{
    return pimpl->float_literals.contains(rvalue);
}
bool Buffer_Accessor::is_allocated_double(RValue const& rvalue)
{
    return pimpl->double_literals.contains(rvalue);
}
/**
 * @brief The counter of literal labels in the data section, e.g. `._L_str1__`
 */
std::size_t* Buffer_Accessor::get_constant_size_index()
{
    return &pimpl->constant_size_index;
}
void Buffer_Accessor::set_constant_size_index(std::size_t index)
{
    pimpl->constant_size_index = index;
}

/**
 * @brief Set the index of the IR instruction under translation
 */
void Table_Accessor::set_ir_iterator_index(unsigned int index_)
{
    pimpl->index = index_;
}

/**
 * @brief Check if the IR instruction under translation is a MOV to a
 * temporary
 */
bool Table_Accessor::is_ir_instruction_temporary()
{
    return type::is_temporary(
        std::get<1>(pimpl->instructions_->at(pimpl->index)));
}

/**
 * @brief Get the last local assigned at or before an IR instruction
 *
 * Temporaries (`_t`) and argument slots (`_p`) are skipped, so the lvalue is
 * a local of the source program
 *
 *  Example:
 *
 *    x = (5:int:4);
 *    _t1 = x * x;
 *    _p1 = _t1;
 *
 *  Becomes:
 *
 *    get_last_lvalue_assignment(2, frame)   // x
 */
LValue Table_Accessor::get_last_lvalue_assignment(unsigned int index_,
    object::Function& stack_frame)
{
    for (; index_ > 0; index_--) {
        auto ir_inst = pimpl->instructions_->at(index_);
        auto lvalue = std::get<1>(ir_inst);
        if (std::get<0>(ir_inst) == ir::Instruction::MOV and
            object::local_contains(lvalue, stack_frame) and
            not lvalue.starts_with("_p") and not lvalue.starts_with("_t")) {
            return lvalue;
        }
    }
    return "";
}

/**
 * @brief Get the lvalue of the IR instruction under translation
 */
std::string Table_Accessor::get_ir_instruction_lvalue()
{
    return std::get<1>(pimpl->instructions_->at(pimpl->index));
}

/**
 * @brief Check if the last IR instruction is an assignment to a local
 *
 * An assignment is a MOV whose lvalue is not a temporary
 */
bool Table_Accessor::last_ir_instruction_is_assignment()
{
    if (pimpl->index < 1)
        return false;
    auto last = pimpl->instructions_->at(pimpl->index - 1);
    return std::get<0>(last) == ir::Instruction::MOV and
           not type::is_temporary(std::get<1>(last));
}

/**
 * @brief Check if the next IR instruction is a MOV to a temporary
 */
bool Table_Accessor::next_ir_instruction_is_temporary()
{
    if (pimpl->instructions_->size() < pimpl->index + 1)
        return false;
    auto next = pimpl->instructions_->at(pimpl->index + 1);
    return std::get<0>(next) == ir::Instruction::MOV and
           type::is_temporary(std::get<1>(next));
}

/**
 * @brief Check if the next IR instruction is an assignment to a local
 *
 * An address-of expression assigned to a local in the next instruction is
 * stored to it directly
 *
 *  Example:
 *
 *    _t1 = &x;
 *    p = _t1;
 */
bool Table_Accessor::next_ir_instruction_is_assignment()
{
    if (pimpl->instructions_->size() < pimpl->index + 1)
        return false;
    auto last = pimpl->instructions_->at(pimpl->index + 1);
    return std::get<0>(last) == ir::Instruction::MOV and
           not type::is_temporary(std::get<1>(last));
}

Table_Pointer& Table_Accessor::get_table()
{
    return pimpl->table_;
}
const Table_Pointer& Table_Accessor::get_table() const
{
    return pimpl->table_;
}
ir::Instructions& Table_Accessor::get_ir_instructions()
{
    return *pimpl->instructions_;
}
unsigned int Table_Accessor::get_index() const
{
    return pimpl->index;
}

/**
 * @brief Get the offset of a vector element indexed by a local
 *
 * The value of the local is known at compile time from the object table
 *
 *  Example:
 *
 *    x = 1;
 *    y = mess[x];
 *
 *  Becomes:
 *
 *    get_offset_from_hoisted_symbols("mess", "x")   // the offset of mess[1]
 */
template<typename Entry>
auto Vector_Accessor<Entry>::get_offset_from_hoisted_symbols(
    LValue const& vector,
    RValue const& offset) -> Entry_Pair
{
    auto& frame = stack_frame_.get_stack_frame();
    auto& vectors = table_->vectors;
    auto index = object::get_rvalue_at_lvalue_object_storage(
        offset, frame, vectors, __source__);
    auto key = std::string{ type::get_value_from_rvalue_data_type(index) };
    if (!vectors.at(vector).get().data.contains(key))
        throw_compiletime_error(
            fmt::format(
                "Invalid out-of-range index '{}' on vector lvalue", key),
            vector);
    return std::make_pair(vectors.at(vector).get().offset.at(key),
        get_size_from_vector_offset(vectors.at(vector).get().data.at(key)));
}

/**
 * @brief Type check the index of a vector element
 *
 * The index must be an integer literal or a symbol of the program
 */
template<typename Entry>
void Vector_Accessor<Entry>::type_check_invalid_vector_symbol(
    LValue const& vector,
    RValue const& offset)
{
    if (!table_->hoisted_symbols.has_key(offset) and
        not language::literal::is_integer_string(offset))
        throw_compiletime_error(
            fmt::format("Invalid index '{}' on vector lvalue", offset), vector);
}

/**
 * @brief Get the offset of a vector element indexed by an integer literal
 */
template<typename Entry>
auto Vector_Accessor<Entry>::get_offset_from_integer_rvalue(
    LValue const& vector,
    RValue const& offset) -> Entry_Pair
{
    auto& vectors = table_->vectors;
    if (!vectors.at(vector).get().data.contains(offset))
        throw_compiletime_error(
            fmt::format(
                "Invalid out-of-range index '{}' on vector lvalue", offset),
            vector);
    return std::make_pair(vectors.at(vector).get().offset.at(offset),
        get_size_from_vector_offset(vectors.at(vector).get().data.at(offset)));
}

/**
 * @brief Get the offset of a vector with no index, which is its first element
 */
template<typename Entry>
auto Vector_Accessor<Entry>::get_offset_from_trivial_vector(
    LValue const& vector) -> Entry_Pair
{
    return std::make_pair(0UL,
        get_size_from_vector_offset(
            table_->vectors.at(vector).get().data.at("0")));
}

/**
 * @brief Get the offset and size of a vector element from its lvalue and
 * index
 *
 * The index is resolved from a symbol, an integer literal, or the first
 * element when the lvalue has no index
 *
 *  Examples:
 *
 *    get_offset_address("mess", "")       // (0, size of mess[0])
 *    get_offset_address("mess[1]", "1")   // (offset of mess[1], its size)
 */
template<typename Entry>
auto Vector_Accessor<Entry>::get_offset_address(LValue const& lvalue,
    RValue const& offset) -> Entry_Pair
{
    auto& vectors = table_->vectors;
    auto vector = type::from_lvalue_offset(lvalue);

    type_check_invalid_vector_symbol(vector, offset);

    if (!is_vector_offset(lvalue))
        return get_offset_from_trivial_vector(vector);

    if (table_->hoisted_symbols.has_key(offset))
        return get_offset_from_hoisted_symbols(vector, offset);

    if (language::literal::is_integer_string(offset))
        return get_offset_from_integer_rvalue(vector, offset);

    return std::make_pair(0UL,
        get_size_from_vector_offset(vectors.at(vector).get().data.at("0")));
}

template class Vector_Accessor<Size>;
template class Vector_Accessor<x86_64::assembly::Operand_Size>;

} // namespace credence::target::common::memory
