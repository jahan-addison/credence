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

using credence::type::semantic::Label;
using credence::type::semantic::LValue;
using credence::type::semantic::RValue;
using credence::type::semantic::Size;
using credence::type::semantic::Type;

using Stack = std::deque<credence::type::semantic::RValue>;

using credence::type::Labels;
using credence::type::Locals;
using credence::type::Parameters;
using credence::type::Pointers;
using credence::type::RValues;
using credence::type::semantic::Address;
using credence::type::semantic::Label;
using credence::type::semantic::Size;

}