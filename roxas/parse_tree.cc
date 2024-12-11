
#include <roxas/parse_tree.h>

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

    // Initialize the Python Interpreter
    Py_Initialize();

    PyRun_SimpleString("import sys");
    PyRun_SimpleString(tree_.location.c_str());
}

std::string_view ParseTreeLoader::get_parse_tree_as_string()
{
    PyObject *pName, *pModule, *pDict, *pFunc, *pValue, *pArgs,
        *pValues = NULL, *pConfig = NULL, *k, *v;

    int ret = 0;

    // Build the name object
    pName = PyString_FromString(module.c_str());

    // Load the module object
    pModule = PyImport_Import(pName);

    if (pModule == NULL) {
        Py_DECREF(pName);
        return 0;
    }

    // pDict is a borrowed reference
    pDict = PyModule_GetDict(pModule);

    if (pDict == NULL) {
        Py_DECREF(pName);
        Py_DECREF(pModule);
        return 0;
    }

    // pFunc is also a borrowed reference
    pFunc = PyDict_GetItemString(pDict, "push_to_datastore");

    if (pFunc == NULL) {
        Py_DECREF(pName);
        Py_DECREF(pModule);
        Py_DECREF(pDict);
        return 0;
    }
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
