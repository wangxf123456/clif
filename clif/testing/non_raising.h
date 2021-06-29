/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef THIRD_PARTY_CLIF_TESTING_NON_RASING_H_
#define THIRD_PARTY_CLIF_TESTING_NON_RASING_H_

#include <Python.h>

#include "clif/python/postconv.h"
#include "clif/python/types.h"

namespace clif_testing {

struct TestNonRaising {};

TestNonRaising MakeTestNonRaising() { return TestNonRaising(); }

// CLIF use `::clif_testing::TestNonRaising` as TestNonRaising
inline PyObject* Clif_PyObjFrom(const TestNonRaising& c,
                                const clif::py::PostConv& pc) {
  int num = pc.isMarkedNonRaising() ? -1 : 1;
  return clif::Clif_PyObjFrom(num, {});
}

}  // namespace clif_testing

#endif  // THIRD_PARTY_CLIF_TESTING_NON_RASING_H_
