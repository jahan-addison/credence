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

#include <cxxopts.hpp>    // for value, OptionAdder, Options, ParseResult
#include <filesystem>     // for path
#include <iostream>       // for basic_ostream, operator<<, endl, cout, cerr
#include <memory>         // for allocator, shared_ptr, __shared_ptr_access
#include <roxas/json.h>   // for JSON, operator<<
#include <roxas/python.h> // for PythonModuleLoader
#include <roxas/util.h>   // for read_file_from_path
#include <stdexcept>      // for runtime_error
#include <stdlib.h>       // for exit
#include <string>         // for char_traits, basic_string, string, operator==
#include <vector>         // for vector

int main(int argc, const char* argv[])
{
    try {
        cxxopts::Options options("Roxas", "Roxas :: Axel... What's this?");
        options.show_positional_help();
        options.add_options()(
            "a,ast-loader",
            "AST Loader (json or python)",
            cxxopts::value<std::string>()->default_value("python"))(
            "d,debug",
            "Enable debugging",
            cxxopts::value<bool>()->default_value("false"))("h,help",
                                                            "Print usage")(
            "source-code", "B Source file", cxxopts::value<std::string>())(
            "python-module",
            "Compiler frontend python module name",
            cxxopts::value<std::string>()->default_value("xion.parser"))(
            "additional",
            "additional arguments for the python loader",
            cxxopts::value<std::vector<std::string>>());
        options.parse_positional(
            { "source-code", "python-module", "additional" });

        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            exit(0);
        }

        json::JSON ast;

        auto type = result["ast-loader"].as<std::string>();
        auto source = roxas::util::read_file_from_path(
            result["source-code"].as<std::string>());

        if (type == "python") {
            auto module_name = result["python-module"].as<std::string>();
            auto python_module = roxas::PythonModuleLoader(module_name);

            if (result["debug"].count()) {
                auto symbol_table = python_module.call_method_on_module(
                    "get_source_program_symbol_table_as_json", { source });
                if (symbol_table.empty())
                    exit(1);
                std::cout << "*** Symbol Table:" << std::endl
                          << json::JSON::Load(symbol_table) << std::endl;
            }

            auto ast_as_json = python_module.call_method_on_module(
                "get_source_program_ast_as_json", { source });

            if (ast_as_json.empty())
                exit(1);

            ast["root"] = json::JSON::Load(ast_as_json);

        } else if (type == "json") {
            ast["root"] = json::JSON::Load(source);
        } else {
            std::cerr << "Error :: No source file provided" << std::endl;
        }

        if (!ast.IsNull()) {
            std::cout << ast["root"] << std::endl;
        }
    } catch (std::runtime_error& e) {
        std::cerr << "Roxas Exception :: " << e.what() << std::endl;
        exit(1);
    }
    return 0;
}
