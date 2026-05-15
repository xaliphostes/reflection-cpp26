---
marp: true
title: Reflection in C++26
paginate: true
theme: default
class: invert
---

<!-- _class: invert lead -->

# Reflection in C++26

From P2996 to a 330-line Rosetta

---

## What is reflection?

> *"The ability of a software to expose its internal structure."*

- One of the **largest** proposals ever shipped to C++ (P2996)
- **Static** reflection: the compiler exposes structure at **compile time**
- Two new syntactic operators:
  - **Meta**: `^^` — *reflect-on* a name into a `std::meta::info` value
  - **Splice**: `[: meta :]` — *reify* a `std::meta::info` back into code

---

## The two operators in one picture

```cpp
constexpr std::meta::info info = ^^Point;  // reflect-on
typename[: info :] p = {.x = 1, .y = 2, .z = 2.1};  // splice
```

- `^^T` lifts the *name* `T` into a constexpr value (`std::meta::info`)
- `[: info :]` drops a `std::meta::info` value back into a *type*, *expression*, or *member*
- Everything that crosses the boundary is `consteval` / `constexpr`

---

## Hello, reflection

```cpp
struct Point { int x; int y; double z; };

int main() {
    constexpr auto ctx = std::meta::access_context::current();
    template for (constexpr auto m :
        std::define_static_array(
            std::meta::nonstatic_data_members_of(^^Point, ctx))) {
        std::println("  {} : {}",
            std::meta::identifier_of(m),
            std::meta::display_string_of(std::meta::type_of(m)));
    }
}
```

---

## State of the art — the design space

Every reflection system trades along the same axes:

| Axis | Endpoints |
|---|---|
| **When** | Compile-time only ↔ Runtime-queryable |
| **Intrusiveness** | Non-intrusive ↔ Macros / annotations required |
| **Source of metadata** | Native compiler ↔ External codegen ↔ Manual |
| **Coverage** | Aggregates only ↔ Methods + virtuals + private |
| **Target use** | Serialization ↔ Bindings ↔ GUIs ↔ RPC ↔ Plugins |

C++ has historically lacked native reflection — that gap spawned **dozens** of libraries with different philosophies.

---

## State of the art — C++ before P2996

| Library | Type | Intrusive? | Coverage |
|---|---|---|---|
| **RTTI** (`typeid`) | language | none | type identity only |
| **Boost.PFR** | header | none | aggregates only |
| **magic_enum** | header | none | enums (via `__PRETTY_FUNCTION__`) |
| **RTTR** | header | macro | fields, methods, enums, inheritance |
| **EnTT::meta** | header | registration | data + funcs (games) |
| **Boost.Describe** | header | macro | members + bases |
| **Qt MOC** | codegen | `Q_OBJECT` | full + signals/slots |
| **Unreal UHT** | codegen | `UCLASS` | full + GC + Blueprint |
| **SWIG** | codegen | none | multi-language bindings |

No single library wins — each targets a specific niche on the axes above.

---

## State of the art — other languages

| Language | Runtime | Compile-time | Idiom |
|---|:-:|:-:|---|
| **Python** | 🟢 deep | ⚪ | `inspect`, `getattr`, decorators |
| **C#** | 🟢 native | 🟢 source generators | `System.Reflection` + attributes |
| **Java** | 🟢 native | 🟡 annotation processors | `java.lang.reflect` |
| **JavaScript** | 🟢 Proxy | ⚪ | `Reflect`, `Object.*` |
| **Go** | 🟢 (slow) | ⚪ | `reflect` + struct tags |
| **Rust** | 🔴 minimal | 🟢 proc-macros | `derive`, `serde`, `bevy_reflect` |
| **D** | 🟡 partial | 🟢 `__traits` + CTFE | `__traits(allMembers, T)` |
| **Zig** | ⚪ | 🟢 `comptime` | `@typeInfo`, `inline for` |
| **C++ < 26** | 🟡 RTTI only | 🟡 TMP, libraries | RTTR / PFR / codegen |
| **C++26** | 🟡 build manually | 🟢 P2996 | `^^T`, `[: r :]`, `consteval` |

