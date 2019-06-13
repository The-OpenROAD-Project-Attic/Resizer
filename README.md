# Gate resizer

#### Installation

Expected directory layout

/resizer - this source tree
/lef - Si2 lef reader/writer
/def - Si2 def reader/writer
/flute - flute steiner tree package version 2.2
/opensta - OpenSTA

git clone https://parallax.xp-dev.com/git/resizer
cd resizer
mkdir build
cd build
cmake ..
make

The default build type is release to compile optimized code.
The resulting executable is in `build/resizer`.

Optional CMake variables passed as -D<var>=<value> arguments to CMake are show below.

CMAKE_BUILD_TYPE DEBUG|RELEASE

#### Running Resizer

```
resizer -help              show help and exit
        -version           show version and exit
        -no_init           do not read .sta init file
        -no_splash         do not show the license splash at startup
	cmd_file           source cmd_file and exit
```

Resizer looks for the files POWV9.dat and PORT9.dat in ../etc relative
to the executable when it starts.

Resizer sources the TCL command file ~/.resizer and enters interactive
command mode unless the command line option -no_init is specified.

The resizer is run using TCL scripts. All OpenSTA commands are available.
Addtional commands are

* read_lef filename
* read_def filename
* set_wire_rc [-resistance res ] [-capacitance cap] [-corner corner_name]
* resize [-resize]
	 [-repair_max_cap]
	 [-repair_max_slew]
	 [-buffer_cell buffer_cell]
* write_def filename

Liberty libraries should be read before LEF and DEF.  Only one LEF and
one DEF file are supported.  

The set_wire_rc command sets the resistance (ohms/meter) and
capacitance (farads/meter) of routing wires. It adds parasitics based
on placed component pin locations.  The resistance and capacitance are
per meter of a routing wire. They should represent "average" routing
layer resistance and capacitance.  If the set_wire_rc command is not
called before resizing, the default_wireload model specified in the
first liberty file or with the SDC set_wire_load command is used to
make parasitics.

The resize command resizes gates and then uses buffer insertion to
repair maximum capacitance and slew violations. Use the -resize,
-repair_max_cap and -repair_max_slew options to invoke a single
mode. With none of the options specified all are done.  The
-buffer_cell argument is required for buffer insertion
(-repair_max_cap or -repair_max_slew).

A typical resizer command file is shown below.

```
read_liberty nlc18.lib
read_lef nlc18.lef
read_def mea.def
read_sdc mea.sdc
set_wire_rc -resistance 1.67e+05 -capacitance 1.33e-10
resize -buffer_cell [get_lib_cell nlc18_worst/snl_bufx4]
write_def mea_resized.def
```

Note that OpenSTA commands can be used to report timing metrics before
or after the resizing.

```
set_wire_rc -resistance 1.67e+05 -capacitance 1.33e-10
report_checks
report_tns
report_wns
report_checks

resize

report_checks
report_tns
report_wns
```

## Authors

* James Cherry (coding)
* Lukas van Ginneken (algorithm)
* Chris Cho (flute steiner tree package)
