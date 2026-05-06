# Virtual Camera Implementation Plan

## Goal
Expose the playmat window as a virtual webcam on Windows (no OBS, no driver install)
so SpellTable and other browser-based camera apps see it directly.
Linux already works via `--vcam /dev/video0` (ffmpeg → v4l2loopback).

## Architecture

### Two-component Windows design (unavoidable)
Windows' Frame Server service loads the media source DLL in its own process.
Cross-process frame delivery is done via named shared memory.

```
SFML app (main process)
  glReadPixels → flip → BGRA pixels
  → write to Named Shared Memory ("Local\MtgSimVCam")
  
  MFCreateVirtualCamera(CLSID_MtgVCamSource) → camera appears in system
  
Frame Server service (separate process, Session 0)
  loads mtg-sim-vcam.dll
  Chrome calls RequestSample()
  → DLL reads from shared memory → returns IMFSample to Chrome
```

### Linux (already implemented)
`--vcam /dev/video0` pipes raw BGRA frames via ffmpeg into v4l2loopback.
No changes needed on Linux path.

---

## Pixel Format Convention
- `glReadPixels(GL_BGRA)` → bottom-up BGRA bytes
- Vertical flip → **top-down BGRA** (same memory layout as `MFVideoFormat_ARGB32`)
- Shared memory stores top-down BGRA
- MF media type: `MFVideoFormat_ARGB32`, stride = `+width*4` (positive = top-down)
- ffmpeg flag: `-pixel_format bgra` (ffmpeg treats as top-down by default)

---

## File Layout

```
src/vcam/
  vcam.hpp                   – abstract Vcam interface (pushFrame / start / stop)
  vcam_ffmpeg.hpp/.cpp       – ffmpeg pipe impl (Linux/fallback)
  vcam_shared_mem.hpp        – shared memory struct (shared with DLL)
  vcam_win32.hpp/.cpp        – Windows app-side: opens SHM, calls MFCreateVirtualCamera
  win32_source/
    guids.hpp                – CLSID_MtgVCamSource + GUID helpers
    dllmain.cpp              – DllMain, DllGetClassObject, DllRegisterServer/Unregister
    media_source.hpp/.cpp    – IMFMediaSource + IKsControl + IMFGetService
    media_stream.hpp/.cpp    – IMFMediaStream (RequestSample reads from SHM)
```

---

## Key GUIDs
```
CLSID_MtgVCamSource = {68943b5f-3ac7-4e2d-8c1a-f9b7d3e2c041}
```

---

## Shared Memory Layout
```cpp
// "Local\MtgSimVCam"
struct VcamSharedMem {
    volatile uint32_t frame_count; // app increments after each write
    uint32_t width;
    uint32_t height;
    uint8_t  bgra[1920 * 1080 * 4]; // top-down BGRA, row-major
};
// Named mutex "Local\MtgSimVCamMutex" guards reads/writes
```

---

## CMake Targets
- `mtg-sim` (existing) – links `src/vcam/vcam_ffmpeg.cpp` (all platforms)
  - Windows also links `src/vcam/vcam_win32.cpp` + `mfsensorgroup.lib` + `mfplat.lib` + `mfuuid.lib`
- `mtg-sim-vcam` (Windows only, SHARED lib) – the COM DLL
  - Sources: `win32_source/*.cpp`
  - Links: `mfplat.lib mfuuid.lib ole32.lib`
  - Output goes to same dir as `mtg-sim.exe`

---

## CLI Flags
- `--vcam <output>` (existing) – ffmpeg pipe to file/URL/v4l2 device
- `--vcam-fps <n>` (existing) – framerate for ffmpeg path
- `--vcam-native` (new, Windows-only) – use IMFVirtualCamera (no ffmpeg needed)
  - On first run: DLL is not registered → app calls `DllRegisterServer` internally
    (writes to HKCU, no admin needed if `MFVirtualCameraAccess_CurrentUser`)
  - Starts virtual camera, appears in Windows camera list + Chrome

---

## PlaymatWindow Integration
`PlaymatWindow` holds `std::unique_ptr<Vcam> vcam_` + a throttle clock.
In `render()`: after drawing + before `display()`, call
`vcam_->pushFrame(pixels, w, h)` if clock elapsed.
The Vcam implementations handle all platform-specific details.

---

## Implementation Phases

### Phase 1 – Abstraction layer (DONE ✓ in principle, needs refactor)
- [x] `vcam.hpp` abstract interface
- [x] `vcam_ffmpeg.cpp` (extract from playmat_window)
- [x] PlaymatWindow uses `unique_ptr<Vcam>`
- [x] CMakeLists updated

### Phase 2 – Shared memory + Windows app side
- [x] `vcam_shared_mem.hpp`
- [x] `vcam_win32.hpp/.cpp` (SHM writer + MFCreateVirtualCamera)

### Phase 3 – COM DLL
- [x] `win32_source/guids.hpp`
- [x] `win32_source/vcam_source.def` (DLL exports)
- [x] `win32_source/dllmain.cpp` (DllMain, ClassFactory, DllRegisterServer/Unregister)
- [x] `win32_source/media_source.hpp/.cpp`
- [x] `win32_source/media_stream.hpp/.cpp`

### Phase 4 – CMake DLL target + main.cpp --vcam-native
- [x] `CMakeLists.txt` DLL target + link libraries
- [x] `main.cpp` --vcam-native flag
- [x] `VCAM_PLAN.md` phase checkboxes updated
- [x] README updated

## Status: Implementation complete — needs first build + test
Known open questions (see bottom of this file).

---

## Key Reference
- smourier/VCamSample: https://github.com/smourier/VCamSample
- MFCreateVirtualCamera: https://learn.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/nf-mfvirtualcamera-mfcreatevirtualcamera
- Frame Server custom source: https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/frame-server-custom-media-source

## Registration Note
DLL must be registered in the COM registry. Trying HKCU first
(`MFVirtualCameraAccess_CurrentUser`) — avoids admin elevation requirement.
Fallback: HKLM with UAC-elevated subprocess (`ShellExecuteW` + `runas`).

## Open Questions
- Does Chrome accept `MFVideoFormat_ARGB32`? Fallback: add NV12 as primary format.
- Does `MFVirtualCameraAccess_CurrentUser` + HKCU registration work end-to-end?
  If not, need UAC elevation for HKLM write on first-run.
