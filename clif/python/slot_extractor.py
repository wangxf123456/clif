# Copyright 2017 Google Inc.
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

# pylint: disable=line-too-long
"""Extracts slot name / type from Python object.h, saves to pyVslot.py.

Usage:
  PYTHONHASHSEED=1 python3 slot_extractor.py ../../python_runtime/v2_7/Include/object.h
  PYTHONHASHSEED=1 python3 slot_extractor.py ../../python_runtime/v3_6/Include/object.h
"""
# pylint: enable=line-too-long

import sys
sys.path.pop(0)  # types.py in this directory breaks python3 re import.
import re  # pylint: disable=g-bad-import-order,g-import-not-at-top

PY_VER = re.compile(r'/v([23])_(\d)/').search
OUT_P = 'py{}slots.py'
STRUCTS = {}


def Scan(source_lines):
  """Yields (struct_name, [struct_lines])."""
  in_struct = False
  for lno, line in enumerate(source_lines):
    if in_struct:
      name = Scan.END(line)
      if name:
        assert not ifdef_level, '#ifdef not closed in struct %s' % name.group(1)  # pylint bug; pylint: disable=used-before-assignment
        yield name.group(1), struct  # pylint: disable=used-before-assignment
        in_struct = False
      elif Scan.IFDEF(line):
        ifdef_level += 1
      elif Scan.ENDIF(line):
        ifdef_level -= 1
        assert ifdef_level >= 0, 'Dangling #endif at %d' % (lno + 1)
      elif not ifdef_level:
        struct.append(line)
    elif Scan.START(line):
      struct = []
      in_struct = True
      ifdef_level = 0
  assert not in_struct, 'Last struct not closed'
  assert not ifdef_level, '#ifdef not closed'
Scan.START = re.compile(r'typedef struct (\w+ )?{').match
Scan.END = re.compile(r'} (\w+);').match
Scan.IFDEF = re.compile(r'\s*#\s*if').match
Scan.ENDIF = re.compile(r'\s*#\s*endif').match

_FUNC_TYPES = {  # O for PyObject*, first argument (always O) omitted.
    'unaryfunc': ('O', ''),    # PyObject* (*unaryfunc)(PyObject *)
    'binaryfunc': ('O', 'O'),  # PyObject* (*binaryfunc)(PyObject *, PyObject *)
    'ternaryfunc': ('O', 'OO'),
    'objobjproc': ('int', 'O'),
    'inquiry': ('int', ''),
    'hashfunc': ('long', ''),
    'lenfunc': ('Py_ssize_t', ''),
    # All non-O args are special cases.
    'ssizeargfunc': ('int', ['Py_ssize_t']),
    'ssizeobjargproc': ('O', ['Py_ssize_t', 'O']),
}
f = _FUNC_TYPES
f['reprfunc'] = f['getiterfunc'] = f['iternextfunc'] = f['unaryfunc']
f['cmpfunc'] = f['objobjproc']
f['getattrofunc'] = f['binaryfunc']
f['setattrofunc'] = f['ternaryfunc']
for f in _FUNC_TYPES.values():
  assert isinstance(f, tuple) and all(isinstance(t, str) for t in f) or (
      len(f) == 2 and isinstance(f[0], str) and isinstance(f[1], list) and
      all(isinstance(t, str) for t in f[1])), f
del f


def ParseFuncs(pytype):
  """Decode slot function type and yield (slot_name, func_signature)."""
  for line in pytype:
    s = ParseFuncs.GetSlotFunc(line)
    if s:
      # DEBUG: print(s.groups())
      f = _FUNC_TYPES.get(s.group(1))  # pylint bug; pylint: disable=redefined-outer-name
      if f:
        yield s.group(2), f
      # DEBUG: else: print(s.group(2), s.group(1), 'ignored')
ParseFuncs.GetSlotFunc = re.compile(
    r' ([a-z]+)\s*\*?\s*((tp|nb|sq|mp)_[a-z_]+);').search


def ParseSlots(struct):
  """Yield C slot names from Python C source."""
  for line in struct:
    if ';' in line:
      assert line.count(',') < 2, 'Max 2 slots per line allowed.'
    if line:
      s = ParseSlots.GetSlots(line)
      if s:
        if s.group(3):
          yield s.group(3)
        assert s.group(4)
        yield s.group(4)
ParseSlots.GetSlots = re.compile(
    # Only special-cased for TYPE SLOT1, SLOT2; ie. no SLOT3 in a single line.
    r' (\w+)\s*\*?\s*(([a-z_]+),\s*)?([a-z_]+);'
).search


def main(args):
  src_filename = args[1]
  pyversion = PY_VER(src_filename)
  if not pyversion:
    print('Input file not in /v2_X/ or /v3_X/ subdir.\n' + src_filename)
    return 1
  pyversion = pyversion.group(1)
  with open(src_filename) as src:
    STRUCTS.update(Scan(src))
  need_at_least = {'PyNumberMethods': 34,  # Py3 has less slots here.
                   'PySequenceMethods': 10,
                   'PyMappingMethods': 3,
                   'PyTypeObject': 46}
  if not all(n in STRUCTS for n in need_at_least):
    print('Not all required groups found:')
    print('needed:', ', '.join(need_at_least))
    print('found:', ', '.join(STRUCTS.keys()))
    return 1
  bad_scan = False
  for n, need_lines in need_at_least.items():
    g = list(ParseSlots(STRUCTS[n]))
    if len(g) < need_lines:
      print('Group', n, 'is too short, need at least', need_lines,
            'items, found', len(g), ','.join(g))
      bad_scan = True
  if bad_scan:
    return 1
  with open(OUT_P.format(pyversion), 'w') as f:  # pylint bug; pylint: disable=redefined-outer-name
    f.write('"""DO NOT TOUCH. Generated by slot_extractor."""\n')
    f.write('# from %s\n\n' % src_filename)
    f.write('''#
# Copyright 2017 Google Inc.
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

''')
    for n in need_at_least:
      f.write(n+' = [\n')
      for s in ParseSlots(STRUCTS[n]):
        f.write("    '%s',\n" % s)
      f.write(']\n')
    f.write('SIGNATURES = {  # c_slot_name: func_type\n')
    for i in ParseFuncs(s for n in need_at_least for s in STRUCTS[n]):  # pylint: disable=g-complex-comprehension
      f.write("    '%s': %s,\n" % i)
    f.write('}\n')


if __name__ == '__main__':
  main(sys.argv)
