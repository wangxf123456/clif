// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "clif/python/types.h"
#include <climits>
#include "absl/numeric/int128.h"

namespace clif {

//// To Python conversions.

// bytes
PyObject* Clif_PyObjFrom(const std::string& c, const py::PostConv& pc) {
  return pc.Apply(PyBytes_FromStringAndSize(c.data(), c.size()));
}

PyObject* UnicodeFromBytes(PyObject* b) {
  if (!b || PyUnicode_Check(b)) return b;
  if (!PyBytes_Check(b)) {
    PyErr_Format(PyExc_TypeError, "expecting bytes, got %s %s",
                 ClassName(b), ClassType(b));
    Py_DECREF(b);
    return nullptr;
  }
  PyObject* u = PyUnicode_FromStringAndSize(PyBytes_AS_STRING(b),
                                            PyBytes_GET_SIZE(b));
  Py_DECREF(b);
  return u;
}


//// From Python conversions.

// int (long)

bool Clif_PyObjAs(PyObject* py, int* c) {
  CHECK(c != nullptr);
  long i;  //NOLINT: runtime/int
  if (PyLong_Check(py)) {
    i = PyLong_AsLong(py);
    if (i == -1 && PyErr_Occurred()) return false;
#if SIZEOF_INT < SIZEOF_LONG
    if (i > INT_MAX || i < INT_MIN) {
      PyErr_SetString(PyExc_ValueError, "value too large for int");
      return false;
    }
#endif
  } else {
    PyErr_SetString(PyExc_TypeError, "expecting int");
    return false;
  }
  *c = i;
  return true;
}

bool Clif_PyObjAs(PyObject* py, short* c) {  //NOLINT: runtime/int
  CHECK(c != nullptr);
  long i;  // NOLINT: runtime/int
  if (!Clif_PyObjAs(py, &i)) {
    return false;
  }
  if (i > SHRT_MAX || i < SHRT_MIN) {
    PyErr_SetString(PyExc_ValueError, "value too large for short int");
    return false;
  }
  *c = i;
  return true;
}

bool Clif_PyObjAs(PyObject* py, signed char* c) {
  CHECK(c != nullptr);
  long i;  // NOLINT: runtime/int
  if (!Clif_PyObjAs(py, &i)) {
    return false;
  }
  if (SCHAR_MIN > i || i > SCHAR_MAX) {
    PyErr_Format(
        PyExc_ValueError, "value %ld is out of range for signed char", i);
    return false;
  }
  *c = i;
  return true;
}

// uint8
bool Clif_PyObjAs(PyObject* py, unsigned char* c) {
  CHECK(c != nullptr);
  unsigned long i;  // NOLINT: runtime/int
  if (!Clif_PyObjAs(py, &i)) {
    return false;
  }
  if (i > UCHAR_MAX) {
    PyErr_Format(
        PyExc_ValueError, "value %ld is too large for unsigned char", i);
    return false;
  }
  *c = i;
  return true;
}

bool Clif_PyObjAs(PyObject* py, unsigned short* c) {  //NOLINT: runtime/int
  CHECK(c != nullptr);
  unsigned long i;  //NOLINT: runtime/int
  if (PyLong_Check(py)) {
    i = PyLong_AsUnsignedLong(py);
  } else {
    PyErr_SetString(PyExc_TypeError, "expecting int");
    return false;
  }
  if (PyErr_Occurred()) return false;
  if (i > USHRT_MAX) {
    PyErr_SetString(PyExc_ValueError, "value too large for unsigned short");
    return false;
  }
  *c = i;
  return true;
}

bool Clif_PyObjAs(PyObject* py, unsigned int* c) {
  CHECK(c != nullptr);
  unsigned long i;  //NOLINT: runtime/int
  if (PyLong_Check(py)) {
    i = PyLong_AsUnsignedLong(py);
  } else {
    PyErr_SetString(PyExc_TypeError, "expecting int");
    return false;
  }
  if (PyErr_Occurred()) return false;
  if (i > UINT_MAX) {
    PyErr_SetString(PyExc_ValueError, "value too large for unsigned int");
    return false;
  }
  *c = i;
  return true;
}

bool Clif_PyObjAs(PyObject* py, unsigned long* c) {  //NOLINT: runtime/int
  CHECK(c != nullptr);
  if (PyLong_Check(py)) {
    *c = PyLong_AsUnsignedLong(py);
  } else {
    PyErr_SetString(PyExc_TypeError, "expecting int");
    return false;
  }
  return !PyErr_Occurred();
}

bool Clif_PyObjAs(PyObject* py, long* c) {  //NOLINT: runtime/int
  CHECK(c != nullptr);
  if (PyLong_Check(py)) {
    *c = PyLong_AsSsize_t(py);
  } else {
    PyErr_SetString(PyExc_TypeError, "expecting int");
    return false;
  }
  return !PyErr_Occurred();
}

// int64
#ifdef HAVE_LONG_LONG
bool Clif_PyObjAs(PyObject* py, long long* c) {  //NOLINT: runtime/int
  CHECK(c != nullptr);
  if (!PyLong_Check(py)) {
    PyErr_SetString(PyExc_TypeError, "expecting int");
    return false;
  }
  *c = PyLong_AsLongLong(py);
  return !PyErr_Occurred();
}

// uint64
bool Clif_PyObjAs(PyObject* py, unsigned long long* c) {  //NOLINT: runtime/int
  CHECK(c != nullptr);
  if (PyLong_Check(py)) {
    *c = PyLong_AsUnsignedLongLong(py);
  } else {
    PyErr_SetString(PyExc_TypeError, "expecting int");
    return false;
  }
  return !PyErr_Occurred();
}

// int128
bool Clif_PyObjAs(PyObject* py, absl::int128* c) {  // NOLINT: runtime/int
  CHECK(c != nullptr);
  if (PyLong_Check(py)) {
    auto lo = PyLong_AsUnsignedLongLong(
        PyNumber_And(py, PyLong_FromUnsignedLongLong(0xFFFFFFFFFFFFFFFF)));
    auto hi = PyLong_AsLongLong(PyNumber_Rshift(py, PyLong_FromLong(64)));
    *c = absl::MakeInt128(hi, lo);
  } else {
    PyErr_SetString(PyExc_TypeError, "expecting int");
    return false;
  }
  return !PyErr_Occurred();
}

// uint128
bool Clif_PyObjAs(PyObject* py, absl::uint128* c) {  // NOLINT: runtime/int
  CHECK(c != nullptr);
  if (PyLong_Check(py)) {
    auto lo = PyLong_AsUnsignedLongLong(
        PyNumber_And(py, PyLong_FromUnsignedLongLong(0xFFFFFFFFFFFFFFFF)));
    auto hi =
        PyLong_AsUnsignedLongLong(PyNumber_Rshift(py, PyLong_FromLong(64)));
    *c = absl::MakeUint128(hi, lo);
  } else {
    PyErr_SetString(PyExc_TypeError, "expecting int");
    return false;
  }
  return !PyErr_Occurred();
}
#endif  // HAVE_LONG_LONG

// float (double)
bool Clif_PyObjAs(PyObject* py, double* c) {
  CHECK(c != nullptr);
  double f = PyFloat_AsDouble(py);
  if (f == -1.0 && PyErr_Occurred()) return false;
  *c = f;
  return true;
}

bool Clif_PyObjAs(PyObject* py, float* c) {
  CHECK(c != nullptr);
  double f = PyFloat_AsDouble(py);
  if (f == -1.0 && PyErr_Occurred()) return false;
  *c = static_cast<float>(f);
  return true;
}

// complex
bool Clif_PyObjAs(PyObject* py, std::complex<double>* c) {
  CHECK(c != nullptr);
  double real = PyComplex_RealAsDouble(py);
  if (real == -1.0 && PyErr_Occurred()) return false;
  double imag = PyComplex_ImagAsDouble(py);
  if (imag == -1.0 && PyErr_Occurred()) return false;

  *c = std::complex<double>(real, imag);
  return true;
}

bool Clif_PyObjAs(PyObject* py, std::complex<float>* c) {
  CHECK(c != nullptr);
  double real = PyComplex_RealAsDouble(py);
  if (real == -1.0 && PyErr_Occurred()) return false;
  double imag = PyComplex_ImagAsDouble(py);
  if (imag == -1.0 && PyErr_Occurred()) return false;

  *c = std::complex<float>(static_cast<float>(real), static_cast<float>(imag));
  return true;
}

// bool
bool Clif_PyObjAs(PyObject* py, bool* c) {
  CHECK(c != nullptr);
  if (!PyBool_Check(py)) {
    PyErr_SetString(PyExc_TypeError, "expecting bool");
    return false;
  }
  *c = (py == Py_True);
  return true;
}

namespace py {

// bytes/unicode
template<typename C>
bool ObjToStr(PyObject* py, C copy) {
#if PY_VERSION_HEX >= 0x03030000
  const char* data;
  Py_ssize_t length;
  if (PyUnicode_Check(py)) {
    data = PyUnicode_AsUTF8AndSize(py, &length);
    if (!data) return false;
  } else if (PyBytes_Check(py)) {
    data = PyBytes_AS_STRING(py);
    length = PyBytes_GET_SIZE(py);
  } else {
    PyErr_SetString(PyExc_TypeError, "expecting str");
    return false;
  }
  copy(data, length);
  return true;
#else
  bool decref = false;
  if (PyUnicode_Check(py)) {
    py = PyUnicode_AsUTF8String(py);
    if (!py) return false;
    decref = true;
  } else if (!PyBytes_Check(py)) {
    PyErr_SetString(PyExc_TypeError, "expecting str");
    return false;
  }
  copy(PyBytes_AS_STRING(py), PyBytes_GET_SIZE(py));
  if (decref) Py_DECREF(py);
  return true;
#endif
}
}  // namespace py

bool Clif_PyObjAs(PyObject* p, std::string* c) {
  CHECK(c != nullptr);
  return py::ObjToStr(p,
      [c](const char* data, size_t length) { c->assign(data, length); });
}
}  // namespace clif
