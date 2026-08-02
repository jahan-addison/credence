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
#include <credence/frontend/compile.h>        // for compile
#include <credence/frontend/hir/hir.h>        // for Unit
#include <credence/frontend/hir/serialize.h>  // for dump
#include <credence/frontend/serialize.h>      // for dump
#include <credence/ir/symbols.h>              // for hoisted_symbols
#include <credence/ir/table.h>                // for emit
#include <credence/ir/temporary.h>            // for queue_dump_stream
#include <credence/target/arm64/generator.h>  // for emit
#include <credence/target/common/assembly.h>  // for Arch_Type
#include <credence/target/common/runtime.h>   // for add_stdlib_functions_t...
#include <credence/target/x86_64/generator.h> // for emit
#include <credence/util.h>                    // for AST_Node, sv, AST, cap...
#include <cxxopts.hpp>                        // for value, Options, ParseR...
#include <easyjson.h>                         // for JSON, operator<<, array
#include <filesystem>                         // for filesystem_error, oper...
#include <fmt/format.h>                       // for format
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
 * 1. Frontend: re2c Lexer Generator, Recursive-Descent Parser, and the
 *    lowering and type checking passes over the flat AST
 *      * See frontend/README.md for details
 * 2. IR Generation: Converts HIR to ITA, see ir/README.md for details
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

namespace {

struct Frontend
{
    credence::frontend::Program program;
    credence::util::AST_Node symbols;
};

} // namespace

/**
 * @brief Build the frontend, reporting anything it rejected
 */
Frontend build_frontend(std::string source)
{
    auto program = credence::frontend::compile(std::move(source));
    credence::frontend::report(std::cerr, program);

    if (program.failed())
        exit(1);

    auto symbols = credence::ir::hoisted_symbols(program.unit);
    return Frontend{ std::move(program), std::move(symbols) };
}

int main(int argc, const char* argv[])
{
    namespace m = matchit;
    try {
        cxxopts::Options options("Credence", "Credence :: B Language Compiler");
        options.show_positional_help();
        // clang-format off
        options.add_options()
            ("t,target", "Target [ast, hir, ir, arm64, x86_64]",
                cxxopts::value<std::string>()->default_value("ir"))
            ("s,symbols", "[Debug] Dump symbol table",
                cxxopts::value<bool>()->default_value("false"))
            ("n,nostdlib", "[Debug] Do not add stdlib symbols",
                cxxopts::value<bool>()->default_value("false"))
            ("q,dump-queue", "[Debug] Dump each expression's queue form to stdout",
                cxxopts::value<bool>()->default_value("false"))
            ("v,verbose", "[Debug] Add node indices and source positions to an ast or hir dump",
                cxxopts::value<bool>()->default_value("false"))
            ("l,linear", "[Debug] Dump the hir target in the linear form the IR reads",
                cxxopts::value<bool>()->default_value("false"))
            ("o,output", "Output file",
                cxxopts::value<std::string>()->default_value("stdout"))
            ("h,help", "Print usage")
            ("source-code", "B Source file", cxxopts::value<std::string>());
        // clang-format on
        options.parse_positional({ "source-code" });

        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            exit(0);
        }
        auto target = result["target"].as<std::string>();
        auto output = result["output"].as<std::string>();
        bool no_stdlib = result["nostdlib"].as<bool>();
        bool verbose = result["verbose"].as<bool>();
        bool linear = result["linear"].as<bool>();

        if (result["dump-queue"].as<bool>())
            credence::ir::queue_dump_stream = &std::cout;

        auto source = credence::util::read_file_from_path(
            result["source-code"].as<std::string>());

        auto frontend = build_frontend(source);
        auto& unit = frontend.program.unit;
        auto& symbols = frontend.symbols;

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

        if (result["symbols"].count() and target != "ast" and target != "hir")
            std::cout << "> Symbol Table:" << std::endl << symbols << std::endl;

        std::ostringstream out_to{};

        const std::string_view extension = m::match(target)(
            m::pattern |
                m::or_(sv("x86_64"), sv("arm64")) = [&] { return "bs"; },
            m::pattern | sv("ast") = [&] { return "bast"; },
            m::pattern | sv("hir") = [&] { return "bhir"; },
            m::pattern | m::_ = [&] { return "bo"; });

        m::match(target)(
            m::pattern | "arm64" =
                [&]() {
                    credence::target::arm64::emit(
                        out_to, symbols, unit, no_stdlib);
                },
            m::pattern | "x86_64" =
                [&]() {
                    credence::target::x86_64::emit(
                        out_to, symbols, unit, no_stdlib);
                },
            m::pattern |
                "ir" = [&]() { credence::ir::emit(out_to, symbols, unit); },
            m::pattern | "ast" =
                [&]() {
                    if (result["symbols"].count())
                        out_to << "> Symbol Table:" << std::endl
                               << symbols << std::endl;
                    credence::frontend::ast::dump(out_to,
                        frontend.program.tree,
                        { .indices = verbose, .positions = verbose });
                },
            m::pattern | "hir" =
                [&]() {
                    if (result["symbols"].count())
                        out_to << "> Symbol Table:" << std::endl
                               << symbols << std::endl;
                    if (linear) {
                        for (auto definition : unit.definitions)
                            credence::frontend::hir::dump_linear(
                                out_to, unit, definition);
                        return;
                    }
                    credence::frontend::hir::dump(
                        out_to, unit, { .indices = verbose });
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
    } catch (cxxopts::exceptions::no_such_option const&) {
        std::cout
            << "Credence :: Invalid option, See \"--help\" for usage overview"
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
