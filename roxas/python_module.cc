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
#include <roxas/python_module.h>
#include <sstream>
#include <stdexcept>

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
 * @brief PythonModuleLoader constructor
 *
 * Constructs object that interfaces with libpython
 *
 * @param module_path an absolute path to a python module
 * @param module_name the module name as a string
 * @param env_path an optional absolute path to a venv directory where
 * dependecies are installed
 */
PythonModuleLoader::PythonModuleLoader(std::string_view module_path,
                                       std::string_view module_name,
                                       std::string const& env_path)
    : module_path_(module_path)
    , module_name_(module_name)
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
 * Call a method on the python module and return the result as a
 * string
 *
 * @param method_name the method name
 * @param args initializer list of arguments to pass to the python method
 * @return std::string result of method call
 */
std::string call_method_on_module(std::string_view method_name,
                                  std::initializer_list<std::string> args);
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
        int pIndex = 0;
        pArgs = PyTuple_New(args.size());
        // construct the arguments from the initializer list `args'
        for (std::string arg const& : args) {
            if (arg == "true") {
                PyTuple_SetItem(pArgs, pIndex, Py_True);
            } else if (arg == "false") {
                PyTuple_SetItem(pArgs, pIndex, Py_False);
            } else {
                PyTuple_SetItem(pArgs, index, PyUnicode_FromString(arg));
            }
            index++;
        }
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
PythonModuleLoader::~PythonModuleLoader()
{
    Py_Finalize();
}

} // namespace roxas
