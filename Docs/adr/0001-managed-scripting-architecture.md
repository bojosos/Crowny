---
status: accepted
---

# Use Crowny-owned managed scripting backends

Crowny will put a deep `ManagedScripting` module above runtime mechanics. Build targets select platform-valid managed backend presets, while the editor has a separate default. Original Mono remains the transition adapter. A Crowny-owned `nethost` and `hostfxr` adapter is the desktop direction, modern .NET Mono WebAssembly interpreter and AOT builds cover the browser, and Native AOT remains an evidence-gated closed-world desktop option. A versioned generated C ABI, runtime-neutral script catalog and values, transactional reload, and stable scene identities keep engine callers and gameplay source independent of runtime objects. Coral remains reference material, and named-console support requires vendor SDK evidence.

## Considered options

- Renaming Mono reflection wrappers would preserve the current coupling and give AOT backends a poor interface.
- One runtime for every target would weaken either editor iteration or restricted-player support.
- Adopting Coral would leave Crowny responsible for the same reload, compatibility, packaging, AOT, browser, and platform work behind a third-party interface.
- A Crowny IL-to-C++ compiler would create a toolchain and platform-maintenance burden the project cannot justify.

## Consequences

Existing C# source must recompile without routine edits, and old scene identities remain readable. CoreCLR cannot replace Mono as the editor default until shared contract, workflow, unload, packaging, and performance gates pass. Browser builds use a dedicated closed-world publish path. Runtime-specific headers, objects, handles, reflection, marshalling, and exception transport stay inside adapters.
