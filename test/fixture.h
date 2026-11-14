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

#include <credence/util.h> // for AST_Node, STRINGIFY
#include <cstdlib>         // for getenv
#include <easyjson.h>      // for JSON
#include <filesystem>      // for path
#include <string>          // for basic_string, string
#include <string_view>     // for basic_string_view, string_view

/****************************************************************************
 *
 * Test fixtures
 *
 * JSON fixtures are loaded by their path under test/fixtures, without the
 * extension. The root is CREDENCE_TEST_ROOT when set, and the source
 * directory otherwise.
 *
 *   load_json_fixture("ir/ita/switch_4")
 *
 * Becomes:
 *
 *   test/fixtures/ir/ita/switch_4.json, as an AST_Node
 *
 *****************************************************************************/

namespace credence::test {

/**
 * @brief The fixture root, CREDENCE_TEST_ROOT or the source directory
 */
inline std::filesystem::path fixture_root()
{
    if (char const* env_root = std::getenv("CREDENCE_TEST_ROOT"))
        return std::filesystem::path(env_root);
    return std::filesystem::path(STRINGIFY(ROOT_TEST_PATH));
}

/**
 * @brief Load a JSON fixture by its path under test/fixtures
 */
inline util::AST_Node load_json_fixture(std::string_view name)
{
    auto path =
        fixture_root() / "test/fixtures" / (std::string{ name } + ".json");
    return easyjson::JSON::load_file(path.string());
}

} // namespace credence::test
