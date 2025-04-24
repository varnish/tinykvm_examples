PYVERSION=3.13.2
PYVERSION_SHORT=3.13

PFOLDER=$PWD/portable-python-cmake-buildsystem/Python-$PYVERSION
PINCLUDES="-I${PFOLDER}/Include -I${PFOLDER}/../.build/bin"
PLIBS=${PFOLDER}/../.build/lib
P=python$PYVERSION_SHORT
set -e

# This script builds a static version of Python 3.13.2 with the necessary libraries
pushd portable-python-cmake-buildsystem
mkdir -p .build
pushd .build
cmake -DCMAKE_BUILD_TYPE=Release .. -DPYTHON_VERSION="3.13.2" -DWITH_COMPUTED_GOTOS=ON -DCMAKE_BUILD_SHARED_LIBS=OFF
cmake --build . -j8
popd
popd

#set -x
gcc -O2 -o python_tinykvm \
    -I. ${PINCLUDES} \
	-rdynamic \
    -L${PLIBS} \
    python.c  \
    -l$P \
    -lcrypto \
    -lm -lpthread -ldl -lutil -lrt

export PYTHONPATH="$PFOLDER/../.build/lib/python$PYVERSION_SHORT"
export PYTHONHOME="$PWD"

# Run the program to test it. It should correctly load and run a small hello world program.
./python_tinykvm /home/gonzo/github/kvm_demo/python/program.py /home/gonzo/github/kvm_demo/python/program.py /home/gonzo/github/kvm_demo/python/program.py

cat << EOF
Copy this into your VCL configurations vcl_init:
*** Begin VCL Configuration ***
    tinykvm.configure("python",
        """{
            "filename": "$PWD/python_tinykvm",
			"concurrency": 4,
			"main_arguments": [
				"$PWD/program.py"
			],
            "allowed_paths": [
				"\$$PWD/portable-python-cmake-buildsystem/.build/lib/python3.13",
				"$PWD/program.py",
				"/"
            ],
			"environment": [
				"PYTHONHOME=$PWD",
				"PYTHONPATH=$PWD/portable-python-cmake-buildsystem/.build/lib/python3.13"
			]
        }""");
*** End VCL Configuration ***
EOF
