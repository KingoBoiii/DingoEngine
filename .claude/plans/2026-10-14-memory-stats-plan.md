**Status**: open

# Memory stats

## Context

The engine can't say how much memory it uses. The only figure today is the adapter's total VRAM in the Engine tab (`ImGuiDebugPanels.cpp:473`). What the engine has allocated, how much of the budget is in use, and whether either keeps growing are all invisible.

The goal is **memory stats**: count what the engine allocates and report it in one public snapshot and a debug-window tab. It is **not** a memory manager: no arenas, pools or allocator tags threaded through engine code. That would touch every allocation site, and the vendor libraries allocate on their own anyway.

**What exists.** A stub from before the repo move: `include/DingoEngine/Core/Memory.h` and `src/DingoEngine/Core/Memory.cpp`, behind a commented-out `//#define ENABLE_MEMORY_TRACKING`. It replaces `new`/`delete` with plain `malloc`/`free` and counts nothing. `Memory::GetAllocationStats()` is private and returns a reference to a temporary. No file includes it. This plan replaces both files.

**Lesson from the v0.6 review** (`.claude/reviews/2026-07-29-v0.6.0-review.md:313`): the working set did not show a leak that the entry count did. So every category reports a **count** next to its bytes, and the count is the number to watch for leaks.

**Decisions (proposed, waiting for the owner):**
- D1. A snapshot API plus a debug tab, not custom allocators. *Proposed: yes.*
- D2. Heap tracking (phase C) is opt-in through a premake option `--track-memory` (define `DE_TRACK_MEMORY`), like `--profile`. A normal build replaces no `operator new`. *Alternative: always on in Debug.*
- D3. GPU bytes are the engine's own **estimate** from each resource's description (size × format), labelled "estimated" in the UI. The driver's figure (phase B) sits next to it. *Proposed: yes.*
- D4. No new example. The Memory tab shows in every example through the debug window. *Proposed: yes.*

## Public API

`include/DingoEngine/Core/Memory.h`, rewritten. No vendor types.

```cpp
namespace Dingo
{
	enum class GpuMemoryCategory : uint8_t
	{
		Texture, RenderTarget, DepthTarget,
		VertexBuffer, IndexBuffer, UniformBuffer, StorageBuffer,
		Internal,   // swap-chain depth, ImGui font and geometry
		Count
	};

	struct MemoryCounter
	{
		uint64_t Count = 0;      // live objects or allocations
		uint64_t Bytes = 0;
		uint64_t PeakBytes = 0;
	};

	struct ProcessMemoryStats
	{
		uint64_t WorkingSet = 0;       // resident bytes (Windows working set, Linux RSS)
		uint64_t PeakWorkingSet = 0;
		uint64_t PrivateBytes = 0;     // committed private (Windows), anonymous RSS (Linux)
	};

	struct VideoMemoryStats
	{
		bool Available = false;   // false: the backend or driver can't report it
		uint64_t Usage = 0;       // this process, device-local heaps
		uint64_t Budget = 0;      // what the OS/driver lets this process use before demoting
	};

	struct HeapStats
	{
		bool Enabled = false;     // false unless built with --track-memory
		MemoryCounter Live;
		uint64_t TotalAllocations = 0;
	};

	struct MemoryStats
	{
		ProcessMemoryStats Process;
		std::array<MemoryCounter, size_t(GpuMemoryCategory::Count)> Gpu;
		VideoMemoryStats Video;
		HeapStats Heap;
		uint64_t Physics2DBytes = 0;   // Box2D's b2GetByteCount
	};

	class Memory
	{
	public:
		static MemoryStats GetStats();   // any thread; the process and video queries cost a syscall, so call it once per frame at most
		static const char* GetCategoryName(GpuMemoryCategory category);
	};
}
```

The engine-internal counter is `Internal::GpuMemoryTracker` in `src/DingoEngine/Core/GpuMemoryTracker.h`: `Add(category, bytes)` and `Remove(category, bytes)` on relaxed atomics. Peak is updated with a CAS loop.

---

## Phase A: GPU counters, process memory, the Memory tab

