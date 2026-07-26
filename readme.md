# RealRTCW Personal Fork: Engine Modernization & Performance Optimization

This repository is a modernized, high-performance fork of the **Return to Castle Wolfenstein** single-player engine. It optimizes the virtual filesystem, streamlines collision and map loading, upgrades the audio pipeline to OpenAL EFX/EAX, introduces SIMD math accelerations, overhauls the legacy renderer for cinematic lighting, and delivers seamless integration for modern OS environments (including immutable Linux distros).

---

## 🛠️ Architectural Overview

| Module | Focus Area | Key Technologies & Core Files |
| --- | --- | --- |
| **Virtual Filesystem** | VFS Performance & I/O Reduction | `files.c`, AVX2, Directory Caching, Tokenized Filtering |
| **Collision & Maps** | Physics Hulls & Spatial Queries | `cm_load.c`, `cm_trace.c`, Branchless Box Optimization |
| **Multithreading** | Parallelization & Job Dispatching | `gp_jobsystem`, Work Stealing, Batched Submissions, Skeletal Skinning |
| **Audio Pipeline** | Spatial Audio & Reverb Simulation | OpenAL EFX, EAX, 128-bit SSE Audio Mixer |
| **Renderer & Shading** | Cinematic Visuals & Tech Modernization | Full-Body MDS Character Shadows, 1st-Person Fallback, Z-Fail Stencil, Pseudo-Soft Shadows, PBR Attenuation |
| **QVM Compiler** | Execution Performance & Security | `vm_x86.c`, `vm.c`, x86_64 JIT, W^X Memory Hardening |
| **Memory Management** | Low-Latency Allocation | `mimalloc`, Eager Page Commits, 2MB Huge Pages |

---

## ⚡ Performance & Architectural Improvements

### 📦 Virtual Filesystem (VFS) & Zip Traversal (`files.c`)

* **PK3 Directory Caching (`pk3cache.dat`)**: Caches ZIP/PK3 directory scans in a binary file validated by size and timestamp. This bypasses minizip traversal on startup, dramatically slashing game boot times and eliminating cold-start delays.
* **Intelligent Stream Seeking**: Overhauls zlib stream seeking to jump forward using absolute offsets in the decompression window, improving background asset streaming responsiveness while reducing CPU load.
* **Expanded File Hash Table**: Increased `MAX_FILEHASH_SIZE` from 1,024 to 32,768, scaling up lookups to near $O(1)$ efficiency under heavy mod loads to eradicate micro-stutters.
* **AVX2 File Comparison**: Utilizes 256-bit AVX2 registers in `FS_FileCompare` to compare 32 bytes per cycle, accelerating binary verification passes prior to level entry.
* **Linear Path Normalization**: Replaces inefficient $O(N^2)$ array shifts in path sanitization with a streamlined $O(N)$ two-pointer pass, lowering CPU overhead and eliminating legacy buffer overrun vulnerabilities.
* **Multi-Segment Path Filtering**: Replaces legacy wildcard string matching with tokenized multi-segment filtering to speed up asset matching across thousands of files and strictly enforce absolute segment priorities.
* **Increased File Tracking Limits**: Raised the `MAX_FOUND_FILES` limit to allow the tracker to index thousands of custom assets simultaneously, preventing crashes on massive total-conversion mods.

### 📐 Collision Model & Map Loading (`cm_load.c` / `cm_trace.c`)

* **RAM-Based Map Caching**: Retains decompressed map collision hulls and geometry in system memory, making level loads and quickloads near-instantaneous by bypassing disk extraction.
* **Branchless Box Collision Math**: Redesigns spatial ray-intersection checks inside bounding box clipping tasks using branchless vector min/max selections, preventing CPU instruction pipeline stalls from branch mispredictions.
* **Model Bounds Loading Fix**: Corrected the modelbounds parsing logic on world asset clip layers, ensuring projectile hitboxes align perfectly with physics shapes at all angles.

### 🧵 Multi-Threaded Architecture & Parallelization (`gp_jobsystem`)

