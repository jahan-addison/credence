
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

#include <credence/util.h>

#include <credence/error.h> // for credence_error
#include <filesystem>       // for file_size, path
#include <fmt/format.h>     // for format
#include <fstream>          // for basic_ofstream, basic_ifstream
#include <iostream>         // for cout
#include <ostream>          // for operator<<, basic_ostream
#include <sstream>          // for basic_ostringstream

/****************************************************************************
 *
 * Utility functions and helpers
 *
 * Common utilities used including:
 * > String manipulation (escaping, unescaping, case conversion)
 * > File I/O operations
 * > Type checking and variant helpers
 * > AST node manipulation
 * > Debugging and source location tracking
 *
 *****************************************************************************/

namespace credence {

namespace util {

/**
 * @brief Create and write to a file by one std::ostream to another
 */
void write_to_file_from_string_stream(std::string_view file_name,
    std::ostringstream const& oss,
    std::string_view ext)
{
    if (file_name == "stdout") {
        std::cout << oss.str();
    } else {
        std::ofstream file_(fmt::format("{}.{}", file_name, ext));
        if (file_.is_open()) {
            file_ << oss.str();
            file_.close();
        } else {
            credence_error(fmt::format("Error creating file: `{}`", file_name));
        }
    }
}

/**
 * @brief read a file from a fs::path
 */
std::string read_file_from_path(std::string_view path)
{
    std::ifstream f(path.data(), std::ios::in | std::ios::binary);
    const auto sz = fs::file_size(path);

    std::string result(sz, '\0');
    f.read(result.data(), sz);

    return result;
}

} // namespace util

} // namespace credence