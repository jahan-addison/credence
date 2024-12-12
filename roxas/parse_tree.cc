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

#include <iostream>
#include <roxas/parse_tree.h>
#include <sstream>
#include <stdexcept>

// https://docs.python.org/3/extending/embedding.html#pure-embedding
#include <Python.h>

namespace roxas {

/**
 * @brief ParseTreeLoader constructor
 *
 * Constructs an object that interfaces with a compiler frontend in python
 *
 * @param module_path an absolute path to the frontend python module
 * @param file_path an absolute path to the source file to parse
 */
ParseTreeLoader::ParseTreeLoader(const char* module_path, const char* file_path)
{
    std::ostringstream python_path;
    tree_.location = std::string{ file_path };
    module_path_ = std::string{ module_path };

    python_path << "sys.path.append(\"" << module_path << "\")";

    // Initialize the Python Interpreter
    Py_Initialize();

    PyRun_SimpleString("import sys");
    PyRun_SimpleString(python_path.str().c_str());
}

/**
 * @brief
 *
 * Parse a source program and provides the parse tree as a string
 *
 * @return std::string_view parse tree as a readable string
 */
std::string ParseTreeLoader::get_parse_tree_as_string_from_module(
    std::string_view module_name)
{
    std::string ret{};
    PyObject *pModule, *pDict, *pFunc, *pValue, *pArgs, *vModule;

    // Load the module object
    pModule = PyImport_ImportModule(module_name.data());

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
    pFunc = PyDict_GetItemString(pDict, "parse_source_program_as_string");

    if (pFunc == NULL) {
        Py_DECREF(pModule);
        Py_DECREF(pDict);
        throw std::runtime_error("memory error: failed to allocate pFunc");
    }

    if (PyCallable_Check(pFunc)) {
        pArgs = PyUnicode_FromString(tree_.location.c_str());
        pValue = PyObject_CallObject(pFunc, pArgs);
        if (pValue != NULL) {
#if VERBOSE
            std::cout << PyUnicode_AsUTF8(pValue) << std::endl;
#endif
            ret = std::string{ PyUnicode_AsUTF8(pValue) };

            Py_DECREF(pValue);
            return ret;
        } else {
            PyErr_Print();
        }
        if (pArgs != NULL) {
            Py_DECREF(pArgs);
        }
    } else {
        // give warning
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
ParseTreeLoader::~ParseTreeLoader()
{
    Py_Finalize();
}

} // namespace roxas
