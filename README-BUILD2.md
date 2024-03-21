# CHERIoT RTOS `build2` support prototype

Note: you will need to use the [latest staged `build2`
version](https://build2.org/community.xhtml#stage) until 0.17.0 is released.

Building the example in source (add `-v` to see the compilation/linking
command lines, configuration report, etc):

```
$ cd cheriot-rtos/examples/01.hello_world
$ b config.cxx=/cheriot-tools/bin/clang++ \
    config.cheriot_rtos.board=ibex-safe-simulator
```

To configure the example (so that we don't have to repeat `config.*`):

```
$ b configure \
    config.cxx=/cheriot-tools/bin/clang++ \
    config.cheriot_rtos.board=ibex-safe-simulator
$ b
```

To build a user project (that will presumably not be inside `cheriot-rtos/`):

```
$ cp -r cheriot-rtos/examples/01.hello_world user-project
$ cd user-project
$ b config.cxx=/cheriot-tools/bin/clang++ \
    config.import.cheriot_rtos=../cheriot-rtos \
    config.cheriot_rtos.board=ibex-safe-simulator
```

To configure an out of source build of the user project (can have multiple
out of source builds, for example, for different boards, different
`cheriot-rtos` and/or `cheriot-tools` versions):

```
$ b configure: ./@../user-project-ibex/ \
    config.cxx=/cheriot-tools/bin/clang++ \
    config.import.cheriot_rtos=../cheriot-rtos \
    config.cheriot_rtos.board=ibex-safe-simulator

$ b ../user-project-ibex/
```

To configure the source directory to forward to one of the out of source
builds (so can build it form the source directory):

```
$ b configure: ./@../user-project-ibex/,forward
$ b
```

See [`b(1)`](https://build2.org/build2/doc/b.xhtml) for details on the
command line syntax.
