#include "PjsipGdiRenderer.h"

#if defined(HAVE_PJSIP) && defined(_WIN32)

#include <pjmedia-videodev/videodev_imp.h>
#include <pjlib.h>
#include <windows.h>
#include <cstring>

// ---------------------------------------------------------------------------
// GDI video renderer for PJSIP on Windows.
//
// Decoded video frames (BGRA format) are rendered into a target HWND via
// StretchDIBits.  The target HWND is set at stream creation time (via the
// pjsua_vid_preview_param.wnd field) or updated later via
// pjsua_vid_win_set_win() → stream_set_cap(PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW).
//
// Thread safety: put_frame() is called from PJSIP's media thread.  GetDC /
// StretchDIBits / ReleaseDC are thread-safe for an HWND owned by another
// thread (Windows serialises DC access per HWND).  The targetHwnd field is
// written by set_cap (also called from PJSIP thread) and read by put_frame.
// Both operations happen on PJSIP's media thread so no mutex is needed.
// ---------------------------------------------------------------------------

struct QtGdiStream {
    pjmedia_vid_dev_stream  base;       // MUST be first
    pj_pool_t              *pool;
    pjmedia_vid_dev_param   param;
    HWND                    targetHwnd;
    pj_bool_t               running;
};

struct QtGdiFactory {
    pjmedia_vid_dev_factory base;       // MUST be first
    pj_pool_t              *pool;
    pj_pool_factory        *pf;
};

// ---- forward declarations -------------------------------------------------
static pj_status_t qt_factory_init(pjmedia_vid_dev_factory *f);
static pj_status_t qt_factory_destroy(pjmedia_vid_dev_factory *f);
static unsigned    qt_factory_get_dev_count(pjmedia_vid_dev_factory *f);
static pj_status_t qt_factory_get_dev_info(pjmedia_vid_dev_factory *f, unsigned idx, pjmedia_vid_dev_info *info);
static pj_status_t qt_factory_default_param(pj_pool_t *pool, pjmedia_vid_dev_factory *f, unsigned idx, pjmedia_vid_dev_param *param);
static pj_status_t qt_factory_create_stream(pjmedia_vid_dev_factory *f, pjmedia_vid_dev_param *param, const pjmedia_vid_dev_cb *cb, void *user_data, pjmedia_vid_dev_stream **p_strm);
static pj_status_t qt_factory_refresh(pjmedia_vid_dev_factory *f);

static pj_status_t qt_stream_get_param(pjmedia_vid_dev_stream *s, pjmedia_vid_dev_param *param);
static pj_status_t qt_stream_get_cap(pjmedia_vid_dev_stream *s, pjmedia_vid_dev_cap cap, void *value);
static pj_status_t qt_stream_set_cap(pjmedia_vid_dev_stream *s, pjmedia_vid_dev_cap cap, const void *value);
static pj_status_t qt_stream_start(pjmedia_vid_dev_stream *s);
static pj_status_t qt_stream_get_frame(pjmedia_vid_dev_stream *s, pjmedia_frame *frame);
static pj_status_t qt_stream_put_frame(pjmedia_vid_dev_stream *s, const pjmedia_frame *frame);
static pj_status_t qt_stream_stop(pjmedia_vid_dev_stream *s);
static pj_status_t qt_stream_destroy(pjmedia_vid_dev_stream *s);

// ---- ops tables -----------------------------------------------------------
static pjmedia_vid_dev_factory_op s_factory_ops = {
    qt_factory_init,
    qt_factory_destroy,
    qt_factory_get_dev_count,
    qt_factory_get_dev_info,
    qt_factory_default_param,
    qt_factory_create_stream,
    qt_factory_refresh
};

static pjmedia_vid_dev_stream_op s_stream_ops = {
    qt_stream_get_param,
    qt_stream_get_cap,
    qt_stream_set_cap,
    qt_stream_start,
    qt_stream_get_frame,
    qt_stream_put_frame,
    qt_stream_stop,
    qt_stream_destroy
};

// ---- global state ---------------------------------------------------------
static pjmedia_vid_dev_index s_device_index = PJMEDIA_VID_INVALID_DEV;

// ---- factory creation function (called by pjmedia_vid_register_factory) ---
static pjmedia_vid_dev_factory *qt_gdi_create_factory(pj_pool_factory *pf)
{
    pj_pool_t *pool = pj_pool_create(pf, "qt_gdi_vid_fac", 512, 512, nullptr);
    if (!pool)
        return nullptr;
    auto *f = PJ_POOL_ZALLOC_T(pool, QtGdiFactory);
    f->base.op = &s_factory_ops;
    f->pool = pool;
    f->pf   = pf;
    return &f->base;
}

