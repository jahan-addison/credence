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

#include <cstddef>     // for size_t
#include <cstdint>     // for uint32_t
#include <string>      // for string
#include <string_view> // for string_view
#include <vector>      // for vector

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

// Handle into Type_Table
using Type_Index = std::uint32_t;

// A type that is absent or not yet resolved
inline constexpr Type_Index null_type_index = 0xFFFFFFFFu;

/**
 * @brief The shape of a type, as opposed to its width
 */
enum class Type_Kind : std::uint32_t
{
    Null,
    Word,
    Int,
    Char,
    Byte,
    Long,
    Float,
    Double,
    Bool,
    String,
    Pointer,
    Vector,
    Function
};

/**
 * @brief One entry in the type table
 *
 * The element handle names the pointee of a pointer, the element of a
 * vector, and the return type of a function. It is null_type_index for a
 * primitive. The count holds a vector length and is zero elsewhere.
 */
struct Type_Entry
{
    Type_Kind kind{ Type_Kind::Null };
    std::uint32_t size{ 0 };
    Type_Index element{ null_type_index };
    std::uint32_t count{ 0 };
};

/**
 * @brief Handles of the primitives, seeded in this order by the table
 */
enum Primitive_Type : Type_Index
{
    type_null = 0,
    type_word = 1,
    type_int = 2,
    type_char = 3,
    type_byte = 4,
    type_long = 5,
    type_float = 6,
    type_double = 7,
    type_bool = 8,
    type_string = 9,
    primitive_type_count = 10
};

/**
 * @brief The interning table of every type in a translation unit
 */
class Type_Table
{
  public:
    Type_Table();

    /**
     * @brief The entry behind a handle
     */
    Type_Entry const& at(Type_Index index) const;

    /**
     * @brief The width in bytes of a type
     */
    std::uint32_t size_of(Type_Index index) const;

    /**
     * @brief The kind of a type
     */
    Type_Kind kind_of(Type_Index index) const;

    /**
     * @brief The handle of a pointer to the given type, interned
     */
    Type_Index pointer_to(Type_Index element);

    /**
     * @brief The handle of a vector of the given type and length, interned
     */
    Type_Index vector_of(Type_Index element, std::uint32_t count);

    /**
     * @brief The handle of a function returning the given type, interned
     */
    Type_Index function_returning(Type_Index result);

    /**
     * @brief Whether a type addresses memory, either a pointer or a vector
     */
    bool is_address(Type_Index index) const;

    /**
     * @brief Whether a type may take part in arithmetic
     */
    bool is_numeric(Type_Index index) const;

    /**
     * @brief Whether a type may take part in a bitwise or shift expression
     */
    bool is_integral(Type_Index index) const;

    /**
     * @brief The type both operands of a binary expression widen to
     *
     * Returns null_type_index when the two cannot be reconciled, which the
     * checker reports and does not resolve.
     */
    Type_Index common_type(Type_Index lhs, Type_Index rhs) const;

    std::size_t size() const { return entries_.size(); }

  private:
    Type_Index intern(Type_Entry entry);

  private:
    std::vector<Type_Entry> entries_;
};

/**
 * @brief The name of a type kind, as the IR and diagnostics print it
 */
std::string_view type_kind_to_string(Type_Kind kind);

/**
 * @brief The name of a type, including any pointer or vector suffix
 */
std::string type_to_string(Type_Table const& types, Type_Index index);

} // namespace credence::frontend::hir
