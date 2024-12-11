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

#include <roxas/config.h>
#include <string>

namespace roxas {

namespace detail {
/**
 * @brief
 *
 * Parse tree data structure.
 *
 */
struct ParseTree
{
    std::string source{};
    std::string location{};
};

} // namespace detail

/**
 * @brief
 *
 * Parse tree loader via python interface to the python compiler frontend
 *
 */
class ParseTreeLoader
{
  public:
    ParseTreeLoader(ParseTreeLoader const&) = delete;
    /**
     * @brief ParseTreeLoader constructor
     *
     * Constructs object that interfaces with a compiler frontend in python
     *
     * @param module_path an absolute path to the frontend python module
     * @param file_path an absolute path to the source file to parse
     */
    explicit ParseTreeLoader(std::string& module_path, std::string& file_path);

    /**
     * @brief clean up
     *
     */
    ~ParseTreeLoader();

  public:
    /**
     * @brief
     *
     * Parses a source program and gives the parse tree as a string.
     *
     * @return std::string_view parse tree as a parsable string
     */
    std::string_view get_parse_tree_as_string();

  private:
    std::string module_path_;
    detail::ParseTree tree_;
};

} // namespace roxas
