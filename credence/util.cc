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

// clang-format off
#include <ctime>             // for localtime, time_t, tm
#include <fstream>           // for basic_ifstream
#include <iomanip>           // for operator<<, put_time
#include <iostream>          // for cout
#include <list>              // for operator==, _List_iterator
#include <map>               // for map
#include <memory>            // for __shared_ptr_access, shared_ptr
#include <credence/operators.h> // for operator<<, operator_to_string, Operato...
#include <credence/queue.h>     // for RValue_Queue
#include <credence/types.h>     // for Type_, RValue, Byte
#include <credence/util.h>
#include <sstream>           // for basic_ostream, basic_ostringstream, ope...
#include <string>            // for char_traits, allocator, operator<<, basic_s...
#include <utility>           // for pair
#include <variant>           // for get, visit, monostate
#include <vector>            // for vector
// clang-format on

namespace credence {

namespace util {

/**
 * @brief Rvalue type to string of it unwrapped data
 */
std::string rvalue_to_string(type::RValue::Type const& rvalue, bool separate)
{
    using namespace credence::type;
    auto oss = std::ostringstream();
    auto space = separate ? " " : "";
    std::visit(
        util::overload{
            [&](std::monostate) {},
            [&](RValue::RValue_Pointer const&) {},
            [&](RValue::Value const& s) {
                oss << util::dump_value_type(s) << space;
            },
            [&](RValue::Value_Pointer const& s) {
                for (auto const& value : s) {
                    oss << util::dump_value_type(value) << space;
                }
            },
            [&](RValue::LValue const& s) { oss << s.first << space; },
            [&](RValue::Unary const& s) {
                oss << s.first << rvalue_to_string(s.second->value) << space;
            },
            [&](RValue::Relation const& s) {
                for (auto& relation : s.second) {
                    oss << rvalue_to_string(relation->value) << space;
                }
            },
            [&](RValue::Function const& s) { oss << s.first.first << space; },
            [&](RValue::Symbol const& s) { oss << s.first.first << space; } },
        rvalue);
    return oss.str();
}

std::string unescape_string(std::string const& escaped_str)
{
    std::string unescaped_str;
    for (size_t i = 0; i < escaped_str.length(); ++i) {
        if (escaped_str[i] == '\\') {
            if (i + 1 < escaped_str.length()) {
                char next_char = escaped_str[i + 1];
                switch (next_char) {
                    case 'n':
                        unescaped_str += '\n';
                        break;
                    case 't':
                        unescaped_str += '\t';
                        break;
                    case '\\':
                        unescaped_str += '\\';
                        break;
                    case '"':
                        unescaped_str += '"';
                        break;
                    // Add more cases for other escape sequences like \r, \f,
                    // \b, \v, \a, etc. For Unicode escape sequences like
                    // \uXXXX, more complex parsing is needed.
                    default:
                        unescaped_str += escaped_str[i]; // If not a recognized
                                                         // escape, keep as is
                        unescaped_str += next_char;
                        break;
                }
                i++; // Skip the escaped character
            } else {
                unescaped_str += escaped_str[i]; // Backslash at end of string
            }
        } else {
            unescaped_str += escaped_str[i];
        }
    }
    return unescaped_str;
}

/**
 * @brief Queue to string of operators and operands in reverse-polish notaiton
 */
std::string queue_of_rvalues_to_string(RValue_Queue* rvalues_queue)
{
    using namespace type;
    auto oss = std::ostringstream();
    for (auto& item : *rvalues_queue) {
        std::visit(util::overload{ [&](type::Operator op) {
                                      oss << type::operator_to_string(op)
                                          << " ";
                                  },
                                   [&](type::RValue::Type_Pointer& s) {
                                       oss << rvalue_to_string(*s);
                                   }

                   },
                   item);
    }
    return oss.str();
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
                   [&](credence::type::Byte i) {
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

} // namespace credence
