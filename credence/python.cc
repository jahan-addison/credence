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

#include <credence/python.h>

#include <Python.h>        // for PyObject_CallObject
#include <boolobject.h>    // for Py_False, Py_True
#include <dictobject.h>    // for PyDict_GetItemString
#include <import.h>        // for PyImport_ImportModule
#include <moduleobject.h>  // for PyModule_GetDict
#include <object.h>        // for Py_DECREF, PyCallable_Check
#include <pylifecycle.h>   // for Py_Finalize, Py_Initialize
#include <pytypedefs.h>    // for PyObject
#include <sstream>         // for basic_ostringstream, ostringstream
#include <stddef.h>        // for NULL
#include <tupleobject.h>   // for PyTuple_SetItem, PyTuple_New
#include <unicodeobject.h> // for PyUnicode_FromString

namespace credence {

/**
 * @brief Python_Module_Loader constructor
 */
Python_Module_Loader::Python_Module_Loader(std::string_view module_name)
    : module_name_(module_name)
{
    std::ostringstream python_path;
    // Initialize the Python Interpreter
    Py_InitializeEx(0);

    PyRun_SimpleString("import sys");
    PyRun_SimpleString(python_path.str().c_str());
}

/**
 * @brief Call a method on the python module and return the result
 */
std::string Python_Module_Loader::call_method_on_module(
    std::string_view method_name,
    std::initializer_list<std::string> args)
{
    std::string ret{};
    PyObject* pModule = NULL;
    PyObject* pDict = NULL;
    PyObject* pFunc = NULL;
    PyObject* vModule = NULL;

    // Load the module object
    pModule = PyImport_ImportModule(module_name_.data());
    if (!pModule) {
        python_loader_error();
    }
    refs_.push(pModule);

    // pDict is a borrowed reference
    pDict = PyModule_GetDict(pModule);
    if (!pDict) {
        python_loader_error();
    }
    refs_.push(pDict);

    vModule = PyDict_GetItemString(pDict, "__name__");
    if (!vModule) {
        python_loader_error();
    }
    refs_.push(vModule);

    // pFunc is also a borrowed reference
    pFunc = PyDict_GetItemString(pDict, method_name.data());
    if (!pFunc) {
        python_loader_error();
    }
    refs_.push(pFunc);

    if (PyCallable_Check(pFunc)) {
        PyObject* pValue = NULL;
        PyObject* pArgs = NULL;
        int index = 0;
        pArgs = PyTuple_New(args.size());
        // construct the arguments from the initializer list `args'
        for (std::string const& arg : args) {
            if (arg == "true") {
                PyTuple_SetItem(pArgs, index, Py_True);
            } else if (arg == "false") {
                PyTuple_SetItem(pArgs, index, Py_False);
            } else {
                PyTuple_SetItem(
                    pArgs, index, PyUnicode_FromString(arg.c_str()));
            }
            index++;
        }
        pValue = PyObject_CallObject(pFunc, pArgs);
        refs_.push(pValue);
        if (pValue != NULL) {
            refs_.push(pValue);
            ret = std::string{ PyUnicode_AsUTF8(pValue) };
        } else {
            python_loader_error();
        }
        if (pArgs != NULL) {
            refs_.push(pArgs);
        }
    } else {
        python_loader_error();
    }
    free_refs();
    return ret;
}

/**
 * @brief clean up
 */
Python_Module_Loader::~Python_Module_Loader()
{
    Py_FinalizeEx();
}

} // namespace credence
