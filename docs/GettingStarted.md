Getting started
===============

This repository contains a dev container that installs the toolchain and the gold model emulator.
The easiest way to use this is with GitHub Code Spaces: simply press . to open an editor with all of the tools set up.
You can achieve the same locally by cloning the repository and opening it in VS Code with the Dev Container Extension installed.

If you do not wish to use the dev container then please read the [section on how to build the dependencies](#building-the-dependencies).

The dev container installs the toolchain and emulator in `/cheriot-tools/bin`.
If you have installed them somewhere else then replace `/cheriot-tools/` with your install location in the following instructions.

Cloning the repository
----------------------

This repository contains submodules and so must be cloned with:

```sh
$ git clone --recurse https://github.com/microsoft/cheriot-rtos
```

Building the test suite
-----------------------

To make sure that everything is working, a good first step is to build the test suite.
If you are using the dev container with VS Code / GitHub Code Spaces, then this is the default target and so can be built by running the `XMake: Build` command from the command box.

Whether you are building the test suite in the dev container or elsewhere, you can build it from the command line as well.
The build system requires a configure step and a build step:

```sh
$ cd tests
$ xmake config --sdk=/cheriot-tools/
$ xmake
```

To get more verbose output, you can try adding `--debug-{loader,allocator,scheduler}=y` to the `xmake config` flags.
These will each turn on (very) verbose debugging output from the named components.

One of the final lines in the output should be something like:

```
[ 96%]: linking firmware build/cheriot/cheriot/release/test-suite
```

This tells you the path to the firmware image.
You can now try running it in the simulator:

```sh
$ /cheriot-tools/bin/cheriot_sim -V build/cheriot/cheriot/release/test-suite
```

The `-V` flag disables instruction-level tracing so that you can see the UART output more clearly.

Running the examples
--------------------

There are several examples in the [`examples/`](../examples/) directory of the repository.
These show how to use individual features of the platform.
Each of these is built and run in exactly the same way as the test suite.
For more detailed instructions [see the examples documentation](../examples/README.md).

Generating `compile_commands.json`
----------------------------------

The `clangd` language server protocol implementation depends on a `compile_commands.json` file to tell it the various flags for building.
If you are using the dev container then this is generated automatically but in other scenarios you must create it yourself from the test suite's build by running the following commands after you have configured the test suite:

```sh
$ cd tests
$ xmake project -k compile_commands ..
```

Building the dependencies
-------------------------

If you do not wish to use the dev container, you will need to build:

 - The LLVM-based toolchain with CHERIoT support
 - The emulator generated from the Sail formal model of the CHERIoT ISA
 - The xmake build tool

Building LLVM is fairly simple, but requires a fast machine and several GiBs of disk space.
Building the executable model requires a working ocaml installation.

### Building CHERIoT LLVM

To build LLVM, you will need `cmake` and `ninja` installed from your distribution's packaging system.
For example on Ubuntu Linux distributions you would need to run (as root):

```sh
# apt install ninja-build
# snap install cmake --classic
```

On FreeBSD:

```sh
# pkg ins cmake ninja
```

The version of LLVM that you need is in the `cheriot` branch of the [CHERI LLVM repository](https://github.com/CHERIoT-Platform/llvm-project/tree/cheriot).
This branch is temporary and will eventually be merged into the main CHERI LLVM branch and upstreamed to LLVM.

First, clone this repo somewhere convenient:

```sh
$ git clone --depth 1 https://github.com/CHERIoT-Platform/llvm-project cheriot-llvm
$ cd cheriot-llvm
$ git checkout cheriot
$ export LLVM_PATH=$(pwd)
```

If you want to do a custom build of LLVM, follow [their build instructions and adjust any configuration options that you want](https://www.llvm.org/docs/CMake.html).
If you want to just build something that works, keep reading.

Next create a directory for the build and enter it:

```sh
$ mkdir -p builds/cheriot-llvm
$ cd builds/cheriot-llvm
```

This can be inside the source checkout but you may prefer to build somewhere else, for example on a different filesystem.
Next, use `cmake` to configure the build:

```sh
$ cmake ${LLVM_PATH}/llvm -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_PROJECTS="clang;clang-tools-extra;lld" -DCMAKE_INSTALL_PREFIX=install -DLLVM_ENABLE_UNWIND_TABLES=NO -DLLVM_TARGETS_TO_BUILD=RISCV -DLLVM_DISTRIBUTION_COMPONENTS="clang;clangd;lld;llvm-objdump;llvm-objcopy" -G Ninja
```

It is very strongly recommended that you do a release build, debug builds can take several minutes to compile files that take seconds with release builds.
The `LLVM_DISTRIBUTION_COMPONENTS` flag will let us build only the components that we want.
You can change the `install` location to somewhere else, for example `~/cheriot-tools` if you want a more memorable path.

Finally, build the toolchain:

```sh
$ export NINJA_STATUS='%p [%f:%s/%t] %o/s, %es'
$ ninja install-distribution
```

The first line here is optional, it will give you a more informative progress indicator.
At the end of this process, you will have the toolchain files installed in the location that you passed as the `CMAKE_INSTALL_PREFIX` option to `cmake` (the `install` directory inside your build directory if you didn't change the `cmake` line).
This will include:

 - `clang` / `clang++`, the C/C++ compiler.
 - `ld.lld`, the linker.
 - `llvm-objdump`, the tool for creating human-readable dumps of object code.
 - `clangd`, the language-server protocol implementation that is aware of our C/C++ extensions.

#### Configuring your editor

If your editor supports the language-server protocol then you should tell it to use the version of `clangd` that you have just built.
There are more ways of doing this than there are editors and so this is not an exhaustive set of instructions.
The `clangd` binary to use with any of them will be `bin/clangd` inside your LLVM install location.

If you are using VS Code, then you can install the [clangd extension](https://github.com/clangd/vscode-clangd).
Open its settings and find the entry labeled `Clangd: Path`.
This should be set to your newly built `clangd` location.

Vim and NeoVim have a number of language-server-protocol implementations.
The [ALE](https://github.com/dense-analysis/ale) extension has separate configuration options for C and C++.
You may wish to set these to the CHERIoT `clangd` only for paths that match a pattern where your CHERIoT code will live, by adding something like the following to your `.vimrc`:

```vim
autocmd BufNewFile,BufRead /home/myuser/cheriot/* let g:ale_c_clangd_executable = "/my/cheriot/llvm/install/bin/clangd"
autocmd BufNewFile,BufRead /home/myuser/cheriot/* let g:ale_cpp_clangd_executable = "/my/cheriot/llvm/install/bin/clangd"
```

### Building the emulator

The emulator is generated from a Sail formal model.
Sail is an ISA specification language that is implemented in ocaml.
None of the ocaml components (or any dependencies other than `libgmp.so`) are needed after the build and so you may prefer to run this build in a disposable container and just extract the emulator at the end.

The first step in building it is to install the dependencies, including your platform's version of `opam`, the ocaml package manager.
On Ubuntu:

```sh
# apt install opam z3 libgmp-dev cvc4
```

On FreeBSD:

```sh
# pkg ins ocaml-opam z3 gmp gmake pkgconf gcc
```

You can then install Sail with `opam`:

```sh
$ opam init --yes
$ opam install --yes sail
```

Now clone the CHERIoT Sail repository:

```sh
$ git clone --depth 1 --recurse https://github.com/microsoft/cheriot-sail
```

Make sure that all of the relevant opam environment variables are set and build the model:

```
$ cd cheriot-sail
$ eval $(opam env)
$ make patch_sail_riscv
$ make csim
```

Note that this is a GNU Make build system, if you are running this on a non-GNU platform then GNU Make may be called `gmake` or similar and not `make`.

This will produce an executable in `c_emulator/cheriot_sim`.
This can be copied to another location such as somewhere in your path.

### Installing xmake

There are a lot of different ways of installing xmake and you should follow [the instructions that best match your platform](https://xmake.io/#/getting_started?id=installation).
For Ubuntu, you can do:

```sh
# add-apt-repository ppa:xmake-io/xmake
# apt update
# apt install xmake
```

Running on the Arty A7
----------------------

We previously ran the test suite in the simulator.
Let us now run it on the Arty A7 FPGA development board.

We will first build the CHERIoT [small and fast FPGA emulator](https://github.com/microsoft/cheriot-safe) (SAFE) configuration and load it onto the FPGA.
Then, we will build, install, and run the CHERIoT RTOS firmware on the board.

### Building and Installing the SAFE FPGA Configuration

We will add documentation for this part later.
In the meantime, our [blog post](https://cheriot.org/fpga/try/2023/11/16/cheriot-on-the-arty-a7.html) provides pointers on how to do this.

### Building, Copying, and Running the Firmware

We have now configured the FPGA.
The LD4 LED on the FPGA board should be blinking green.
We are ready to build, copy, and run the firmware.

#### Building the Firmware

We first need to reconfigure the build, and rebuild.
For this example, we'll show rebuilding the test suite, but the same set of steps should work for any CHERIoT RTOS project (try the examples!):

```sh
$ cd tests
$ xmake config --sdk=/cheriot-tools/ --board=ibex-arty-a7-100
$ xmake
```

Note that `/cheriot-tools` is the location in the dev container.
This value may vary if you did not use the dev container and must be the directory containing a `bin` directory with your LLVM build in it.

Then, we need to build the firmware.
This repository comes with a script to do this:

```sh
$ ../scripts/ibex-build-firmware.sh build/cheriot/cheriot/release/test-suite
```

The `./firmware` directory should now contain a firmware file `cpu0_iram.vhx`.
This is the firmware we want copy onto the FPGA development board.

#### Installing and Running the Firmware

To copy the firmware onto the FPGA board, we will use minicom, which you can obtain through your your distribution's packaging system.
For example on Ubuntu Linux distributions you would need to run (as root):

```sh
# apt install minicom
```

Now, plug the FPGA development board to your computer.
We need to identify which serial device we will be using.
On Linux, we do this by looking at the dmesg output:

```sh
$ sudo dmesg | grep tty
(...)
[19966.674679] usb 1-4: FTDI USB Serial Device converter now attached to ttyUSB1
```

The most recent lines of this command appear when plugging and unplugging your FPGA board, and indicate which serial device corresponds to the board.
Here, it is `ttyUSB1`.

Now we can open minicom (replace `ttyUSB1` with the serial device you just determined):

```sh
$ sudo minicom -c on -D ttyUSB1
Welcome to minicom 2.8

OPTIONS: I18n
Port /dev/ttyUSB1, 13:51:28

Press CTRL-A Z for help on special keys

Ready to load firmware, hold BTN0 to ignore UART input.
```

Hitting the RESET button on the FPGA should produce the "Ready to load firmware..." line, which is the output from the loader on the FPGA.

The "Press CTRL-A Z for help on special keys" message tells you which meta key is configured on your system.
Here, the meta key is `CTRL-A`.
The meta key varies across systems (the default on macOS is `<ESC>`) and configurations and so we refer to it as `<META>`.

We must now configure a few things:
- Hit `<META>` + `U` to turn carriage return `ON`
- Hit `<META>` + `W` to turn linewrap `ON`
- Hit `<META>` + `O`, then select `Serial Port Setup`, to ensure that `Bps/Par/Bits` (E) is set to `115200 8N1`, F to L on `No`, and M and N on `0`.

In particular, make sure that hardware and software flow control are *off*.
On macOS, the kernel silently ignores these if they are not supported but on Linux the kernel will refuse to send data unless the flow control is in the correct state.
Unfortunately, the hardware flow control lines in the Arty A7's UART are not physically connected to the USB controller.

We can now send our firmware to the FPGA.
Hit `<META>` + `Y`, and select the `cpu0_iram.vhx` file we produced earlier.
Minicom should now start outputing:

```
Ready to load firmware, hold BTN0 to ignore UART input.
Starting loading.  First word was: 40812A15
..
```

Minicom may block after printing a small number of dots.
If it does, then it will resume if you press any key that would be sent over the serial link.
Each dot represents 1 KiB of transmitted data.

Once the firmware is fully loaded, the test suite will start executing:

```
Ready to load firmware, hold BTN0 to ignore UART input.
Starting loading.  First word was: 40812A15
.............................................................................................
............
Finished loading.  Last word was: 0300012C
Number of words loaded to IRAM: 00006892
Loaded firmware, jumping to IRAM.

Test runner: Checking that rel-ro caprelocs work.  This will crash if they don't.  CHERI Perm
issions are:
Test runner: Global(0x0)
...
```
