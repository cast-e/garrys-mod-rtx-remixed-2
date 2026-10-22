#pragma once
#include <remix/remix_c.h>
#include <windows.h>

namespace proxy::frame_profile {
// Optional CSV diagnostics. Disabled sessions perform no API calls or file I/O.
// Samples describe completed GPU frames; IDs distinguish repeated readbacks.
void sample(double hostFrameMilliseconds, remixapi_ErrorCode presentStatus,
            HWND hostWindow, PFN_remixapi_GetWindowState getWindowState);
}
