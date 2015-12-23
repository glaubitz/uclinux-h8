#!/usr/bin/ruby

file = readlines
if file.size ==6 then
  if file[2] == "\n" && file[4] == "\n" && /^#include\s+<asm-generic/ =~ file[3]
    print file[3]
    exit 0
  end
end
print file
