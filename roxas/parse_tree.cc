
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

#include <roxas/parse_tree.h>
#include <stdexcept>

// https://docs.python.org/3/extending/embedding.html#pure-embedding
#include <Python.h>

namespace roxas {

/**
 * @brief Construct a new Parse Tree Loader:: Parse Tree Loader object
 *
 * @param module_path an absolute path to the frontend python module
 * @param file_path an absolute path to the source file to parse
 */
ParseTreeLoader::ParseTreeLoader(std::string& module_path,
                                 std::string& file_path)
{
    tree_.location = std::move(file_path);
    module_path_ = std::move(module_path);

    // Initialize the Python Interpreter
    Py_Initialize();

    PyRun_SimpleString("import sys");
    PyRun_SimpleString(tree_.location.c_str());
}

std::string_view ParseTreeLoader::get_parse_tree_as_string()
{
    const char* ret;
    PyObject *pName, *pModule, *pDict, *pFunc, *pValue, *pArgs,
        *pValues = NULL, *pConfig = NULL, *k, *v;
    // Build the name object
    pName = PyUnicode_FromString(module_path_.c_str());

    // Load the module object
    pModule = PyImport_Import(pName);

    if (pModule == NULL) {
        Py_DECREF(pName);
        throw std::runtime_error("memory error: failed to allocate pModule");
    }

    // pDict is a borrowed reference
    pDict = PyModule_GetDict(pModule);

    if (pDict == NULL) {
        Py_DECREF(pName);
        Py_DECREF(pModule);
        throw std::runtime_error("memory error: failed to allocate pDict");
    }
    // pFunc is also a borrowed reference
    pFunc = PyDict_GetItemString(pDict, "parse_source_program_as_string");

    if (pFunc == NULL) {
        Py_DECREF(pName);
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
            ret = PyUnicode_AsUTF8(pValue);
            Py_DECREF(pValue);
            return ret;
        }
        if (pArgs != NULL) {
            Py_DECREF(pArgs);
        }
    }
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
