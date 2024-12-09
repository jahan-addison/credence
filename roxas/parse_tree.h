#pragma once

// https://github.com/hackable-devices/polluxnzcity/blob/master/PolluxGateway/include/pollux/pollux_extension.h

#include <string>
#include <string_view>

namespace roxas {

namespace detail {
/**
 * @brief
 *
 * Data structure of parse tree.
 *
 */
struct ParseTree
{
    std::string source;
};

} // namespace detail

/**
 * @brief
 *
 * Parse tree loader via python interface to the python compiler frontend.
 *
 */
class ParseTreeLoader
{
  public:
    ParseTreeLoader(ParseTreeLoader const&) = delete;
    ParseTreeLoader(std::string_view file_path)
        : file_path_(file_path);

    void parse();

    ~ParseTreeLoader();

  private:
    std::string_view file_path_;
    detail::ParseTree tree_{};
};

} // namespace roxas
