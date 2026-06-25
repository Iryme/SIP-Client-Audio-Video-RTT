#pragma once

#if defined(HAVE_PJSIP) && defined(_WIN32)
#include <pjmedia-videodev/videodev.h>

// Custom pjmedia video renderer that blits decoded frames into a Win32 HWND
// via StretchDIBits.  Supports PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW so that
// pjsua_vid_win_set_win() redirects rendering into our Qt widget surface.
// Must be registered (registerFactory) once after pjsua libStart().
class PjsipGdiRenderer {
public:
    static pj_status_t registerFactory();
    static pjmedia_vid_dev_index deviceIndex();
};

#endif // HAVE_PJSIP && _WIN32
