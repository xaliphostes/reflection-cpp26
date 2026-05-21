## A. Multiple args

`detail::make_invoker<T, Fn>(index_sequence<Is...>)` is the core trick:

`std::any_cast<std::remove_cvref_t<typename[: type_of(params[Is]) :]>>(args[Is])...`

Each pack element splices the i-th parameter's reflected type out of `parameters_of(Fn)`, strips cvref so `any_cast` returns by value, then index-extracts the corresponding `Any` from the runtime span. Calling the spliced method `obj->[:Fn:](...)` does the rest. The whole invoker is a stateless lambda (no captures — `Fn` is a template parameter, `params` is a function-local constexpr).
 
### Two minor things to know

- The splice in a template argument must be parenthesized or prefixed with typename — `typename[: ... :]` works, bare `[: ... :]>` confuses the parser.
- Param types are passed by value through `Any` (cvref stripped). `T&` non-const out-params won't round-trip — that needs a differentstorage model. Const-ref and by-value are fine.

### Gaps that still remain 

- Overloads — map keyed by name, last one wins. Real Rosetta keys by `(name, signature)`.
- Static methods, constructors, inheritance — skipped.
- `const` method on const object — we always cast through non-const `T*`, so calling a non-const method on a const handle wouldn't be
caught.

## B. Overloads

### What changed vs register_reflected.cpp
 
1. `MethodMeta` gained param_types: `vector<type_index>` (for dispatch) and param_type_names: `vector<string>` (for display/signature).
2. `ClassMeta::methods` became `unordered_map<name, vector<MethodMeta>>` — one bucket per name, multiple entries per overload set.
3. `ClassMeta::resolve(name, args)` and `ClassMeta::invoke(name, obj, args)` do the runtime dispatch by walking the bucket and matching 
arg types element-by-element.
4. New `detail::build_param_info<Fn>(seq)` produces the dispatch table per overload at registration time, using the same splice trick 
as the invoker. Subtle gotcha: the inline `display_string_of(type_of(params[Is]))` form has the info value leaking into a 
non-constant-evaluated expression context. Fix was to wrap it in a `consteval helper param_type_display_name<Fn, I>()`. 
 
### Limitations still on the list

- Dispatch is exact-match on type_index. No implicit conversion — pass int for an int parameter, not anything else. Production
Rosetta would do best-match with a conversion-cost score.
- `std::any_cast` accepts only the exact stored type. A literal 42 stored in Any won't bind to a long long parameter even if 
implicitly convertible.
- `bool` and `int` are distinct overloads. That's actually correct for C++, but Python-style "everything is int/float" callers would
need a coercion layer.

## C. Inheritance

### What was added

- `ClassMeta::bases`: `vector<string>` — names of registered base classes.
- `MethodMeta::is_static`: `bool` — flag flowing through to signature() display.
- `detail::register_self_only<Derived, Self>` — emits invokers that do `static_cast<Self*>(static_cast<Derived*>(obj))`. For Derived ==
Self this is a no-op cast; for an inherited member it's the implicit derived-to-base conversion.
- `detail::register_with_bases<Derived, Self>` — depth-first recursion: walk `bases_of(^^Self)` first (registering each base's members as if they belonged to Derived), then register_self_only. Bases-then-self order means derived registrations naturally land after inherited ones.
- `detail::make_static_invoker<Fn>` — splices `[: Fn :](args...)` with no obj cast. Selected at compile time via if `constexpr (is_static_member(fn))`.
- Signature dedupe in the overload bucket — when adding a method, if an existing entry in the bucket has the same param_types, replace it. That's what collapses Shape::area into a single area entry in Circle's metadata.

### Two subtle points worth flagging

- The dedupe matters for correctness as well as display. Without it, the bucket would have two zero-arg area() entries; resolve() would pick whichever it found first, which is order-dependent. Replacing-on-match means the derived's direct invoker wins (no extra
`static_cast<Shape*>` indirection, though virtual dispatch would produce the same result).
- Static methods are still walked per derived class. Circle's metadata has its own copy of next_id from Shape's base walk. That's
intentional — language bindings (Circle.next_id() in Python) expect the static accessible from the derived class.

### Still on the gap list

- Diamond inheritance / virtual bases. Recursion would visit a virtual base twice; we'd need a visited set keyed on type info.
- Const-correctness on instance methods. A const-method invoker should accept const void* to avoid casting away const.
- Private base members. P2996's access_context decides what members_of returns; we're using current() from the registration site, so private members are excluded unless we use unchecked().


## D. Annotation

### The new pieces

1. Three annotation types, each structural and NTTP-eligible:
    ```cpp
    struct doc {
        const char *text;
        consteval doc(const char *t) : text(std::define_static_string(t)) {}
        bool operator==(const doc&) const = default;
    };
    struct range  { double min, max; bool operator==(const range&) const = default; };
    struct readonly {  bool operator==(const readonly&) const = default; };
    ```
1. Compile-time extraction inside the field walk:
   ```cpp
    constexpr auto doc_opt= std::meta::annotation_of_type<doc>(m);
    constexpr auto range_opt = std::meta::annotation_of_type<range>(m);
    constexpr bool ro  = std::meta::annotation_of_type<readonly>(m).has_value();
    ```
1. Three setter shapes selected at compile time (if-constexpr outside the lambda, not inside — see the gotcha below):
  - ro → setter always throws.
  - range on arithmetic field → setter validates against captured rmin/rmax, then assigns.
  - Otherwise → plain setter.
2. CMake: per-target flag target_compile_options(register_annotations PRIVATE -fannotation-attributes) — annotations are gated on
a separate clang flag from -freflection.

### Two non-obvious gotchas  
 
- `[[=rosetta::doc{"text"}]]` won't compile with a plain `string_view` text member. The literal's address has no linkage, so it's not 
a valid NTTP pointer. The consteval constructor running `std::define_static_string` interns the literal into linkage-having static
storage; that pointer is NTTP-eligible. This is the trick that makes string annotations ergonomic.  
- if `constexpr` inside a lambda body doesn't see the enclosing `constexpr` local as `constexpr`. The constexpr-ness has to be carried
in either as a non-type template parameter or by gating the lambda choice with if constexpr outside. I went with the latter —  
three distinct setter lambdas, one branch each.
 
### What this unlocks for Rosetta 
  
The fluent .doc("…").range(0, 150).readonly() chain becomes obsolete for in-source types — the metadata travels with the field 
declaration. Your generators (REST, Qt editor, JSON) now read FieldMeta::doc, FieldMeta::range, FieldMeta::is_readonly directly.
The fluent API is still useful for third-party types you can't annotate. 
 
### Still on the list

- Custom annotation types — the user should be able to define their own `[[=my_validator{}]]` and have a generic walk hand them to a
 callback. Right now it's hard-coded to three types.
- Annotations on parameters — `int years_until([[=range{0, 200}]] int target)` would validate inputs before invocation. 
- Annotations on classes themselves — `[[=rosetta::category{"shapes"}]] struct Circle` etc.

