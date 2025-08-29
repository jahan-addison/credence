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

#include <cpptrace/basic.hpp>        // for stacktrace
#include <cpptrace/from_current.hpp> // for from_current_exception
#include <credence/ir/qaud.h>        // for build_from_definitions, emit_qu...
#include <credence/json.h>           // for JSON, operator<<
#include <credence/python.h>         // for Python_Module_Loader
#include <credence/symbol.h>         // for Symbol_Table
#include <credence/util.h>           // for read_file_from_path, CREDENCE_CATCH
#include <cxxopts.hpp>               // for value, Options, OptionAdder
#include <deque>                     // for operator==, _Deque_iterator
#include <filesystem>                // for path
#include <iostream>                  // for basic_ostream, endl, operator<<
#include <matchit.h>                 // for match, pattern, PatternHelper
#include <memory>                    // for allocator, shared_ptr, __shared...
#include <stdexcept>                 // for runtime_error
#include <stdlib.h>                  // for exit
#include <string>                    // for char_traits, operator==, string
#include <tuple>                     // for tuple

int main(int argc, const char* argv[])
{

    using namespace matchit;
    cpptrace::try_catch(
        [&] {
            cxxopts::Options options("Credence",
                                     "Credence :: B Language Compiler");
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
                auto python_module =
                    credence::Python_Module_Loader("xion.parser");
                auto hoisted_symbols = python_module.call_method_on_module(
                    "get_source_program_symbol_table_as_json", { source });
                hoisted = json::JSON::Load(hoisted_symbols);
                if (result["debug"].count()) {
                    std::cout << "*** Symbol Table:" << std::endl
                              << hoisted.ToString() << std::endl;
                }

                auto ast_as_json = python_module.call_method_on_module(
                    "get_source_program_ast_as_json", { source });

                if (ast_as_json.empty())
                    throw std::runtime_error(
                        "symbol table construction failed");

                ast["root"] = json::JSON::Load(ast_as_json);

            } else if (type == "json") {
                ast["root"] = json::JSON::Load(source);
            } else {
                std::cerr << "Error :: No source file provided" << std::endl;
            }

            match(result["target"].as<std::string>())(
                pattern | "ir" =
                    [&]() {
                        credence::Symbol_Table<> symbols{};
                        auto ir_instructions =
                            credence::ir::build_from_definitions(
                                symbols, ast["root"], hoisted);
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
        },
        [&](const std::runtime_error& e) {
            std::cerr << "Credence :: " << e.what() << std::endl;
        },
        [&](const cxxopts::exceptions::option_has_no_value&) {
            std::cout << "Credence :: See \"--help\" for usage overview"
                      << std::endl;
        },
        [&](const std::exception& e) {
            std::cerr << "Error :: " << e.what() << std::endl;
            cpptrace::from_current_exception().print();
        },
        [&]() { // catch (...)
            std::cerr << "Unknown exception occurred: " << std::endl;
            cpptrace::from_current_exception().print();
        });

    return 0;
}
