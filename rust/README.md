# Rust in VMOD TinyKVM

Rust is well supported in TinyKVM.

# Build settings

Rust requires the `crt-static` feature, which produces static PIE executables. Optionally, adding `link_arg=-no-pie` will make the executable fully static.

```sh
RUSTFLAGS="-C target-feature=+crt-static" cargo build ...
$ file target/release/demo
demo: ELF 64-bit LSB pie executable, x86-64, version 1 (GNU/Linux), static-pie linked, BuildID[sha1]=7540c7e40170350d30f183e01a1907b7c21eab39, for GNU/Linux 3.2.0, not stripped
```

# Special JSON settings

Rust does not need any special settings to be able to boot.
