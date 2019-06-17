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
// -lef ../test/liberty1.lef -liberty ../test/liberty1.lib -verilog ../test/reg1.v -top_module top -units 100 -die_area "0 0 1000 1000" -site site1 -auto_place_pins -def ../test/results/foo.def
// parse multiple liberty files

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
    units = strtol(units_str, nullptr, 10);
  }

  const char *die_area = findCmdLineKey(argc, argv, "-die_area");
  double die_lx = 0.0;
  double die_ly = 0.0;
  double die_ux = 0.0;
  double die_uy = 0.0;
  if (die_area) {
    string die_area1(die_area);
    StringVector die_tokens;
    split(die_area1, " ,", die_tokens);
    if (die_tokens.size() != 4)
      report->printWarn("Warning: -die_area should be a list of 4 coordinates\n");
    // microns to meters.
    die_lx = strtod(die_tokens[0].c_str(), nullptr) * 1e-6;
    die_ly = strtod(die_tokens[1].c_str(), nullptr) * 1e-6;
    die_ux = strtod(die_tokens[2].c_str(), nullptr) * 1e-6;
    die_uy = strtod(die_tokens[3].c_str(), nullptr) * 1e-6;
  }

  const char *site_name = findCmdLineKey(argc, argv, "-site");
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
      for (auto liberty_filename : liberty_filenames)
	readLibertyFile(liberty_filename.c_str(), false, &network);
      readLef(lef_filename, &network);
      readVerilogFile(verilog_filename, &network);
      network.linkNetwork(top_module, true, report);
      writeDef(def_filename, units, die_lx, die_ly, die_ux, die_uy,
	       site_name, auto_place_pins, &network);
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
  printf("  [-help]                   show help and exit\n");
  printf("  [-version]                show version and exit\n");
  printf("  -liberty liberty_file     liberty for linking verilog\n");
  printf("  -lef lef_file             lef_file for site size\n");
  printf("  -verilog verilog_file     \n");
  printf("  -top_module module_name   verilog module to expand\n");
  printf("  -units units              def units per micron\n");
  printf("  [-die_area \"lx ly ux uy\"] die area in microns\n");
  printf("  [-site site_name]         \n");
  printf("  [-auto_place_pins]        \n");
  printf("  -def def_file             def file to write\n");
}
