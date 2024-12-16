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

#include <string>

namespace roxas {

/**
 * @brief
 *
 * Module loader via libpython interface to the compiler frontend module
 *
 */
class ParseTreeModuleLoader
{
  public:
    ParseTreeModuleLoader(ParseTreeModuleLoader const&) = delete;
    /**
     * @brief ParseTreeModuleLoader constructor
     *
     * Constructs object that interfaces with a compiler frontend in python
     *
     * @param module_path an absolute path to the frontend python module
     * @param file_path an absolute path to the source file to parse
     * @param env_path an optional absolute path to a venv directory where
     * dependecies are installed
     */
    ParseTreeModuleLoader(std::string const& module_path,
                          std::string const& file_path,
                          std::string const& env_path = "");

    /**
     * @brief clean up
     *
     */
    ~ParseTreeModuleLoader();

  public:
    /**
     * @brief
     *
     * Parse a source program and provides the parse tree as a string
     *
     * @param module_name a python frontend compiler for B
     * @param pretty boolean for pretty format
     * @return std::string parse tree as a readable string
     */
    std::string get_parse_tree_as_string_from_module(
        std::string const& module_name,
        bool pretty = false);

  private:
    std::string module_path_;
    std::string file_path_;
};

} // namespace roxas
