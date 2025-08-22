# Python in VMOD TinyKVM

Python is well supported in TinyKVM.

# Build settings

Python can be built statically using the magnificent [python-cmake-buildsystem](https://github.com/bjia56/portable-python-cmake-buildsystem). Build it somewhere on your machine, and point to it from this folders [build.sh](build.sh).

# Run-time settings

Once you have the static Python executable, you will need a customized compute.json incantation. Here is an example from my machine:

```json
{
	"filename": "/home/gonzo/github/kvm_demo/python/python_tinykvm",
	"concurrency": 4,
	"main_arguments": [
		"/home/gonzo/github/kvm_demo/python/program.py"
	],
	"current_working_directory": "/home/gonzo/github/kvm_demo/python",
	"allowed_paths": [
		{
			"real": "/lib/x86_64-linux-gnu",
			"prefix": true
		}, {
			"real": "/home/gonzo/github/kvm_demo/python",
			"prefix": true
		}
	],
	"environment": [
		"PYTHONHOME=/home/gonzo/github/kvm_demo/python",
		"PYTHONPATH=/home/gonzo/github/kvm_demo/python/portable-python-cmake-buildsystem/.build/lib/python3.13"
	]
}
```

The [build.sh](build.sh) will also generate the JSON for you, that matches your machine.

We're passing the program as part of main arguments, and allowing access to Python modules in `allowed_paths`. We also have to tell Python about where to find the modules, which path can be set in the `PYTHONPATH` environment variable.
