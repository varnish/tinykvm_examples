PYVERSION=3.13.2
PYVERSION_SHORT=3.13
PFOLDER=$PWD/portable-python-cmake-buildsystem/Python-$PYVERSION

export PYTHONPATH="$PFOLDER/../.build/lib/python$PYVERSION_SHORT"
export PYTHONHOME="$PWD"

# Run the program to test it. It should correctly load and run a small hello world program.
./python_tinykvm
