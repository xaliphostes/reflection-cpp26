# Naming & visibility per backend

- alias("foo") / rename_for("python", "foo_bar") — override the exposed name; lets you keep snake_case in Python and 
camelCase in JS from a single C++ source. 
- hidden (or internal) — exclude from all bindings; useful for cache/scratch fields you can't make private for ABI reasons.
- writeonly — the dual of readonly. Passwords, set-once tokens: bind a setter but no getter. 

# Validation (beyond range)

- min / max as separate one-sided bounds. 
- length(min, max) — for strings and containers. 
- regex("…") — string pattern. 
- non_empty, non_null, required — terse predicates.
- one_of(a, b, c) — restricted set; could be auto-derived from an enum but useful for std::string fields.

# Serialization shape

- json_name("foo") — wire-format key when it must differ from the identifier (e.g. "$ref").
- skip_if_default / skip_if_null — omit from JSON when value is trivial. Big quality-of-life win for REST/WASM payloads. 
- format("iso8601"), format("base64") — string encoding hints for dates, blobs.
- default_value(42) — what to fill in when missing from input. Also feeds OpenAPI generation.

# REST/OpenAPI-specific 

- endpoint("/users/{id}"), http_method(GET) on methods.
- path_param, query_param, body on parameters.
- example("alice@example.com") — populates Swagger UI samples. 
- status_code(201) for response types.

# UI / editor hints (if Rosetta ever drives a property editor)

- widget("slider"), widget("color"), widget("file").
- step(0.1) — pairs with range.
- display_name("User-friendly label"), group("Advanced"), order(3).

# Versioning / lifecycle

- since("1.2.0") — first version that exposed this.
- deprecated("use foo()") — bindings emit a runtime warning or DeprecationWarning in Python.
- experimental — gate behind an opt-in flag in each backend.

# Units & semantic types

- unit("m/s") — physical units carried into Python (could plug into Pint) or rendered in REST docs.
- currency("USD") — for finance-flavored APIs.

# Threading / call-site 

- noexcept_binding — backend skips the try/catch wrapper.
- async — N-API exposes the method as Promise-returning; WASM dispatches to a worker.
- gil_release — pybind11 releases the GIL during the call.

# Persistence (if you ever cross into ORM territory) 

- primary_key, unique, indexed, foreign_key(^^OtherType, &OtherType::id) — the last one is the genuinely interesting case, 
because a reflection-based annotation can refer to another reflected entity, which a string-based attribute system can't.
