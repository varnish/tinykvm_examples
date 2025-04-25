/**
 * Copyright (c) 2025 Varnish Software AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
**/
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "../kvm_api.h"

// A function that produces a HTTP response ala backend_response_str
static PyObject* pyv_backend_response_str(PyObject* self, PyObject* args) {
	const char* content_type;
	const char* content;
	int status;
	if (!PyArg_ParseTuple(args, "iss", &status, &content_type, &content)) {
		return NULL;
	}
	backend_response_str(status, content_type, content);
	Py_RETURN_NONE;
}
// And a variant with a byte array
static PyObject* pyv_backend_response(PyObject* self, PyObject* args) {
	const char* content_type;
	const char* content;
	int status;
	Py_ssize_t content_len;
	if (!PyArg_ParseTuple(args, "iss#", &status, &content_type, &content, &content_len)) {
		return NULL;
	}
	backend_response(status, content_type, strlen(content_type), content, content_len);
	Py_RETURN_NONE;
}

// Add a function that waits for requests as a return value
static PyObject* pyv_wait_for_requests(PyObject* self, PyObject* args) {
	// The backend_request struct contains the following fields:
	// 1. method - The HTTP method (GET, POST, etc.)
	// 2. url - The URL of the request
	// 3. arg - Extra arguments to the program (eg. JSON config)
	// 4. content_type - The content type of the request
	// 5. content - The content of the request (if any)
	// 6. content_len - The length of the content (if any)
	// 7. method_len - The length of the method
	// 8. url_len - The length of the URL
	// 9. arg_len - The length of the arguments
	// 10. content_type_len - The length of the content type
	// We will take content as a binary type (y*) and *NOT* a string (s)
	struct backend_request req;
	wait_for_requests_paused(&req);
	PyObject* result = Py_BuildValue(
		"{s:s, s:s, s:s, s:s, s:y#}",
		"method", req.method,
		"url", req.url,
		"arg", req.arg,
		"content_type", req.content_type,
		"content", req.content, req.content_len
	);
	return result;
}

// A function that logs a string to the Varnish log
static PyObject* pyv_log(PyObject* self, PyObject* args) {
	const char* message;
	if (!PyArg_ParseTuple(args, "s", &message)) {
		return NULL;
	}
	sys_log(message, strlen(message));
	Py_RETURN_NONE;
}

// Set a HTTP header field as a string (we assume where == BERESP)
static PyObject* pyv_http_set_str(PyObject* self, PyObject* args) {
	const int where = BERESP;
	const char* field;
	if (!PyArg_ParseTuple(args, "s", &field)) {
		return NULL;
	}
	http_set_str(where, field);
	Py_RETURN_NONE;
}
// Unset a HTTP header field by key
static PyObject* pyv_http_unset_str(PyObject* self, PyObject* args) {
	const int where = BERESP;
	const char* key;
	if (!PyArg_ParseTuple(args, "s", &key)) {
		return NULL;
	}
	http_unset_str(where, key);
	Py_RETURN_NONE;
}
// Retrieve a header field by key from BEREQ (the request)
static PyObject* pyv_http_find_str(PyObject* self, PyObject* args) {
	const int where = BEREQ;
	const char* key;
	char buffer[HTTP_FMT_SIZE];
	if (!PyArg_ParseTuple(args, "s", &key)) {
		return NULL;
	}
	const unsigned len = http_find_str(where, key, buffer, sizeof(buffer));
	if (len == 0) {
		Py_RETURN_NONE;
	}
	PyObject* result = Py_BuildValue("s#", buffer, len);
	return result;
}

static PyMethodDef VarnishMethods[] = {
	{"backend_response_str", pyv_backend_response_str, METH_VARARGS, "Send a backend response."},
	{"backend_response", pyv_backend_response, METH_VARARGS, "Send a backend response with a byte array."},
	{"wait_for_requests", pyv_wait_for_requests, METH_VARARGS, "Wait for requests."},
	{"log", pyv_log, METH_VARARGS, "Log a message to the Varnish log."},
	{"http_get", pyv_http_find_str, METH_VARARGS, "Get a HTTP header field from the request."},
	{"http_set", pyv_http_set_str, METH_VARARGS, "Set a HTTP header field in the response."},
	{"http_unset", pyv_http_unset_str, METH_VARARGS, "Unset a HTTP header field in the response."},
	{NULL, NULL, 0, NULL}
};
static struct PyModuleDef varnish_module = {
	PyModuleDef_HEAD_INIT,
	"varnish",  // name of module
	NULL,       // module documentation, may be NULL
	-1,         // size of per-interpreter state of the module,
				// or -1 if the module keeps state in global variables.
	VarnishMethods
};
PyMODINIT_FUNC PyInit_varnish(void) {
	PyObject* m;
	m = PyModule_Create(&varnish_module);
	if (m == NULL)
		return NULL;
	return m;
}

