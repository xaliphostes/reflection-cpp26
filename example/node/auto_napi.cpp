// Auto-N-API demo: the binding kit lives in <napi_visitor.hpp> and
// <rosetta/walk.hpp>. The demo type lives in ../person.hpp. This file
// just registers it as the Node module entry point.
//
// Build flags: -freflection -freflection-latest -fannotation-attributes
// See CMakeLists.txt in this folder.

#include "../person.hpp"
#include "napi_visitor.hpp"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set("Person", rosetta::bind_napi<Person>(env, "Person"));
    return exports;
}

NODE_API_MODULE(reflected_person, Init)
