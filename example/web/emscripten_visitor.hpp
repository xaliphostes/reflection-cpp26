// Emscripten/embind backend: implements the rosetta::walk visitor concept.
//
// Provides:
//   - rosetta::EmscriptenVisitor<T> — five visitor methods emitting
//     emscripten::class_<T> registration calls
//   - rosetta::bind_emscripten<T>(class_name) — entry point: builds the
//     class_ handle, registers a default ctor, runs the walk
//
// All lambdas are non-capturing (Fld / Fn / Rmin / Rmax are NTTPs of the
// enclosing template, accessible without capture) so embind can take
// them as function-pointer types via `+[]`.

#pragma once

#include <emscripten/bind.h>
#include <experimental/meta>
#include <rosetta/walk.h>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace rosetta {

    template <typename T> struct EmscriptenVisitor {
        emscripten::class_<T> &cls;

        template <std::meta::info Fld>
        void field_plain(const char *name, const char * /*doc*/) {
            using F = [:std::meta::type_of(Fld):];
            cls.property(
                name,
                +[](const T &s) -> F { return s.[:Fld:]; },
                +[](T &s, F v) { s.[:Fld:] = v; });
        }

        template <std::meta::info Fld>
        void field_readonly(const char *name, const char * /*doc*/) {
            using F = [:std::meta::type_of(Fld):];
            // Pair the getter with a throwing setter so JS-side assignment
            // surfaces a clear error (embind silently no-ops a getter-only
            // accessor in non-strict mode).
            cls.property(
                name,
                +[](const T &s) -> F { return s.[:Fld:]; },
                +[](T &, F) {
                    constexpr auto fname =
                        std::define_static_string(std::meta::identifier_of(Fld));
                    throw std::runtime_error(
                        std::string(fname) + " is read-only");
                });
        }

        template <std::meta::info Fld, double Rmin, double Rmax>
        void field_ranged(const char *name, const char * /*doc*/) {
            using F = [:std::meta::type_of(Fld):];
            cls.property(
                name,
                +[](const T &s) -> F { return s.[:Fld:]; },
                +[](T &s, F v) {
                    constexpr auto fname =
                        std::define_static_string(std::meta::identifier_of(Fld));
                    double d = static_cast<double>(v);
                    if (d < Rmin || d > Rmax) {
                        throw std::runtime_error(
                            std::string(fname) + " out of range");
                    }
                    s.[:Fld:] = v;
                });
        }

        template <std::meta::info Fn>
        void method_instance(const char *name, const char * /*doc*/) {
            cls.function(name, &[:Fn:]);
        }

        template <std::meta::info Fn>
        void method_static(const char *name, const char * /*doc*/) {
            cls.class_function(name, &[:Fn:]);
        }
    };

    template <typename T> void bind_emscripten(const char *class_name) {
        emscripten::class_<T> cls(class_name);
        cls.template constructor<>();
        EmscriptenVisitor<T> v{cls};
        walk<T>(v);
    }

} // namespace rosetta
