// One consteval walk over T's reflected members. The walk decides which
// shape each field/method takes (readonly / ranged / plain, instance /
// static) and calls into a backend-specific visitor.
//
// Visitor concept — five `consteval`-template members:
//
//   v.template field_plain    <Fld>                 (name, doc)
//   v.template field_readonly <Fld>                 (name, doc)
//   v.template field_ranged   <Fld, Rmin, Rmax>     (name, doc)
//   v.template method_instance<Fn>                  (name, doc)
//   v.template method_static  <Fn>                  (name, doc)
//
// `Fld` / `Fn` are `std::meta::info` NTTPs; `name` and `doc` are
// `const char*` pointing to static storage (via define_static_string).
// `Rmin` / `Rmax` are `double` NTTPs baked into each ranged instantiation.

#pragma once

#include <experimental/meta>
#include <rosetta/annotations.hpp>
#include <type_traits>

namespace rosetta {

    // Keep regular and static methods; drop constructors/destructors and
    // the compiler-generated copy/move specials.
    consteval bool is_exportable_member_function(std::meta::info fn) {
        return std::meta::is_function(fn) && !std::meta::is_constructor(fn) &&
               !std::meta::is_destructor(fn) && !std::meta::is_special_member_function(fn);
    }

    template <typename T, typename Visitor> void walk(Visitor &v) {
        constexpr auto ctx = std::meta::access_context::current();

        // -------- fields --------
        template for (constexpr auto fld :
                      std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {

            using FieldT               = [:std::meta::type_of(fld):];
            constexpr auto        name = std::define_static_string(std::meta::identifier_of(fld));
            constexpr auto        doc_opt   = std::meta::annotation_of_type<doc>(fld);
            constexpr auto        range_opt = std::meta::annotation_of_type<range>(fld);
            constexpr bool        ro     = std::meta::annotation_of_type<readonly>(fld).has_value();
            constexpr const char *docstr = doc_opt.has_value() ? doc_opt->text : "";

            if constexpr (ro) {
                v.template field_readonly<fld>(name, docstr);
            } else if constexpr (range_opt.has_value() && std::is_arithmetic_v<FieldT>) {
                constexpr double rmin = range_opt->min;
                constexpr double rmax = range_opt->max;
                v.template field_ranged<fld, rmin, rmax>(name, docstr);
            } else {
                v.template field_plain<fld>(name, docstr);
            }
        }

        // -------- methods (instance + static) --------
        template for (constexpr auto fn :
                      std::define_static_array(std::meta::members_of(^^T, ctx))) {
            if constexpr (is_exportable_member_function(fn)) {
                constexpr auto name      = std::define_static_string(std::meta::identifier_of(fn));
                constexpr auto m_doc_opt = std::meta::annotation_of_type<doc>(fn);
                constexpr const char *mdoc = m_doc_opt.has_value() ? m_doc_opt->text : "";

                if constexpr (std::meta::is_static_member(fn)) {
                    v.template method_static<fn>(name, mdoc);
                } else {
                    v.template method_instance<fn>(name, mdoc);
                }
            }
        }
    }

} // namespace rosetta