1. **Tracker.** `GpuMemoryTracker.h/.cpp` as above.
2. **Textures** (`NvrhiTexture.cpp`). `Initialize()` (`:92`) works out the bytes from the `nvrhi::TextureDesc` with `nvrhi::getFormatInfo(format).bytesPerBlock` × width × height. Every texture has one mip and one layer today (`:101-103`). If that changes, sum the mips. Category: `D32` → DepthTarget, `IsRenderTarget` → RenderTarget, otherwise Texture. Store `m_TrackedBytes` and `m_TrackedCategory`. Release them in `Destroy()` (`:121`), in `Reinitialize()` before the handle is nulled (`:129`), and in the destructor. Releasing must be idempotent: `Destroy()` followed by the destructor counts once.
3. **Buffers** (`NvrhiGraphicsBuffer.cpp`). Track in `Initialize()` (`:73`) and release in `Destroy()` (`:76`) and the destructor. Category comes from `m_Params.Type`. Bytes are the padded `ByteSize`. Volatile buffers count `ByteSize` once: NVRHI keeps their versions in its upload manager, which this can't see (noted under Left out).
4. **Internal resources.** Swap-chain depth (`NvrhiSwapChainFramebuffer.cpp:44`, `Vulkan/VulkanFramebuffer.cpp:36`), the ImGui font texture (`ImGuiRenderer.cpp:210`) and the ImGui buffers (`ImGuiRenderer.cpp:382`) count as `Internal`. Staging textures and buffers (`NvrhiTexture.cpp:188`, `:274`, `NvrhiGraphicsBuffer.cpp:190`) are short-lived and not counted.
5. **Process memory.** `Memory.cpp`. Windows: `GetProcessMemoryInfo` with `PROCESS_MEMORY_COUNTERS_EX` (`WorkingSetSize`, `PeakWorkingSetSize`, `PrivateUsage`), linking `psapi`. Linux: `/proc/self/status` (`VmRSS`, `VmHWM`, `RssAnon`). `<Windows.h>` stays in the `.cpp`.
6. **Box2D.** `Physics2DBytes = b2GetByteCount()`. It's process-global and needs no hook. Box2D stays out of the header.
7. **Debug tab.** Add `DebugTab::Memory` to `DebugPanels.h:150`, a public `MemorySection()` and `MemoryStatsWindow(bool* open)` like the profiler pair, the tab in `ImGuiDebugPanels.cpp` after Profiler (`:1191`), and **F9** in `Application.cpp` beside F8 (`:392`), plus the start-up log line (`:110`). Layout: a process block; a GPU table (category | count | MB | peak MB) with a total row labelled "estimated"; Box2D bytes. Phases B and C fill in their rows when they land. Until then the rows show "n/a".
8. **Check.** In `TextureTest` (`test/src/Tests/Renderer/TextureTest.cpp`): create a 256×256 RGBA8 texture and expect Texture bytes to rise by 262144 and the count by 1. Destroy it and expect both back where they started. Do the same for a uniform buffer in `UniformBufferTest`. Use the `TestChecks.h` helpers.
9. **Docs.** A new `docs/memory.md` (what each number means, which are estimates, F9). Add it to `docs/README.md` and link it from `docs/profiling.md`. Add the F9 key wherever F3–F8 are documented.

**Verify.** Linux Debug, every project (public header changed). `--test=texture`, then `--test=uniform`. Needs a GPU: open F9 on Vulkan, DX11 and DX12 and check that the numbers look sane and that resizing the window doesn't grow RenderTarget.

## Phase B: driver-reported video memory

