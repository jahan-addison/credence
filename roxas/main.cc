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

#include <cstdlib>
#include <cxxopts.hpp>
#include <iostream>
#include <roxas/python_module.h>
#include <roxas/util.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cout << "Roxas :: Axel... What's this?" << std::endl;
        return EXIT_SUCCESS;
    }

    try {
        cxxopts::Options options("roxas", "B Compiler in C++");

        options.add_options()(
            "j,json", "Load AST from JSON file", cxxopts::value<std::string>())(
            "p,python",
            "Use PythonModuleLoader to load a Parser python module and build "
            "an AST",
            cxxopts::value<bool>()->default_value("false"))(
            "python-use",
            "Arguments to pass to python module loader",
            cxxopts::value<std::vector<std::string>>())("h,help",
                                                        "Print usage");

        auto result = options.parse(argc, argv);
        auto python_opt = result["python"].as<bool>();

        std::string ast_as_json{};

        if (result.count("help")) {
            std::cout
                << "Usage: \n\t-j,--json: Load AST from JSON file\n"
                << "\t-p,--python: Use PythonModuleLoader to load a Parser "
                   "python module and build an AST\n"
                << "\t--python-use: Arguments to pass to PythonModuleLoader. "
                   "I.e. --python-use=module,path,env_dir\n";
            return EXIT_SUCCESS;
        }

        if (result.count("json")) {
            ast_as_json = roxas::util::read_file_from_path(
                result["json"].as<std::string>());
        } else if (python_opt) {
            auto module_args =
                result["python-use"].as<std::vector<std::string>>();

            if (module_args.size() != 4) {
                std::cerr << "Error :: Invalid arguments passed to --python-use"
                          << std::endl;
                return EXIT_FAILURE;
            }
            auto source = roxas::util::read_file_from_path(module_args[2]);
            auto python_module = roxas::PythonModuleLoader(
                module_args[1], module_args[0], module_args[3]);

            ast_as_json = python_module.call_method_on_module(
                "get_source_program_ast_as_json", { source });
        } else {
            std::cerr << "Error :: Invalid arguments try -j <json file> or -p "
                         "--python-use=module,path,env_dir"
                      << std::endl;
            return EXIT_FAILURE;
        }

        simdjson::ondemand::parser parser;
        auto doc = parser.iterate(ast_as_json);

        roxas::util::recursive_print_json(doc);

    } catch (std::runtime_error& e) {
        std::cerr << "Runtime Exception :: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
