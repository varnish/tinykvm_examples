#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "../kvm_api.h"

int main(int argc, char** argv)
{
    Py_Initialize();
    PyRun_SimpleString("print('Hello Python World')");

    if (IS_LINUX_MAIN()) {
        Py_Finalize();
        return 0;
    }

    while (true) {
        struct backend_request req;
        wait_for_requests_paused(&req);

        backend_response_str(200, "text/plain", "Hello Python World\n");
    }
    Py_Finalize();
}