* **Main-Thread Work Stealing (`Sys_WaitJobs`)**: Eliminates main-thread idle stalls during wait barriers by having the main engine thread actively steal and execute remaining queue jobs instead of sleeping on OS condition variables.
* **Micro-Spin Waiting & Thread Prioritization**: Configures worker threads with `SDL_THREAD_PRIORITY_HIGH` and lightweight spin-wait loops prior to committing to OS kernel sleeps (`SDL_WaitCondition`), cutting context-switch wakeup overhead for micro-jobs down to near zero.
* **Power-of-Two Ring Buffer & Bitwise Masking**: Expanded job queue capacity to 2,048 entries using bitwise AND masking (`JOB_MASK`), replacing hardware integer division modulo operations (`% MAX_JOBS`) to maximize enqueue/dequeue throughput.
* **Batched Job Submissions (`Sys_QueueJobBatch`)**: Enables bulk job dispatches under a single lock acquisition, reducing thread mutex lock contention from $O(N)$ down to $O(1)$ while signaling background workers in batches.
* **Parallel PVS Leaf Visibility Testing (`tr_world.c`)**: Multi-threads potential visibility set (PVS) leaf node array checks across job system workers during scene construction (`R_MarkLeaves`).
* **Parallel Brush Model Dynamic Light Culling (`tr_light.c`)**: Partitions dynamic light plane equations across non-overlapping surface chunks for complex brush models (`R_DlightBmodel`).
* **VM Engine-Game Syscall Extensions (`g_public.h` / `cg_syscalls.asm` / `g_syscalls.asm`)**: Exposes job system queuing (`trap_Sys_QueueJob`, `trap_Sys_WaitJobs`) across native and QVM module boundaries with aligned syscall assembly equates.
* **Parallel CPU Skeletal Vertex Skinning (`tr_animation.c`)**: Partitions large skeletal model surfaces (`numVerts > 256`) into 256-vertex chunks, distributing skinning across background workers to maximize high refresh-rate monitors.
* **Thread-Isolated Memory Alignment**: Configures concurrent skinning chunks to write into isolated target blocks to eliminate false cache-line sharing, optimizing L1/L2 cache efficiency.
* **Asynchronous Background Asset Loading (`snd_openal.c` / `snd_codec.c`)**: Offloads file loading and WAV/OGG/Opus decompression tasks to worker threads using a thread-safe custom allocator (`S_CodecAllocateTemp`).

### 🔊 Dynamic Spatial Audio & Environmental Reverb (OpenAL EFX / EAX)

* **Physical Doppler Shift & Velocity-Based Acoustic Compression**: Calculates real-time 3D velocity vectors for both sound entities (`sentity_t`) and the listener (`S_AL_Respatialize` / `S_AL_UpdateEntityPosition`). Feeds velocity vectors directly into OpenAL's native `AL_VELOCITY` parameters with EMA smoothing ($0.7 \cdot v_{\text{old}} + 0.3 \cdot v_{\text{new}}$) and teleport guards ($15,000\text{ units/s}$), producing authentic high-speed pitch and frequency warping for passing projectiles, rockets, artillery, and vehicles.
* **Dynamic Sound Propagation & Material Surface Occlusion**: Queries collision surface flags (`tr.surfaceFlags`) from BSP material definitions (glass, wood, metal, carpet, rubble, stone) alongside acoustic wave diffraction (`S_AL_CalculateDiffractionHF`). Sounds behind thin glass or wooden doors attenuate high frequencies lightly ($\text{gainHF} \approx 0.70 - 0.85$), while metal blast doors or thick reinforced concrete muffle low/mid frequencies aggressively ($\text{gainHF} \approx 0.20 - 0.25$).
* **Full Native Opus Codec Integration (`snd_codec_opus.c`)**: Adds a native `.opus` file reader alongside Ogg Vorbis and WAV parsers. Streams compressed high-efficiency 48kHz audio using `opusfile-0.12` and `opus-1.5.2` with `S_CodecAllocateTemp` thread safety, reducing disk memory footprint and CPU decoding overhead for sound clips and background tracks.
* **High-Frequency Audio Resampling & Sample Rate Normalization**: Integrates a 4-point cubic Hermite spline interpolator (`S_CodecResample`) into the asset loading pipeline (`snd_codec.c`). Automatically upscales legacy 11.025kHz and 22.05kHz audio clips to match 48kHz output targets (`s_resampleRate`), eliminating metallic aliasing and temporal distortion on modern DACs without runtime playback CPU overhead.
* **Dynamic Ray-Traced Reverb**: Casts geometric rays into the map to calculate room volume, outdoor exposure, and structural boundaries to dynamically adjust OpenAL EFX reverb parameters on the fly.
* **OpenAL EFX Occlusion**: Applies low-pass filters to sound sources when line-of-sight is blocked, realistically simulating acoustic muffling behind walls, doors, or solid geometry.
* **HRTF Support**: Exposes full Head-Related Transfer Function configurations directly in the UI, delivering precise binaural 3D spatial audio on headphones.
* **X-Macro API Bindings**: Consolidates dynamically loaded function pointer declarations, definitions, loading, and unloading routines for OpenAL and cURL using unified compile-time X-Macros.
* **SIMD Audio Mixer**: Employs 128-bit SSE lanes and saturation packing (`_mm_packs_epi32`) to mix up to 4 stereo streams in parallel without branching, completely eliminating audio cracking.

