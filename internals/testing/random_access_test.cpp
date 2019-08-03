#include "bargain_v0/bargain.hpp"

#include "testing_v1/test.hpp"

#include <cstdlib>

using namespace testing_v1;
using namespace bargain_v0;

auto random_access_test = test([]() {
  STM::atom_t<int> xAs[11] = {3, 1, 4, 1, 5, 9, 2, 6, 5, 3, 8};

  auto randomIx = []() { return rand() % 11; };

  for (auto i = 0; i < 100; ++i) {
    auto op = *xAs[randomIx()] >> [&](auto &) { return *xAs[randomIx()]; } >>
              [&](auto &) { return *xAs[randomIx()]; } >>
              [&](auto &) { return *xAs[randomIx()]; } >>
              [&](auto &) { return *xAs[randomIx()]; } >> [&](auto &) {
                return *xAs[randomIx()] >>
                       [&](auto &v) { return xAs[randomIx()] = v; };
              };
    STM::run(op);
  }
});
