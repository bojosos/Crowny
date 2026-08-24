# Per-frame memory

Crowny keeps render snapshots in the render thread's two frame slots. The simulation thread calls `RenderThread::BeginFrame()`, extracts the scene directly into that slot, then calls `SubmitFrame()`. Do not construct a temporary snapshot in this path. Moving a fresh snapshot into a slot discards the slot's retained capacities and restores per-frame heap traffic.

`FrameVector<T>` keeps its constructed slots when `Reset()` rewinds the active range. This lets nested `Vector` and `String` members reuse their storage. Call `Acquire()` and overwrite every field read during that frame. `RenderSnapshot::Clear()` releases asset references while retaining container capacity, so removed scene objects do not keep assets alive.

`CommandQueue` reserves both of its frame buffers at construction. `GetMetrics()` reads their size and capacity only when diagnostics or tests request it. The task queue also reserves its common working set at startup. Neither path updates counters while processing a Release frame.

`FrameAllocationTests.cpp` supplies a counting allocator to nested payload vectors. Its 120-frame, 128-object workload performs 15,360 nested allocations with a fresh vector each frame and 128 with `FrameVector`, all during the first frame. The following 119 stable frames perform zero nested allocations. The command-queue test also confirms that both buffers retain their initial capacity across 120 swaps.

Remaining allocation targets need separate ownership work. `VulkanCmdBuffer::Reset()` clears node-based resource maps, which preserves buckets but frees and recreates nodes. Material updates still perform name-based hash lookups during draws. `Task::Create` and large `std::function` captures still allocate per submitted task. Do not replace these globally without profiler captures and lifetime-specific tests.

# String identity

Use `StringID` for runtime names that need stable ownership and frequent comparisons. Interning hashes the input into a bucket, then compares the complete text before reusing an ID. Hash collisions therefore affect lookup cost, not correctness. Interned strings live in immutable, lazily allocated chunks for the process lifetime, so engine restarts cannot make an old ID refer to different text. `c_str()` performs an atomic indexed read without locking, and its pointer remains stable. A repeated intern does not add another entry. Use `"name"_sid` for hot literal paths: it interns on first use and returns a copy of the cached four-byte ID afterward.

Use `HashedString` and the `"name"_hstr` literal for static dispatch names that do not need ownership. It is a non-owning view, so the referenced text must outlive it. The literal's FNV-1a hash excludes the terminating null byte. `HashedString` equality checks hash, length, and bytes. Code that switches on `GetHash()` alone must still confirm the text before accepting a match, because a 64-bit hash is not a unique string identity. For owning dynamic keys, declare `UnorderedMap<String, T, StringHash, StringEqual>` to look up by `StringView`, `const char*`, or `HashedString` without constructing a temporary `String`.
