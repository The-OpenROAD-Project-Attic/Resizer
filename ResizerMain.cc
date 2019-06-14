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

#include <stdio.h>
#include <tcl.h>
#include "Machine.hh"
#include "ResizerConfig.hh"  // RESIZER_VERSION
#include "StringUtil.hh"
#include "StaMain.hh"
#include "Resizer.hh"

using sta::Sta;
using sta::initSta;
using sta::stringEq;
using sta::Resizer;
using sta::findCmdLineFlag;
using sta::findCmdLineKey;
using sta::evalTclInit;
using sta::sourceTclFile;

// Swig uses C linkage for init functions.
extern "C" {
extern int Resizer_Init(Tcl_Interp *interp);
extern int Sta_Init(Tcl_Interp *interp);
}

namespace sta {
extern const char *resizer_tcl_inits[];
extern const char *tcl_inits[];
}

// "Arguments" passed to staTclAppInit.
static int resizer_argc;
static char **resizer_argv;

static void
showUseage(char *prog);
static int
resizerTclAppInit(Tcl_Interp *interp);

int
main(int argc,
     char **argv)
{
  if (argc == 2 && stringEq(argv[1], "-help")) {
    showUseage(argv[0]);
    return 0;
  }
  else if (argc == 2 && stringEq(argv[1], "-version")) {
    printf("%s\n", RESIZER_VERSION);
    return 0;
  }
  else {
    Resizer *resizer = new Resizer();
    initSta();
    Sta::setSta(resizer);
    resizer->makeComponents();
    resizer->initFlute(argv[0]);

    resizer_argc = argc;
    resizer_argv = argv;
    // Set argc to 1 so Tcl_Main doesn't source any files.
    // Tcl_Main never returns.
    Tcl_Main(1, argv, resizerTclAppInit);
    return 0;
  }
}

static void
showUseage(char *prog)
{
  printf("Usage: %s [-help] [-version] [-no_init] [-no_splash] cmd_file\n", prog);
  printf("  -help              show help and exit\n");
  printf("  -version           show version and exit\n");
  printf("  -no_init           do not read .sta init file\n");
  printf("  -no_splash         do not show the license splash at startup\n");
  printf("  cmd_file           source cmd_file and exit\n");
}

// Tcl init executed inside Tcl_Main.
static int
resizerTclAppInit(Tcl_Interp *interp)
{
  int argc = resizer_argc;
  char **argv = resizer_argv;

  // source init.tcl
  Tcl_Init(interp);

  // Define swig commands.
  Resizer_Init(interp);
  Sta_Init(interp);

  Sta *sta = Sta::sta();
  sta->setTclInterp(interp);

  // Eval encoded sta TCL sources.
  evalTclInit(interp, sta::tcl_inits);
  evalTclInit(interp, sta::resizer_tcl_inits);

  if (!findCmdLineFlag(argc, argv, "-no_splash"))
    Tcl_Eval(interp, "sta::show_splash");

  // Import exported commands from sta namespace to global namespace.
  Tcl_Eval(interp, "sta::define_sta_cmds");
  Tcl_Eval(interp, "namespace import sta::*");

  if (!findCmdLineFlag(argc, argv, "-no_init")) {
    const char *init_filename = "[file join $env(HOME) .resizer]";
    sourceTclFile(init_filename, true, false, interp);
  }

  bool exit_after_cmd_file = findCmdLineFlag(argc, argv, "-exit");

  if (argc > 2 ||
      (argc > 1 && argv[1][0] == '-'))
    showUseage(argv[0]);
  else {
    if (argc == 2) {
      char *cmd_file = argv[1];
      if (cmd_file) {
	sourceTclFile(cmd_file, false, false, interp);
	if (exit_after_cmd_file)
	  exit(EXIT_SUCCESS);
      }
    }
  }
  return TCL_OK;
}
