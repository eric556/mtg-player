# Virtual Camera Implementation Options for MTG Sim

## 1. Native Windows 11 API (Current Implementation)
**Technology:** Media Foundation `IMFVirtualCamera`
**Architecture:** Main App (`mtg-sim.exe`) + Media Source DLL (`mtg-sim-vcam.dll`) + Shared Memory + Mutex.

### Pros:
- **Zero Dependencies:** Doesn't require external drivers or OBS.
- **Modern:** Uses the latest Windows 11 API (future-proof).
- **System Native:** The camera is treated as a real hardware device by the OS.

### Cons:
- **Massive Boilerplate:** Requires ~1,000 lines of COM and Media Foundation code.
- **Maintenance Nightmare:** Very difficult to debug (runs inside the system Frame Server process).
- **Complexity:** Requires registration in HKLM (Admin) or complex HKCU workarounds.

---

## 2. OBS-Based Wrapper (`mycrl/vcam`) - RECOMMENDED
**Technology:** C++ wrapper around the OBS Virtual Camera module.
**Architecture:** High-level "Sender" API.

### Pros:
- **Simplicity:** Deletes ~80% of your current VCam code. You just call `vcam::push_frame(pixels)`.
- **Performance:** Inherits the high-performance delivery logic used by OBS.
- **Reliability:** Battle-tested by millions of OBS users.

### Cons:
- **Dependency:** Adds a third-party library to the build.
- **Older Model:** Uses a stable but older registration model compared to `IMFVirtualCamera`.

---

## 3. Legacy DirectShow (`catid/softcam`)
**Technology:** DirectShow Filter.
**Architecture:** Simple C interface.

### Pros:
- **Ultimate Compatibility:** Works with everything, including very old browsers and legacy apps.
- **Easiest API:** The simplest possible way to push a buffer to a virtual device.

### Cons:
- **Legacy:** Microsoft considers DirectShow deprecated (though it still works fine in Win 11).

---

## Recommendation
Switch to **Option 2 (`mycrl/vcam`)**. It provides the best balance of performance and code cleanliness. You can remove the complex COM DLL and shared memory logic entirely, replacing it with a clean, high-level C++ API.
