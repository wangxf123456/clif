/*
 * Copyright 2017 Google Inc.
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
#ifndef CLIF_PYTHON_STLTYPES_H_
#define CLIF_PYTHON_STLTYPES_H_

// Conversion functions for std types.

/*
From .../python-2.7.3-docs-html/c-api/intro.html#include-files:
Since Python may define some pre-processor definitions which affect the
standard headers on some systems, you must include Python.h before any standard
headers are included.
*/
#include "Python.h"
#include <functional>
#include <deque>
#include <iterator>
#include <list>
#include <queue>
#include <stack>
#include <memory>
#include <type_traits>
#include <utility>
#include <typeinfo>
// Clang and gcc define __EXCEPTIONS when -fexceptions flag passed to them.
// (see https://gcc.gnu.org/onlinedocs/cpp/Common-Predefined-Macros.html and
// http://llvm.org/releases/3.6.0/tools/clang/docs/ReleaseNotes.html#the-exceptions-macro ) NOLINT(whitespace/line_length)
#ifdef __EXCEPTIONS
#include <system_error>  // NOLINT(build/c++11)
#endif  // __EXCEPTIONS

#include "absl/base/config.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "clif/python/types.h"

#ifdef ABSL_HAVE_STD_VARIANT
// CLIF use `::std::variant` as OneOf
//
// For example,
//
//  C++:
//    int Sum(const std::variant<std::vector<int>, int>&);
//
//  Python:
//    assert Sum(1) == 1
//    assert Sum((1, 2, 3)) == 6
//
// If the Python object can be converted into more than one alternative, then
// the first successful conversion is used.
//
// Python -> C++ conversion procedure outline:
//
// ```
// for typename U in variant:
//   Sequentially check if the following conversions exist.
//     1. PyObject -> U,
//     2. PyObject -> optional<U> and U is copy/move constructible,
//     3. PyObject -> unique_ptr<U> and U is copy/move constructible.
//   If no conversion exists, then compile error.
//   Use the first conversion found in the list above.
//     If conversion was successful (returned true), then return true, and stop.
//     Otherwise, continue to the next typename U.
// If PyObject could not be converted to any type U, then return false.
// ```
namespace clif_std_py_variant {
// Dummy argument types to resolve conversion priorities.
template <int N>
struct Priority : public Priority<N + 1> {
  static_assert(N < 5, "Programming error");
};

template <>
struct Priority<5> {};  // Terminates the recursion.

// Returns whether ADL finds a conversion into T.  This function is purposely
// placed outside the ::clif namespace, to ensure that ::clif::Clif_PyObjAs()
// is not used.
template <typename T, typename R = decltype(Clif_PyObjAs(
                          std::declval<PyObject*>(), std::declval<T*>()))>
constexpr bool HasNonClifConversion(int) {
  static_assert(std::is_same<R, bool>::value, "");
  return true;
}

// Overloaded version returning the default value, false.
template <typename T>
constexpr bool HasNonClifConversion(...) {
  return false;
}

// Returns whether a conversion is found by ADL or in ::clif namespace.
template <typename T, typename R = decltype(::clif::Clif_PyObjAs(
                          std::declval<PyObject*>(), std::declval<T*>()))>
constexpr bool HasConversion(int) {
  static_assert(std::is_same<R, bool>::value, "");
  return true;
}

template <typename T>
constexpr bool HasConversion(...) {
  return HasNonClifConversion<T>(0);
}

// Convert() attempts to convert PyObject into I-th alternative type T.
// There are several overload versions with different Priority arguments.

template <std::size_t I, typename T, typename... U>
typename std::enable_if<
    HasConversion<T>(0) && std::is_default_constructible<T>::value, bool>::type
Convert(PyObject* obj, ::std::variant<U...>* v, Priority<0>) {
  using ::clif::Clif_PyObjAs;
  return Clif_PyObjAs(obj, &v->template emplace<I>());
}

// In case direct conversion to T is not available, the next attempts are
// conversions via std::optional or std::unique_ptr. For example, if T is not
// default constructible, library may provide conversions to std::optional<T> or
// std::unique_ptr<T>.
//
// Unlike when searching for a direct conversion to T, ::clif namespace is
// excluded from search, because generic conversions are provided in that
// namespace, which depend on the direct conversions to T. Those generic
// conversions are not what we look for here because this code path takes place
// only when there is no direct conversion to T available.
#ifdef ABSL_HAVE_STD_OPTIONAL
template <std::size_t I, typename T, typename... U>
typename std::enable_if<HasNonClifConversion<::std::optional<T>>(0) &&
                            (std::is_copy_constructible<T>::value ||
                             std::is_move_constructible<T>::value),
                        bool>::type
Convert(PyObject* py, ::std::variant<U...>* v, Priority<1>) {
  ::std::optional<T> value;
  if (Clif_PyObjAs(py, &value)) {
    v->template emplace<I>(std::move(*value));
    return true;
  }
  return false;
}
#endif  // ABSL_HAVE_STD_OPTIONAL

template <std::size_t I, typename T, typename... U>
typename std::enable_if<HasNonClifConversion<std::unique_ptr<T>>(0) &&
                            (std::is_copy_constructible<T>::value ||
                             std::is_move_constructible<T>::value),
                        bool>::type
Convert(PyObject* py, ::std::variant<U...>* v, Priority<2>) {
  std::unique_ptr<T> value;
  if (Clif_PyObjAs(py, &value)) {
    v->template emplace<I>(std::move(*value));
    return true;
  }
  return false;
}

// If needed, add shared_ptr<T> and/or T* support here.

// When no conversion is available, Convert() makes the return type
// std::false_type. This way, PyObjAsVariantAt() can static_assert() and display
// helpful static error message rather than obscure compiler error messages
// about no matching function signature.
// Do not make this function return bool (false), because run-time conversion
// failures and compile-time missing conversions are different errors.
template <std::size_t I, typename T, typename... U>
constexpr std::false_type Convert(PyObject* obj, ::std::variant<U...>* v, ...);

template <std::size_t I, typename... U>
bool PyObjAsVariantAt(PyObject* py, ::std::variant<U...>* v) {
  using T = typename std::tuple_element<I, std::tuple<U...>>::type;
  auto success = Convert<I, T>(py, v, Priority<0>());
  static_assert(std::is_same<decltype(success), bool>::value,
                "No known CLIF conversion");
  return success;
}

template <std::size_t I, typename... U>
typename std::enable_if<I == sizeof...(U), bool>::type PyObjAsVariantTryAll(
    PyObject* py, ::std::variant<U...>* v) {
  PyErr_SetString(PyExc_TypeError,
                  "Failed to convert to any of std::variant alternatives");
  return false;
}

template <std::size_t I, typename... U>
typename std::enable_if<(I < sizeof...(U)), bool>::type PyObjAsVariantTryAll(
    PyObject* py, ::std::variant<U...>* v) {
  {
    bool success = PyObjAsVariantAt<I>(py, v);
    DCHECK(success || PyErr_Occurred() != nullptr);
    if (success) {
      return true;
    }
    // Conversion failure to I-th type is not a real failure.
    PyErr_Clear();
  }
  // Try to convert to the next type.
  return PyObjAsVariantTryAll<I + 1>(py, v);
}

// When exception is disabled, this should be unreachable.
template <std::size_t I, typename... U>
typename std::enable_if<I == sizeof...(U), PyObject*>::type PyObjFromHelper(
    const ::std::variant<U...>& v, const ::clif::py::PostConv&) {
  DCHECK(v.valueless_by_exception());
  PyErr_SetString(PyExc_ValueError, "std::variant is valueless");
  return nullptr;
}

template <std::size_t I, typename... U>
typename std::enable_if<(I < sizeof...(U)), PyObject*>::type PyObjFromHelper(
    const ::std::variant<U...>& v, const ::clif::py::PostConv& conv) {
  if (v.index() == I) {
    using ::clif::Clif_PyObjFrom;
    return Clif_PyObjFrom(::std::get<I>(v), conv.Get(I));
  }
  DCHECK_GT(v.index(), I);
  return PyObjFromHelper<I + 1>(v, conv);
}
}  // namespace clif_std_py_variant

