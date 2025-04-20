PFOLDER=~/github/python-cmake-buildsystem/Python-3.9.17
PINCLUDES="-I${PFOLDER}/Include -I${PFOLDER}/../.build/bin"
PLIBS=${PFOLDER}/../.build/lib
P=${PLIBS}/libpython3.9.a

#set -x
gcc -O2 -static -o python_tinykvm \
    -I. ${PINCLUDES} \
    -L${PLIBS} \
    python.c  \
    -lpython3.9 \
    -lcrypto \
    -lm -lpthread -ldl -lutil -lrt

export PYTHONPATH="$PFOLDER/../.build/lib/python3.9"
export PYTHONHOME="$PWD"

./python_tinykvm
