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

#include <filesystem>
#include <fstream>
#include <roxas/parse_tree.h>
#include <sstream>
#include <stdexcept>

// https://docs.python.org/3/extending/embedding.html#pure-embedding
#include <Python.h>

namespace roxas {

namespace fs = std::filesystem;

/**
 * @brief read source file
 *
 * @param path path to file
 * @return std::string
 */
std::string read_source_file(fs::path path)
{
    std::ifstream f(path, std::ios::in | std::ios::binary);
    const auto sz = fs::file_size(path);

    std::string result(sz, '\0');
    f.read(result.data(), sz);

    return result;
}

/**
 * @brief ParseTreeModuleLoader constructor
 *
 * Constructs object that interfaces with a compiler frontend in python
 *
 * @param module_path an absolute path to the frontend python module
 * @param module_name the module name as a string
 * @param file_path an absolute path to the source file to parse
 * @param env_path an optional absolute path to a venv directory where
 * dependecies are installed
 */
ParseTreeModuleLoader::ParseTreeModuleLoader(std::string const& module_path,
                                             std::string const& module_name,
                                             std::string const& file_path,
                                             std::string const& env_path)
    : module_path_(std::move(module_path))
    , module_name_(std::move(module_name))
    , file_path_(std::move(file_path))
{
    std::ostringstream python_path;
    python_path << "sys.path.append(\"" << module_path_ << "\")";

    if (not env_path.empty()) {
        python_path << std::endl << "sys.path.append(\"" << env_path << "\")";
    }

    // Initialize the Python Interpreter
    Py_Initialize();

    PyRun_SimpleString("import sys");
    PyRun_SimpleString(python_path.str().c_str());
}

/**
 * @brief
 *
 * Call a method on the parser module and provides the result as a
 * string
 *
 * @param method_name the method name
 * @return std::string result of method call
 */
std::string ParseTreeModuleLoader::call_method_on_module(
    std::string const& method_name)
{
    std::string ret{};
    PyObject *pModule, *pDict, *pFunc, *vModule;

    // Load the module object
    pModule = PyImport_ImportModule(module_name_.c_str());

    if (pModule == NULL) {
        throw std::runtime_error("memory error: failed to allocate pModule");
    }

    // pDict is a borrowed reference
    pDict = PyModule_GetDict(pModule);

    if (pDict == NULL) {
        Py_DECREF(pModule);
        throw std::runtime_error("memory error: failed to allocate pDict");
    }

    vModule = PyDict_GetItemString(pDict, "__name__");

    if (vModule == NULL) {
        Py_DECREF(pModule);
        Py_DECREF(pDict);
        throw std::runtime_error(
            "memory error: failed to allocate module name");
    }

    // pFunc is also a borrowed reference
    pFunc = PyDict_GetItemString(pDict, method_name.c_str());

    if (pFunc == NULL) {
        Py_DECREF(pModule);
        Py_DECREF(pDict);
        throw std::runtime_error("memory error: failed to allocate pFunc");
    }

    if (PyCallable_Check(pFunc)) {
        PyObject *pValue, *pArgs;
        pArgs = PyTuple_New(2);
        PyTuple_SetItem(
            pArgs,
            0,
            PyUnicode_FromString(read_source_file(file_path_).c_str()));
        PyTuple_SetItem(pArgs, 1, Py_True);

        pValue = PyObject_CallObject(pFunc, pArgs);
        if (pValue != NULL) {
            ret = std::string{ PyUnicode_AsUTF8(pValue) };
            Py_DECREF(pValue);
        } else {
            throw std::runtime_error("memory error: failed to allocate pValue");
        }
        if (pArgs != NULL) {
            Py_DECREF(pArgs);
        }
    } else {
        throw std::runtime_error(
            "error: pFunc not callable object in python interface");
    }
    Py_DECREF(pModule);
    Py_DECREF(pDict);
    Py_DECREF(pFunc);
    Py_DECREF(vModule);
    return ret;
}

/**
 * @brief clean up
 *
 */
ParseTreeModuleLoader::~ParseTreeModuleLoader()
{
    Py_Finalize();
}

} // namespace roxas
