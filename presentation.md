---
marp: true
title: Reflection in C++26
paginate: true
theme: default
class: invert
---

<!-- _class: invert lead -->

# Reflection in C++26

From P2996 to a 105-line Rosetta

<br>

*Frantz Maerten*, Look Up Geoscience

---

## What is reflection?

> *"The ability of software to expose its internal structure."*

- **Static** reflection: the **compiler** exposes structure at compile time
- Two new syntactic operators:
  - **Meta**: `^^` — *reflect-on* a name into a `std::meta::info` value
  - **Splice**: `[: meta :]` — *reify* a `std::meta::info` back into code
- Everything that crosses the boundary is `consteval` / `constexpr`

P2996 — one of the **largest** proposals in C++ history.

---

## Why should you care?

One C++ description → *everything*:

- **Python / JS / WASM bindings** — auto-generated
- **JSON / YAML / Protobuf serialization** — no schema
- **Qt / ImGui editors** — GUI for free
- **REST / RPC** — each method an endpoint
- **Validation, docs, persistence** — same metadata

Today: hand-written, drifting, repeated for each backend.
**With C++26 reflection: one walk, all of them.**

---

## The two operators in one picture

```cpp
constexpr std::meta::info info = ^^Circle;            // reflect-on a type

typename[: info :] c = {.name = "c", .radius = 1.0};  // splice it back

// also works on members, expressions, namespaces, templates...
```

- `^^T` lifts the *name* `T` into a constexpr value (`std::meta::info`)
- `[: info :]` drops a `std::meta::info` value back into a *type*, *expression*, or *member*

---

## Our running example

```cpp
struct Shape {
    std::string name;

    virtual double area() const { return 0.0; }
    std::string    describe() const { return "I am a " + name; }
    static int     next_id() { return ++id_; }
private:
    static inline int id_ = 0;
};

struct Circle    : Shape { double radius;        double area() const override; };
struct Rectangle : Shape { double width, height; double area() const override; };
```

We'll grow this same example through the rest of the deck.

---

## Hello, reflection — enumerate `Circle`'s fields

```cpp
constexpr auto ctx = std::meta::access_context::current();

template for (constexpr auto m :
    std::define_static_array(
        std::meta::nonstatic_data_members_of(^^Circle, ctx))) {
    std::println("  {} : {}",
        std::meta::identifier_of(m),
        std::meta::display_string_of(std::meta::type_of(m)));
}
```

Output:

```
  name   : std::string
  radius : double
```

No macros, no inheritance, no registration. Just the type.

---

## Why not earlier? — the design space

Every reflection system trades along the same axes:

| Axis | Endpoints |
|---|---|
| **When** | Compile-time only ↔ Runtime-queryable |
| **Intrusiveness** | Non-intrusive ↔ Macros / annotations required |
| **Source of metadata** | Native compiler ↔ External codegen ↔ Manual |
| **Coverage** | Aggregates only ↔ Methods + virtuals + private |
| **Target use** | Serialization ↔ Bindings ↔ GUIs ↔ RPC |

C++ historically lacked native reflection → **dozens** of libraries with different philosophies.

---

## Why not earlier? — C++ libraries pre-P2996

| Library | Type | Intrusive? | Coverage |
|---|---|---|---|
| **RTTI** (`typeid`) | language | none | type identity only |
| **Boost.PFR** | header | none | aggregates only |
| **magic_enum** | header | none | enums (via `__PRETTY_FUNCTION__`) |
| **RTTR** | header | macro | fields, methods, inheritance |
| **EnTT::meta** | header | registration | data + funcs (games) |
| **Boost.Describe** | header | macro | members + bases |
| **Qt MOC** | codegen | `Q_OBJECT` | full + signals/slots |
| **Unreal UHT** | codegen | `UCLASS` | full + GC + Blueprint |
| **SWIG** | codegen | none | multi-language bindings |

No single library wins — each picks a different point on the axes.

---

## Why not earlier? — every other language has this

