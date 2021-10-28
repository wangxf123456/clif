# Copyright 2020 Google LLC
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

from absl.testing import absltest

from clif.testing.python import variables


class VariablesTest(absltest.TestCase):

  def test_const_int(self):
    self.assertEqual(variables.kMyConstInt, 42)

  def test_const_int_renamed(self):
    self.assertEqual(variables.const_int, 123)

  def test_const_float(self):
    self.assertEqual(variables.kMyConstFloat, 15.0)

  def test_const_bool(self):
    self.assertEqual(variables.kMyConstBool, True)

  def test_const_complex(self):
    self.assertEqual(variables.kMyConstComplex, complex(1))

  def test_const_array(self):
    expected_array = [0, 10, 20, 30, 40]
    self.assertSequenceEqual(expected_array, variables.kMyConstIntArray)

  def test_const_pair(self):
    expected_tuple = [0, 10]
    self.assertSequenceEqual(expected_tuple, variables.kMyConstPair)

  def test_const_dict(self):
    expected_dict = {1: 10, 2: 20, 3: 30}
    self.assertDictEqual(expected_dict, variables.kMyConstMap)

  def test_const_set(self):
    expected_set = {1, 2, 3}
    self.assertSetEqual(expected_set, variables.kMyConstSet)


if __name__ == '__main__':
  absltest.main()
