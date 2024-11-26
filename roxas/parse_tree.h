
#pragma once

#include <string_view>
// https://docs.python.org/3/extending/embedding.html#pure-embedding
#include <Python.h>

std::string get_parse_tree(std::string const& file_path);
