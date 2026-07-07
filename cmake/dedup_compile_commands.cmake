# Keeps the first compile-command entry per unique "file", since a source
# compiled into more than one target (e.g. a library and its test binary)
# otherwise gets linted once per target.
file(READ "${SOURCE_COMPILE_COMMANDS}" _json)
string(JSON _len LENGTH "${_json}")

set(_seen_files "")
set(_out "[]")
set(_out_len 0)
math(EXPR _last "${_len} - 1")
foreach(_i RANGE ${_last})
  string(JSON _entry GET "${_json}" ${_i})
  string(JSON _file GET "${_entry}" file)
  list(FIND _seen_files "${_file}" _seen_at)
  if(_seen_at EQUAL -1)
    list(APPEND _seen_files "${_file}")
    string(JSON _out SET "${_out}" ${_out_len} "${_entry}")
    math(EXPR _out_len "${_out_len} + 1")
  endif()
endforeach()

file(WRITE "${OUTPUT_COMPILE_COMMANDS}" "${_out}")
