# Helper functions common to multiple regressions.

set result_dir "/tmp"

# puts [exec cat $file] without forking.
proc report_file { file } {
  set stream [open $file r]
  gets $stream line
  while { ![eof $stream] } {
    puts $line
    gets $stream line
  }
  close $stream
}

proc make_result_file { filename } {
  variable result_dir
  return [file join $result_dir $filename]
}
