## TinyKVM VMOD examples

This repository contains simple example programs for the KVM and TinyKVM VMODs.

[![C programs](https://github.com/varnish/kvm_demo/actions/workflows/c.yml/badge.svg)](https://github.com/varnish/kvm_demo/actions/workflows/c.yml)  [![C++ programs](https://github.com/varnish/kvm_demo/actions/workflows/cpp.yml/badge.svg)](https://github.com/varnish/kvm_demo/actions/workflows/cpp.yml)  [![Kotlin programs](https://github.com/varnish/kvm_demo/actions/workflows/kotlin.yml/badge.svg)](https://github.com/varnish/kvm_demo/actions/workflows/kotlin.yml)  [![Nelua programs](https://github.com/varnish/kvm_demo/actions/workflows/nelua.yml/badge.svg)](https://github.com/varnish/kvm_demo/actions/workflows/nelua.yml)  [![Nim programs](https://github.com/varnish/kvm_demo/actions/workflows/nim.yml/badge.svg)](https://github.com/varnish/kvm_demo/actions/workflows/nim.yml)  [![JavaScript programs](https://github.com/varnish/kvm_demo/actions/workflows/quickjs.yml/badge.svg)](https://github.com/varnish/kvm_demo/actions/workflows/quickjs.yml)  [![Rust programs](https://github.com/varnish/tinykvm_examples/actions/workflows/rust.yml/badge.svg)](https://github.com/varnish/tinykvm_examples/actions/workflows/rust.yml)  [![Zig programs](https://github.com/varnish/kvm_demo/actions/workflows/zig.yml/badge.svg)](https://github.com/varnish/kvm_demo/actions/workflows/zig.yml)

### About the VMOD

The KVM VMOD allows you to sandbox systems languages, allowing them to run natively on the CPU while Varnish is running safely. These languages are compiled down to machine code as regular Linux executables and archived. The archive is then published along with a library JSON file that describes each program and its limits. Libraries and programs may also be loaded from the local filesystem.

The VMOD focuses on data processing. That is, programs take inputs and generate HTTP response outputs with access to Varnish internals. It is also possible to do many other things like scheduling tasks at regular intervals and communicating with other non-HTTP services. The sandboxes have permission-based access to the local filesystem and Internet, allowing for things like database communication and as a programmable alternative to regular Varnish backends.

### Enable hardware virtualization

If you have not enabled VMX or SVM already in your BIOS, you will need to do so in order to run things like qemu-kvm and the KVM VMOD on your own machine. Most servers already have this enabled and can skip this step.

To see if you have enabled hardware virtualization, check:

- Generally:
  - Presence of `dev/kvm`: `stat /dev/kvm`
- Specific to AMD64:
  - Intel: `grep vmx /proc/cpuinfo`
  - AMD: `grep svm /proc/cpuinfo`

If you are running Linux inside a VM, such as VMware Player or VMware Workstation, you will need to enable nested virtualization:

- Enable VT-x/EPT virtualization in the Processors tab.

### Add your own user to KVM group

`vmod_compute` and `vmod_kvm` requires access to `/dev/kvm`. In order to give Varnish access, run this in a terminal:

```sh
sudo addgroup varnish kvm
```
You may need to fully log out and then in again in order for the change to take effect. It may not be enough to reopen the terminal.

### Documentation

You can find [the documentation here](http://89.162.68.187:5173/). It is still being written and may change daily.

There are several examples in the documentation, as well how to use the VMOD from VCL.

### Select your language

- C: Go into the [c folder](c) and edit [hello_world.c](c/hello_world.c)
- C++: Go into the [cpp folder](cpp) and edit [hello_world.cpp](cpp/src/hello_world.cpp)
- Go: Go into the [go folder](go) and edit [hello_world.go](go/varnish/hello_world.go)
- Kotlin: Go into the [kotlin folder](kotlin) and edit [main.kt](kotlin/main.kt)
- Nelua: Go into the [nelua folder](nelua) and edit [example.nelua](nelua/example.nelua)
- Nim: Go into the [nim folder](nim) and edit [hello.nim](nim/hello.nim)
- JavaScript: Go into the [javascript folder](javascript) and edit [chat.js](javascript/src/chat.js)
- Rust: Go into the [rust folder](rust) and edit [main.rs](rust/src/main.rs)
- Zig: Go into the [zig folder](zig) and edit [example.zig](zig/example.zig)

You can find the C API here: [kvm_api.h](kvm_api.h). It contains the complete API with some explanations and examples. The API is not necessarily intended for end-users, but it is required for those who wants to integrate other languages.

There are work-in-progress APIs for other languages as well, but they are not always fully fleshed out. The JavaScript API is for the most part written out in the beginning of the examples, which [you can find here](javascript/src/chat.js).

The APIs will be more fleshed out over time for each language, likely based on interest and demand. Each language has different pros and cons, strengths and weaknesses. For example, many programs don't require the highest possible performance and would work just fine with a scripting language.

### Example: Nelua

```lua
require 'varnish'
local image = #[embed '../assets/rose.jpg']#

local function on_get(url: string, arg: string)
    varnish.set("X-Language: Nelua 0.2.0-dev")
    varnish.response(200, "image/jpeg", image)
end

varnish.set_backend_get(on_get)
varnish.wait_for_requests()
```

### Significant demos

`cpp/avif` converts JPEG to AVIF, and if any error happens it will instead deliver the original JPEG.

`cpp/webp` converts JPEG or PNG to WebP, with support for cropping and resizing.

`cpp/gbc` is a self-hosting co-operative GameBoy Color emulator demo. It delivers a static website that communicates with the emulator and each viewer contributes inputs in order to create chaos. The demo has complex code in order to share the state of the emulator and the current frame with request VMs. There are two PNG frames, and the emulator employs double-buffering (rendering to one frame while presenting another). Each request VM enters storage in order to contribute inputs, while at the same time potentially scheduling new frames. New frames are generated asynchronously in storage, and read from shared memory by request VMs freeing up storage access. It uses requests to drive frame generation, resulting in no CPU usage when nobody is watching.

`cpp/src/espeak.cpp` generates voice audio based on input.

`cpp/src/minify.cpp` will minify JSON at 6GB/s.