### 💡 Cinematic Rendering & Lighting Pipeline (`tr_shadows.c` / `tr_light.c` / `tr_bloom.c` / `tr_flares.c` / `tr_animation.c`)

* **Full-Body Character & NPC Shadow Pass (`tr_animation.c` / `tr_mesh.c` / `tr_model_iqm.c` / `tr_cmesh.c`)**: Added missing shadow surface generation passes to `.mds` character model rendering (`R_AddAnimSurfaces`) and expanded shader sort evaluation beyond `SS_OPAQUE` to `shader->sort <= SS_BANNER`. This allows alpha-tested uniforms, clothing, and skinned body parts on enemy guards and NPCs to cast complete, full-body shadows instead of just head/weapon props.
* **1st-Person Player 3D Stencil Shadow System (`cg_players.c` / `tr_shadows.c`)**: Enabled true 3D animated stencil shadows for the local player in first-person mode (`cg_shadows 2`). Utilizes camera-relative near-plane eye clamping (`maxEyeZ`) during volume extrusion (`RB_ShadowTessEnd`) to project pixel-perfect 3D body, leg, arm, and weapon shadows onto floors and walls in real-time without camera near-plane clipping glitches or crashes.
* **Stencil-Masked Solid Projection Shadows (`tr_shade.c` / `tr_shader.c`)**: Enhanced `projectionShadowShader` with OpenGL stencil buffer single-pass masking (`qglStencilFunc(GL_EQUAL, 0)`) and increased constant alpha (`constantColor[3] = 180`). Discards overlapping internal model geometry (torso, limbs, weapon, pouches) during shadow rendering, turning transparent 3D wireframe mesh overlays into clean, solid, dark 70% opacity shadow silhouettes.
* **Auto-Promoted OpenGL Stencil Allocation (`sdl_glimp.c`)**: Auto-requests at least 8-bit stencil framebuffers during OpenGL context creation whenever `r_shadows 2` is enabled, preventing silent stencil allocation failures.
* **Carmack's Reverse (Z-Fail Stencil Shadows)**: Switched the volumetric shadow rendering pipeline from Z-Pass to Z-Fail culling to prevent the entire screen from glitching when the camera clips inside a shadow volume.
* **Volumetric Contact Hardening (Pseudo-Soft Shadows)**: Injects a high-frequency, vertex-indexed dither pattern into the light extrusion vectors, creating realistic light penumbras that blur naturally over distance.
* **Cinematic Ambient Shadow Blending**: Replaces the hardcoded multiplicative gray blend with an alpha-blended, slate-blue tinted shadow pass, allowing shadows to absorb environmental ambient colors.
* **Extended Shadow Extrusion**: Increased the hardcoded shadow volume extrusion distance from 512 units to 3,000 units so shadows no longer disappear mid-air from high balconies.
* **Physically Based Dynamic Light Attenuation**: Replaces legacy inverse-square distance division with a windowed smooth-step falloff curve to generate smooth lighting gradients across models.
* **Intensity-Weighted Light Grid Direction**: Upgraded ambient map lighting trilerp calculations to weight directional vectors by their actual luminous intensity rather than spatial proximity, fixing severe shading flickers on moving NPCs.
* **Saturation-Preserving Tone Mapping (Color Clamp)**: Modifies the final entity lighting clamp to scale RGB vectors proportionally instead of clipping channels independently, preventing intense explosions from bleaching out.
* **Simulated Global Illumination (Bounce Light)**: Injects a fractional percentage of dynamic directed light back into the entity's ambient channel, simulating floor and wall light bounces without raytracing overhead.
* **Fast Surface-Aware Plane Culling (`R_DlightBmodel`)**: Evaluates individual polygon faces against active dynamic lights using accelerated dot-product plane filtering, cutting wasted GPU vertex cycles.
* **Modernized Aspect-Correct Bloom & Spectral Saturation**: Fixes legacy aspect-ratio viewport bugs and implements screen-space energy conservation blending alongside integrated $O(1)$ single-pass photographic highlight extraction.
* **Precomputed Flare Falloff & Fast Bounds Culling (`tr_flares.c`)**: Precomputes intensity falloff square roots globally upon CVAR updates to eliminate runtime per-flare `sqrtf()` calls, early-culls back-facing flare normals prior to clip transformations, unrolls 3D fog volume bounds queries, and pre-calculates quad vertex attributes.

