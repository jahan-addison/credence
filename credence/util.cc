
#include <credence/util.h>

#include <credence/assert.h>   // for credence_error
#include <credence/ir/ita.h>   // for ITA
#include <credence/ir/table.h> // for Table
#include <filesystem>          // for file_size, path
#include <format>              // for format
#include <fstream>             // for basic_ofstream, basic_ifstream
#include <iostream>            // for cout
#include <ostream>             // for operator<<, basic_ostream
#include <sstream>             // for basic_ostringstream

namespace credence {

namespace util {

/**
 * @brief Create and write to a file by one std::ostream to another
 */
void write_file_to_path_from_stringstream(
    std::string_view file_name,
    std::ostringstream const& oss,
    std::string_view ext)
{
    if (file_name == "stdout") {
        std::cout << oss.str();
    } else {
        std::ofstream file_(std::format("{}.{}", file_name, ext));
        if (file_.is_open()) {
            file_ << oss.str();
            file_.close();
        } else {
            credence_error(std::format("Error creating file: `{}`", file_name));
        }
    }
}

/**
 * @brief Emit ita instructions to an std::ostream
 */
void emit_partial_ita(
    std::ostream& os,
    AST_Node const& symbols,
    AST_Node const& ast)
{
    using namespace credence::ir;
    auto instructions =
        Table::build_from_ast(symbols, ast)->build_from_ita_instructions();
    ITA::emit(os, instructions);
}

/**
 * @brief Emit ita instructions type and vector boundary checking an
 * and to an std::ostream Passes the global symbols from the ITA object
 */
void emit_complete_ita(
    std::ostream& os,
    AST_Node const& symbols,
    AST_Node const& ast)
{
    using namespace credence::ir;
    auto ita = ITA{ symbols };
    auto instructions = ita.build_from_definitions(ast);
    auto table = Table{ symbols, instructions };
    table.build_vector_definitions_from_globals(ita.globals_);
    table.build_from_ita_instructions();
    ITA::emit(os, table.instructions);
}

/**
 * @brief read a file from a fs::path
 */
std::string read_file_from_path(std::string_view path)
{
    std::ifstream f(path.data(), std::ios::in | std::ios::binary);
    const auto sz = fs::file_size(path);

    std::string result(sz, '\0');
    f.read(result.data(), sz);

    return result;
}

} // namespace util

} // namespace credence