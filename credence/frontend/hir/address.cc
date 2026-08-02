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

#include <credence/frontend/hir/address.h>

#include <fmt/format.h> // for format
#include <matchit.h>    // for match, pattern
#include <utility>      // for move

/****************************************************************************
 *
 * Address resolution
 *
 * Resolves what an expression addresses:
 *
 *    - a vector used as a value decays to a pointer to its first element,
 *      recorded as the type of that use
 *    - a subscript with a constant index is folded to a byte offset, so a
 *      backend reads a number and not an index with an element width
 *    - every string literal is collected, as one needs storage before its
 *      address can be taken
 *
 * Decay is a property of a use and not of a declaration. The symbol keeps
 * its vector type and the use is typed as a pointer, so both facts remain
 * available. Nothing is inserted into the node array, so an index resolved
 * by an earlier pass stays valid.
 *
 * A vector is not decayed where it is the base of a subscript or the
 * operand of an address-of, as both of those want the vector itself.
 *
 *****************************************************************************/

namespace credence::frontend::hir {

namespace m = matchit;

namespace detail {

void Addresses::error(Node_Index index, std::string message)
{
    auto const& meta = unit_.metadata[index];
    diagnostics_.push_back(
        Diagnostic{ std::move(message), meta.line, meta.column });
}

/**
 * @brief Note the uses that want a vector itself and not a pointer
 *
 * The base of a subscript and the operand of an address-of both address
 * the vector, so neither decays. Everything else that names a vector in a
 * value position does.
 */
void Addresses::mark_wanted_whole()
{
    whole_.assign(unit_.nodes.size(), false);

    for (auto const& node : unit_.nodes) {
        m::match(node.type)(
            m::pattern |
                Type::Subscript = [&] { whole_[node.data.binary.lhs] = true; },
            m::pattern | Type::Address_Of =
                [&] {
                    if (node.data.unary != null_node_index)
                        whole_[node.data.unary] = true;
                },
            m::pattern | m::_ = [] {});
    }
}

std::vector<Diagnostic> Addresses::run()
{
    if (unit_.offsets.size() != unit_.nodes.size())
        unit_.offsets.assign(unit_.nodes.size(), unknown_offset);

    mark_wanted_whole();

    for (Node_Index index = 0; index < unit_.nodes.size(); ++index) {
        auto const& node = unit_.nodes[index];

        m::match(node.type)(
            m::pattern | Type::Subscript = [&] { resolve_subscript(index); },
            m::pattern | Type::Symbol_Ref = [&] { decay_vector_use(index); },
            m::pattern | Type::String =
                [&] { unit_.string_literals.push_back(node.data.string); },
            m::pattern | m::_ = [] {});
    }

    return std::move(diagnostics_);
}

/**
 * @brief Fold a constant subscript into a byte offset
 *
 * A variable index is left as unknown_offset, since the address is only
 * known once the program runs.
 */
void Addresses::resolve_subscript(Node_Index index)
{
    auto const& node = unit_.nodes[index];
    auto base = node.data.binary.lhs;
    auto subscript = node.data.binary.rhs;

    auto element = unit_.types[index];
    auto width = unit_.type_table.size_of(element);

    if (unit_.nodes[subscript].type != Type::Integer)
        return;

    auto offset = unit_.nodes[subscript].data.integer;
    if (offset < 0) {
        error(index, "a subscript may not be negative");
        return;
    }

    // the element width comes from the type table, so the byte offset is
    // computed here once and not by each backend
    unit_.offsets[index] =
        static_cast<std::uint32_t>(offset) * (width == 0 ? 1 : width);

    // a string is a pointer to bytes, so its elements are addressed
    // through it and not laid out inside the symbol
    if (unit_.nodes[base].type != Type::Symbol_Ref)
        return;

    auto const& declared = unit_.symbol_table.at(unit_.nodes[base].data.symbol);
    if (declared.storage != Storage::Vector)
        unit_.offsets[index] = unknown_offset;
}

/**
 * @brief Type a vector used as a value as a pointer to its first element
 *
 * The declaration keeps its vector type. Only this use is retyped, so the
 * two facts stay separable afterwards.
 */
void Addresses::decay_vector_use(Node_Index index)
{
    if (whole_[index])
        return;

    auto const& node = unit_.nodes[index];
    auto const& declared = unit_.symbol_table.at(node.data.symbol);

    if (unit_.type_table.kind_of(declared.type) != Type_Kind::Vector)
        return;

    auto element = unit_.type_table.at(declared.type).element;
    unit_.types[index] = unit_.type_table.pointer_to(element);
}

} // namespace detail

std::vector<Diagnostic> resolve_addresses(Unit& unit)
{
    detail::Addresses addresses{ unit };
    return addresses.run();
}

} // namespace credence::frontend::hir
