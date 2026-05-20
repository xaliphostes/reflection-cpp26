// Pybind11 backend: implements the rosetta::walk visitor concept.
//
// Provides:
//   - rosetta::PybindVisitor<T> — five visitor methods emitting pybind11 calls
//   - rosetta::bind_pybind<T>(module, py_name) — entry point: declares the
//     class, registers a default ctor, runs the walk

#pragma once

#include <experimental/meta>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <rosetta/walk.h>
#include <string>
#include <type_traits>

namespace py = pybind11;

namespace rosetta {

    template <typename T> struct PybindVisitor {
        py::class_<T> &cls;

        template <std::meta::info Fld>
        void field_plain(const char *name, const char *doc) {
            using F = [:std::meta::type_of(Fld):];
            cls.def_property(
                name, [](const T &s) -> F { return s.[:Fld:]; },
                [](T &s, F v) { s.[:Fld:] = v; }, doc);
        }

        template <std::meta::info Fld>
        void field_readonly(const char *name, const char *doc) {
            using F = [:std::meta::type_of(Fld):];
            cls.def_property_readonly(
                name, [](const T &s) -> F { return s.[:Fld:]; }, doc);
        }

        template <std::meta::info Fld, double Rmin, double Rmax>
        void field_ranged(const char *name, const char *doc) {
            using F = [:std::meta::type_of(Fld):];
            cls.def_property(
                name, [](const T &s) -> F { return s.[:Fld:]; },
                [name](T &s, F v) {
                    double d = static_cast<double>(v);
                    if (d < Rmin || d > Rmax) {
                        throw py::value_error(
                            std::string(name) + " out of range [" +
                            std::to_string(Rmin) + ", " +
                            std::to_string(Rmax) + "]");
                    }
                    s.[:Fld:] = v;
                },
                doc);
        }

        template <std::meta::info Fn>
        void method_instance(const char *name, const char *doc) {
            cls.def(name, &[:Fn:], doc);
        }

        template <std::meta::info Fn>
        void method_static(const char *name, const char *doc) {
            cls.def_static(name, &[:Fn:], doc);
        }
    };

    template <typename T>
    void bind_pybind(py::module_ &m, const char *py_name) {
        py::class_<T> cls(m, py_name);
        cls.def(py::init<>());
        PybindVisitor<T> v{cls};
        walk<T>(v);
    }

} // namespace rosetta
