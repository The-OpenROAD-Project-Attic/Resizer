# Gate resizer

#### Installation

Expected directory layout

/resizer - this source tree
/lef - lef reader/writer
/def - def reader/writer
/flute - flute steiner tree package
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

Resizer looks for the files ../etc/POWV9.dat and ../etc/PORT9.dat relative
to the executable when it starts.

The resizer is run using TCL scripts. All OpenSTA commands are available.
Addtional commands are

* read_lef filename
* read_def filename
* resize [-wire_res_per_length res]
         [-wire_cap_per_length cap]
         [-corner corner_name]
* write_def filename

Liberty libraries should be read before LEF and DEF.
Only one LEF and one DEF file are supported.
The res (ohms/meter) and cap (farads/meter) args are used to add parasitics
based on placed component locations.
A typical resizer command file is shown below.

```
source helpers.tcl
read_liberty nlc18.lib
read_lef nlc18.lef
read_def mea.def
source mea.sdc
resize
write_def mea_sized.def
```
To run this example use the following commands.

```
cd test
../resizer -f resize_mea1.tcl
```

Note that OpenSTA commands can be used to report timing metrics before or after
the resizing.

```
puts "TNS before = [format %.2f [total_negative_slack]]"
puts "WNS before = [format %.2f [worst_negative_slack]]"

resize

puts "TNS before = [format %.2f [total_negative_slack]]"
puts "WNS before = [format %.2f [worst_negative_slack]]"
```

## Authors

* James Cherry
* Lukas van Ginneken