int main(int argc, char** argv)
{
	// Initialize the Python interpreter
	PyImport_AppendInittab("varnish", PyInit_varnish);
	wchar_t *program = Py_DecodeLocale(argv[0], NULL);
	PyConfig config;
	PyConfig_InitPythonConfig(&config);

	PyStatus status = PyConfig_SetString(&config, &config.program_name, program);
	if (PyStatus_Exception(status)) {
		fprintf(stderr, "PyConfig_SetString failed\n");
		PyMem_RawFree(program);
		return 1;
	}

	status = Py_InitializeFromConfig(&config);
	if (PyStatus_Exception(status)) {
		fprintf(stderr, "Py_InitializeFromConfig failed\n");
		PyMem_RawFree(program);
		return 1;
	}
	PyImport_ImportModule("varnish");

	printf("Python version: %s\n", Py_GetVersion());
	PyRun_SimpleString(R"(
print('Hello Python Compute World')
	)");

	if (IS_LINUX_MAIN()) {
		Py_Finalize();
		return 0;
	}

	// Open the main Python file
	FILE *fp = fopen(argv[3], "r");
	if (fp == NULL) {
		fprintf(stderr, "Could not open Python file: %s\n", argv[3]);
		Py_Finalize();
		return 1;
	}
	// Read the file into a string
	char *python_file_string = NULL;
	long length = 0;
	if (fseek(fp, 0, SEEK_END) == 0) {
		length = ftell(fp);
		python_file_string = malloc(length + 1);
	}
	if (python_file_string == NULL) {
		fprintf(stderr, "Could not allocate memory for Python file\n");
		fclose(fp);
		Py_Finalize();
		return 1;
	}
	if (fseek(fp, 0, SEEK_SET) != 0) {
		fprintf(stderr, "Could not seek to beginning of Python file\n");
		free(python_file_string);
		fclose(fp);
		Py_Finalize();
		return 1;
	}
	if (fread(python_file_string, 1, length, fp) != length) {
		fprintf(stderr, "Could not read Python file\n");
		free(python_file_string);
		fclose(fp);
		Py_Finalize();
		return 1;
	}
	python_file_string[length] = '\0';
	fclose(fp);

	printf("*** Python code ***\n%s*** End of Python code ***\n", python_file_string);
	fflush(stdout);

	// Precompile the string as Python code object
	// We want to use optimization level 2
	PyObject* pCode = Py_CompileStringExFlags(python_file_string, argv[3], Py_file_input, NULL, 2);
	if (pCode == NULL) {
		PyErr_Print();
		free(python_file_string);
		Py_Finalize();
		return 1;
	}
	// Execute the code object
	PyObject* pModule = PyImport_ExecCodeModule("varnish", pCode);
	if (pModule == NULL) {
		PyErr_Print();
		Py_XDECREF(pCode);
		free(python_file_string);
		Py_Finalize();
		return 1;
	}
	// Clean up
	Py_XDECREF(pCode);
	free(python_file_string);
	// Call the main function in the module
	PyObject* pFunc = PyObject_GetAttrString(pModule, "main");
	if (pFunc == NULL) {
		PyErr_Print();
		Py_XDECREF(pModule);
		Py_Finalize();
		return 1;
	}
	// Pass argv arguments to the main function
	PyObject* pArgs = PyTuple_Pack(3, argv[1], argv[2], argv[3]);
	if (pArgs == NULL) {
		PyErr_Print();
		Py_XDECREF(pFunc);
		Py_XDECREF(pModule);
		Py_Finalize();
		return 1;
	}
	PyObject* pValue = PyObject_CallObject(pFunc, pArgs);
	if (pValue == NULL) {
		PyErr_Print();
		Py_XDECREF(pArgs);
		Py_XDECREF(pFunc);
		Py_XDECREF(pModule);
		Py_Finalize();
		return 1;
	}
	Py_XDECREF(pValue);
	Py_XDECREF(pArgs);
	Py_XDECREF(pFunc);
	Py_XDECREF(pModule);
	// Finalize the Python interpreter
	Py_Finalize();
}
