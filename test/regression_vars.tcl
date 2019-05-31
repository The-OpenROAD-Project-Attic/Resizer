# Parallax Static Timing Analyzer
# Copyright (c) 2019, Parallax Software, Inc.
# All rights reserved.
# 
# No part of this document may be copied, transmitted or
# disclosed in any form or fashion without the express
# written consent of Parallax Software, Inc.

# Regression variables.

# Application program to run tests on.
if { [regexp "CYGWIN" [exec uname -a]] } {
  set app "resizer.exe"
} else {
  set app "resizer"
}
set app_path [file join $sta_dir "build" $app]
# Application options.
set app_options "-no_init -no_splash"
set test_dir [file join $sta_dir "test"]
# Log files for each test are placed in result_dir.
set result_dir [file join $sta_dir "test/results"]
# Collective diffs.
set diff_file [file join $result_dir "diffs"]
# File containing list of failed tests.
set failure_file [file join $result_dir "failures"]
# Use the DIFF_OPTIONS envar to change the diff options
# (Solaris diff doesn't support this envar)
set diff_options "-c"
if [info exists env(DIFF_OPTIONS)] {
  set diff_options $env(DIFF_OPTIONS)
}

set valgrind_suppress [file join $test_dir valgrind.suppress]
set valgrind_options "--num-callers=20 --leak-check=full --freelist-vol=100000000 --leak-resolution=high --suppressions=$valgrind_suppress"
if { [exec "uname"] == "Darwin" } {
  append valgrind_options " --dsymutil=yes"
}
if { 0 } {
  append valgrind_options " --gen-suppressions=all"
}

set tcheck_path "/opt/intel/itt/tcheck/bin/32e/tcheck_cl"
set tcheck_options "--stack_depth 10 --quiet --format csv --options nowarnings,noverbose"

# Regexp to filter log files to compare with pt.
set summary_regexp "^ *\\(Startpoint:\\|Endpoint:\\|slack\\|data arrival time\\|data required time\\)"

if [info exists env(STAX)] {
  # Directory that holds log files -cmp comparison option.
  set cmp_log_dir [file join $env(STAX) "doc" "reports"]
}

proc cleanse_logfile { test log_file } {
  # Nothing to be done here.
}

################################################################

# Record a test in the regression suite.
proc record_test { test cmd_dir } {
  global cmd_dirs test_groups
  set cmd_dirs($test) $cmd_dir
  lappend test_groups(all) $test
  return $test
}

# Record a test in the $STA/test directory.
proc record_sta_tests { tests } {
  global test_dir
  foreach test $tests {
    # Prune commented tests from the list.
    if { [string index $test 0] != "#" } {
      record_test $test $test_dir
    }
  }
}

################################################################

proc define_test_group { name tests } {
  global test_groups
  set test_groups($name) $tests
}

proc group_tests { name } {
  global test_groups
  return $test_groups($name)
}

# Clear the test lists.
proc clear_tests {} {
  global test_groups
  unset test_groups
}

proc list_delete { list delete } {
  set result {}
  foreach item $list {
    if { [lsearch $delete $item] == -1 } {
      lappend result $item
    }
  }
  return $result
}

################################################################

# Regression test lists.

# Record tests in $STA/test
record_sta_tests {
  read_def1
  read_def2
  resize1
  resize2
  resize_mea1
  write_def1
}

# Regression test groups

# Medium speed tests.
# run time <15s with optimized compile
define_test_group med {
  resize_mea1
}

set fast [group_tests all]
set fast [list_delete $fast [group_tests med]]

define_test_group fast $fast