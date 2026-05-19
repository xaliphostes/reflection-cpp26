// Auto-pybind11 demo: the binding kit lives in <pybind_visitor.hpp> and
// <rosetta/walk.hpp>. The demo type lives in ../person.hpp. This file
// just registers it with pybind11.
//
// Build flags: -freflection -freflection-latest -fannotation-attributes
// (see CMakeLists.txt in this directory).

#include "../person.hpp"
#include "pybind_visitor.hpp"

PYBIND11_MODULE(reflected_person, m) {
    m.doc() = "Bindings generated automatically by C++26 reflection.";
    rosetta::bind_pybind<Person>(m, "Person");
}
