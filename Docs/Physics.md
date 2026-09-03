# Physics backends

Crowny exposes 2D and 3D physics through engine-owned facades. Game and editor code should include `Physics2D.h` or `Physics3D.h`; do not retain Box2D, Box3D, Jolt, or Bullet native pointers outside their adapter.

## Bootstrap

The physics setup scripts fetch exact upstream commits, build static libraries, and install them under the ignored `.deps/physics/install/<Configuration>/` directory:

```powershell
Scripts\crowny.bat deps physics --configuration Release --simd avx2
```

```sh
./Scripts/crowny deps physics --configuration Release --simd avx2
```

`Scripts/crowny.bat setup` runs the Windows bootstrap automatically and uses AVX2 by default. A configuration stamp includes the SIMD level and skips unchanged builds. Pass `--force` only when the installed artifacts must be rebuilt.

| Backend | Version | Commit |
| --- | --- | --- |
| Box3D | 0.1.0 | `8441b4a06d6d09dcfb0b0f704df4d847d1437b92` |
| Jolt | 5.6.0 | `e77f175595e64cb44218cc9d9d56fc365ad0e36a` |
| Bullet | 3.25 | `2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5` |

The Box3D build receives a local CMake-only `/MD` adjustment so its runtime library matches Crowny. Upstream source is otherwise unchanged. Jolt's instruction-set definitions must match Crowny's, so the setup script and Premake generation must use the same SIMD level.

Linux CI runs `crowny deps physics --configuration Release --simd avx2` before Premake generation. macOS developers use the same script; the generated Web platform excludes all native 3D adapters and links none of these SDKs.

## Selection and capabilities

`Physics3DSettings::Backend` selects `Box3D`, `Jolt`, or `Bullet`; Box3D is the default. Premake compiles all available adapters. Use `--without-box3d`, `--without-jolt`, or `--without-bullet` for reduced builds. Query `Physics3D::Supports()` before using optional behavior.

The common API covers rigid bodies, compound primitive/convex/mesh/height-field shapes, filters, materials, per-shape triggers, CCD, sleep and damping, forces and impulses, constraints with limits/motors/springs, ray casts, shape casts, overlaps, and queued enter/stay/exit contacts. Native callbacks never invoke game code during a solver step.

## Physics materials

Colliders reference reusable `PhysicsMaterial2D` or `PhysicsMaterial3D` assets. Create them from the Asset Browser as `.pmat` or `.pmat3d`; density, friction, restitution, restitution threshold, and both combine modes are shared by the native, editor, prefab, scene, and C# APIs. Editing a loaded material refreshes its live collider shapes. A missing asset keeps its UUID so a later import can resolve it, while simulation uses the project default material until then.

Combine-mode priority is `GeometricMean`, `Average`, `Minimum`, `Multiply`, then `Maximum`. The higher-priority mode from the two contacting materials wins on every backend. The defaults preserve the prior behavior: geometric-mean friction and maximum restitution.

Scene format 6 stores persistent and unresolved materials as UUIDs. Transient materials remain inline so migrated values are not lost; YAML and binary readers convert version 1-5 inline 3D coefficients to this representation. Physics material binary format 2 adds the asset header, threshold, and combine modes. Headerless 2D materials remain readable with legacy defaults, while new 3D assets require format 2.

## Portability notes

- Bullet applies material and sensor response at body level, so mixed trigger/solid children are approximated. Its current ray and sweep adapters return the closest hit.
- Jolt does not currently map connected-body collision overrides or automatic break-force and break-torque handling. Bullet reduces the two break thresholds to one impulse threshold.
- Triangle meshes and height fields are restricted to static bodies where required by the selected SDK. Box2D exposes 16 collision category bits even though Crowny retains 32 layer slots for compatibility.
- Character controllers, vehicles, and soft bodies remain backend extensions until a useful common contract exists; unsupported capability bits are not advertised.
- Scenes support rigid bodies plus box, sphere, and capsule colliders. Their settings are available in the editor, persist through YAML and binary serialization, and are exposed to C# through `Rigidbody3D`, `Collider3D`, and `Physics3D`. Scripts can implement `OnCollisionEnter/Stay/Exit(Collision3D)` and `OnTriggerEnter/Stay/Exit(Entity)`.
- Convex, mesh, height-field, and compound shapes, constraints, controllers, vehicles, and soft bodies remain native API features. Add scene components only after defining serialization and backend-neutral editor behavior for them.
- Box3D is pre-1.0 and its API can change. The exact pin isolates Crowny from that churn.

No fork is currently required. Create one only if Crowny needs unreleased fixes, long-lived API compatibility patches, or changes upstream will not accept; keep build-system compatibility patches in the bootstrap scripts.

The focused Catch2 checks live in `Crowny-Tests/Source/PhysicsTests.cpp`. They run the Box2D contract and every compiled 3D backend through body creation, queries, constraints, stepping, and deferred contacts.