| Language | Runtime | Compile-time | Idiom |
|---|:-:|:-:|---|
| **Python** | 🟢 deep | ⚪ | `inspect`, `getattr`, decorators |
| **C#** | 🟢 native | 🟢 source generators | `System.Reflection` + attributes |
| **Java** | 🟢 native | 🟡 processors | `java.lang.reflect` |
| **JavaScript** | 🟢 Proxy | ⚪ | `Reflect`, `Object.*` |
| **Go** | 🟢 (slow) | ⚪ | `reflect` + struct tags |
| **Rust** | 🔴 minimal | 🟢 proc-macros | `derive`, `serde`, `bevy_reflect` |
| **D** | 🟡 partial | 🟢 `__traits` + CTFE | `__traits(allMembers, T)` |
| **Zig** | ⚪ | 🟢 `comptime` | `@typeInfo`, `inline for` |
| **C++ < 26** | 🟡 RTTI | 🟡 TMP, libraries | RTTR / PFR / codegen |
| **C++26** | 🟡 build manually | 🟢 P2996 | `^^T`, `[: r :]`, `consteval` |

C++ was **the only major language without first-class reflection** — until now.

---

## Lessons from the neighbours

- **D** is the spiritual ancestor of P2996 — `__traits` + CTFE + `static foreach` shaped the C++26 design.
- **Zig** confirms the philosophy: `comptime` *is* the reflection.
- **Rust `bevy_reflect`** proves runtime registries can stay idiomatic.
- **Go's struct tags** map cleanly onto P3394 annotations.
- **C# attributes + source generators** — cleanest runtime/codegen hybrid.

> C++26 closes the **language-level** gap.
> Runtime registries (Rosetta, RTTR, EnTT) don't go away — they get **auto-filled**.

---

# Supported compilers

| Compilateur | Réflexion C++26 | `template for` | Mainline ? | Quand l'utiliser |
  |---|---|---|---|---|
  | **GCC 16.1+** | ✅ Quasi complet | ✅ | ✅ | Production-prototyping aujourd'hui |
  | **Clang (Bloomberg fork)** | ✅ Quasi complet | ✅ (`-freflection-latest`) | ❌ | Compiler Explorer / dev local |
  | **Clang mainline** | 🟡 Partiel | 🟡 En cours | ✅ | Pas encore prêt |
  | **MSVC** | ❌ Rien | ❌ | — | Attendre 2027+ |
  | **EDG** | 🟡 Partiel | 🟡 | — | Usage interne vendors |

---

## Rosetta — *before* C++26

For each class, hand-write the registration:

```cpp
rosetta::Class<Shape>("Shape")
    .field("name", &Shape::name).doc("display name")
    .method("describe", &Shape::describe).doc("greeting")
    .method("area",     &Shape::area)
    .static_method("next_id", &Shape::next_id);

rosetta::Class<Circle>("Circle")
    .base<Shape>()
    .field("radius", &Circle::radius)
        .doc("radius in meters")
        .range(0.0, 1e6);

// ... repeat for Rectangle, every other type, every project ...
```

Real numbers from our internal lib: **~6000 lines** of glue.

---

## Rosetta — *with* C++26

```cpp
rosetta::register_reflected<Shape>();
rosetta::register_reflected<Circle>();
rosetta::register_reflected<Rectangle>();
```

That's it. Same registry. Same downstream generators.

**~105 lines** of `register_reflected` machinery — *once*, in the library — and you never touch it again.

The next 5 slides build that machinery, **one feature at a time**.

---

## Step 1 — fields, automatically

```cpp
template <typename T> void register_reflected() {
    constexpr auto ctx = std::meta::access_context::current();
    ClassMeta &cls = Registry::instance().get_or_create(
        std::meta::identifier_of(^^T));

    template for (constexpr auto m :
        std::define_static_array(
            std::meta::nonstatic_data_members_of(^^T, ctx))) {
        cls.fields[std::string(std::meta::identifier_of(m))] = FieldMeta{
            .get = [](void* o) -> Any { return static_cast<T*>(o)->[: m :]; },
            .set = [](void* o, const Any& v) {
                static_cast<T*>(o)->[: m :] =
                    std::any_cast<[: std::meta::type_of(m) :]>(v);
            },
        };
    }
}
```

