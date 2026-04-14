# REQUIRES: loongarch

## Test that R_LARCH_MARK_LA is preserved with --emit-relocs.

# RUN: llvm-mc --filetype=obj --triple=loongarch64 %s -o %t.o

# RUN: ld.lld --emit-relocs --relax %t.o -o %t.relax
# RUN: llvm-readobj -r %t.relax | FileCheck %s

# RUN: ld.lld --emit-relocs --no-relax %t.o -o %t.norelax
# RUN: llvm-readobj -r %t.norelax | FileCheck %s

# CHECK:      R_LARCH_MARK_LA

  .global _start
_start:
  la.abs $a0, a
  ret

  .global a
a:
  .quad 0
