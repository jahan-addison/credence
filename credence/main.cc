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

#include <pybind11/pybind11.h> // for error_already_set::what, module

#include <credence/ir/qaud.h> // for build_from_definitions, emit_qu...
#include <credence/symbol.h>  // for Symbol_Table
#include <credence/util.h>    // for read_file_from_path
#include <cxxopts.hpp>        // for value, Options, OptionAdder
#include <exception>          // for exception
#include <iostream>           // for cerr, cout
#include <matchit.h>          // for pattern, match, PatternHelper
#include <memory>             // for shared_ptr
#include <ostream>            // for basic_ostream, operator<<, endl
#include <pybind11/cast.h>    // for object_api::operator(), object:...
#include <pybind11/embed.h>   // for scoped_interpreter
#include <pybind11/pytypes.h> // for object, accessor, str_attr, err...
#include <simplejson.h>       // for JSON, operator<<
#include <stdexcept>          // for runtime_error
#include <string>             // for basic_string, char_traits, string

int main(int argc, const char* argv[])
{
    namespace py = pybind11;

    using namespace matchit;
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
            cxxopts::value<bool>()->default_value("false"))("h,help",
                                                            "Print usage")(
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
                py::object python_module = py::module::import("chakram.parser");

                py::object symbol_table_call = python_module.attr(
                    "get_source_program_symbol_table_as_json");
                py::object symbols = symbol_table_call(source);

                hoisted = json::JSON::Load(symbols.cast<std::string>());
                if (result["debug"].count()) {
                    std::cout << "*** Symbol Table:" << std::endl
                              << symbols.cast<std::string>() << std::endl;
                }
                py::object get_ast_call =
                    python_module.attr("get_source_program_ast_as_json");

                py::object ast_as_json = get_ast_call(source);

                if (ast_as_json.is_none())
                    throw std::runtime_error("could not construct ast");

                ast["root"] = json::JSON::Load(ast_as_json.cast<std::string>());
            } catch (py::error_already_set const& e) {
                auto error_message = std::string_view{ e.what() };
                std::cerr << "Credence :: "
                          << error_message.substr(0, error_message.find("At:"));
                return 1;
            }

        } else if (type == "json") {
            ast["root"] = json::JSON::Load(source);
        } else {
            std::cerr << "Credence :: No source file provided" << std::endl;
            return 1;
        }

        match(result["target"].as<std::string>())(
            // cppcheck-suppress syntaxError
            pattern | "ir" =
                [&]() {
                    credence::Symbol_Table<> symbols{};
                    credence::Symbol_Table<> globals{};
                    auto ir_instructions = credence::ir::build_from_definitions(
                        symbols, globals, ast["root"], hoisted);
                    for (auto const& inst : ir_instructions) {
                        emit_quadruple(std::cout, inst);
                    }
                },
            pattern | "ast" =
                [&]() {
                    if (!ast.IsNull()) {
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
