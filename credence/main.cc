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

#include <credence/error.h>    // for Credence_Exception
#include <credence/ir/table.h> // emit_complete_ita
#include <credence/util.h>     // for AST_Node, capitalize,
#include <cxxopts.hpp>         // for value, ParseResult, OptionAdder, Opti...
#include <filesystem>          // for filesystem_error, operator<<
#include <iostream>            // for basic_ostream, operator<<, endl, cerr
#include <matchit.h>           // for pattern, match, PatternHelper, Patter...
#include <memory>              // for allocator, shared_ptr, __shared_ptr_a...
#include <pybind11/cast.h>     // for object_api::operator(), object::cast
#include <pybind11/embed.h>    // for scoped_interpreter
#include <pybind11/pybind11.h> // for error_already_set::what, module, module_
#include <pybind11/pytypes.h>  // for object, object_api, error_already_set
#include <simplejson.h>        // for JSON, operator<<
#include <sstream>             // for basic_ostringstream
#include <string>              // for char_traits, string, basic_string
#include <string_view>         // for operator<<, string_view

// clang-format on

#include <credence/target/x86_64/generator.h> // for x86_64

int main(int argc, const char* argv[])
{
    namespace py = pybind11;
    namespace m = matchit;
    try {
        cxxopts::Options options("Credence", "Credence :: B Language Compiler");
        options.show_positional_help();

        options.add_options()("a,ast-loader",
            "AST Loader [json, python]",
            cxxopts::value<std::string>()->default_value("python"))("t,target",
            "Target [ir, syntax, ast, arm64, x86_64, z80]",
            cxxopts::value<std::string>()->default_value("ir"))("d,debug",
            "Dump symbol table",
            cxxopts::value<bool>()->default_value("false"))("o,output",
            "Output file",
            cxxopts::value<std::string>()->default_value("stdout"))(
            "h,help", "Print usage")(
            "source-code", "B Source file", cxxopts::value<std::string>());

        options.parse_positional({ "source-code" });

        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            exit(0);
        }

        std::string syntax_tree;
        credence::util::AST_Node ast;
        credence::util::AST_Node symbols;
        auto type = result["ast-loader"].as<std::string>();
        auto target = result["target"].as<std::string>();
        auto output = result["output"].as<std::string>();

        auto source = credence::util::read_file_from_path(
            result["source-code"].as<std::string>());

        if (type == "python") {
            py::scoped_interpreter guard{};
            try {
                // PYTHONHOME should be set to an env that has this package
                py::object python_module = py::module::import("chakram.parser");

                py::object symbol_table_call = python_module.attr(
                    "get_source_program_symbol_table_as_json");
                py::object syntax_symbols = symbol_table_call(source);
                py::object get_ast_call;
                symbols = credence::util::AST_Node::load(
                    syntax_symbols.cast<std::string>());

                if (result["debug"].count() and target != "ast")
                    std::cout << "> Symbol Table:" << std::endl
                              << syntax_symbols.cast<std::string>()
                              << std::endl;
                if (target == "syntax") {
                    auto get_source_ast =
                        python_module.attr("parse_source_program_as_string");
                    syntax_tree =
                        get_source_ast(source, py::arg("pretty") = true)
                            .cast<std::string>();
                }

                get_ast_call =
                    python_module.attr("get_source_program_ast_as_json");

                py::object ast_as_json = get_ast_call(source);

                if (ast_as_json.is_none())
                    credence_error("could not construct ast");

                ast["root"] = credence::util::AST_Node::load(
                    ast_as_json.cast<std::string>());
            } catch (py::error_already_set const& e) {
                auto error_message = std::string_view{ e.what() };
                std::cerr << "Credence :: " << "\033[33m"
                          << error_message.substr(0, error_message.find("At:"))
                          << "\033[0m" << std::endl;
                return 1;
            }

        } else if (type == "json") {
            ast["root"] = credence::util::AST_Node::load(source);
        } else {
            std::cerr << "Credence :: No source file provided" << std::endl;
            return 1;
        }

        std::ostringstream out_to{};

        const std::string_view extension = m::match(target)(
            m::pattern | m::or_(sv("x86_64"), sv("arm64"), sv("z80")) =
                [&] { return "bs"; },
            m::pattern |
                m::or_(sv("ast"), sv("syntax")) = [&] { return "bast"; },
            m::pattern | m::_ = [&] { return "bo"; });

        m::match(target)(
            // cppcheck-suppress syntaxError
            m::pattern | "x86_64" =
                [&]() {
                    credence::target::add_stdlib_functions_to_symbols(symbols,
                        credence::target::Platform::credence_x86_64_platform);
                    // clang-format off
                    credence::target::x86_64::emit(out_to, symbols, ast["root"]);
                    // clang-format on
                },
            m::pattern | "ir" =
                [&]() {
                    credence::target::add_stdlib_functions_to_symbols(symbols,
                        credence::target::Platform::credence_x86_64_platform);
                    credence::ir::emit(out_to, symbols, ast["root"]);
                },
            m::pattern | "ast" =
                [&]() {
                    if (result["debug"].count()) {
                        auto group = credence::util::AST::array();
                        group[0] = symbols;
                        group[1] = ast["root"];
                        out_to << group << std::endl;
                    } else
                        out_to << ast["root"] << std::endl;
                },
            m::pattern |
                "syntax" = [&]() { out_to << syntax_tree << std::endl; },
            m::pattern | m::_ =
                [&]() {
                    std::cerr << "Credence :: Invalid target option"
                              << std::endl;
                });

        credence::util::write_to_from_string_stream(output, out_to, extension);

    } catch (cxxopts::exceptions::option_has_no_value const&) {
        std::cout << "Credence :: See \"--help\" for usage overview"
                  << std::endl;
    } catch (std::filesystem::filesystem_error const& e) {
        std::cerr << "Credence :: Invalid file path: " << e.path1()
                  << std::endl;
        return 1;
    } catch (credence::detail::Credence_Exception const& e) {
        auto what = credence::util::capitalize(e.what());
        std::cerr << std::endl
                  << "Credence Error :: " << "\033[31m" << what << "\033[0m"
                  << std::endl;
        return 1;
    }
    return 0;
}