C++ was **the only major language without first-class reflection** — until now.

---

## Lessons from the neighbours

- **D** is the spiritual ancestor of P2996 — `__traits` + CTFE + `static foreach` shaped the C++26 design.
- **Zig** confirms the philosophy: `comptime` *is* the reflection. Runtime metadata is something you build explicitly.
- **Rust `bevy_reflect`** proves runtime registries can be idiomatic even in a language with no runtime reflection in std.
- **Go's struct tags** — lightweight per-field metadata strings; maps cleanly onto P3394 annotations.
- **C# attributes + source generators** — cleanest combination of runtime queryability and compile-time codegen.

> C++26 closes the **language-level** gap. Runtime registries (Rosetta, RTTR, EnTT) don't go away — they get auto-filled.

---

## Rosetta in C++26

A real-world payoff for one of our internal libraries:

| Before | After |
|---|---|
| **~6000 lines** of hand-written registration code | **~330 lines** |
| Per-field, per-method boilerplate | One call: `register_reflected<T>()` |
| Descriptions kept in sync by convention | Descriptions travel **with** the field via annotations |

The rest of this deck walks the **four prototypes** in this repo.

---

## Prototype A — `register_reflected.cpp`

One consteval walk populates a runtime registry:

```cpp
template <typename T> void register_reflected() {
    constexpr auto ctx = std::meta::access_context::current();
    ClassMeta &cls = Registry::instance().get_or_create(
        std::meta::identifier_of(^^T));

    template for (constexpr auto m :
        std::define_static_array(
            std::meta::nonstatic_data_members_of(^^T, ctx))) {
        cls.fields[std::string(std::meta::identifier_of(m))] = ...;
    }
}
```

No per-field registration. Query the registry by **name** at runtime — language bindings, GUI editors, REST endpoints all sit on top.

---

## The invoker trick

```cpp
template <typename T, std::meta::info Fn, std::size_t... Is>
auto make_invoker(std::index_sequence<Is...>) {
    return [](void *obj, std::span<const Any> args) -> Any {
        constexpr auto params =
            std::define_static_array(std::meta::parameters_of(Fn));
        return Any{(static_cast<T *>(obj)->[: Fn :])(
            std::any_cast<std::remove_cvref_t<
                typename[: std::meta::type_of(params[Is]) :]>>(args[Is])...)};
    };
}
```

Per pack element: splice the i-th parameter's reflected type, strip cvref, `any_cast` the i-th runtime `Any`. Stateless lambda — no captures.

---

## Two parser quirks worth knowing

- A splice in a template argument must be **parenthesized** or **prefixed with `typename`**:
  ```cpp
  typename[: ... :]   // OK
  [: ... :]>           // confuses the parser
  ```
- Params travel **by value** through `Any` (cvref stripped). `T&` non-const out-params won't round-trip — needs a different storage model. Const-ref and by-value are fine.

---

## Prototype B — `register_overloads.cpp`

Methods become an **overload set**, dispatched by signature:

```cpp
struct MethodMeta {
    std::vector<std::type_index> param_types;       // for dispatch
    std::vector<std::string>     param_type_names;  // for display
    std::function<Any(void *, std::span<const Any>)> invoke;
};

struct ClassMeta {
    std::unordered_map<std::string, std::vector<MethodMeta>> methods;
};
```

`ClassMeta::resolve(name, args)` walks the bucket and matches arg types element-by-element.

---

## Overload dispatch in action

```cpp
struct Calc {
    int         add(int a, int b) const;
    double      add(double a, double b) const;
    std::string add(const std::string &a, const std::string &b) const;
};

cls->invoke("add", &c, rosetta::args(1, 2));            // int
cls->invoke("add", &c, rosetta::args(1.5, 2.5));        // double
cls->invoke("add", &c, rosetta::args(std::string("foo"),
                                     std::string("bar"))); // string
```

- Exact-match on `type_index` (no implicit conversion)
- `bool` and `int` are distinct overloads — correct for C++

