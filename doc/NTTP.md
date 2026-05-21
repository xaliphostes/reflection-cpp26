A non-type template parameter (NTTP) is a value (not a type) used as a template argument: `template <int N>`, `template <auto X>`, `template <std::meta::info F>`. The set of types you can use there is restricted because the compiler must be able to (a) construct the value at compile time and (b) decide whether two template instantiations were given "the same" argument.
 
## The pre-C++20 list
 
Originally NTTPs were limited to: 
- integer / enum types 
- pointer / pointer-to-member / lvalue reference (with constraints on the pointee) 
- `std::nullptr_t`

You could not say `template <std::string S>` or pass `range{0,1}` as a template argument.

## C++20 added "structural class types"

**P0732** introduced a category of class types usable as NTTPs. A class type T is structural iff:

1. It is a literal type (constexpr-constructible, constexpr-destructible, no virtual functions, …).
2. Every non-static data member and base subobject is public and non-mutable.
3. Each of those members/bases is itself a structural type, a scalar, an lvalue reference, or an array of such.

That's why the three annotations in include/rosetta/annotations.h are written the way they are: simple aggregates of public, non-mutable members of scalar or pointer types. The operator== = default is no longer strictly required for NTTP-ness (since **P1907** — equivalence is memberwise), but it keeps the types easily comparable for other purposes.

Counter-examples: `std::string` is not structural (private members, owning resources). std::vector is not structural. A struct with a private: field is not structural.

## Why doc runs the string through `std::define_static_string`

Even when the class type is structural, a pointer member must point to something whose address is a constant expression. A bare `const char*` initialised from a string literal in some random TU doesn't satisfy that — the literal's address isn't guaranteed unique/stable across translation units. `std::define_static_string("hi")` materializes the bytes into a uniquely-identified, program-lifetime array and returns a pointer with the required properties — so the resulting doc{...} value can sit in template-argument storage and be compared with another doc{...} from a different TU correctly.

In short: structural is about the class layout being inspectable and comparable at compile time; NTTP-eligible is structural-class plus the additional rules on what scalar values (especially pointers) the compiler accepts as template arguments.