### 🖼️ Modernized Backend Renderer & Geometry Streaming (`tr_surface.c`)

* **Immediate Mode Eradication (`RB_SurfaceBeam` / `RB_SurfaceAxis`)**: Stripped all legacy fixed-function immediate mode paths (`qglBegin`/`qglEnd`) from procedural entities, re-routing them to stream directly into the unified engine vertex and index geometry cache buffers.
* **Dynamic Light Pass Consolidation (`RB_SurfaceTriangles` / `RB_SurfaceFace`)**: Merged the secondary standalone bitwise masking loop (`vertexDlightBits`) directly into the main coordinate vertex allocation stride to optimize cache efficiency.
* **Branch-Hoisted Keyframe Interpolation (`LerpCMeshVertexes`)**: Hoisted structural model compression evaluation flags completely outside high-frequency vertex processing loops, eradicating instruction pipeline stalls.
* **Model Surface Frame Bounds Validation (`tr_surface.c` / `tr_animation.c`)**: Added modulo bounds checking (`(frame % numFrames + numFrames) % numFrames`) and `NULL` entity safeguards in `LerpMeshVertexes_scalar`, `LerpCMeshVertexes`, and `RB_MDRSurfaceAnim`, preventing out-of-bounds pointer calculations and SIGSEGV crashes during Alt-Tab window state transitions.
* **Auto-Vectorized Array Normalization (`VectorArrayNormalize`)**: Re-engineered vector operations using loop unrolling by 4 and optimized coordinate bitwise shifts (`<< 2`) to fully saturate L1 data cache line bandwidth.
* **Dual-Channel Coordinate Streaming (`RB_SurfaceMesh` / `RB_SurfaceCMesh`)**: Refactored multi-stage UV texture loops to simultaneously push diffuse and lightmap properties across both coordinate streaming channels.

### 📐 Core Math & Renderer Geometry (`q_math.c` / `q_shared.h` / `tr_mesh.c`)

* **SIMD Vector Normalization**: Replaces scalar vector normalizations with SSE intrinsics and reciprocal square root approximations (`_mm_rsqrt_ps`), speeding up transformation math while safeguarding routines against division-by-zero errors.
* **Branchless Coordinate Clamping**: Replaces clamping branches with `fminf` and `fmaxf`, compiling down to single-instruction `minss`/`maxss` assembly primitives.
* **SIMD Fog Volume Checks**: Checks mesh bounding spheres against fog volume limits in parallel using SSE masking (`_mm_movemask_ps`), eliminating calculation lag or visual pops when crossing dynamic fog boundaries.
* **Inlined LOD Calculations**: Replaces indirect function calls in `R_ComputeLOD` with direct inline calculations, eliminating call stack overhead when processing complex mesh details.
* **User Command Optimizations**: Streamlines view-angle calculations and command processing (`usercmd_t`), minimizing input latency for high-polling-rate mice and gamepads.

### 🖱️ Native SDL3 Raw Input & Sub-Tick Mouse Accumulation (`sdl_input.c` / `cl_input.c`)

* **Sub-Tick Floating-Point Accumulation**: Decouples mouse motion from the engine's `com_maxfps` server tick rates by accumulating raw `float` relative movements directly within the SDL3 event loop. This prevents sub-pixel truncation and eliminates camera judder for high-polling-rate mice at lower frame rate caps.
* **Context-Aware Input Filtering**: Intelligently switches between high-precision float accumulation in-game and standard integer event queuing when UI menus, console, or the weapon wheel are active, ensuring seamless cursor navigation.
* **Cvar Toggle Option**: Includes the `in_subTickMouse` (default `1`) console variable to dynamically toggle between sub-tick float accumulation and the legacy double-buffered integer path for comparison or debugging.

### 🤖 Bot Intelligence Optimization (`ai_main.c` / `g_bot.c` / `ai_cast_sight.c`)

