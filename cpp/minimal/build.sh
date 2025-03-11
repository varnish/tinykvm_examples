$CXX -static -O2 -ffreestanding -nostdlib -Wl,-Ttext-segment=0x40200000 storage.cpp -o storage
objcopy -w --extract-symbol --strip-symbol=!*storage* --strip-symbol=* storage storage.syms
strip --strip-all storage
$CXX -static -O2 -ffreestanding -nostdlib -Wl,-s,--just-symbols=storage.syms minimal.cpp -o minimal
rm -f storage.syms
