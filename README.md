# Gate resizer

#### Installation

Expected directory layout

/resizer - this source tree
/lef - lef reader/writer
/def - def reader/writer
/opensta - OpenSTA

git clone https://parallax.xp-dev.com/git/resizer
cd resizer
mkdir build
cd build
cmake ..
make

The default build type is release to compile optimized code.
The resulting executable is in `resizer`.

Optional CMake variables passed as -D<var>=<value> arguments to CMake are show below.

CMAKE_BUILD_TYPE DEBUG|RELEASE

#### Running Resizer

The resizer is run using TCL scripts. All OpenSTA commands are available.
Addtional commands are

* read_lef
* read_def
* resize

Liberty libraries should be read before LEF and DEF.
See /test for examples.

...
cd test
../resizer -f resize1.tcl
...

## Authors

* James Cherry
* Lukas van Ginneken
