# Python in VMOD TinyKVM

Python is well supported in TinyKVM.

# Build settings

Python can be built statically using the magnificent [python-cmake-buildsystem](https://github.com/python-cmake-buildsystem/python-cmake-buildsystem). Build it somewhere on your machine, and point to it from this folders [build.sh](build.sh).

# Run-time settings

Once you have the static Python executable, you will need a customized compute.json incantation. Here is an example from my machine:

```json
{
	"filename": "/home/gonzo/github/kvm_demo/python/python_tinykvm",
	"main_arguments": [
		"/home/gonzo/github/kvm_demo/python/program.py"
	],
	"allowed_paths": [
		"$/home/gonzo/github/python-cmake-buildsystem/.build/lib/python3.9",
		"/home/gonzo/github/kvm_demo/python/program.py",
		"/"
	],
	"environment": [
		"PYTHONHOME=/home/gonzo/github/kvm_demo/python",
		"PYTHONPATH=/home/gonzo/github/python-cmake-buildsystem/.build/lib/python3.9"
	]
}
```

We're passing the program as part of main arguments, and allowing access to Python modules in `allowed_paths`. We also have to tell Python about where to find the modules, which path can be set in the `PYTHONPATH` environment variable.

Why Python needs access to `/` is still a mystery, but probably not something that is dangerous to do. It is read-only access, and is not used as prefix for other accesses.