---

## A subtle compile-time leak

The inline form has the `info` value leak into a non-constant-evaluated context:

```cpp
display_string_of(type_of(params[Is]))   // BAD: leaks
```

Fix: wrap in a `consteval` helper:

```cpp
template <std::meta::info Fn, std::size_t I>
consteval std::string_view param_type_display_name() {
    constexpr auto params =
        std::define_static_array(std::meta::parameters_of(Fn));
    return std::meta::display_string_of(std::meta::type_of(params[I]));
}
```

---

## Prototype C — `register_inheritance.cpp`

Walk the base chain depth-first, flatten members into the derived `ClassMeta`:

```cpp
template <typename Derived, typename Self>
void register_with_bases() {
    template for (constexpr auto B :
        std::define_static_array(std::meta::bases_of(^^Self))) {
        using Base = [: std::meta::type_of(B) :];
        register_with_bases<Derived, Base>();
    }
    register_self_only<Derived, Self>();
}
```

Inherited invokers do `static_cast<Self *>(static_cast<Derived *>(obj))`.

---

## Static methods and signature dedupe

```cpp
if constexpr (std::meta::is_static_member(Fn)) {
    // no obj cast — splice and call directly
    return Any{[: Fn :](args...)};
}
```

**Signature dedupe**: when adding a method, if an existing entry in the bucket has the same `param_types`, **replace it**. That collapses `Shape::area` into a single `area` entry in `Circle`'s metadata — derived's direct invoker wins.

Still on the gap list: diamond/virtual bases, const-correctness, private base members.

---

## Prototype D — `register_annotations.cpp`

P3394 lets us attach values to declarations:

```cpp
struct Person {
    [[=rosetta::doc{"the person's display name"}]]
    std::string name;

    [[=rosetta::doc{"age in whole years"},
      =rosetta::range{0.0, 150.0}]]
    int age = 0;

    [[=rosetta::doc{"server-assigned identifier"},
      =rosetta::readonly{}]]
    std::string id;
};
```

Requires the extra clang flag `-fannotation-attributes`.

---

## Annotations are NTTPs

Each annotation type must be **structural** and NTTP-eligible:

```cpp
struct doc {
    const char *text;
    consteval doc(const char *t) : text(std::define_static_string(t)) {}
    bool operator==(const doc &) const = default;
};
struct range    { double min, max; bool operator==(const range&) const = default; };
struct readonly { bool operator==(const readonly&) const = default; };
```

`[[=rosetta::doc{"text"}]]` with a plain `string_view` **won't compile** — the literal's address has no linkage. The `consteval` constructor runs `define_static_string` to intern the literal into static storage with linkage. That pointer is NTTP-eligible.

---

## Compile-time extraction

```cpp
constexpr auto doc_opt   = std::meta::annotation_of_type<doc>(m);
constexpr auto range_opt = std::meta::annotation_of_type<range>(m);
constexpr bool ro        =
    std::meta::annotation_of_type<readonly>(m).has_value();
```

Three setter lambdas, chosen with `if constexpr` **outside** the lambda:

- `ro`              → setter throws
- `range` on arithmetic → setter validates against `rmin`/`rmax`, then assigns
- otherwise         → plain setter

> `if constexpr` inside a lambda doesn't see the enclosing `constexpr` local as `constexpr`. Gate the choice outside.

---

## What this unlocks for Rosetta

```cpp
person.doc("...").range(0, 150).readonly()   // old fluent API
```

becomes obsolete for in-source types — the metadata travels with the field declaration. Generators (REST, Qt editor, JSON) read `FieldMeta::doc`, `FieldMeta::range`, `FieldMeta::is_readonly` directly.

The fluent API is still useful for third-party types you can't annotate.

---

## Applications of Reflection — language bindings

The flagship use case: **one C++ description → bindings for any host language**.

