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
 * Module loader via libpython interface to a compiler frontend (lexer/parser)
 * python module
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
     * @param module_name the module name as a string
     * @param file_path an absolute path to the source file to parse
     * @param env_path an optional absolute path to a venv directory where
     * dependecies are installed
     */
    ParseTreeModuleLoader(std::string module_path,
                          std::string module_name,
                          std::string file_path,
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
     * Call a method on the parser module and provides the result as a
     * string
     *
     * @param method_name the method name
     * @return std::string result of method call
     */
    std::string call_method_on_module(std::string const& method_name);

  private:
    std::string module_path_;
    std::string module_name_;
    std::string file_path_;
};

} // namespace roxas