`register_reflected<Circle>()` ⇒ `name`, `radius` queryable by string at runtime.

---

## Step 2 — methods (the invoker trick)

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

Splice the function + each parameter's type, `any_cast` the runtime args. Stateless lambda — no captures.

```cpp
cls->invoke("describe", &circle, {});  // → "I am a unit-circle"
```

---

## Step 3 — overloads

Same name, different signatures → bucket by name, dispatch by `type_index`.

```cpp
struct Shape {
    void scale(double k);
    void scale(double kx, double ky);   // overload
};

cls->invoke("scale", &s, rosetta::args(2.0));        // → scale(double)
cls->invoke("scale", &s, rosetta::args(2.0, 3.0));   // → scale(double, double)
```

`ClassMeta::methods` becomes `unordered_map<string, vector<MethodMeta>>` — walk the bucket, exact-match on argument types.

---

## Step 4 — inheritance

Walk `bases_of(^^Self)` first; register inherited members **as if** they were the derived's own.

```cpp
rosetta::register_reflected<Circle>();

const auto* cls = Registry::find("Circle");
cls->invoke("describe", &c, {});       // ← inherited from Shape, works
cls->invoke("area",     &c, {});       // ← virtual override, resolves correctly
cls->invoke("next_id",  nullptr, {});  // ← static method, no instance
```

Inherited invokers do `static_cast<Self*>(static_cast<Derived*>(obj))` — the implicit derived-to-base conversion expressed via two splices.

---

## Step 5 — annotations (P3394)

Metadata travels **with** the field declaration:

```cpp
struct Circle : Shape {
    [[=rosetta::doc{"radius in meters"},
      =rosetta::range{0.0, 1e6}]]
    double radius;

    [[=rosetta::doc{"server-assigned"}, =rosetta::readonly{}]]
    std::string id;
};
```

Extract at compile time inside the walk:

```cpp
constexpr auto doc_opt   = std::meta::annotation_of_type<doc>(m);
constexpr auto range_opt = std::meta::annotation_of_type<range>(m);
constexpr bool ro        = std::meta::annotation_of_type<readonly>(m).has_value();
```

Setter shape chosen with `if constexpr` — read-only throws, ranged validates, otherwise plain assign.

---

## Applications — Language bindings

One C++ description, **N hosts**:

| Target | What reflection drives |
|---|---|
| **Python** | `pybind11` `def(...)`, `__init__`, dunder methods |
| **JavaScript** | Node.js N-API wrappers, getters/setters |
| **WebAssembly** | Emscripten `EMSCRIPTEN_BINDINGS` |
| **TypeScript** | `.d.ts` declaration files |
| **REST API** | HTTP routes mapped to methods, JSON I/O |
| **Lua / Ruby / Julia** | Sol2, Rice, CxxWrap |
| **Java/JNI**, **C#/.NET**, **Swift** | bridge generation |

The alternative is **N hand-maintained binding files** that drift as the C++ API evolves.

---

## Applications — Serialization · GUI · REST

**Serialize:** JSON / XML / YAML / TOML / CBOR / MsgPack / Protobuf / HDF5 — *no schema needed*.

**GUI generation:**

- Qt property editors (`ObjectInspector<Circle>` for free)
- ImGui debug panels — sliders, color pickers, drag-floats per field
- QML data binding, web forms, Blueprint-style node editors

**REST / RPC:** each method → endpoint, each parameter → JSON field.

```cpp
for (auto cls : registry.list_classes())
    for (auto m : cls->methods())
        router.add(cls->name + "/" + m.name, &dispatch);
```

---

## Applications — Validation · Docs · Persistence · Live

