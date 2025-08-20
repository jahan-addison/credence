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

#include <chrono>   // for system_clock
#include <cstddef>  // for size_t
#include <ctime>    // for localtime, time_t, tm
#include <fstream>  // for basic_ifstream
#include <iomanip>  // for operator<<, put_time
#include <iostream> // for cout
#include <map>      // for map
#include <roxas/json.h>
#include <roxas/types.h> // for Type_, RValue, Value_Type, Byte
#include <roxas/util.h>  // for overload
#include <sstream>       // for basic_ostringstream, ostringstream
#include <string>        // for basic_string, char_traits, allo...
#include <utility>       // for pair
#include <variant>       // for variant, get, monostate, visit

namespace roxas {

namespace util {

/**
 * @brief unwrap AST arrays with one child
 */
json::JSON* unravel_nested_node_array(json::JSON* node)
{
    if (node->JSONType() == json::JSON::Class::Array) {
        for (auto& child_node : node->ArrayRange()) {
            if (child_node.JSONType() == json::JSON::Class::Array) {
                if (child_node.ArrayRange().get()->at(0).JSONType() ==
                        json::JSON::Class::Array and
                    child_node.ArrayRange().get()->size() == 1) {
                    return unravel_nested_node_array(&child_node);
                } else {
                    return &child_node;
                }
            }
        }
    }
    return node;
}

/**
 * @brief RValue::Value tuple as a string
 */
std::string dump_value_type(type::RValue::Value type,
                            std::string_view separator)
{
    using namespace type;
    std::ostringstream os;
    os << "(";
    std::visit(util::overload{
                   [&](int i) {
                       os << i << separator << Type_["int"].first << separator
                          << Type_["int"].second;
                   },
                   [&](long i) {
                       os << i << separator << Type_["long"].first << separator
                          << Type_["long"].second;
                   },
                   [&](float i) {
                       os << i << separator << Type_["float"].first << separator
                          << Type_["float"].second;
                   },
                   [&](double i) {
                       os << i << separator << Type_["double"].first
                          << separator << Type_["double"].second;
                   },
                   [&](bool i) {
                       os << std::boolalpha << i << separator
                          << Type_["bool"].first << separator
                          << Type_["bool"].second;
                   },
                   [&]([[maybe_unused]] std::monostate i) {
                       os << "null" << separator << Type_["null"].first
                          << separator << Type_["null"].second;
                   },
                   [&](roxas::type::Byte i) {
                       os << i << separator << Type_["byte"].first << separator
                          << type.second.second;
                   },
                   [&](char i) {
                       os << i << Type_["char"].first << separator
                          << Type_["char"].second;
                   },
                   [&]([[maybe_unused]] std::string const& s) {
                       if (s == "__WORD_") {
                           // pointer
                           os << "__WORD_" << separator << Type_["word"].first
                              << separator << Type_["word"].second;
                       } else {
                           os << std::get<std::string>(type.first) << separator
                              << "string" << separator
                              << std::get<std::string>(type.first).size();
                       }
                   },
               },
               type.first);
    os << ")";
    return os.str();
}

/**
 * @brief read source file
 */
std::string read_file_from_path(fs::path path)
{
    std::ifstream f(path, std::ios::in | std::ios::binary);
    const auto sz = fs::file_size(path);

    std::string result(sz, '\0');
    f.read(result.data(), sz);

    return result;
}

/**
 * @brief log function
 */
void log(Logging level, std::string_view message)
{
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

    std::tm* localTime = std::localtime(&currentTime);
    std::cout << "[" << std::put_time(localTime, "%Y-%m-%d %H:%M:%S") << "] ";

    switch (level) {
        case Logging::INFO:
#ifndef DEBUG
            return;
#endif
            std::cout << "[INFO] " << message << std::endl;
            break;
        case Logging::WARNING:
            std::cout << "[WARNING] " << message << std::endl;
            break;
        case Logging::ERROR:
            std::cout << "[ERROR] " << message << std::endl;
            break;
    }
}

} // namespace util

} // namespace roxas
