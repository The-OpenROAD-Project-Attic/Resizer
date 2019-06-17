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

using std::string;
using sta::StringVector;
using sta::Report;
using sta::Debug;
using sta::makeReportStd;
using sta::isDigits;
using sta::findCmdLineKey;
using sta::findCmdLineFlag;
using sta::LefDefNetwork;

static void
showUsage(const char *prog);

int
main(int argc,
     char *argv[])
{
  if (findCmdLineFlag(argc, argv, "-help")) {
    showUsage(argv[0]); 
    exit(EXIT_SUCCESS);
  }

  if (findCmdLineFlag(argc, argv, "-version")) {
    printf("verilog2def %s\n", RESIZER_VERSION);
    exit(EXIT_SUCCESS);
  }

  Report *report = makeReportStd();

  const char *lef_filename = findCmdLineKey(argc, argv, "-lef");
  if (lef_filename == nullptr)
    report->printError("missing -lef argument.\n");

  const char *liberty_filename = findCmdLineKey(argc, argv, "-liberty");
  if (liberty_filename == nullptr)
    report->printError("missing -liberty argument.\n");

  const char *verilog_filename = findCmdLineKey(argc, argv, "-verilog");
  if (verilog_filename == nullptr)
    report->printError("missing -verilog argument.\n");

  const char *top_module = findCmdLineKey(argc, argv, "-top_module");
  if (top_module == nullptr)
    report->printError("missing -top_module argument.\n");

  const char *units_str = findCmdLineKey(argc, argv, "-units");
  int units = 0;
  if (units_str == nullptr)
    report->printError("missing -units argument.\n");
  else {
    if (!isDigits(units_str))
      report->printError("-units is not a positiveinteger\n");
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
      report->printError("-die_area should be a list of 4 coordinates\n");
    die_lx = strtod(die_tokens[0].c_str(), nullptr);
    die_ly = strtod(die_tokens[1].c_str(), nullptr);
    die_ux = strtod(die_tokens[2].c_str(), nullptr);
    die_uy = strtod(die_tokens[3].c_str(), nullptr);
  }

  const char *site_name = findCmdLineKey(argc, argv, "-site");
  bool auto_place_pins = findCmdLineFlag(argc, argv, "-auto_place_pins");
  const char *def_filename = findCmdLineKey(argc, argv, "-def");

  Debug debug(report);
  LefDefNetwork network;

  readLef(lef_filename, &network);
  readLibertyFile(liberty_filename, false, &network);
  readVerilogFile(verilog_filename, &network);
  network.linkNetwork(top_module, true, report);
  writeDef(def_filename, units, die_lx, die_ly, die_ux, die_uy,
	   site_name, auto_place_pins, &network);

  exit(EXIT_SUCCESS);
}

static void
showUsage(const char *prog)
{
  printf("Usage %s\n", prog);
  printf("  [-help]                   show help and exit\n");
  printf("  [-version]                show version and exit\n");
  printf("  -lef lef_file             lef_file for site size\n");
  printf("  -liberty liberty_file     liberty for linking verilog\n");
  printf("  -verilog verilog_file     \n");
  printf("  -top_module module_name   verilog module to expand\n");
  printf("  -units units              def units per micron\n");
  printf("  [-die_area \"lx ly ux uy\"] die area in microns\n");
  printf("  [-site site_name]         \n");
  printf("  [-auto_place_pins]        \n");
  printf("  -def def_file             def file to write\n");
}
