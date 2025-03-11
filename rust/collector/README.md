# mandelbrot-simd-rs
Rust implementation of mandelbrot set with **simd**(Single instruction, multiple data) using packed-simd crate.

It's recommended to add following to [cargo config](https://doc.rust-lang.org/cargo/reference/config.html)
```
rustflags = ["-C", "target-cpu=native"]
```
This will ensure AVX/AVX2 is used when compiling. (only applicable to x86 machines, that support AVX/AVX2)

To compile: run
```
cargo build --release
```
<img src="https://github.com/chamatht/mandelbrot-simd-rs/raw/master/image.png" width="500">
