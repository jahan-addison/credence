#pragma once

// https://github.com/hackable-devices/polluxnzcity/blob/master/PolluxGateway/include/pollux/pollux_extension.h

#include <string>

namespace roxas {

namespace detail {
/**
 * @brief
 *
 * Parse tree data structure.
 *
 */
struct ParseTree
{
    std::string source{};
    std::string location{};
};

} // namespace detail

/**
 * @brief
 *
 * Parse tree loader via python interface to the python compiler frontend
 *
 */
class ParseTreeLoader
{
  public:
    ParseTreeLoader(ParseTreeLoader const&) = delete;
    /**
     * @brief Construct a new Parse Tree Loader:: Parse Tree Loader object
     *
     * @param module_path an absolute path to the frontend module
     * @param file_path an absolute path to the source file to parse
     */
    ParseTreeLoader(std::string& module_path, std::string& file_path);

    /**
     * @brief clean up
     *
     */
    ~ParseTreeLoader();

  public:
    std::string_view get_parse_tree_as_string();

  private:
    detail::ParseTree tree_{};
};

} // namespace roxas
