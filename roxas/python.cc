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

#include <Python.h>       // for PyObject_CallObject
#include <boolobject.h>   // for Py_False, Py_True
#include <dictobject.h>   // for PyDict_GetItemString
#include <import.h>       // for PyImport_ImportModule
#include <moduleobject.h> // for PyModule_GetDict
#include <object.h>       // for Py_DECREF, PyCallable_Check
#include <pylifecycle.h>  // for Py_Finalize, Py_Initialize
#include <pytypedefs.h>   // for PyObject
#include <roxas/python.h>
#include <sstream>         // for basic_ostringstream, ostringstream
#include <stddef.h>        // for NULL
#include <stdexcept>       // for runtime_error
#include <tupleobject.h>   // for PyTuple_SetItem, PyTuple_New
#include <unicodeobject.h> // for PyUnicode_FromString

#include "pythonrun.h"     // for PyRun_SimpleString
#include "unicodeobject.h" // for PyUnicode_AsUTF8

namespace roxas {

/**
 * @brief PythonModuleLoader constructor
 *
 * @param module_name the module name as a string
 */
PythonModuleLoader::PythonModuleLoader(std::string_view module_name)
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
 *
 * @param method_name the method name
 * @param args initializer list of arguments to pass to the python method
 * @return std::string result of method call
 */
std::string PythonModuleLoader::call_method_on_module(
    std::string_view method_name,
    std::initializer_list<std::string> args)
{
    std::string ret{};
    PyObject *pModule, *pDict, *pFunc, *vModule;

    // Load the module object
    pModule = PyImport_ImportModule(module_name_.data());

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
    pFunc = PyDict_GetItemString(pDict, method_name.data());

    if (pFunc == NULL) {
        Py_DECREF(pModule);
        Py_DECREF(pDict);
        throw std::runtime_error("memory error: failed to allocate pFunc");
    }

    if (PyCallable_Check(pFunc)) {
        PyObject *pValue, *pArgs;
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
 */
PythonModuleLoader::~PythonModuleLoader()
{
    Py_FinalizeEx();
}

} // namespace roxas
