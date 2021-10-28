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

/*
From .../python-2.7.3-docs-html/c-api/intro.html#include-files:
Since Python may define some pre-processor definitions which affect the
standard headers on some systems, you must include Python.h before any standard
headers are included.
*/
#include <Python.h>
#include <string>

#include "clif/python/pyproto.h"
#include "clif/python/runtime.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"

namespace {

class ModNameComponents {
 public:
  explicit ModNameComponents(const std::string& str) : str_(str) {
    Split();
  }

  std::vector<std::string>::const_iterator begin() const noexcept {
    return components_.begin();
  }

  std::vector<std::string>::const_iterator end() const noexcept {
    return components_.end();
  }

 private:
  void Split() {
    std::size_t cur_start = 0;
    while (true) {
      std::size_t i = str_.find('.', cur_start);
      if (i == std::string::npos) {
        components_.emplace_back(str_.substr(cur_start));
        break;
      } else {
        components_.emplace_back(str_.substr(cur_start, i - cur_start));
        cur_start = i + 1;
      }
    }
  }

  std::string str_;
  std::vector<std::string> components_;
};

}  // namespace

namespace clif {

namespace proto {

// Get py.DESCRIPTOR.full_name as a new object.
PyObject* GetMessageName(PyObject* py) {
  PyObject* pyd = PyObject_GetAttrString(py, "DESCRIPTOR");
  if (pyd == nullptr) return nullptr;
  PyObject* fn = PyObject_GetAttrString(pyd, "full_name");
  Py_DECREF(pyd);
  if (fn == nullptr) return nullptr;
  if (!PyUnicode_Check(fn)) {
    PyErr_SetString(PyExc_TypeError, "DESCRIPTOR.full_name must return str");
    Py_DECREF(fn);
    return nullptr;
  }
  return fn;
}
}  // namespace proto

bool Internal_Clif_PyObjAs(PyObject* py, std::unique_ptr<::proto2::Message>* c,
                           bool force_from_generated_pool) {
  CHECK(c != nullptr);
  PyObject* fn = proto::GetMessageName(py);
  if (fn == nullptr) return false;
  const proto2::DescriptorPool* dp = proto2::DescriptorPool::generated_pool();
  if (dp == nullptr) {
    PyErr_SetNone(PyExc_MemoryError);
    Py_DECREF(fn);
    return false;
  }
  const proto2::Descriptor* d = dp->FindMessageTypeByName(
      PyUnicode_AsUTF8(fn));
  if (d == nullptr) {
    PyErr_Format(PyExc_TypeError, "DESCRIPTOR.full_name %s not found",
                 PyUnicode_AsUTF8(fn));
    Py_DECREF(fn);
    return false;
  }
  Py_DECREF(fn);
  proto2::Message* m = proto2::MessageFactory::generated_factory()->
      GetPrototype(d)->New();
  if (m == nullptr) {
    PyErr_SetNone(PyExc_MemoryError);
    return false;
  }
  if (!proto::TypeCheck(
      py, ImportFQName("google.protobuf.message.Message"),
      "", "proto2_Message_subclass")) return false;
  PyObject* ser = proto::Serialize(py);
  if (ser == nullptr) return false;
  proto2::io::CodedInputStream coded_input_stream(
      reinterpret_cast<uint8_t*>(PyBytes_AS_STRING(ser)),
      PyBytes_GET_SIZE(ser));
  if (!m->MergePartialFromCodedStream(&coded_input_stream)) {
    PyErr_SetString(PyExc_ValueError, "Parse from serialization failed");
    Py_DECREF(ser);
    return false;
  }
  Py_DECREF(ser);
  *c = ::std::unique_ptr<::proto2::Message>(m);
  return true;
}

bool Clif_PyObjAs(PyObject* py, std::unique_ptr<::proto2::Message>* c) {
  return Internal_Clif_PyObjAs(py, c, false);
}

namespace proto {

bool SetNestedName(PyObject** module_name, const char* nested_name) {
  DCHECK(module_name != nullptr);
  DCHECK(*module_name != nullptr);
  DCHECK(nested_name != nullptr);
  if (*nested_name) {
    for (const auto& n : ModNameComponents(nested_name)) {
      PyObject* attr_name = PyUnicode_FromStringAndSize(n.data(), n.size());
      if (attr_name == nullptr) {
        Py_DECREF(*module_name);
        return false;
      }
      PyObject* atr = PyObject_GetAttr(*module_name, attr_name);
      Py_DECREF(attr_name);
      Py_DECREF(*module_name);
      if (atr == nullptr) return false;
      *module_name = atr;
    }
  }
  return true;
}

// Check the given pyproto to be class_name instance.
bool TypeCheck(PyObject* pyproto,
              PyObject* imported_pyproto_class,  // takes ownership
              const char* element_name,
              const char* class_name) {
  if (imported_pyproto_class == nullptr) return false;  // Import failed.
  if (!SetNestedName(&imported_pyproto_class, element_name)) return false;
  int proto_instance = PyObject_IsInstance(pyproto, imported_pyproto_class);
  Py_DECREF(imported_pyproto_class);
  if (proto_instance < 0 ) return false;  // Exception already set.
  if (!proto_instance)
    PyErr_Format(PyExc_TypeError, "expecting %s proto, got %s %s",
                 class_name, ClassName(pyproto), ClassType(pyproto));
  return proto_instance;
}

// Return bytes serialization of the given pyproto.
PyObject* Serialize(PyObject* pyproto) {
  PyObject* raw = PyObject_CallMethod(pyproto, "SerializePartialToString",
                                      nullptr);
  if (raw == nullptr) return nullptr;
  if (!PyBytes_Check(raw)) {
    PyErr_Format(PyExc_TypeError, "%s.SerializePartialToString() must return"
                 " bytes, got %s %s", ClassName(pyproto),
                 ClassName(raw), ClassType(raw));
    Py_DECREF(raw);
    return nullptr;
  }
  return raw;
}

// If pyproto.DESCRIPTOR.full_name in C++ generated pool and is same as cproto,
// copy pyproto into cproto and return true else return false.
bool InGeneratedPool(PyObject* pyproto, proto2::Message* cproto) {
  if (cproto->GetDescriptor()) {
    PyObject *ptype, *pvalue, *ptraceback;
    PyErr_Fetch(&ptype, &pvalue, &ptraceback);
    if (PyObject* full_name = GetMessageName(pyproto)) {
      std::string py_name(PyUnicode_AsUTF8(full_name));
      Py_DECREF(full_name);
      if (py_name == cproto->GetDescriptor()->full_name()) {
        // Try to get the named message from the C++ generated pool.
        std::unique_ptr<proto2::Message> temp;
        PyErr_Clear();
        // The cproto->CopyFrom() reqiures messages are from the same pool.
        // Force the temp message is from C++ generated pool even python side
        // does not linked in the C++ generated lib.
        if (Internal_Clif_PyObjAs(pyproto, &temp, true)) {
          cproto->CopyFrom(*temp);
          return true;
        }
      }
    }
    PyErr_Restore(ptype, pvalue, ptraceback);
  }
  return false;
}

PyObject* PyProtoFrom(const ::proto2::Message* cproto,
                      PyObject* imported_pyproto_class,
                      const char* element_name) {
  DCHECK(cproto != nullptr);
  if (imported_pyproto_class == nullptr) return nullptr;  // Import failed.
  if (!SetNestedName(&imported_pyproto_class, element_name)) return nullptr;
  PyObject* pb = PyObject_CallObject(imported_pyproto_class, nullptr);
  Py_DECREF(imported_pyproto_class);
  if (pb == nullptr) return nullptr;
  std::string bytes = cproto->SerializePartialAsString();
  PyObject* merge = PyUnicode_FromString("MergeFromString");
  PyObject* cpb = PyMemoryView_FromMemory(const_cast<char*>(bytes.data()),
                                          bytes.size(), PyBUF_READ);
  if (!merge || !cpb) {
    Py_DECREF(pb);
    Py_XDECREF(merge);
    Py_XDECREF(cpb);
    return nullptr;
  }
  PyObject* ret = PyObject_CallMethodObjArgs(pb, merge, cpb, nullptr);
  Py_DECREF(merge);
  Py_DECREF(cpb);
  if (ret == nullptr) {
    Py_DECREF(pb);
    return nullptr;
  }
  Py_DECREF(ret);
  return pb;
}
}  // namespace proto
}  // namespace clif
