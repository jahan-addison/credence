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

#include <credence/assert.h>

#include <credence/ir/ita.h>   // for build_from_definitions, emit_quadruple
#include <credence/symbol.h>   // for Symbol_Table
#include <credence/util.h>     // for read_file_from_path
#include <cxxopts.hpp>         // for value, Options, OptionAdder, ParseResult
#include <deque>               // for operator==, _Deque_iterator
#include <exception>           // for exception
#include <iostream>            // for basic_ostream, operator<<, endl, cerr
#include <matchit.h>           // for match, pattern, PatternHelper, Patter...
#include <memory>              // for allocator, shared_ptr, __shared_ptr_a...
#include <pybind11/cast.h>     // for object_api::operator(), object::cast
#include <pybind11/embed.h>    // for scoped_interpreter
#include <pybind11/pybind11.h> // for error_already_set::what, module, module_
#include <pybind11/pytypes.h>  // for object, error_already_set, object_api
#include <simplejson.h>        // for JSON, operator<<
#include <stdexcept>           // for runtime_error
#include <stdlib.h>            // for exit
#include <string>              // for char_traits, basic_string, string
#include <string_view>         // for operator<<, string_view
#include <tuple>               // for tuple
#include <vector>              // for vector

int main(int argc, const char* argv[])
{
    namespace py = pybind11;
    try {
        cxxopts::Options options("Credence", "Credence :: B Language Compiler");
        options.show_positional_help();
        options.add_options()(
            "a,ast-loader",
            "AST Loader [json, python]",
            cxxopts::value<std::string>()->default_value("python"))(
            "t,target",
            "Target [ast, ir, arm64, x86_64, z80]",
            cxxopts::value<std::string>()->default_value("ir"))(
            "d,debug",
            "Enable debugging",
            cxxopts::value<bool>()->default_value("false"))(
            "h,help", "Print usage")(
            "source-code", "B Source file", cxxopts::value<std::string>());
        options.parse_positional({ "source-code" });

        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            exit(0);
        }

        json::JSON ast;
        json::JSON hoisted;

        auto type = result["ast-loader"].as<std::string>();
        auto source = credence::util::read_file_from_path(
            result["source-code"].as<std::string>());

        if (type == "python") {
            py::scoped_interpreter guard{};
            try {
                // PYTHONHOME should be set to an env that has this package
                py::object python_module = py::module::import("chakram.parser");

                py::object symbol_table_call = python_module.attr(
                    "get_source_program_symbol_table_as_json");
                py::object symbols = symbol_table_call(source);

                hoisted = json::JSON::load(symbols.cast<std::string>());
                if (result["debug"].count()) {
                    std::cout << "*** Symbol Table:" << std::endl
                              << symbols.cast<std::string>() << std::endl;
                }
                py::object get_ast_call =
                    python_module.attr("get_source_program_ast_as_json");

                py::object ast_as_json = get_ast_call(source);

                if (ast_as_json.is_none())
                    throw std::runtime_error("could not construct ast");

                ast["root"] = json::JSON::load(ast_as_json.cast<std::string>());
            } catch (py::error_already_set const& e) {
                auto error_message = std::string_view{ e.what() };
#ifndef DEBUG
                std::cerr << "Credence :: "
                          << error_message.substr(0, error_message.find("At:"))
                          << std::endl;
#else
                std::cerr << "Credence :: " << error_message << std::endl;
#endif
                return 1;
            }

        } else if (type == "json") {
            ast["root"] = json::JSON::load(source);
        } else {
            std::cerr << "Credence :: No source file provided" << std::endl;
            return 1;
        }

        matchit::match(result["target"].as<std::string>())(
            // cppcheck-suppress syntaxError
            matchit::pattern | "ir" =
                [&]() {
                    credence::ir::ITA ita{ hoisted };
                    ita.build_from_definitions(ast["root"]);
                    ita.emit(std::cout, true);
                },
            matchit::pattern | "ast" =
                [&]() {
                    if (!ast.is_null()) {
                        std::cout << ast["root"] << std::endl;
                    }
                });

    } catch (std::runtime_error const& e) {
        std::cerr << "Credence :: " << e.what() << std::endl;
        return 1;
    } catch (const cxxopts::exceptions::option_has_no_value&) {
        std::cout << "Credence :: See \"--help\" for usage overview"
                  << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error :: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred: " << std::endl;
        return 1;
    }

    return 0;
}
