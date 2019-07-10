// Resizer, LEF/DEF gate resizer
// Copyright (c) 2019, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <inttypes.h>
#include <cmath> 		// sqrt
#include <stdlib.h>
#include "Machine.hh"
#include "Report.hh"
#include "ReportStd.hh"
#include "Debug.hh"
#include "StringUtil.hh"
#include "LefReader.hh"
#include "LibertyReader.hh"
#include "VerilogReader.hh"
#include "DefWriter.hh"
#include "LefDefNetwork.hh"
#include "ResizerConfig.hh" // RESIZER_VERSION
#include "StaMain.hh" // findCmdLineKey, findCmdLineFlag
#include "Sta.hh" // initSta

// ../build/verilog2def -lef liberty1.lef -liberty liberty1.lib -verilog reg1.v -top_module top -units 100 -die_area "0 0 1000 1000" -site site1 -auto_place_pins -def results/foo.def
// -lef ../test/liberty1.lef -liberty ../test/liberty1.lib -verilog ../test/reg1.v -top_module top -units 100 -utilization 30 -aspect_ratio 1.0 -core_space 2 -site site1 -auto_place_pins -verbose -def ../test/results/foo.def

using std::string;
using sta::StringVector;
using sta::Report;
using sta::Debug;
using sta::makeReportStd;
using sta::isDigits;
using sta::findCmdLineKey;
using sta::findCmdLineFlag;
using sta::LefDefNetwork;
using sta::initSta;
using sta::StaException;

static void
showUsage(const char *prog);
static double
metersToMicrons(double meters);
static double
micronsToMeters(double microns);
static double
parseFloat(const char *token,
	   const char *arg_name,
	   Report *report);
static int
parseInt(const char *token,
	 const char *arg_name,
	 Report *report);

