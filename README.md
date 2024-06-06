MTB daemon
==========

MTB daemon is a simple console computer application intended to be run as
a service. It features 2 interfaces:

 * MTB-USB v4 connection via USB CDC
 * json tcp server

Aim of this daemon is

 1. to allow multiple applications to control single MTBbus
 2. to provide nice communication API with MTBbus.

JSON server features API for setting MTBbus modules state, getting MTBbus modules
state as well as configuring modules, changing MTBbus speed etc.

[TCP protocol description](tcp-protocol/README.md)

## Program workflow

When program starts, configuration file `mtb-daemon.json` is read. If the file
does not exists, it is created. JSON server is started. When server cannot be
started, program dies. Main application loop is started.

Daemon searches for available MTB-USB modules. If it finds module specified
in configuration, it connects. It reports changes to clients and allows clients
to control MTBbus and MTBbus modules. When connection with MTB-USB fails,
daemon reports this event to clients and keeps running. When MTB-USB is
available again, connection is established again.

## Using

Just start the console application and let it run as a daemon on background.

```bash
$ ./mtb-daemon [config-filename]
```

The program takes a single optional argument – path to main configuration file
(default: `./mtb-daemon.json`).

See [Description of mtb-daemon.json file](doc.mtb-daemon.json.md).

## Building & toolkit

This library was developed in `vim` using `qmake` & `make`. It is suggested
to use `clang` as a compiler, because then you may use `clang-tools` (see below).

### Prerequisities

 * Qt 5/6
 * Qt's `serialport`
 * [Bear](https://github.com/rizsotto/Bear)

### Example: toolchain setup on Debian 12 Bookworm

```bash
$ apt install qt6-base-dev qt6-serialport-dev
$ apt install bear
$ apt install clang clang-tools clang-tidy clang-format
```

### Build

Clone this repository:

```
$ git clone https://github.com/kmzbrnoI/mtb-daemon
```

And then build:

```
$ mkdir build
$ cd build
$ qmake -spec linux-clang ..
$ # qmake CONFIG+=debug -spec linux-clang ..
$ bear make
```

## Compiling for Windows

Just open the project in *Qt Creator* and compile it. This approach is currently
used to build windows binaries in releases.

## Testing

This repository contains tests for MTB Daemon. See
[README in `test` directory](test).

## Style checking

```bash
$ clang-tidy -p build src/*.cpp
$ clang-format <all .cpp and .h files in the 'src' directory>
$ clang-include-fixer -p build src/*.cpp
```

For `clang-include-fixer` to work, it is necessary to [build `yaml` symbols
database](https://clang.llvm.org/extra/clang-include-fixer.html#creating-a-symbol-index-from-a-compilation-database).
You can do it this way:

 1. Download `run-find-all-symbols.py` from
    [github repo](https://github.com/microsoft/clang-tools-extra/blob/master/include-fixer/find-all-symbols/tool/run-find-all-symbols.py).
 2. Execute it in `build` directory.

## Authors

This library was created by:

 * Jan Horacek ([jan.horacek@kmz-brno.cz](mailto:jan.horacek@kmz-brno.cz))

Do not hesitate to contact author in case of any troubles!

## License

This application is released under the [Apache License v2.0
](https://www.apache.org/licenses/LICENSE-2.0).
