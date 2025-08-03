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

#include <initializer_list>
#include <roxas/util.h>
#include <string>
#include <string_view>

namespace roxas {

/**
 * @brief
 *
 * Module loader via libpython.
 *
 */
class PythonModuleLoader
{
    ROXAS_PUBLIC:
    PythonModuleLoader(PythonModuleLoader const&) = delete;
    /**
     * @brief PythonModuleLoader constructor
     *
     * Constructs object that interfaces with libpython
     *
     * @param module_path an absolute path to a python module
     * @param module_name the module name as a string
     * @param file_path an absolute path to the source file to parse
     * @param env_path an optional absolute path to a venv directory where
     * dependecies are installed
     */
    PythonModuleLoader(std::string_view module_path,
                       std::string_view module_name,
                       std::string const& env_path = "");

    /**
     * @brief clean up
     *
     */
    ~PythonModuleLoader();

    ROXAS_PUBLIC:
    /**
     * @brief
     *
     * Call a method on the python module and return the result as a
     * string
     *
     * @param method_name the method name
     * @param args initializer list of arguments to pass to the python method
     * @return std::string result of method call
     */
    std::string call_method_on_module(std::string_view method_name,
                                      std::initializer_list<std::string> args);

    ROXAS_PRIVATE:
    std::string_view module_path_;
    std::string_view module_name_;
};

} // namespace roxas