1. Add a virtual `GraphicsContext::QueryVideoMemory()` that returns `VideoMemoryStats`, with a default `{ Available = false }`. Headless keeps the default.
2. **D3D11 and D3D12.** `IDXGIAdapter3::QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, ...)` → `CurrentUsage`, `Budget`. QueryInterface the adapter the context already holds. If `IDXGIAdapter3` isn't available, return `Available = false`.
3. **Vulkan.** Enable `VK_EXT_memory_budget` when the device supports it (it's optional, so not having it is fine) and chain `VkPhysicalDeviceMemoryBudgetPropertiesEXT` into `vkGetPhysicalDeviceMemoryProperties2`. Sum `heapUsage` and `heapBudget` over the heaps flagged `DEVICE_LOCAL`. Check whether llvmpipe exposes the extension. If it doesn't, the Linux run shows "n/a" and the check only asserts `Available == false || Budget > 0`.
4. Show it in the Memory tab: usage / budget with a bar. Turn the bar amber above 80% and red above 95%.
5. Update the docs page. Add a check to `TextureTest`: if `Available`, then `Usage > 0` and `Usage <= Budget`.

**Verify.** Linux Debug build and `--test=texture`. Needs a GPU: all three backends must report `Available`, and the usage should roughly match Task Manager's dedicated GPU memory for the process.

## Phase C: heap tracking (opt-in, `--track-memory`)

1. **premake.** Add the option `--track-memory`, defining `DE_TRACK_MEMORY` workspace-wide (games get the counters too). Fail generation with a clear message if it's combined with `Debug-ASan`: ASan replaces `operator new` itself, and a second replacement hides its new/delete mismatch reports.
2. **Replacements** in `Memory.cpp`, all of them under `#ifdef DE_TRACK_MEMORY`: `new`, `new[]`, the `nothrow` forms, the `std::align_val_t` forms, and `delete`/`delete[]` with their sized, aligned and nothrow forms. Size on free comes from the allocator: `_msize`/`_aligned_msize` on Windows, `malloc_usable_size` on Linux. That means no header in front of each block, and counts are usable bytes, not requested bytes; the docs say so. Aligned allocations use `_aligned_malloc` on Windows and `aligned_alloc` on Linux. Counters are relaxed atomics, and nothing in the path allocates or logs.
3. **Linking risk, check it first.** The engine is a static lib. MSVC and GNU ld both take a replacement `operator new` from an earlier lib before the CRT or libstdc++, but only if that object file is pulled in. Put `Memory::GetStats()` in the same `.cpp` (`Application` calls it every frame for the tab), so the object is always linked. Prove it with a check: allocate 1 MB through `new`, then expect `Heap.Live.Bytes` to rise by at least 1 MB.
4. **Tracy.** Under `--profile` as well, call `TracySecureAlloc`/`TracySecureFree`. Those are safe before Tracy's profiler object exists, which matters during static init. Only `Memory.cpp` includes Tracy, the same as `Profiler.cpp`. Without `--track-memory`, Tracy gets no memory events.
5. **Out of reach** (documented, not fixed): assimp (its own DLL and CRT), miniaudio (`malloc`), Jolt (its own `Allocate` hooks, see Left out), and NVRHI and driver allocations.
6. **Check** in `BackgroundTest` or a small new `Core/MemoryTest`: the 1 MB check from step 3, and that freeing it brings `Live.Bytes` back within 4 KB. Compile-guard it on `DE_TRACK_MEMORY`.
7. **Docs.** A "Heap tracking" section in `docs/memory.md` and a line in `docs/profiling.md` on Tracy's memory view. Update `.claude/reference/build-details.md` with the new option.

**Verify.** Linux: generate with `--track-memory`, build Debug for every project, then run the memory check and `--test=texture`. Then generate again without the option and build once more, to prove the default build is unchanged. Needs Windows: an MSVC build with `--track-memory` to confirm the replacement links (the main risk), and a `--profile --track-memory` run to see the Memory tab in Tracy.

---

## Left out

- **Allocator tags per subsystem** (arenas, pools, `DE_NEW(tag)`). Not needed yet. Revisit if a game hits a memory ceiling.
- **Jolt through the tracker.** Jolt's `Allocate`/`Free` hooks could feed a `Physics3D` counter, but its global setup is ref-counted (`docs/physics-3d.md:256`) and has to be installed before `RegisterDefaultAllocator`. That's a separate change if wanted.
- **NVRHI's upload and volatile chunks**, and the driver's own overhead. Phase B's driver figure covers those in total.
- **Exact GPU allocation sizes** (alignment, VMA blocks). Phase A is an estimate on purpose (D3).
- **Leak reports at shutdown** (listing what's still alive). Possible later from the Phase A counts.

"As built" notes are added here per phase as the work lands.
