#!/usr/bin/ruby

def normalize(sym)
  sym.sub!(/^_H8300/,"__ASM_RX")
  sym.sub!(/^__ARCH_H8300/,"__ASM_RX")
  sym.sub!(/^_ASM_H8300/,"__ASM_RX")
  sym.sub!(/^__ASM_H8300/,"__ASM_RX")
  sym.sub!(/^__ASMH8300/,"__ASM_RX")
  sym.sub!(/^__H8300/,"__ASM_RX")
  sym.sub!(/H_*$/,"H__")
  return sym
end

STDIN.each_line { |line|
  if line.index("H8300") then
    line.sub!(/^#ifndef\s+(\S+)$/) { "#ifndef #{normalize($1)}" }
    line.sub!(/^#define\s+(\S+)$/) { "#define #{normalize($1)}" }
    line.sub!(/^#endif\s+\/\*\s+(\S+)\s+\*\/$/) { "#endif /* #{normalize($1)} */" }
  end
  print line
}
