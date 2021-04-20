MTB daemon
==========

**TODO TODO text taken from RCS XN library, rewrite**

## Building & toolkit

This library was developed in `vim` using `qmake` & `make`. It is suggested
to use `clang` as a compiler, because then you may use `clang-tools` (see below).

### Prerequisities

 * Qt 5
 * Qt's `serialport`
 * [Bear](https://github.com/rizsotto/Bear)

### Toolchain setup on debian

```bash
$ apt install qt5-default libqt5serialport5-dev
$ apt install bear
$ apt install clang-7 clang-tools-7 clang-tidy-7 clang-format-7
```

### Build

Clone this repository:

```
$ git clone TODO
```

And then build:

```
$ mkdir build
$ cd build
$ qmake -spec linux-clang .. / qmake CONFIG+=debug -spec linux-clang ..
$ bear make
```

## Cross-compiling for Windows

This library could be cross-compiled for Windows via [MXE](https://mxe.cc/).
Follow [these instructions](https://stackoverflow.com/questions/14170590/building-qt-5-on-linux-for-windows)
for building standalone `dll` file.

You may want to use similar script as `activate.sh`:

```bash
$ export PATH="$HOME/...../mxe/usr/bin:$PATH"
$ ~/...../mxe/usr/i686-w64-mingw32.static/qt5/bin/qmake ..
```

Make MXE this way:

```bash
$ make qtbase qtserialport
```

## Style checking

```bash
$ cd src
$ clang-tidy-7 -p ../build -extra-arg-before=-x -extra-arg-before=c++ -extra-arg=-std=c++14 -header-filter=src/ *.cpp
$ clang-format-7 *.cpp *.h
$ clang-include-fixer-7 -p ../build *.cpp
```

For `clang-include-fixer` to work, it is necessary to [build `yaml` symbols
database](https://clang.llvm.org/extra/clang-include-fixer.html#creating-a-symbol-index-from-a-compilation-database).
You can do it this way:

 1. Download `run-find-all-symbols.py` from [github repo](https://github.com/microsoft/clang-tools-extra/blob/master/include-fixer/find-all-symbols/tool/run-find-all-symbols.py).
 2. Execute it in `build` directory.

## Authors

This library was created by:

 * Jan Horacek ([jan.horacek@kmz-brno.cz](mailto:jan.horacek@kmz-brno.cz))

Do not hesitate to contact author in case of any troubles!

## License

This application is released under the [Apache License v2.0
](https://www.apache.org/licenses/LICENSE-2.0).
