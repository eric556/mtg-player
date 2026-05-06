#pragma once
#ifdef _WIN32
#include <guiddef.h>

// {68943b5f-3ac7-4e2d-8c1a-f9b7d3e2c041}
// Declared here; defined exactly once per binary via DEFINE_CLSID_MtgVCamSource()
// in either dllmain.cpp (DLL) or vcam_win32.cpp (exe).
extern const GUID CLSID_MtgVCamSource;

// Drop this macro in ONE .cpp per binary to emit the definition.
#define DEFINE_CLSID_MtgVCamSource() \
    extern "C" const GUID CLSID_MtgVCamSource = \
        {0x68943b5f,0x3ac7,0x4e2d,{0x8c,0x1a,0xf9,0xb7,0xd3,0xe2,0xc0,0x41}};

#endif // _WIN32
