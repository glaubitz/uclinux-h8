#!/usr/bin/ruby

STDIN.each { |line|
  next if /RELOC_NUMBER \((\S+),\s+0x([0-9a-f]+)\)/ !~ line
  puts "#define #{$1}\t#{$2.hex.to_s}"
}
