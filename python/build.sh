PYVERSION=3.13.2
PYVERSION_SHORT=3.13

PFOLDER=~/github/python-cmake-buildsystem/Python-$PYVERSION
PINCLUDES="-I${PFOLDER}/Include -I${PFOLDER}/../.build/bin"
PLIBS=${PFOLDER}/../.build/lib
P=python$PYVERSION_SHORT

#set -x
gcc -O2 -static -o python_tinykvm \
    -I. ${PINCLUDES} \
    -L${PLIBS} \
    python.c  \
    -l$P \
    -lcrypto \
    -lm -lpthread -ldl -lutil -lrt

export PYTHONPATH="$PFOLDER/../.build/lib/python$PYVERSION_SHORT"
export PYTHONHOME="$PWD"

./python_tinykvm