| Target | What reflection drives |
|---|---|
| **Python** | `pybind11` `def(...)`, `__init__`, dunder methods |
| **JavaScript** | Node.js N-API wrappers, getters/setters |
| **WebAssembly** | Emscripten `EMSCRIPTEN_BINDINGS` blocks |
| **TypeScript** | `.d.ts` declaration files |
| **REST API** | HTTP routes mapped to methods, JSON I/O |
| **Lua / Ruby / Julia** | Sol2, Rice, CxxWrap modules |
| **Java/JNI**, **C#/.NET**, **Swift** | bridge generation |

The alternative is **N hand-maintained binding files** that drift out of sync as the C++ API evolves.

---

## Applications — Serialization · GUI · REST

**Serialize:** JSON / XML / YAML / TOML / CBOR / MsgPack / Protobuf / HDF5 — *no schema needed*.

**GUI generation:**
- Qt property editors (`ObjectInspector<T>`)
- ImGui debug panels — sliders, checkboxes, color pickers per field
- QML data binding, web forms, Blueprint-style node editors

**REST / RPC:** each method → endpoint, each parameter → JSON field.

```cpp
for (auto cls : registry.list_classes())
    for (auto method : registry.get_by_name(cls)->get_methods())
        router.add(cls + "/" + method, &dispatch);
```

---

## Applications — Validation · Docs · Persistence · Live

- **Validation & constraints** — range, regex, not-null, cross-field invariants.
- **Documentation** — Markdown / HTML / OpenAPI / Sphinx, generated from `doc("…")`.
- **Persistence & ORM** — `CREATE TABLE`, `INSERT/SELECT`, migrations from class diffs.
- **Undo/Redo** — generic `SetFieldCommand` round-tripping through the registry.
- **Configuration & DI** — config-file → object hydration, CLI flag generation.
- **Live scripting / REPL** — every method auto-callable from the console.
- **Testing** — property-based, fuzzing, snapshot tests, binding-coverage smoke tests.

---

## Composition is the leverage

One `.field("position", ...)` line can be **simultaneously**:

- A Python attribute
- A JavaScript getter/setter
- A REST endpoint
- A JSON-serializable element
- An ImGui slider
- A Qt property in the inspector
- An undo-redo target
- A documented entry in the reference
- A range-validated database column
- A telemetry metric

> **Every line of registration unlocks features across multiple subsystems at once.** That's the multiplier.

---

## When *not* to use reflection

- **Hot inner loops** — virtual dispatch and `Any` boxing have measurable cost; keep numerical kernels typed.
- **Tiny throw-away tools** — registration is overhead if no extension needs it.
- **Highly templated header-only math types** — there's nothing to expose.
- **Where compile-time reflection alone covers your needs** — skip the runtime registry.

Use it where the leverage is high: APIs that cross language boundaries, are edited live, persisted, validated, documented, versioned — **application-shaped code**, not algorithmic kernels.

---

## Still on the list

- Custom annotation types — `[[=my_validator{}]]` with a generic walk
- Annotations on **parameters** — `int years_until([[=range{0, 200}]] int target)`
- Annotations on **classes** — `[[=rosetta::category{"shapes"}]] struct Circle`
- Diamond / virtual bases (needs a visited set)
- Const-correctness on instance methods (const invoker → `const void *`)
- Best-match overload resolution with conversion-cost scoring

---

# Takeaways

- **`^^` + `[: :]`** are the whole surface area — everything else is library
- Static reflection turns **6000 lines of glue into ~330** for Rosetta
- The same `info` value can become a **type**, an **expression**, or a **member access** — that's what makes it composable
- Annotations let metadata **live next to the declaration** it describes

---

# References

- [Reflection proposal](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2996r13.html)
- [Python Bindings with Value-Based Reflection](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2911r0.pdf)
- [Using Reflection to Replace a Metalanguage for Generating JS Bindings](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3010r0.pdf)

---

# Want to help building a lib that uses C++26 reflection?
- Core: 
  ```cpp
  rosetta::reflector<T>();
  ```
With extensions (applications) to:
- Generate auto bindings for Python, Java, JavaScript, wasp, REST...
- Auto-generate Qt panels (using annotation)
- Binding for Qml
- Serialization
- Have another idea ?

---

# Questions?
