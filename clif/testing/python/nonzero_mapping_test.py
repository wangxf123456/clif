# Copyright 2020 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Tests for clif.testing.python.nonzero_mapping."""

from absl.testing import absltest

from clif.testing.python import nonzero_mapping


class NonzeroMappingTest(absltest.TestCase):

  def testDefaultCppName(self):
    always_false = nonzero_mapping.AlwaysFalse()
    self.assertEqual(bool(always_false), False)

  def testCustomizedCppName(self):
    might_be_true = nonzero_mapping.MightBeTrue(False)
    self.assertEqual(bool(might_be_true), False)
    might_be_true = nonzero_mapping.MightBeTrue(True)
    self.assertEqual(bool(might_be_true), True)


if __name__ == '__main__':
  absltest.main()