int
main(int argc,
     char *argv[])
{
  initSta();
  if (findCmdLineFlag(argc, argv, "-help")) {
    showUsage(argv[0]);
    exit(EXIT_SUCCESS);
  }

  if (findCmdLineFlag(argc, argv, "-version")) {
    printf("verilog2def %s\n", RESIZER_VERSION);
    exit(EXIT_SUCCESS);
  }

  Report *report = makeReportStd();
  bool errors = false;
  bool verbose = findCmdLineFlag(argc, argv, "-verbose");

  StringVector liberty_filenames;
  const char *liberty_filename = findCmdLineKey(argc, argv, "-liberty");
  if (liberty_filename == nullptr) {
    report->printError("Error: missing -liberty argument.\n");
    errors = true;
  }
  else {
    do {
      liberty_filenames.push_back(liberty_filename);
      liberty_filename = findCmdLineKey(argc, argv, "-liberty");
    } while (liberty_filename);
  }

  const char *lef_filename = findCmdLineKey(argc, argv, "-lef");
  if (lef_filename == nullptr) {
    report->printError("Error: missing -lef argument.\n");
    errors = true;
  }

  const char *verilog_filename = findCmdLineKey(argc, argv, "-verilog");
  if (verilog_filename == nullptr) {
    report->printError("Error: missing -verilog argument.\n");
    errors = true;
  }

  const char *top_module = findCmdLineKey(argc, argv, "-top_module");
  if (top_module == nullptr) {
    report->printError("Error: missing -top_module argument.\n");
    errors = true;
  }

  const char *units_str = findCmdLineKey(argc, argv, "-units");
  int units = 0;
  if (units_str == nullptr) {
    report->printError("Error: missing -units argument.\n");
    errors = true;
  }
  else {
    if (!isDigits(units_str))
      report->printError("Error: -units is not a positiveinteger\n");
    units = parseInt(units_str, "-units", report);
  }

  double die_lx = 0.0;
  double die_ly = 0.0;
  double die_ux = 0.0;
  double die_uy = 0.0;
  double core_lx = 0.0;
  double core_ly = 0.0;
  double core_ux = 0.0;
  double core_uy = 0.0;

  const char *die_area = findCmdLineKey(argc, argv, "-die_area");
  if (die_area) {
    string die_area1(die_area);
    StringVector die_tokens;
    split(die_area1, " ,", die_tokens);
    if (die_tokens.size() != 4)
      report->printWarn("Warning: -die_area should be a list of 4 coordinates\n");
    // microns to meters.
    die_lx = micronsToMeters(parseFloat(die_tokens[0].c_str(), "-die_area", report));
    die_ly = micronsToMeters(parseFloat(die_tokens[1].c_str(), "-die_area", report));
    die_ux = micronsToMeters(parseFloat(die_tokens[2].c_str(), "-die_area", report));
    die_uy = micronsToMeters(parseFloat(die_tokens[3].c_str(), "-die_area", report));
  }

  const char *core_area = findCmdLineKey(argc, argv, "-core_area");
  if (core_area) {
    string core_area1(core_area);
    StringVector core_tokens;
    split(core_area1, " ,", core_tokens);
    if (core_tokens.size() != 4)
      report->printWarn("Warning: -core_area should be a list of 4 coordinates\n");
    // microns to meters.
    core_lx = micronsToMeters(parseFloat(core_tokens[0].c_str(), "-core_area", report));
    core_ly = micronsToMeters(parseFloat(core_tokens[1].c_str(), "-core_area", report));
    core_ux = micronsToMeters(parseFloat(core_tokens[2].c_str(), "-core_area", report));
    core_uy = micronsToMeters(parseFloat(core_tokens[3].c_str(), "-core_area", report));
  }

  const char *utilization = findCmdLineKey(argc, argv, "-utilization");
  const char *aspect_ratio = findCmdLineKey(argc, argv, "-aspect_ratio");
  const char *core_space = findCmdLineKey(argc, argv, "-core_space");
  const char *site_name = findCmdLineKey(argc, argv, "-site");
  const char *tracks_file = findCmdLineKey(argc, argv, "-tracks");
  bool auto_place_pins = findCmdLineFlag(argc, argv, "-auto_place_pins");

  const char *def_filename = findCmdLineKey(argc, argv, "-def");
  if (def_filename == nullptr) {
    report->printError("Error: missing -def argument.\n");
    errors = true;
  }

  if (!errors) {
    Debug debug(report);
    LefDefNetwork network;
    network.initState(report, &debug);

    try {
      bool first = true;
      for (auto liberty_filename : liberty_filenames) {
	if (verbose) {
	  if (!first)
	    report->print("\n");
	  report->print("Reading liberty %s...", liberty_filename.c_str());
	}
	readLibertyFile(liberty_filename.c_str(), false, &network);
	first = false;
      }
      if (verbose)
	report->print("\nReading LEF %s...", lef_filename);
      readLef(lef_filename, &network);

      if (verbose)
	report->print("\nReading verilog %s...", verilog_filename);
      readVerilogFile(verilog_filename, &network);

      if (verbose)
	report->print("\nLinking...");
      network.linkNetwork(top_module, true, report);

      if (utilization) {
	double util = parseFloat(utilization, "-utilization", report) / 100.0;
	double aspect_ratio1 = aspect_ratio ? strtod(aspect_ratio, nullptr) : 1.0;
	double core_sp = 0.0;
	if (core_space)
	  core_sp = micronsToMeters(parseFloat(core_space, "-core_space", report));
	double design_area = network.designArea();
	double core_area = design_area / util;
	double core_width = std::sqrt(core_area / aspect_ratio1);
	double core_height = core_width / aspect_ratio1;

	core_lx = core_sp;
	core_ly = core_sp;
	core_ux = core_sp + core_width;
	core_uy = core_sp + core_height;
	if (verbose)
	  report->print("\nCore size ( %.0fum %.0fum ) ( %.0fum %.0fum )",
			metersToMicrons(core_lx),
			metersToMicrons(core_ly),
			metersToMicrons(core_ux),
			metersToMicrons(core_uy));
	die_lx = 0.0;
	die_ly = 0.0;
	die_ux = core_width + core_sp * 2.0;
	die_uy = core_height + core_sp * 2.0;
	if (verbose)
	  report->print("\nDie size ( %.0fum %.0fum ) ( %.0fum %.0fum )",
			metersToMicrons(die_lx),
			metersToMicrons(die_ly),
			metersToMicrons(die_ux),
			metersToMicrons(die_uy));
      }

      if (verbose)
	report->print("\nWriting DEF %s...", def_filename);
      writeDef(def_filename, units,
	       die_lx, die_ly, die_ux, die_uy,
	       core_lx, core_ly, core_ux, core_uy,
	       site_name, tracks_file, auto_place_pins, true,
	       &network);
      if (verbose)
	report->print("\n");
    }
    catch (StaException &excp) {
      report->printError("Error: %s\n", excp.what());
      errors = true;
    }
  }
  else
    showUsage(argv[0]);
  exit(errors ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
showUsage(const char *prog)
{
  printf("Usage %s\n", prog);
  printf("  [-help]                    show help and exit\n");
  printf("  [-version]                 show version and exit\n");
  printf("  [-verbose]                 report progress\n");
  printf("  -liberty liberty_file      liberty for linking verilog\n");
  printf("  -lef lef_file              lef_file for site size\n");
  printf("  -verilog verilog_file      \n");
  printf("  -top_module module_name    verilog module to expand\n");
  printf("  -units units               def units per micron\n");
  printf("\n");
  printf("  -utilization util          utilization (0-100 percent)\n");
  printf("  [-aspect_ratio ratio]      height / width (default 1.0)\n");
  printf("  [-core_space space]        space around core (microns)\n");
  printf("  or\n");
  printf("  -die_area \"lx ly ux uy\"   die area in microns\n");
  printf("  -core_area \"lx ly ux uy\"  core area in microns\n");
  printf("\n");
  printf("  [-site site_name]          \n");
  printf("  [-auto_place_pins]         \n");
  printf("  -def def_file              def file to write\n");
}

static double
metersToMicrons(double meters)
{
  return meters * 1e+6;
}

static double
micronsToMeters(double microns)
{
  return microns * 1e-6;
}

static double
parseFloat(const char *token,
	   const char *arg_name,
	   Report *report)
{
  char *end;
  double value = strtod(token, &end);
  if (*end != '\0')
    report->printError("Error: %s value '%s' is not a float.\n", arg_name, token);
  return value;
}

static int
parseInt(const char *token,
	 const char *arg_name,
	 Report *report)
{
  char *end;
  int value = strtol(token, &end, 10);
  if (*end != '\0')
    report->printError("Error: %s value '%s' is not a integer.\n", arg_name, token);
  return value;
}
