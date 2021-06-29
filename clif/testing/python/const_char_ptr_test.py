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
from absl.testing import parameterized

from clif.testing.python import const_char_ptr
# TODO: Restore simple import after OSS setup includes pybind11.
# pylint: disable=g-import-not-at-top
try:
  from clif.testing.python import const_char_ptr_pybind11
except ImportError:
  const_char_ptr_pybind11 = None
# pylint: enable=g-import-not-at-top


@parameterized.named_parameters([
    np for np in zip(('c_api', 'pybind11'), (const_char_ptr,
                                             const_char_ptr_pybind11))
    if np[1] is not None
])
class ConstCharPtrTest(absltest.TestCase):

  def testReturnConstCharPtrAsStr(self, wrapper_lib):
    res = wrapper_lib.ReturnConstCharPtrAsStr()
    self.assertIsInstance(res, str)
    self.assertEqual(res, 'string literal')

  def testReturnConstCharPtrAsBytes(self, wrapper_lib):
    res = wrapper_lib.ReturnConstCharPtrAsBytes()
    if wrapper_lib is const_char_ptr:
      # BUG: Return value should be bytes but is str (Python 3).
      self.assertIsInstance(res, str)  # Should be bytes.
      self.assertEqual(res, 'string literal')  # Should be b'string literal'.
    else:
      self.assertIsInstance(res, bytes)
      self.assertEqual(res, b'string literal')


if __name__ == '__main__':
  absltest.main()
