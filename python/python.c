#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "../kvm_api.h"

// Add a custom function to the Python interpreter that produces
// a HTTP response ala backend_response_str
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

static PyMethodDef VarnishMethods[] = {
	{"backend_response_str", pyv_backend_response_str, METH_VARARGS, "Send a backend response."},
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
	Py_SetProgramName(argv[0]);
	Py_Initialize();
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

	// Wait for requests
    while (true) {
        struct backend_request req;
        wait_for_requests_paused(&req);

		PyRun_SimpleString(python_file_string);
    }
    Py_Finalize();
}
