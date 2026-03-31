// RUN: rm -rf %t && mkdir -p %t
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/ModulesCache -fdisable-module-hash -fapinotes-modules\
// RUN:             -I %S/Inputs/Headers %s -x c++ -verify -Wunsafe-buffer-usage -Wno-unused-value
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/ModulesCache -fdisable-module-hash -fapinotes-modules\
// RUN:             -I %S/Inputs/Headers %s -x c++ -ast-dump -ast-dump-filter unsafeFunc | FileCheck %s

#include "UnsafeBufferUsage.h"

// CHECK: FunctionDecl {{.+}} unsafeFunc
// CHECK: UnsafeBufferUsageAttr

void caller(int *p, int n) {
  unsafeFunc(p, n); // expected-warning{{function introduces unsafe buffer manipulation}}
  safeFunc(p, n);   // no warning
}

void unsafeFunc(int *p, int n) {
  p[n]; // no warning
}

void safeFunc(int *p, int n) {
  p[n]; // expected-warning{{unsafe buffer access}}
}
