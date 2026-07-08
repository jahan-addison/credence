/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#include <credence/error.h>                   // for Credence_Exception
#include <credence/ir/table.h>                // for emit
#include <credence/ir/temporary.h>            // for queue_dump_stream
#include <credence/language/parser.h>         // for Parser
#include <credence/language/symbol_table.h>   // for Symbol_Table_Builder
#include <credence/target/arm64/generator.h>  // for emit
#include <credence/target/common/assembly.h>  // for Arch_Type
#include <credence/target/common/runtime.h>   // for add_stdlib_functions_t...
#include <credence/target/x86_64/generator.h> // for emit
#include <credence/util.h>                    // for AST_Node, sv, AST, cap...
#include <cxxopts.hpp>                        // for value, Options, ParseR...
#include <easyjson.h>                         // for JSON, operator<<, array
#include <filesystem>                         // for filesystem_error, oper...
#include <iostream>                           // for ostringstream, cerr, cout
#include <matchit.h>                          // for pattern, Or, PatternHe...
#include <memory>                             // for shared_ptr
#include <sstream>                            // for basic_ostringstream
#include <string>                             // for basic_string, char_traits
#include <string_view>                        // for basic_string_view, str...

/****************************************************************************
 *
 * Credence 🚂
 *
 * The compiler works in 3 stages:
 *
 * 1. Lexer/Parser: re2c Lexer Generator and Recursive-Descent Parser
 *    See language/README.md for details
 * 2. IR Generation: Converts AST to ITA, see ir/README.md for details
 * 3. Code Generation: Emits x86-64 or ARM64 assembly from the IR
 *      * See target/README.md for details
 *
 * Example usage:
 *
 * Note: There is an entry point with a linker and assembler available via the
 * install.sh
 *
 *   $ credence --target x86_64 --output program program.b
 *   $ ./program
 *
 * Example program:
 *
 *   main() {
 *     auto x;
 *     x = 42;
 *     return(x);
 *   }
 *
 *****************************************************************************/

int main(int argc, const char* argv[])
{
    namespace m = matchit;
    try {
        cxxopts::Options options("Credence", "Credence :: B Language Compiler");
        options.show_positional_help();

        options.add_options()("a,ast-loader",
            "AST Loader [parser, json]",
            cxxopts::value<std::string>()->default_value("parser"))("t,target",
            "Target [ir, ast, arm64, x86_64]",
            cxxopts::value<std::string>()->default_value("ir"))("d,debug",
            "[Debug] Dump symbol table",
            cxxopts::value<bool>()->default_value("false"))("q,dump-queue",
            "[Debug] Dump each expression's queue form to stdout",
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
        credence::util::AST_Node ast, symbols;
        auto type = result["ast-loader"].as<std::string>();
        auto target = result["target"].as<std::string>();
        auto output = result["output"].as<std::string>();

        if (result["dump-queue"].as<bool>())
            credence::ir::queue_dump_stream = &std::cout;

        auto source = credence::util::read_file_from_path(
            result["source-code"].as<std::string>());

        m::match(type)(
            m::pattern | "parser" =
                [&] {
                    auto parser = credence::language::Parser{ source };
                    ast["root"] = parser.parse_program();
                    symbols = credence::language::Symbol_Table_Builder::build(
                        ast["root"]);
                },
            m::pattern | "json" =
                [&] { ast["root"] = credence::util::AST_Node::load(source); },
            m::pattern | m::_ =
                [&] {
                    std::cerr << "Credence :: No source file provided"
                              << std::endl;
                    exit(1);
                });

        // Populate the symbol table with standard library functions
        auto os_type = credence::target::common::assembly::get_os_type();
        if (target == "x86_64" or target == "ir")
            credence::target::common::runtime::add_stdlib_functions_to_symbols(
                symbols,
                os_type,
                credence::target::common::assembly::Arch_Type::X8664);
        else if (target == "arm64")
            credence::target::common::runtime::add_stdlib_functions_to_symbols(
                symbols,
                os_type,
                credence::target::common::assembly::Arch_Type::ARM64);

        if (result["debug"].count() and target != "ast")
            std::cout << "> Symbol Table:" << std::endl << symbols << std::endl;

        std::ostringstream out_to{};

        const std::string_view extension = m::match(target)(
            m::pattern |
                m::or_(sv("x86_64"), sv("arm64")) = [&] { return "bs"; },
            m::pattern | sv("ast") = [&] { return "bast"; },
            m::pattern | m::_ = [&] { return "bo"; });

        m::match(target)(
            m::pattern | "arm64" =
                [&]() {
                    credence::target::arm64::emit(out_to, symbols, ast["root"]);
                },
            m::pattern | "x86_64" =
                [&]() {
                    credence::target::x86_64::emit(
                        out_to, symbols, ast["root"]);
                },
            m::pattern | "ir" =
                [&]() { credence::ir::emit(out_to, symbols, ast["root"]); },
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
            m::pattern | m::_ =
                [&]() {
                    std::cerr << "Credence :: Invalid target option"
                              << std::endl;
                });

        credence::util::write_to_file_from_string_stream(
            output, out_to, extension);

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
