---
status: accepted
---

# Use one generated managed binding contract

CrownySharp will call one versioned, typed host-function table on every managed backend. Entities, components, and assets carry stable Crowny identities rather than runtime wrapper pointers. The binding manifest generates both sides of the transport and native registration, while each engine operation has one runtime-neutral native implementation.

CrownySharp also owns the runtime-independent scripting policy: serializable-member discovery, inspector visibility, callback discovery, required-component preparation, stable catalog generation, and transactional state capture and application. Mono and CoreCLR adapt runtime hosting and invocation; they do not define separate gameplay, metadata, or state contracts. Persisted state includes explicit top-level value kinds so it remains readable without a loaded assembly catalog. Decimal values use invariant strings to preserve all 128 bits, and entity, component, asset, and UUID references use stable Crowny identities.

Mono-specific C# is limited to acquiring the host table and resolving managed-script instances. Its native reflected serializer remains isolated as a legacy scene-data and domain-refresh adapter, not as the canonical state model. Old CrownySharp binaries are not part of the current compatibility promise, but that adapter is the intended future binary/data compatibility seam.

## Considered options

- Parallel Mono internal calls and CoreCLR delegates duplicate every feature and allow the backends to drift.
- A generic blob or reflection dispatcher has a smaller table but moves type errors to runtime and makes AOT analysis harder.
- Keeping Mono wrapper pointers as managed identity couples gameplay objects to one runtime's lifetime rules.
- Keeping reflected state and callback policy inside each native adapter makes parity a permanent two-implementation problem.

## Consequences

Adding an engine binding requires one manifest entry, one native implementation, and the public CrownySharp member. Adding a serializable kind, callback, or lifecycle rule requires one shared CrownySharp implementation and its runtime-neutral native representation. No backend-specific feature code is allowed. Source and serialized scene compatibility remain requirements; binary compatibility with previously compiled CrownySharp assemblies does not.
