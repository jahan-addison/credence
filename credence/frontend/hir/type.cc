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

#include <credence/frontend/hir/type.h>

#include <fmt/format.h> // for format

/****************************************************************************
 *
 * Type table
 *
 * The type of every HIR node is a Type_Index into one table and not a
 * string. Two types are the same when their handles are equal, so a type
 * comparison is a uint32_t compare and each distinct type is stored once.
 *
 * The primitives are seeded in a fixed order, so their handles are compile
 * time constants and cost no lookup:
 *
 *    word int char byte long float double bool null
 *
 * Derived types are interned on demand. Asking twice for a pointer to int
 * gives back the same handle both times.
 *
 * A type holds the size the backend needs, so the (value : type : size)
 * tuple the IR prints is recovered from the handle alone.
 *
 *****************************************************************************/

namespace credence::frontend::hir {

Type_Table::Type_Table()
{
    entries_.reserve(primitive_type_count * 2);

    // seeded in the order Primitive_Type declares
    entries_.push_back({ Type_Kind::Null, 0, null_type_index, 0 });
    entries_.push_back({ Type_Kind::Word, sizeof(void*), null_type_index, 0 });
    entries_.push_back({ Type_Kind::Int, sizeof(int), null_type_index, 0 });
    entries_.push_back({ Type_Kind::Char, sizeof(char), null_type_index, 0 });
    entries_.push_back(
        { Type_Kind::Byte, sizeof(unsigned char), null_type_index, 0 });
    entries_.push_back({ Type_Kind::Long, sizeof(long), null_type_index, 0 });
    entries_.push_back({ Type_Kind::Float, sizeof(float), null_type_index, 0 });
    entries_.push_back(
        { Type_Kind::Double, sizeof(double), null_type_index, 0 });
    entries_.push_back({ Type_Kind::Bool, sizeof(bool), null_type_index, 0 });
    entries_.push_back({ Type_Kind::String, sizeof(void*), type_char, 0 });
}

Type_Entry const& Type_Table::at(Type_Index index) const
{
    if (index == null_type_index or index >= entries_.size())
        return entries_[type_null];
    return entries_[index];
}

std::uint32_t Type_Table::size_of(Type_Index index) const
{
    return at(index).size;
}

Type_Kind Type_Table::kind_of(Type_Index index) const
{
    return at(index).kind;
}

/**
 * @brief Return the handle of an entry, adding it only if it is new
 *
 * The table holds a handful of entries for a translation unit, so a linear
 * scan finds a match faster than a hash of the entry would.
 */
Type_Index Type_Table::intern(Type_Entry entry)
{
    for (Type_Index index = 0; index < entries_.size(); ++index) {
        auto const& candidate = entries_[index];
        if (candidate.kind == entry.kind and candidate.size == entry.size and
            candidate.element == entry.element and
            candidate.count == entry.count)
            return index;
    }
    entries_.push_back(entry);
    return static_cast<Type_Index>(entries_.size() - 1);
}

Type_Index Type_Table::pointer_to(Type_Index element)
{
    return intern({ Type_Kind::Pointer, sizeof(void*), element, 0 });
}

Type_Index Type_Table::vector_of(Type_Index element, std::uint32_t count)
{
    // a vector decays to the address of its first element, so it is the
    // width of a word regardless of how many elements it holds
    return intern({ Type_Kind::Vector, sizeof(void*), element, count });
}

Type_Index Type_Table::function_returning(Type_Index result)
{
    return intern({ Type_Kind::Function, sizeof(void*), result, 0 });
}

/**
 * @brief Whether a type may address memory
 *
 * A word is included. B has no separate pointer type at the language
 * level, so a word holds an address as readily as it holds a number, and
 * an undeclared or external name arrives here as a word.
 */
bool Type_Table::is_address(Type_Index index) const
{
    auto kind = kind_of(index);
    return kind == Type_Kind::Pointer or kind == Type_Kind::Vector or
           kind == Type_Kind::String or kind == Type_Kind::Word;
}

bool Type_Table::is_numeric(Type_Index index) const
{
    switch (kind_of(index)) {
        case Type_Kind::Word:
        case Type_Kind::Int:
        case Type_Kind::Char:
        case Type_Kind::Byte:
        case Type_Kind::Long:
        case Type_Kind::Float:
        case Type_Kind::Double:
        case Type_Kind::Bool:
            return true;
        default:
            return false;
    }
}

bool Type_Table::is_integral(Type_Index index) const
{
    switch (kind_of(index)) {
        case Type_Kind::Word:
        case Type_Kind::Int:
        case Type_Kind::Char:
        case Type_Kind::Byte:
        case Type_Kind::Long:
        case Type_Kind::Bool:
            return true;
        default:
            return false;
    }
}

/**
 * @brief The type both operands of a binary expression widen to
 *
 * Pointer arithmetic keeps the address type, so that a pointer plus an
 * integer stays a pointer. Two numerics widen to the larger of the two.
 */
Type_Index Type_Table::common_type(Type_Index lhs, Type_Index rhs) const
{
    if (lhs == rhs)
        return lhs;
    if (lhs == null_type_index or rhs == null_type_index)
        return null_type_index;

    // a word is the width of an address, and B lets the two stand in for
    // each other freely, so any pair of addresses reconciles to the left
    if (is_address(lhs) and is_address(rhs))
        return lhs;

    if (is_address(lhs) and is_numeric(rhs))
        return lhs;
    if (is_numeric(lhs) and is_address(rhs))
        return rhs;

    if (is_numeric(lhs) and is_numeric(rhs)) {
        // a floating operand widens the result
        if (kind_of(lhs) == Type_Kind::Double or
            kind_of(rhs) == Type_Kind::Double)
            return type_double;
        if (kind_of(lhs) == Type_Kind::Float or
            kind_of(rhs) == Type_Kind::Float)
            return type_float;
        return size_of(lhs) >= size_of(rhs) ? lhs : rhs;
    }

    return null_type_index;
}

std::string_view type_kind_to_string(Type_Kind kind)
{
    switch (kind) {
        case Type_Kind::Null:
            return "null";
        case Type_Kind::Word:
            return "word";
        case Type_Kind::Int:
            return "int";
        case Type_Kind::Char:
            return "char";
        case Type_Kind::Byte:
            return "byte";
        case Type_Kind::Long:
            return "long";
        case Type_Kind::Float:
            return "float";
        case Type_Kind::Double:
            return "double";
        case Type_Kind::Bool:
            return "bool";
        case Type_Kind::String:
            return "string";
        case Type_Kind::Pointer:
            return "pointer";
        case Type_Kind::Vector:
            return "vector";
        case Type_Kind::Function:
            return "function";
    }
    return "unknown";
}

std::string type_to_string(Type_Table const& types, Type_Index index)
{
    if (index == null_type_index)
        return "null";

    auto const& entry = types.at(index);
    switch (entry.kind) {
        case Type_Kind::Pointer:
            return fmt::format("{}*", type_to_string(types, entry.element));
        case Type_Kind::Vector:
            return fmt::format(
                "{}[{}]", type_to_string(types, entry.element), entry.count);
        case Type_Kind::Function:
            return fmt::format("{}()", type_to_string(types, entry.element));
        default:
            return std::string{ type_kind_to_string(entry.kind) };
    }
}

} // namespace credence::frontend::hir