* **Fast Bot Pathing & Vectorized Transforms**: Uses hardware-native rounding (`roundf`) for fast angle wrapping in navigation meshes alongside SSE unaligned loads/stores (`_mm_loadu_ps` / `_mm_storeu_ps`) to copy 3D transform structures in single operations.
* **Vector Dot Product FOV Check**: Replaces nested conditional loops and expensive `AngleMod` calculations in visibility checks with a 3D vector dot product, bypassing multiple trigonometric conversions per sight check.
* **Early-Exit PVS Check**: Runs a fast Potential Visibility Set (`trap_InPVS`) check at the start of visibility routines to instantly discard calculations for characters separated by solid world geometry.
* **Proximity-Based Dynamic Time-Slicing**: Dynamically adjusts sight check delays based on the 3D distance between characters, lowering latency to 100ms for close-range actors while safely scaling up delays for far-away actors to 800ms.
* **Real-Time Lightgrid Perception**: Queries BSP lightgrid and dynamic light nodes (`trap_GetLightAtPoint`) during visibility evaluations, dynamically scaling guard detection distances based on player illumination levels when crouching or hiding in shadows without requiring map recompilation.
* **Cached Animation Speed Lookup**: Caches movement speeds retrieved from the character animation script based on `aiState`, eradicating redundant complex animation script parsing and indexing operations per character every frame.

### ⚡ Modernized x86_64 JIT QVM Compiler (`vm_x86.c` / `vm.c`)

* **Dynamic Register Caching**: Caches QVM stack variables directly in x86-64 registers (`RAX`, `RCX`, `RDX`) via `vm_optimize.h`, cutting stack memory traffic by up to 80% and accelerating virtual machine execution toward native speeds.
* **W^X Memory Hardening**: Transitioned JIT buffer allocation to a strict Write-or-Execute model using `mprotect`, switching memory pages to `PROT_READ | PROT_EXEC` after code emission to secure the engine against dynamic code injection vulnerabilities.
* **Instruction Merging**: Merges related VM instructions into single machine code blocks and maps floating-point ceil/floor operations directly to hardware SSE4.1 `roundss` instructions, reducing the instruction cache footprint.
* **QVM Verification & Fixups**: Introduces pre-compilation instruction verification (`VM_CheckInstructions`), LCC compiler instruction fixups (`VM_Fixup`), and CRC32 lookup validation to prevent crash loops when executing legacy or malformed custom mods.
* **JIT Enabled by Default**: Configured the engine to default to JIT execution (`vm_* 1`) for game, cgame, and UI modules, unlocking maximum mod execution performance out of the box without manual user overrides.

---

## 🎮 UI, Gameplay Systems, & Dev Infrastructure

### ⚡ Low-Latency Memory Management (`mimalloc`)

> **Architectural Shift:** The legacy custom multi-segment zone sub-allocator (`Z_Malloc` and `Z_Free` in `common.c`) has been completely replaced with a direct mapping to Microsoft `mimalloc`. This eliminates double-allocation layers and mitigates memory-management execution overhead.

* **Eager Page Commits**: Configured memory segments to commit eagerly (`mi_option_eager_commit`) to physical RAM upon request, eliminating runtime micro-stutters and frame hitches when dynamic assets are loaded during high-action scenes.
* **Large OS Pages**: Enforces 2MB huge page allocation targets (`mi_option_large_os_pages`) on compatible Linux and Windows kernels to dramatically minimize CPU Translation Lookaside Buffer (TLB) cache misses.
* **Lock-Free Thread Safety**: Removed legacy spinlocks from engine memory pools. Multi-threaded worker tasks and job systems compile down to lock-free allocation paths using mimalloc's internal thread-local heap cache structures.
* **Diagnostic Telemetry**: Introduced the `/mi_stats` console command, exposing low-level allocator pool metrics, active blocks, and fragmentation reports live inside the developer console.

### 🎛️ Feature Additions & Enhancements

