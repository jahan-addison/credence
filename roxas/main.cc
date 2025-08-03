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

#include <iostream>
#include <roxas/json.h>
#include <roxas/python.h>
#include <roxas/symbol.h>
#include <roxas/util.h>

#include <cxxopts.hpp>

int main(int argc, const char* argv[])
{
    try {
        cxxopts::Options options("Roxas", "Roxas :: Axel... What's this?");
        options.show_positional_help();
        options.add_options()(
            "a,ast-loader",
            "AST Loader (json or python)",
            cxxopts::value<std::string>()->default_value("json"))(
            "d,debug",
            "Enable debugging",
            cxxopts::value<bool>()->default_value("false"))("h,help",
                                                            "Print usage")
            /*
                Positional arguments
                *  1) B Program source code
                *  2) compiler frontend module name
                *  3) Directory to python module
                *  4) Path to python site-packages

            */
            ("source-code", "B Source file", cxxopts::value<std::string>())(
                "python-module",
                "Compiler frontend python module name",
                cxxopts::value<std::string>())(
                "module-directory",
                "Directory of compiler frontend module",
                cxxopts::value<std::string>())(
                "site-packages",
                "Directory of python site-packages",
                cxxopts::value<std::string>())(
                "additional",
                "additional arguments for the python loader",
                cxxopts::value<std::vector<std::string>>());
        options.parse_positional({ "source-code",
                                   "python-module",
                                   "module-directory",
                                   "site-packages",
                                   "additional" });

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
            auto module_directory =
                result["module-directory"].as<std::string>();
            auto site_packages = result["site-packages"].as<std::string>();
            auto python_module = roxas::PythonModuleLoader(
                module_directory, module_name, site_packages);

            if (result["debug"].count()) {
                auto symbol_table = python_module.call_method_on_module(
                    "get_source_program_symbol_table_as_json", { source });
                std::cout << "*** Symbol Table:" << std::endl
                          << json::JSON::Load(symbol_table) << std::endl;
            }

            auto ast_as_json = python_module.call_method_on_module(
                "get_source_program_ast_as_json", { source });

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
        std::cerr << "Runtime Exception :: " << e.what() << std::endl;
        exit(1);
    }
    return 0;
}