namespace clif {
template <typename... U>
PyObject* Clif_PyObjFrom(const ::std::variant<U...>& v,
                         const ::clif::py::PostConv& conv) {
  return ::clif_std_py_variant::PyObjFromHelper<0>(v, conv);
}

template <typename... U>
bool Clif_PyObjAs(PyObject* obj, ::std::variant<U...>* v) {
  DCHECK(v != nullptr);
  return ::clif_std_py_variant::PyObjAsVariantTryAll<0>(obj, v);
}
}  // namespace clif
#endif  // ABSL_HAVE_STD_VARIANT

namespace clif {

// Ensure that the current thread is ready to call the Python C API.
class GilLock {
 public:
  GilLock() { threadstate_ = PyGILState_Ensure(); }
  ~GilLock() { PyGILState_Release(threadstate_); }
 private:
  PyGILState_STATE threadstate_;
};

inline void HandlePyExc() {
#ifdef __EXCEPTIONS
  if (PyErr_Occurred()) throw std::domain_error(python::ExcStr());
  throw std::system_error(std::error_code(), "Python: exception not set");
#else
  PyErr_PrintEx(1);
#endif  // __EXCEPTIONS
}

// ------------------------------------------------------------------

template<typename Container, typename ContainerIter>
class Iterator {
 private:  // Have a short type name alias to make it more readable.
  using T = typename ContainerIter::value_type;