* **"Sofia" Advanced Settings Menu**: Features a custom settings tab to control HRTF, audio occlusion, reverb, and Discord options. The UI layout has been overhauled to eliminate text clipping or overlapping fields.
* **Viewmodel FOV Decoupling (`cg_gunFov`) & UI Options Slider**: Decouples 1st-person weapon viewmodel presentation from the main player camera FOV (`cg_fov`). Setting `cg_gunFov 90` locks the viewmodel perspective at a natural 90° FOV presentation, eliminating gun stretching or dropping at high player FOVs (100°–120°) while scaling smoothly during scope/binoculars zoom. Adds an interactive **GUN FOV** slider (`cg_gunFov`) to the Options menu (**Options -> View/Crosshair**).
* **Discord Rich Presence**: Integrates game state (active mod, weapons with dynamic emojis, score/kills) into Discord via background thread sockets without introducing gameplay micro-stutters.
* **Bot Intelligence & Real-Time Lightgrid Perception**: Uses hardware-native rounding (`roundf`) for fast angle wrapping, 3D vector dot-product FOV checks, early-exit PVS checks (`trap_InPVS`), and dynamic proximity time-slicing. Queries BSP lightgrid and dynamic light nodes (`trap_GetLightAtPoint`) during visibility evaluations, dynamically scaling guard detection distances based on player illumination levels when crouching or hiding in shadows without requiring map recompilation.
* **Light & Shadow Stealth System & UI Option**: Integrates real-time map lightgrid and dynamic light sampling into AI perception (`AICast_CheckVisibility`). Crouching or concealing yourself in dark areas ($\text{Illumination} < 35\%$) scales down AI detection range, while fully preserving vanilla RTCW AI perception when out of shadows. Adds the `g_stealthShadows` (default `1`) cvar and a **Stealth Shadows** toggle to the Gameplay Options menu (**Options -> Gameplay**).
* **Tactical Flashlight**: Adds a battery-powered flashlight featuring dynamic dimming curves, standalone HUD indicators, and dedicated AI detection paths for enhanced stealth options. Flashlight illumination or unsilenced gunshots override shadow concealment.
* **WWII Realistic Bullet Physics & Ballistics**: Adds an option to toggle physical bullet projectiles instead of standard hitscan traces. Flight speeds are modeled after real-world muzzle velocities of WWII weapons (e.g., Luger, Colt, MP40, Mauser, Venom minigun) with optional gravity-drop trajectory simulations, while retaining full compatibility with headshot zones, damage falloff, and material penetration.
* **Stencil-Masked Solid Projection Shadows (`tr_shade.c`)**: Integrates OpenGL stencil buffer single-pass polygon masking during planar shadow rendering. Discards overlapping internal 3D model polygons to render a clean, solid, dark shadow silhouette without semi-transparent wireframe line artifacts.
* **MDC Model Compression Guard & Crash Immunity (`tr_surface.c`)**: Implemented strict validation of compression frame offsets (`ofsFrameCompFrames`, `ofsXyzCompressed`) and non-null pointer checks in `LerpCMeshVertexes`, eliminating invalid memory access (SIGSEGV) crashes on compressed MDC character surface meshes.
* **Engine Thread Safety & AI Performance Optimizations**: Replaced legacy un-synchronized `pthread` sleep threads with frame-based engine timers, converted per-frame AI enemy distance calculations to `DistanceSquared()` to remove unnecessary `sqrt()` calls, and enforced bounds checking on client damage commands (`g_cmds.c`).
* **Automatic Crash Callstack Logger & Diagnostic Telemetry (`sys_main.c`)**: Installed automatic signal trap handling (`SIGSEGV`, `SIGFPE`, `SIGILL`, `SIGABRT`, `SIGBUS`) and `Sys_Error` diagnostics. Translates raw signal codes into descriptive error names, captures a 32-frame callstack backtrace via `backtrace()`, dynamically resolves library symbol names and byte offsets using POSIX `dladdr()`, embeds runtime telemetry (active map name, display resolution, window focus/minimized state, engine uptime), dual-writes formatted entries to both working directory `crash.log` and user configuration home directory (`fs_homepath/main/crashlog.txt`), and triggers a graphical `Sys_ErrorDialog` popup for immediate user feedback.
* **High-DPI Console & OS Enhancements**: Scales the developer console layout dynamically for 1440p and 4K displays. The engine also now automatically pauses and mutes audio when the application window loses focus (Alt-Tab).
* **Bazzite & Distrobox Dev Support**: Configured custom build scripts for compiling within `Bazzite-dev-nvidia` Distrobox containers, streamlining dev environment setup on immutable Linux operating systems.
* **Modernized Dependencies & CI/CD**: Added native support for Steam Workshop paths on Linux, updated zlib from 1.2.11 to 1.3.2, updated Opus to 1.5.2, Opusfile to 0.12, Libogg to 1.3.5, Libvorbis to 1.3.7, and cURL headers to 8.8.0, and automated package compilation and release distribution packaging through streamlined CI/CD build scripts.

