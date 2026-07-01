---
name: HydraCore Configuration Module 3
description: Architecture decisions and traps for Module 3 Configuration system.
---

# HydraCore — Module 3 Configuration Architecture

## File Layout

- `include/HydraCore/Configuration/` — 7 headers
- `src/Configuration/` — 6 source files
- Coexists alongside Module 1 `Config/` (different directory, different namespace concern)

## ConfigValue::Type enumerator "String" shadows Hydra::String

`ConfigValue::Type` has an enumerator named `String`. Inside any TU that includes
`ConfigValue.h` and also uses `Hydra::String`, GCC 14 emits `-Wshadow`. This is a
warning only — it does NOT prevent compilation. Do not rename the enumerator (it
is part of the public API).

**Why:** The enum value `ConfigValue::Type::String` is always accessed with its
full qualified form, so there is no actual ambiguity at the code level.

## MergeOver (overlay wins) — deep merge via nlohmann::json

`Configuration::MergeOver(overlay)` is implemented by converting both roots to
`nlohmann::json`, running a recursive `DeepMerge()` (src wins on scalar conflict,
recurse on object-object conflict), then converting back to `ConfigValue`.

**How to apply:** If future modules need merge with base-wins semantics, swap the
argument order at the call site — `base.MergeOver(overlay)` means overlay wins.

## ConfigurationManager — layer stack ordering

Priority (lowest → highest):
  1. SetDefaults() — programmatic defaults
  2. LoadFile() calls — in order of invocation
  3. LoadEnvironmentOverlay() — derives filename as `<base>.<envName>.<ext>`

`RebuildMerged()` is called after every mutation (SetDefaults, LoadFile, etc.).

## Hot Reload — commit only on full acceptance

`Reload()` builds a `candidate` config from fresh disk reads. It runs schema
validation and calls `NotifyListeners()` before committing. If either step
rejects, `m_Merged` keeps the old value. This means the old config is still
served after a rejected reload.

## ConfigValue backing store

`ConfigValue` internally uses `nlohmann::json` as its backing store. This avoids
the recursive-type problem of `std::variant<..., vector<ConfigValue>, ...>` and
lets YAML/JSON round-tripping be zero-cost.  `nlohmann/json.hpp` is transitively
included by `ConfigValue.h` — acceptable since it is already a public dep.
