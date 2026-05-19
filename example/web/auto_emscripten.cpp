// Auto-embind demo: binding kit lives in <emscripten_visitor.hpp> and
// <rosetta/walk.hpp>. The demo type lives in ../person.hpp. This file
// just registers it inside an EMSCRIPTEN_BINDINGS block.
//
// Build flags: -freflection -freflection-latest -fannotation-attributes
// See CMakeLists.txt in this folder.

#include "../person.hpp"
#include "emscripten_visitor.hpp"

EMSCRIPTEN_BINDINGS(reflected_person) {
    rosetta::bind_emscripten<Person>("Person");
}
