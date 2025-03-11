set -v
$CXX -static -O2 -Wl,-Ttext-segment=0x40200000 storage.cpp -o storage
objcopy -w --extract-symbol --strip-symbol=!*storage* --strip-symbol=* storage storage.syms
$CXX -static -O2 -Wl,-s,--just-symbols=storage.syms collector.cpp -o collector
rm -f storage.syms
strip --strip-all storage collector