 public:
  explicit Iterator(std::shared_ptr<Container> obj)
  : self_(std::move(obj)),
    // Allow to find custom begin() via ADL.
    it_([&] { using std::begin; return begin(*self_); }()) {}

  Iterator(std::shared_ptr<Container> obj, ContainerIter&& start)
  : self_(std::move(obj)), it_(std::move(start)) {}

  static_assert(std::is_same<T&,
                typename std::iterator_traits<ContainerIter>::reference>::value,
      "Iterators returning proxy refererence not currently supported.");
  static_assert(
      std::is_base_of_v<
          std::forward_iterator_tag,
          typename std::iterator_traits<ContainerIter>::iterator_category>,
      "Input iterators are not currently supported.");

  const T* Next() noexcept {
    if (!self_) return nullptr;
    using std::end;  // Allow to find custom end() via ADL.
    if (it_ != end(*self_)) return &*it_++;
    self_.reset();
    return nullptr;
  }

 private:
  std::shared_ptr<Container> self_;  // Shared with CLIF-wrapped class.
  ContainerIter it_;
};

// ------------------------------------------------------------------

namespace callback {

using std::swap;

// Convert arguments.

inline void ArgIn(PyObject** a, Py_ssize_t idx, const py::PostConv& pc) {}

template<typename T1, typename... T>
void ArgIn(PyObject** a, Py_ssize_t idx, const py::PostConv& pc, T1&& c1,
           T&&... c) {
  if (a && *a) {
    PyObject* py = Clif_PyObjFrom(std::forward<T1>(c1), pc.Get(idx));
    if (!py) {
      Py_CLEAR(*a);
    } else {
      PyTuple_SET_ITEM(*a, idx, py);
    }
    ArgIn(a, idx+1, pc, std::forward<T>(c)...);
  }
}

template<typename R>
class ReturnValue {
 public:
  R FromPyValue(PyObject* result) {
    if (PyErr_Occurred()) {
      Py_XDECREF(result);
      HandlePyExc();
      return R();
    }
    CHECK(result != nullptr);
    R r;
    bool ok = Clif_PyObjAs(result, &r);
    Py_DECREF(result);
    if (!ok) {
      HandlePyExc();
    }
    return r;
  }
};

template <>
class ReturnValue<PyObject*> {
 public:
  PyObject* FromPyValue(PyObject* result) {
    if (PyErr_Occurred()) {
      HandlePyExc();
    }
    return result;
  }
};

template <>
class ReturnValue<void> {
 public:
  void FromPyValue(PyObject* result) {
    Py_XDECREF(result);
    if (PyErr_Occurred()) {
      HandlePyExc();
    }
  }
};

// Callback wrapper template class.

template<typename R, typename... T>
class Func {
 public:
  explicit Func(PyObject* callable, const py::PostConv& pc)
      : callback_(CHECK_NOTNULL(callable),
                  [](PyObject* obj) {
                    GilLock holder;
                    Py_CLEAR(obj);
                  }),
        pc_(pc) {
    GilLock holder;
    Py_INCREF(callable);
  }

