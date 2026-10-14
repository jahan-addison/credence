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

#include <credence/types.h> // for get_type_from_rvalue_data_type, Data...

namespace credence::language::hir {

using type::semantic::Label;
using type::semantic::LValue;
using type::semantic::RValue;
using type::semantic::Size;
using type::semantic::Type;

using Stack = std::deque<type::semantic::RValue>;

using type::Labels;
using type::Locals;
using type::Parameters;
using type::Pointers;
using type::RValues;
using type::semantic::Address;
using type::semantic::Label;
using type::semantic::Size;

}