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
#include <string>
#include <string_view>

namespace credence {

/**
 * @brief
 *
 * Module loader via libpython.
 *
 */
class Python_Module_Loader
{
  public:
    Python_Module_Loader(Python_Module_Loader const&) = delete;
    Python_Module_Loader(std::string_view module_name);

    ~Python_Module_Loader();

  public:
    std::string call_method_on_module(std::string_view method_name,
                                      std::initializer_list<std::string> args);

  private:
    std::string_view module_path_;
    std::string_view module_name_;
};

} // namespace credence