// ---- factory callbacks ----------------------------------------------------
static pj_status_t qt_factory_init(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}

static pj_status_t qt_factory_destroy(pjmedia_vid_dev_factory *f)
{
    auto *factory = reinterpret_cast<QtGdiFactory *>(f);
    pj_pool_release(factory->pool);
    return PJ_SUCCESS;
}

static unsigned qt_factory_get_dev_count(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return 1;
}

static pj_status_t qt_factory_get_dev_info(pjmedia_vid_dev_factory *f, unsigned idx,
                                            pjmedia_vid_dev_info *info)
{
    PJ_UNUSED_ARG(f);
    if (idx != 0)
        return PJMEDIA_EVID_INVDEV;

    pj_bzero(info, sizeof(*info));
    pj_ansi_strxcpy(info->name,   "Qt GDI Renderer", sizeof(info->name));
    pj_ansi_strxcpy(info->driver, "QtGDI",            sizeof(info->driver));
    info->dir          = PJMEDIA_DIR_RENDER;
    info->has_callback = PJ_FALSE;
    info->caps         = PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
    info->fmt_cnt      = 1;
    pjmedia_format_init_video(&info->fmt[0], PJMEDIA_FORMAT_BGRA, 640, 480, 30000, 1001);
    return PJ_SUCCESS;
}

static pj_status_t qt_factory_default_param(pj_pool_t *pool, pjmedia_vid_dev_factory *f,
                                             unsigned idx, pjmedia_vid_dev_param *param)
{
    PJ_UNUSED_ARG(pool);
    PJ_UNUSED_ARG(f);
    if (idx != 0)
        return PJMEDIA_EVID_INVDEV;

    pj_bzero(param, sizeof(*param));
    param->dir     = PJMEDIA_DIR_RENDER;
    param->rend_id = PJMEDIA_VID_INVALID_DEV;
    param->cap_id  = PJMEDIA_VID_INVALID_DEV;
    pjmedia_format_init_video(&param->fmt, PJMEDIA_FORMAT_BGRA, 640, 480, 30000, 1001);
    return PJ_SUCCESS;
}

static pj_status_t qt_factory_create_stream(pjmedia_vid_dev_factory *f,
                                             pjmedia_vid_dev_param *param,
                                             const pjmedia_vid_dev_cb *cb,
                                             void *user_data,
                                             pjmedia_vid_dev_stream **p_strm)
{
    PJ_UNUSED_ARG(cb);
    PJ_UNUSED_ARG(user_data);

    auto *factory = reinterpret_cast<QtGdiFactory *>(f);
    pj_pool_t *pool = pj_pool_create(factory->pf, "qt_gdi_vid_strm", 512, 512, nullptr);
    if (!pool)
        return PJ_ENOMEM;

    auto *strm = PJ_POOL_ZALLOC_T(pool, QtGdiStream);
    strm->base.op  = &s_stream_ops;
    strm->pool     = pool;
    strm->running  = PJ_FALSE;
    strm->targetHwnd = nullptr;
    pj_memcpy(&strm->param, param, sizeof(*param));

    // Accept an initial target window if provided via pvp.wnd / OUTPUT_WINDOW cap.
    if ((param->flags & PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) &&
         param->window.info.win.hwnd) {
        strm->targetHwnd = static_cast<HWND>(param->window.info.win.hwnd);
    }

    *p_strm = &strm->base;
    return PJ_SUCCESS;
}

static pj_status_t qt_factory_refresh(pjmedia_vid_dev_factory *f)
{
    PJ_UNUSED_ARG(f);
    return PJ_SUCCESS;
}

// ---- stream callbacks -----------------------------------------------------
static pj_status_t qt_stream_get_param(pjmedia_vid_dev_stream *s, pjmedia_vid_dev_param *param)
{
    auto *strm = reinterpret_cast<QtGdiStream *>(s);
    pj_memcpy(param, &strm->param, sizeof(*param));
    if (strm->targetHwnd) {
        param->flags |= PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW;
        param->window.type           = PJMEDIA_VID_DEV_HWND_TYPE_WINDOWS;
        param->window.info.win.hwnd  = strm->targetHwnd;
    }
    return PJ_SUCCESS;
}

