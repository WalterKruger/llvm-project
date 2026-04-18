//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// REQUIRES: std-at-least-c++23

// Check that functions are marked [[nodiscard]]

#include <algorithm>
#include <functional>
#include <vector>

void test() {
  auto const data = std::vector<int>{};

  // expected-warning@+1 {{ignoring return value of function declared with 'nodiscard' attribute}}
  std::ranges::fold_left_with_iter(data.begin(), data.end(), 0, std::plus());
  // expected-warning@+1 {{ignoring return value of function declared with 'nodiscard' attribute}}
  std::ranges::fold_left_with_iter(data, 0, std::plus());
  // expected-warning@+1 {{ignoring return value of function declared with 'nodiscard' attribute}}
  std::ranges::fold_left(data.begin(), data.end(), 0, std::plus());
  // expected-warning@+1 {{ignoring return value of function declared with 'nodiscard' attribute}}
  std::ranges::fold_left(data, 0, std::plus());

  // expected-warning@+1 {{ignoring return value of function declared with 'nodiscard' attribute}}
  std::ranges::fold_left_first_with_iter(data.begin(), data.end(), std::plus());
  // expected-warning@+1 {{ignoring return value of function declared with 'nodiscard' attribute}}
  std::ranges::fold_left_first_with_iter(data, std::plus());
  // expected-warning@+1 {{ignoring return value of function declared with 'nodiscard' attribute}}
  std::ranges::fold_left_first(data.begin(), data.end(), std::plus());
  // expected-warning@+1 {{ignoring return value of function declared with 'nodiscard' attribute}}
  std::ranges::fold_left_first(data, std::plus());
}