  R operator()(T... arg) const {
    GilLock holder;  // Hold GIL during Python callback.
    int nargs = sizeof...(T);
    PyObject* pyargs = PyTuple_New(nargs);
    if (pyargs && nargs) ArgIn(&pyargs, 0, pc_, std::forward<T>(arg)...);
    if (!pyargs || PyErr_Occurred()) {
      // Error converting arg1 to Python.
      Py_XDECREF(pyargs);
      return ReturnValue<R>().FromPyValue(nullptr);
    } else {
      // Call the user function with our parameters.
      PyObject* result = PyObject_CallObject(callback_.get(), pyargs);
      Py_DECREF(pyargs);
      // Convert callback result to C++.
      return ReturnValue<R>().FromPyValue(result);
    }
  }

 private:
  // User-provided Python callback.
  // shared_ptr provides copy safety for this functor used for std::function arg
  std::shared_ptr<PyObject> callback_;
  const py::PostConv pc_;
};

}  // namespace callback

template<typename R, typename... T>
PyObject* FunctionCapsule(std::function<R(T...)> cfunction) {
  if (!cfunction) {
    // May be just CHECK() instead of exception?
    PyErr_SetString(PyExc_ValueError, "std::function target not set");
    return nullptr;
  }
  PyCapsule_Destructor dtor = +[](PyObject* caps) {
    // decltype is needed for GCC which erroneously complains that
    // cfunction is not captured.
    delete reinterpret_cast<std::function<R(T...)>*>(
        PyCapsule_GetPointer(caps, typeid(decltype(cfunction)).name()));
  };
  auto fp = new std::function<R(T...)>(cfunction);
  PyObject* f = PyCapsule_New(fp, typeid(decltype(cfunction)).name(), dtor);
  if (f == nullptr) {
    delete fp;
    return nullptr;
  }
  return f;
}

template<typename R, typename... T>
bool Clif_PyObjAs(PyObject* py, std::function<R(T...)>* c,
                  const py::PostConv& pc = {}) {
  CHECK(c != nullptr);
  if (!PyCallable_Check(py)) {
    PyErr_SetString(PyExc_TypeError, "callable expected");
    return false;
  }
  // Ensure we have enough args for callback. (Catch T* output args misuse.)
  if (!CallableNeedsNarguments(py, sizeof...(T))) return false;
  *c = callback::Func<R, T...>(py, pc);
  return true;
}

// ------------------------------------------------------------------

template<typename T>
bool Clif_PyObjAs(PyObject* py, std::unique_ptr<T>* c) {
  CHECK(c != nullptr);
  std::unique_ptr<T> pt(new T);
  if (!pt.get() || !Clif_PyObjAs(py, pt.get())) return false;
  *c = std::move(pt);
  return true;
}

// ------------------------------------------------------------------

// pair
template<typename T, typename U>
PyObject* Clif_PyObjFrom(const std::pair<T, U>& c, const py::PostConv& pc) {
  PyObject* py = PyTuple_New(2);
  if (py == nullptr) return nullptr;
  if (PyObject* item = Clif_PyObjFrom(c.first, pc.Get(0))) {
    PyTuple_SET_ITEM(py, 0, item);
  } else {
    Py_DECREF(py);
    return nullptr;
  }
  if (PyObject* item = Clif_PyObjFrom(c.second, pc.Get(1))) {
    PyTuple_SET_ITEM(py, 1, item);
  } else {
    Py_DECREF(py);
    return nullptr;
  }
  return py;
}
template<typename T, typename U>
bool Clif_PyObjAs(PyObject* py, std::pair<T, U>* c) {
  Py_ssize_t len = PySequence_Length(py);
  if (len != 2) {
    if (len != -1) {
      PyErr_Format(PyExc_ValueError, "expected a sequence"
                   " with len==2, got %zd", len);
    }
    return false;
  }
  using Key = typename std::remove_const<T>::type;
  using Val = typename std::remove_const<U>::type;
  Key k;
  PyObject* item = PySequence_ITEM(py, 0);
  if (!item || !Clif_PyObjAs(item, &k)) { Py_XDECREF(item); return false; }
  Py_DECREF(item);
  Val v;
  item = PySequence_ITEM(py, 1);
  if (!item || !Clif_PyObjAs(item, &v)) { Py_XDECREF(item); return false; }
  Py_DECREF(item);
  const_cast<Key&>(c->first) = std::move(k);
  const_cast<Val&>(c->second) = std::move(v);
  return true;
}

// ------------------------------------------------------------------

namespace py {

// Determine the type to use when forwarding an object of type U that is a
// subobject of a function parameter of type T&&.
template<typename T, typename U> struct ForwardedType {
  typedef typename std::remove_reference<U>::type &&type;
};
template<typename T, typename U> struct ForwardedType<T&, U> {
  typedef U &type;
};

// Forward an object of type U that is a subobject of another object of type
// T (usually passed as T&& to a function template).
template<typename T, typename U>
typename ForwardedType<T, U>::type forward_subobject(U &&u) {
  return static_cast<typename ForwardedType<T, U>::type>(u);
}

template<typename T>
PyObject* ListFromSizableCont(T&& c, const py::PostConv& pc) {
  PyObject* py = PyList_New(c.size());
  if (py == nullptr) return nullptr;
  const py::PostConv& pct = pc.Get(0);
  PyObject* v;
  Py_ssize_t i = 0;
  for (auto& j : c) {
    if ((v = Clif_PyObjFrom(forward_subobject<T>(j), pct)) == nullptr) {
      Py_DECREF(py);
      return nullptr;
    }
    PyList_SET_ITEM(py, i++, v);
  }
  return py;
}

template <typename T>
PyObject* ListFromIterators(T begin, T end, const py::PostConv& pc) {
  PyObject* py = PyList_New(std::distance(begin, end));
  if (py == nullptr) return nullptr;
  const py::PostConv& pct = pc.Get(0);
  PyObject* v;
  Py_ssize_t i = 0;
  for (auto it = begin; it != end; ++it) {
    if ((v = Clif_PyObjFrom(std::forward<typename std::iterator_traits<T>
                                         ::value_type>(*it), pct)) == nullptr) {
      Py_DECREF(py);
      return nullptr;
    }
    PyList_SET_ITEM(py, i++, v);
  }
  return py;
}

template<typename T>
PyObject* DictFromCont(T&& c, const py::PostConv& pc) {
  using std::get;
  PyObject* py = PyDict_New();
  if (py == nullptr) return nullptr;
  const py::PostConv& pck = pc.Get(0);
  const py::PostConv& pcv = pc.Get(1);
  for (const auto& i : c) {
    PyObject *k, *v{};
    if ((k = Clif_PyObjFrom(get<0>(i), pck)) == nullptr ||
        (v = Clif_PyObjFrom(forward_subobject<T>(get<1>(i)), pcv)) == nullptr ||
        PyDict_SetItem(py, k, v) < 0) {
      Py_DECREF(py);
      Py_XDECREF(k);
      Py_XDECREF(v);
      return nullptr;
    }
    Py_DECREF(k);
    Py_DECREF(v);
  }
  return py;
}

template<typename T>
PyObject* SetFromCont(const T& c, const py::PostConv& pc) {
  PyObject* py = PySet_New(0);
  if (py == nullptr) return nullptr;
  const py::PostConv& pct = pc.Get(0);
  for (const auto& i : c) {
    PyObject* j = Clif_PyObjFrom(i, pct);
    if (j == nullptr ||
        PySet_Add(py, j) < 0) {
      Py_DECREF(py);
      Py_XDECREF(j);
      return nullptr;
    }
    Py_DECREF(j);
  }
  return py;
}

template<size_t I> struct Tuple {
  template<typename... T>
  static bool FillFrom(PyObject* pytuple, const std::tuple<T...>& c,
                       const py::PostConv& pc) {
    PyObject* py = Clif_PyObjFrom(std::get<I-1>(c), pc.Get(I-1));
    if (py == nullptr) {
      Py_XDECREF(py);
      Py_DECREF(pytuple);
      return false;
    }
    PyTuple_SET_ITEM(pytuple, I-1, py);
    return Tuple<I-1>::FillFrom(pytuple, c, pc);
  }

  template<typename... T>
  static bool As(PyObject* pytuple, std::tuple<T...>* c) {
    if (!Clif_PyObjAs(PyTuple_GET_ITEM(pytuple, I-1), &std::get<I-1>(*c))) {
      return false;
    }
    return Tuple<I-1>::As(pytuple, c);
  }
};

// Stop template recursion at index 0.
template<>struct Tuple<0> {
  template<typename... T>
  static bool FillFrom(PyObject*, const std::tuple<T...>&, py::PostConv) {
    return true;
  }
  template<typename... T>
  static bool As(PyObject*, std::tuple<T...>*) { return true; }
};
}  // namespace py

// tuple
#define _TUPLE_SIZE(t) \
    std::tuple_size<typename std::remove_reference<decltype(t)>::type>::value

template<typename... T>
PyObject* Clif_PyObjFrom(const std::tuple<T...>& c, const py::PostConv& pc) {
  PyObject* py = PyTuple_New(_TUPLE_SIZE(c));
  if (py == nullptr) return nullptr;
  if (!py::Tuple<_TUPLE_SIZE(c)>::FillFrom(py, c, pc)) return nullptr;
  return py;
}
template<typename... T>
bool Clif_PyObjAs(PyObject* py, std::tuple<T...>* c) {
  CHECK(c != nullptr);
  Py_ssize_t len = PyTuple_Size(py);
  if (len != _TUPLE_SIZE(*c)) {
    if (len != -1) {
      PyErr_Format(PyExc_ValueError, "expected a tuple"
                   " with len==%zd, got %zd", _TUPLE_SIZE(*c), len);
    }
    return false;
  }
  return py::Tuple<_TUPLE_SIZE(*c)>::As(py, c);
}
#undef _TUPLE_SIZE

// optional
#ifdef ABSL_HAVE_STD_OPTIONAL
template <typename T>
PyObject* Clif_PyObjFrom(const std::optional<T>& opt,
                         const ::clif::py::PostConv& pc) {
  if (!opt) Py_RETURN_NONE;
  using ::clif::Clif_PyObjFrom;
  return Clif_PyObjFrom(*opt, pc.Get(0));
}

template <typename T>
bool Clif_PyObjAs(PyObject* py, std::optional<T>* c) {
  DCHECK(c != nullptr);
  if (Py_None == py) {  // Uninitialized case.
    c->reset();
    return true;
  }
  // Initialized case.
  using ::clif::Clif_PyObjAs;
  if (!Clif_PyObjAs(py, &c->emplace())) {
    c->reset();
    return false;
  }
  return true;
}
#endif  // ABSL_HAVE_STD_OPTIONAL

// list
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(const std::vector<T, Args...>& c,
                         const py::PostConv& pc) {
  return py::ListFromSizableCont(c, pc);
}
template<typename... Args>
PyObject* Clif_PyObjFrom(const std::vector<bool, Args...>& c,
                         const py::PostConv& pc) {
  return py::ListFromIterators(c.cbegin(), c.cend(), pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(std::vector<T, Args...>&& c, const py::PostConv& pc) {
  return py::ListFromSizableCont(std::move(c), pc);
}
template <typename... Args>
PyObject* Clif_PyObjFrom(std::vector<bool, Args...>&& c,
                         const py::PostConv& pc) {
  return py::ListFromIterators(c.cbegin(), c.cend(), pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(const std::list<T, Args...>& c,
                         const py::PostConv& pc) {
  return py::ListFromSizableCont(c, pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(std::list<T, Args...>&& c, const py::PostConv& pc) {
  return py::ListFromSizableCont(std::move(c), pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(const std::queue<T, Args...>& c,
                         const py::PostConv& pc) {
  return py::ListFromSizableCont(c, pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(std::queue<T, Args...>&& c, const py::PostConv& pc) {
  return py::ListFromSizableCont(std::move(c), pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(const std::priority_queue<T, Args...>& c,
                         const py::PostConv& pc) {
  return py::ListFromSizableCont(c, pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(std::priority_queue<T, Args...>&& c,
                         const py::PostConv& pc) {
  return py::ListFromSizableCont(std::move(c), pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(const std::deque<T, Args...>& c,
                         const py::PostConv& pc) {
  return py::ListFromSizableCont(c, pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(std::deque<T, Args...>&& c, const py::PostConv& pc) {
  return py::ListFromSizableCont(std::move(c), pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(const std::stack<T, Args...>& c,
                         const py::PostConv& pc) {
  return py::ListFromSizableCont(c, pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(std::stack<T, Args...>&& c, const py::PostConv& pc) {
  return py::ListFromSizableCont(std::move(c), pc);
}
template<typename T, std::size_t N>
PyObject* Clif_PyObjFrom(const std::array<T, N>& c, const py::PostConv& pc) {
  return py::ListFromSizableCont(c, pc);
}
template<typename T, std::size_t N>
PyObject* Clif_PyObjFrom(std::array<T, N>&& c, const py::PostConv& pc) {
  return py::ListFromSizableCont(std::move(c), pc);
}

// dict
template<typename T, typename U, typename... Args>
PyObject* Clif_PyObjFrom(const std::unordered_map<T, U, Args...>& c,
                         const py::PostConv& pc) {
  return py::DictFromCont(c, pc);
}
template<typename T, typename U, typename... Args>
PyObject* Clif_PyObjFrom(std::unordered_map<T, U, Args...>&& c,
                         const py::PostConv& pc) {
  return py::DictFromCont(std::move(c), pc);
}
template<typename T, typename U, typename... Args>
PyObject* Clif_PyObjFrom(const std::map<T, U, Args...>& c,
                         const py::PostConv& pc) {
  return py::DictFromCont(c, pc);
}
template<typename T, typename U, typename... Args>
PyObject* Clif_PyObjFrom(std::map<T, U, Args...>&& c, const py::PostConv& pc) {
  return py::DictFromCont(std::move(c), pc);
}

// set
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(const std::unordered_set<T, Args...>& c,
                         const py::PostConv& pc) {
  return py::SetFromCont(c, pc);
}
template<typename T, typename... Args>
PyObject* Clif_PyObjFrom(const std::set<T, Args...>& c,
                         const py::PostConv& pc) {
  return py::SetFromCont(c, pc);
}

// ------------------------------------------------------------------

namespace py {

// Helper function to walk Python iterable and put converted elements to a C++
// container of T via functor Inserter.
template<typename T, typename Inserter>
bool IterToCont(PyObject* py, Inserter add) {
  PyObject* it = PyObject_GetIter(py);
  if (it == nullptr) return false;
  PyObject *el;
  while ((el = PyIter_Next(it)) != nullptr) {
    using Item = std::remove_const_t<T>;
    if constexpr (std::is_default_constructible_v<Item>) {
      Item item;
      bool ok = Clif_PyObjAs(el, &item);
      Py_DECREF(el);
      if (!ok) {
        Py_DECREF(it);
        return false;
      }
      add(std::move(item));
    } else {
      Item* item;
      bool ok = Clif_PyObjAs(el, &item);
      if (!ok) {
        Py_DECREF(el);
        Py_DECREF(it);
        return false;
      }
      add(Item(*item));
      Py_DECREF(el);
    }
  }
  Py_DECREF(it);
  return !PyErr_Occurred();
}

// Helper function to walk Python dict (via items()) and put converted elements
// to a C++ container via functor add.
template<typename T, typename U, typename F>
bool ItemsToMap(PyObject* py, F add) {
  py = PyObject_CallMethod(py, "items", nullptr);
  if (py == nullptr) return false;
  bool ok = py::IterToCont<std::pair<T, U>>(py, add);
  Py_DECREF(py);
  return ok;
}
}  // namespace py

// list
template<typename T, typename... Args>
bool Clif_PyObjAs(PyObject* py, std::vector<T, Args...>* c) {
  CHECK(c != nullptr);
  c->clear();
  return py::IterToCont<T>(py, [&c](T&& i) {  //NOLINT: build/c++11
    c->push_back(std::move(i));
  });
}

template <typename T, std::size_t N>
bool Clif_PyObjAs(PyObject* py, std::array<T, N>* c) {
  CHECK(c != nullptr);

  int index = 0;
  bool rval = py::IterToCont<T>(py, [&c, &index](T&& i) {
    if (index < N) {
      (*c)[index] = std::move(i);
    }
    ++index;  // Continue to increment, so we know true size for reporting.
  });
  if (index != N) {
    PyErr_Format(PyExc_ValueError, "expected a size of %zd, got %zd", N, index);
    return false;
  }
  return rval;
}

// set
template<typename T, typename... Args>
bool Clif_PyObjAs(PyObject* py, std::unordered_set<T, Args...>* c) {
  CHECK(c != nullptr);
  c->clear();
  return py::IterToCont<T>(py, [&c](T&& i) {  //NOLINT: build/c++11
    c->insert(std::move(i));
  });
}
template<typename T, typename... Args>
bool Clif_PyObjAs(PyObject* py, std::set<T, Args...>* c) {
  CHECK(c != nullptr);
  c->clear();
  return py::IterToCont<T>(py, [&c](T&& i) {  //NOLINT: build/c++11
    c->insert(std::move(i));
  });
}

// dict
template<typename T, typename U, typename... Args>
bool Clif_PyObjAs(PyObject* py, std::unordered_map<T, U, Args...>* c) {
  CHECK(c != nullptr);
  c->clear();
  return py::ItemsToMap<T, U>(py, [&c](typename std::pair<T, U>&& i) {  //NOLINT: build/c++11
    // TODO: Use insert_or_assign since c++17
    (*c)[i.first] = std::move(i.second);
  });
}
template<typename T, typename U, typename... Args>
bool Clif_PyObjAs(PyObject* py, std::map<T, U, Args...>* c) {
  CHECK(c != nullptr);
  c->clear();
  return py::ItemsToMap<T, U>(py, [&c](typename std::pair<T, U>&& i) {  //NOLINT: build/c++11
    (*c)[i.first] = std::move(i.second);
  });
}
}  // namespace clif

#endif  // CLIF_PYTHON_STLTYPES_H_
