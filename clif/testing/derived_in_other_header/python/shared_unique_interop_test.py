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

from clif.testing.derived_in_other_header.python import concrete_base
from clif.testing.derived_in_other_header.python import concrete_derived
from clif.testing.derived_in_other_header.python import shared_unique_interop
from clif.testing.derived_in_other_header.python import virtual_derived

CONCRETE_BASE_EMPTY_GET_RESULT = 90146438
CONCRETE_DERIVED_EMPTY_GET_RESULT = 31607978
VIRTUAL_DERIVED_EMPTY_GET_RESULT = 29852452


class ConcreteTest(parameterized.TestCase):

  def testBaseAndDerived(self):
    cbe = concrete_base.ConcreteBaseEmpty()
    self.assertEqual(cbe.Get(), CONCRETE_BASE_EMPTY_GET_RESULT)
    cde = concrete_derived.ConcreteDerivedEmpty()
    self.assertEqual(cde.Get(), CONCRETE_DERIVED_EMPTY_GET_RESULT)
    self.assertEqual(
        cde.BaseGet(cbe),
        CONCRETE_BASE_EMPTY_GET_RESULT + CONCRETE_DERIVED_EMPTY_GET_RESULT)
    self.assertEqual(
        cde.BaseGet(cde),
        CONCRETE_BASE_EMPTY_GET_RESULT + CONCRETE_DERIVED_EMPTY_GET_RESULT)

  @parameterized.named_parameters(
      ('DefaultDeleter', False),
      ('CustomDeleter', True))
  def testUnableToDisownOriginalShared(self, use_custom_deleter):
    cbe = shared_unique_interop.make_shared_concrete_derived_empty_up_cast(
        use_custom_deleter)
    with self.assertRaises(ValueError):
      shared_unique_interop.pass_unique_concrete_base_empty(cbe)

  def testPassUniqueConcreteBaseEmpty(self):
    # b/175568410
    cbe = (
        shared_unique_interop.make_unique_concrete_derived_empty_up_cast())
    self.assertEqual(cbe.Get(), CONCRETE_BASE_EMPTY_GET_RESULT)
    i = shared_unique_interop.pass_unique_concrete_base_empty(cbe)
    self.assertEqual(i, CONCRETE_BASE_EMPTY_GET_RESULT)
    with self.assertRaises(ValueError):  # Disowned.
      cbe.Get()

  def testOriginalUniqueNotDisownedByShared(self):
    # b/175568410
    cbe = (
        shared_unique_interop.make_unique_concrete_derived_empty_up_cast())
    i = shared_unique_interop.pass_shared_concrete_base_empty(cbe)
    self.assertEqual(i, CONCRETE_BASE_EMPTY_GET_RESULT)
    self.assertEqual(cbe.Get(), CONCRETE_BASE_EMPTY_GET_RESULT)
    i = shared_unique_interop.pass_unique_concrete_base_empty(cbe)
    self.assertEqual(i, CONCRETE_BASE_EMPTY_GET_RESULT)
    with self.assertRaises(ValueError):  # Disowned.
      cbe.Get()

  @parameterized.named_parameters(
      ('DefaultDeleter', False),
      ('CustomDeleter', True))
  def testPassSharedConcreteBaseEmpty(self, use_custom_deleter):
    cbe = shared_unique_interop.make_shared_concrete_derived_empty_up_cast(
        use_custom_deleter)
    self.assertEqual(cbe.Get(), CONCRETE_BASE_EMPTY_GET_RESULT)
    i = shared_unique_interop.pass_shared_concrete_base_empty(cbe)
    self.assertEqual(i, CONCRETE_BASE_EMPTY_GET_RESULT)
    self.assertEqual(cbe.Get(), CONCRETE_BASE_EMPTY_GET_RESULT)


class VirtualTest(parameterized.TestCase):

  @classmethod
  def setUpClass(cls):
    super().setUpClass()
    cls.shared_unique_interop = shared_unique_interop
    cls.virtual_derived = virtual_derived

  def testBaseAndDerived(self):
    vde = self.virtual_derived.VirtualDerivedEmpty()
    self.assertEqual(vde.Get(), VIRTUAL_DERIVED_EMPTY_GET_RESULT)
    self.assertEqual(vde.BaseGet(vde), 2 * VIRTUAL_DERIVED_EMPTY_GET_RESULT)

  @parameterized.named_parameters(
      ('DefaultDeleter', False),
      ('CustomDeleter', True))
  def testUnableToDisownOriginalShared(self, use_custom_deleter):
    vbe = shared_unique_interop.make_shared_virtual_derived_empty_up_cast(
        use_custom_deleter)
    with self.assertRaises(ValueError):
      shared_unique_interop.pass_unique_virtual_base_empty(vbe)

  def testPassUniqueVirtualBaseEmpty(self):
    vbe = shared_unique_interop.make_unique_virtual_derived_empty_up_cast()
    self.assertEqual(vbe.Get(), VIRTUAL_DERIVED_EMPTY_GET_RESULT)
    i = shared_unique_interop.pass_unique_virtual_base_empty(vbe)
    self.assertEqual(i, VIRTUAL_DERIVED_EMPTY_GET_RESULT)
    with self.assertRaises(ValueError):  # Disowned.
      vbe.Get()

  def testOriginalUniqueNotDisownedByShared(self):
    vbe = shared_unique_interop.make_unique_virtual_derived_empty_up_cast()
    i = shared_unique_interop.pass_shared_virtual_base_empty(vbe)
    self.assertEqual(i, VIRTUAL_DERIVED_EMPTY_GET_RESULT)
    self.assertEqual(vbe.Get(), VIRTUAL_DERIVED_EMPTY_GET_RESULT)
    i = shared_unique_interop.pass_unique_virtual_base_empty(vbe)
    self.assertEqual(i, VIRTUAL_DERIVED_EMPTY_GET_RESULT)
    with self.assertRaises(ValueError):  # Disowned.
      vbe.Get()

  @parameterized.named_parameters(
      ('DefaultDeleter', False),
      ('CustomDeleter', True))
  def testPassSharedVirtualBaseEmpty(self, use_custom_deleter):
    vbe = shared_unique_interop.make_shared_virtual_derived_empty_up_cast(
        use_custom_deleter)
    self.assertEqual(vbe.Get(), VIRTUAL_DERIVED_EMPTY_GET_RESULT)
    i = shared_unique_interop.pass_shared_virtual_base_empty(vbe)
    self.assertEqual(i, VIRTUAL_DERIVED_EMPTY_GET_RESULT)
    self.assertEqual(vbe.Get(), VIRTUAL_DERIVED_EMPTY_GET_RESULT)


if __name__ == '__main__':
  absltest.main()
