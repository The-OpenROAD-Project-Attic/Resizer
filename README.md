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
resizer
  -help              show help and exit
  -version           show version and exit
  -no_init           do not read .sta init file
  -no_splash         do not show the license splash at startup
  -exit              exit after reading cmd_file
  cmd_file           source cmd_file
```

Resizer looks for the files POWV9.dat and PORT9.dat in ../etc relative
to the executable when it starts.

Resizer sources the TCL command file ~/.resizer unless the command
line option -no_init is specified.

Resizer then sources the command file cmd_file. Unless the -exit
command line flag is specified it enters and interactive TCL command
interpreter.

The resizer is run using TCL scripts. All OpenSTA commands are available.
Addtional commands are shown below.

```
read_lef filename
read_def filename
set_wire_rc [-resistance res ] [-capacitance cap] [-corner corner_name]
resize [-resize]
       [-repair_max_cap]
       [-repair_max_slew]
       [-buffer_cell buffer_cell]
write_def [-units dist_units]
          [-die_area {lx ly ux uy}]
          [-site site_name]
          [-auto_place_pins]
          filename
```

Liberty libraries should be read before LEF and DEF. Only one DEF file
is supported.

The set_wire_rc command sets the resistance (ohms/meter) and
capacitance (farads/meter) of routing wires. It adds parasitics based
on placed component pin locations. The resistance and capacitance are
per meter of a routing wire. They should represent "average" routing
layer resistance and capacitance. If the set_wire_rc command is not
called before resizing, the default_wireload model specified in the
first liberty file or with the SDC set_wire_load command is used to
make parasitics.

The resize command resizes gates and then uses buffer insertion to
repair maximum capacitance and slew violations. Use the -resize,
-repair_max_cap and -repair_max_slew options to invoke a single
mode. With none of the options specified all are done. The
-buffer_cell argument is required for buffer insertion
(-repair_max_cap or -repair_max_slew).

The write_def command -units, -die_area, -site and -auto_place_pins
arguments are only used when writing a DEF file from a Verilog
netlist. dist_units are the DEF database units per micron. The die
area is a list of corner coordinates in microns (lower x, lower y,
upper x, upper y). site_name is a LEF site name that is used to write
ROW statements to fill the die area.  The -auto_place_pins argument
adds locations for the pins equally spaced around the die perimeter.

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

#### Verilog to DEF

An example command script to translate DEF to Verilog is shown below.

```
read_liberty liberty1.lib
read_lef liberty1.lef
read_verilog reg1.v
link_design top
write_def -units 100 -die_area {0 0 1000 1000} -site site1 -auto_place_pins reg1.def
```

The "verilog2def" executable can also be used to translate DEF to
Verilog using command line arguments.

```
verilog2def
  [-help]                   show help and exit
  [-version]                show version and exit
  -liberty liberty_file     liberty library
  -lef lef_file             lef_file for site size
  -verilog verilog_file     
  -top_module module_name   verilog module to expand
  -units units              def units per micron
  [-die_area "lx ly ux uy"] die area in microns
  [-site site_name]         
  [-auto_place_pins]        
  -def def_file             def file to write
```

Multiple Liberty files can be read by using multiple -liberty keywords.

## Authors

* James Cherry (coding)
* Lukas van Ginneken (algorithm)
* Chris Cho (flute steiner tree package)
