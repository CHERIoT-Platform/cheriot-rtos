CHERIoT RTOS is a real-time operating system designed specifically for CHERIoT hardware.
It uses CHERIoT's unique features to provide very strong, efficient isolation between program components called 'compartments'.
Compartments are used to protect internal OS components such as the scheduler, interrupt handler and memory allocator and may also be used by application code to increase robustness, security and isolate untrusted third party components.

These pages contain API reference documentation generated from the RTOS sources, as well as high-level documentation of some OS concepts and features that are unique to CHERIoT RTOS.

# Getting Started

For a quick guide to building applications with the RTOS see the [getting started guide](GettingStarted.md).
TODO: include platform specific gettings started guides and link to template project.

# High-level concepts

See the [related pages](pages.html) for some documentation of key features and high level concepts of CHERIoT RTOS. 
Of particular interest may be:

 - [The architecture page](architecture.md) gives a quick birds-eye view of the RTOS structure and defines some key concepts.
 - For a description of compartmentalisation see [chapter 5 of the CHERioT Programmers' Guide](https://cheriot.org/book/compartments.html).
 - [Debugging](Debugging.md) describes the debug printing and tracing features.
 - [Error handling](ErrorHandling.md) describes how to add error handlers to your compartments.

# Language support and libraries

CHERIoT supports code written in C, C++ (without exceptions), Rust or assembly, all of which benefit from the hardware memory safety features of CHERIoT. 
The core RTOS APIs are designed to be compatible with C, although there are often C++ wrappers that provide a more idiomatic interface for C++ code.
By convention C header files use the `.h` extenion and C++ headers use `.hh`.
Subsets of the C and C++ standard libraries are included: just include standard headers as you normally would and link the relevant libraries (TODO document this).
Rust support is currently [work in progress](https://rust.cheriot.org).

A compatibility layer for the core FreeRTOS APIs [is available](#sdk/include/FreeRTOS-Compat).
For information on porting code from FreeRTOS see [the programmers' guide](https://cheriot.org/book/porting_from_freertos.html).

For full API documentation see the [files page](files.html).

# Examples

For examples of RTOS usage see:

 - the [examples directory](https://github.com/CHERIoT-Platform/cheriot-rtos/tree/main/examples) of the CHERIoT RTOS repo
 - the [CHERIoT demos repo](https://github.com/CHERIoT-Platform/cheriot-demos/)
 - the [CHERIoT network stack examples](https://github.com/CHERIoT-Platform/network-stack/tree/main/examples)

# Further reading

For further information about CHERIoT see the following sources:

 - The CHERIoT Programmers' Guide [HTML](https://cheriot.org/book/) [PDF](https://cheriot.org/book/cheriot-programmers-guide.pdf): a thorough guide to CHERIoT, including the ISA, programming model and RTOS. Also available in epub and paper formats.
 - [The CHERIoT Architecture Specification](https://github.com/CHERIoT-Platform/cheriot-sail/releases/download/v1.0/cheriot-architecture-v1.0.pdf): the low level specification of the CHERIoT instruction set architecture (ISA) and application binary interface (ABI) used by the toolchain.
 - [CHERIoT News](https://cheriot.org/news.html): blog of CHERIoT news including some deep dives into RTOS features.
 - [CHERIoT Publications](https://cheriot.org/publications.html): an index of CHERIoT-related academic publications.