static pj_status_t qt_stream_get_cap(pjmedia_vid_dev_stream *s, pjmedia_vid_dev_cap cap, void *value)
{
    auto *strm = reinterpret_cast<QtGdiStream *>(s);
    if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        auto *hwnd = static_cast<pjmedia_vid_dev_hwnd *>(value);
        hwnd->type           = PJMEDIA_VID_DEV_HWND_TYPE_WINDOWS;
        hwnd->info.win.hwnd  = strm->targetHwnd;
        return PJ_SUCCESS;
    }
    return PJMEDIA_EVID_INVCAP;
}

static pj_status_t qt_stream_set_cap(pjmedia_vid_dev_stream *s, pjmedia_vid_dev_cap cap,
                                      const void *value)
{
    auto *strm = reinterpret_cast<QtGdiStream *>(s);
    if (cap == PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW) {
        const auto *hwnd = static_cast<const pjmedia_vid_dev_hwnd *>(value);
        strm->targetHwnd = static_cast<HWND>(hwnd->info.win.hwnd);
        return PJ_SUCCESS;
    }
    return PJMEDIA_EVID_INVCAP;
}

static pj_status_t qt_stream_start(pjmedia_vid_dev_stream *s)
{
    auto *strm = reinterpret_cast<QtGdiStream *>(s);
    strm->running = PJ_TRUE;
    return PJ_SUCCESS;
}

static pj_status_t qt_stream_get_frame(pjmedia_vid_dev_stream *s, pjmedia_frame *frame)
{
    PJ_UNUSED_ARG(s);
    PJ_UNUSED_ARG(frame);
    return PJ_EINVALIDOP; // render-only
}

static pj_status_t qt_stream_put_frame(pjmedia_vid_dev_stream *s, const pjmedia_frame *frame)
{
    auto *strm = reinterpret_cast<QtGdiStream *>(s);
    if (!strm->running || !strm->targetHwnd || !frame->buf || frame->size == 0)
        return PJ_SUCCESS;

    const int w = static_cast<int>(strm->param.fmt.det.vid.size.w);
    const int h = static_cast<int>(strm->param.fmt.det.vid.size.h);
    if (w <= 0 || h <= 0)
        return PJ_SUCCESS;

    HDC hdc = GetDC(strm->targetHwnd);
    if (!hdc)
        return PJ_SUCCESS;

    RECT rc{};
    GetClientRect(strm->targetHwnd, &rc);
    if (rc.right > 0 && rc.bottom > 0) {
        // BGRA: byte order B,G,R,A — matches BI_RGB 32bpp (B,G,R,padding).
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = w;
        bmi.bmiHeader.biHeight      = -h;   // negative = top-down
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        StretchDIBits(hdc,
                      0, 0, rc.right, rc.bottom,
                      0, 0, w,        h,
                      frame->buf, &bmi, DIB_RGB_COLORS, SRCCOPY);
    }

    ReleaseDC(strm->targetHwnd, hdc);
    return PJ_SUCCESS;
}

static pj_status_t qt_stream_stop(pjmedia_vid_dev_stream *s)
{
    auto *strm = reinterpret_cast<QtGdiStream *>(s);
    strm->running = PJ_FALSE;
    return PJ_SUCCESS;
}

static pj_status_t qt_stream_destroy(pjmedia_vid_dev_stream *s)
{
    auto *strm = reinterpret_cast<QtGdiStream *>(s);
    strm->running = PJ_FALSE;
    pj_pool_release(strm->pool);
    return PJ_SUCCESS;
}

// ---- Public API -----------------------------------------------------------

pj_status_t PjsipGdiRenderer::registerFactory()
{
    if (s_device_index != PJMEDIA_VID_INVALID_DEV)
        return PJ_SUCCESS;

    pj_status_t st = pjmedia_vid_register_factory(qt_gdi_create_factory, nullptr);
    if (st != PJ_SUCCESS)
        return st;

    // Locate the newly registered device by driver name.
    const unsigned count = pjmedia_vid_dev_count();
    for (unsigned i = 0; i < count; ++i) {
        pjmedia_vid_dev_info info;
        pj_bzero(&info, sizeof(info));
        if (pjmedia_vid_dev_get_info(static_cast<pjmedia_vid_dev_index>(i), &info) == PJ_SUCCESS
            && info.dir == PJMEDIA_DIR_RENDER
            && std::strcmp(info.driver, "QtGDI") == 0) {
            s_device_index = static_cast<pjmedia_vid_dev_index>(i);
            return PJ_SUCCESS;
        }
    }

    return PJ_ENOTFOUND;
}

pjmedia_vid_dev_index PjsipGdiRenderer::deviceIndex()
{
    return s_device_index;
}

#endif // HAVE_PJSIP && _WIN32