- **Validation** — range, regex, not-null, cross-field invariants (from annotations).
- **Documentation** — Markdown / HTML / OpenAPI / Sphinx from `doc("…")`.
- **Persistence / ORM** — `CREATE TABLE`, migrations from class diffs.
- **Undo/Redo** — generic `SetFieldCommand` round-tripping through the registry.
- **Configuration & DI** — config-file → object hydration, CLI flag generation.
- **Live scripting / REPL** — every method auto-callable from the console.
- **Testing** — property-based, fuzzing, snapshot tests, binding-coverage.

---

## Composition is the leverage

One `[[=doc{...}, =range{...}]] double radius;` is **simultaneously**:

- A Python attribute
- A JavaScript getter/setter
- A REST endpoint
- A JSON-serializable element
- An ImGui slider with the right bounds
- A Qt property in the inspector
- An undo-redo target
- A documented entry in the reference
- A range-validated database column
- A telemetry metric

> **Every line of registration unlocks features across multiple subsystems at once.**

---

## When *not* to use reflection

- **Hot inner loops** — virtual dispatch and `Any` boxing have measurable cost; keep numerical kernels typed.
- **Tiny throw-away tools** — registration is overhead if no extension needs it.
- **Highly templated header-only math types** — nothing to expose.
- **When compile-time reflection alone covers your needs** — skip the runtime registry.

Use it where leverage is high: APIs that cross language boundaries, are edited live, persisted, validated, documented, versioned — **application-shaped code**, not algorithmic kernels.

---

# What C++26 brings

- **`^^` + `[: :]`** — the entire surface area; the rest is library
- **One consteval walk** does what hand-written glue did in 6000 lines
- The same `info` value becomes a **type**, an **expression**, or a **member access** — that's what makes it composable
- **Annotations (P3394)** let metadata live *next to* the declaration it describes
- Runtime registries (Rosetta, RTTR, EnTT) don't disappear — they get **auto-filled**

---

# References

- [Reflection proposal](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p2996r13.html)
- [Python Bindings with Value-Based Reflection](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2911r0.pdf)
- [Using Reflection to Replace a Metalanguage for Generating JS Bindings](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p3010r0.pdf)
- [Short video presentation for bindings](https://www.youtube.com/watch?v=TOKP7k66VBw)

---

# Want to help building a lib that uses C++26 reflection?

- Core: Extend the possibilities of 
  ```cpp
  rosetta::register_reflected<T>();
  ```

- With extensions (applications) to:
  - Generate auto bindings for Python, Java, JavaScript, wasp, REST...
  - Auto-generate Qt panels (using annotation)
  - Binding for Qml
  - Serialization
  - ...

---

# Questions?

---

# Rosetta before ➜ after C++26 (lines of code)
| **Spec** | **Before** | **After** |
|---|---|---|
|core|     5,903| 106|
|common|   2,380|0|
|js|       1,808| 204|
|py|       1,553| 90|
|rest|     3,327|305|
|wasm|     1,435|102|
|**Total**|     16,500|807|


---

# Example Python binding

```cpp
template <typename T> void bind(py::module_ &m, const char *py_name) {
    py::class_<T> cls(m, py_name);
    cls.def(py::init<>());

    // Fields
    template for (constexpr auto fld : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        if constexpr (ro) {
            // readonly annotation -> Python read-only property
            cls.def_property_readonly(name, [](const T &self) -> MemberT {
                return self.[:fld:]; }, docstr);
            }
    }
    // Methods
    template for (constexpr auto fn : std::define_static_array(std::meta::members_of(^^T, ctx))) {
        if constexpr (is_exportable_member_function(fn)) {
            constexpr auto name  = std::define_static_string(std::meta::identifier_of(fn));
            constexpr auto m_doc = std::meta::annotation_of_type<doc>(fn);
            constexpr const char *mdoc = m_doc.has_value() ? m_doc->text : "";

            if constexpr (std::meta::is_static_member(fn)) {
                cls.def_static(name, &[:fn:], mdoc);
            } else {
                cls.def(name, &[:fn:], mdoc);
            }
        }
    }
}
```