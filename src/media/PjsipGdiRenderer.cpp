#include "PjsipGdiRenderer.h"

#if defined(HAVE_PJSIP) && defined(_WIN32)

#include <pjmedia-videodev/videodev_imp.h>
#include <pjlib.h>
#include <windows.h>
#include <atomic>
#include <cstring>
#include <memory>

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QMetaObject>
#include <QPixmap>
#include <QVariant>
#include <QWidget>

// ---------------------------------------------------------------------------
// Qt video renderer for PJSIP on Windows.
//
// Decoded video frames (I420) are converted to BGRA and delivered to the Qt
// main thread via QMetaObject::invokeMethod (Qt::QueuedConnection).  The
// target widget is located by HWND using QWidget::find().  This avoids all
// GDI/DWM compositing conflicts and lets Qt own the painting lifecycle.
//
// Thread safety: put_frame() is called from PJSIP's media thread.
// targetHwnd is written by set_cap (PJSIP thread) and read by put_frame
// (also PJSIP thread) — no mutex needed between them.  The QImage deep-copy
// inside put_frame() transfers ownership to the lambda before the buffer is
// reused, so the main-thread lambda always sees its own private copy.
// ---------------------------------------------------------------------------

struct QtGdiStream {
    pjmedia_vid_dev_stream  base;       // MUST be first
    pj_pool_t              *pool;
    pjmedia_vid_dev_param   param;
    HWND                    targetHwnd;
    pj_bool_t               running;
    uint32_t                fmtId;       // negotiated input format id
    uint8_t                *convertBuf;  // BGRA staging buffer for non-BGRA input
    unsigned                convertBufSize;
    // Shared with in-flight Qt lambdas via shared_ptr copy so it outlives the stream.
    std::shared_ptr<std::atomic<bool>> *framePendingPtr;
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
    info->caps         = PJMEDIA_VID_DEV_CAP_OUTPUT_WINDOW
                       | PJMEDIA_VID_DEV_CAP_FORMAT;
    // Advertise only I420 so PJSIP can connect VP8 decoder (I420 output)
    // directly to this renderer without needing a format converter
    // (PJMEDIA_HAS_FFMPEG=0, PJMEDIA_HAS_LIBYUV=0 — no converters available).
    // qt_stream_put_frame() does I420→BGRA conversion internally before
    // calling StretchDIBits, so BGRA must NOT be advertised here.
    info->fmt_cnt = 4;
    pjmedia_format_init_video(&info->fmt[0], PJMEDIA_FORMAT_I420,  640,  480, 30000, 1001);
    pjmedia_format_init_video(&info->fmt[1], PJMEDIA_FORMAT_I420, 1280,  720, 30000, 1001);
    pjmedia_format_init_video(&info->fmt[2], PJMEDIA_FORMAT_I420, 1920, 1080, 30000, 1001);
    pjmedia_format_init_video(&info->fmt[3], PJMEDIA_FORMAT_I420,  320,  240, 30000, 1001);
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
    param->rend_id = idx;                   // local index → make_global_index() will convert to global
    param->cap_id  = PJMEDIA_VID_INVALID_DEV;
    param->clock_rate = 90000;
    pjmedia_format_init_video(&param->fmt, PJMEDIA_FORMAT_I420, 640, 480, 30000, 1001);
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

    // Allocate a BGRA staging buffer (heap-owned so set_cap can reallocate it).
    strm->fmtId = param->fmt.id;
    {
        const unsigned w = param->fmt.det.vid.size.w;
        const unsigned h = param->fmt.det.vid.size.h;
        strm->convertBufSize = w * h * 4;
        strm->convertBuf = static_cast<uint8_t *>(malloc(strm->convertBufSize));
        if (!strm->convertBuf) {
            pj_pool_release(pool);
            return PJ_ENOMEM;
        }
    }

    strm->framePendingPtr = new std::shared_ptr<std::atomic<bool>>(
        std::make_shared<std::atomic<bool>>(false));

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

    if (cap == PJMEDIA_VID_DEV_CAP_FORMAT) {
        const auto *fmt = static_cast<const pjmedia_format *>(value);
        // We only handle I420; VP8 decoder always outputs I420.
        if (fmt->id != PJMEDIA_FORMAT_I420)
            return PJMEDIA_EVID_INVCAP;

        const unsigned w = fmt->det.vid.size.w;
        const unsigned h = fmt->det.vid.size.h;
        if (w == 0 || h == 0)
            return PJMEDIA_EVID_INVCAP;

        const unsigned newSize = w * h * 4;
        uint8_t *newBuf = static_cast<uint8_t *>(realloc(strm->convertBuf, newSize));
        if (!newBuf)
            return PJ_ENOMEM;

        strm->convertBuf     = newBuf;
        strm->convertBufSize = newSize;
        strm->fmtId          = PJMEDIA_FORMAT_I420;
        pjmedia_format_copy(&strm->param.fmt, fmt);
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

// Software I420→BGRA conversion.  Called on PJSIP's media thread; no locks needed.
static void i420ToBgra(const uint8_t *src, uint8_t *dst, int w, int h)
{
    const uint8_t *Y = src;
    const uint8_t *U = Y + w * h;
    const uint8_t *V = U + (w >> 1) * (h >> 1);
    for (int row = 0; row < h; ++row) {
        const uint8_t *yRow = Y + row * w;
        const uint8_t *uRow = U + (row >> 1) * (w >> 1);
        const uint8_t *vRow = V + (row >> 1) * (w >> 1);
        uint8_t *d = dst + row * w * 4;
        for (int col = 0; col < w; col += 2) {
            const int c0 = yRow[col]     - 16;
            const int c1 = yRow[col + 1] - 16;
            const int uu = uRow[col >> 1] - 128;
            const int vv = vRow[col >> 1] - 128;
            auto clamp = [](int x) -> uint8_t {
                return x < 0 ? 0 : x > 255 ? 255 : static_cast<uint8_t>(x);
            };
            *d++ = clamp((298 * c0 + 516 * uu + 128) >> 8);           // B
            *d++ = clamp((298 * c0 - 100 * uu - 208 * vv + 128) >> 8);// G
            *d++ = clamp((298 * c0 + 409 * vv + 128) >> 8);           // R
            *d++ = 0xFF;
            *d++ = clamp((298 * c1 + 516 * uu + 128) >> 8);
            *d++ = clamp((298 * c1 - 100 * uu - 208 * vv + 128) >> 8);
            *d++ = clamp((298 * c1 + 409 * vv + 128) >> 8);
            *d++ = 0xFF;
        }
    }
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

    // Convert I420 → BGRA into the staging buffer.
    if (strm->fmtId != PJMEDIA_FORMAT_I420 || !strm->convertBuf
            || strm->convertBufSize < static_cast<unsigned>(w * h * 4))
        return PJ_SUCCESS;

    // Drop this frame if the previous one hasn't been consumed by the Qt main
    // thread yet.  This caps the event-queue depth to 1 and prevents latency
    // from accumulating when the main thread is momentarily busy.
    auto framePending = *strm->framePendingPtr;  // shared_ptr copy
    bool notPending = false;
    if (!framePending->compare_exchange_strong(notPending, true))
        return PJ_SUCCESS;

    i420ToBgra(static_cast<const uint8_t *>(frame->buf), strm->convertBuf, w, h);

    // Deep-copy the BGRA data so the staging buffer can be reused immediately
    // while the Qt main thread is still rendering the previous copy.
    // i420ToBgra writes bytes B,G,R,0xFF per pixel; on little-endian x86 that
    // is 0xFFRRGGBB as a uint32, which matches QImage::Format_ARGB32.
    QImage imgCopy(strm->convertBuf, w, h, w * 4, QImage::Format_ARGB32);
    imgCopy = imgCopy.copy();

    // Post to the Qt main thread.  QWidget::find() maps a native HWND to the
    // QWidget that holds it (requires WA_NativeWindow on that widget).
    const HWND hwnd = strm->targetHwnd;
    QMetaObject::invokeMethod(qApp,
        [hwnd, img = std::move(imgCopy), fp = framePending]() mutable {
        fp->store(false);  // release the slot whether or not we render
        QWidget *widget = QWidget::find(reinterpret_cast<WId>(hwnd));
        if (!widget) {
            // One-shot warning: hwnd not (yet) known to Qt's widget registry.
            static std::atomic<int> s_missCount{0};
            if (s_missCount.fetch_add(1) < 5)
                qWarning("[GDI] QWidget::find(%p) returned null", hwnd);
            return;
        }
        if (auto *label = qobject_cast<QLabel *>(widget)) {
            // Local preview PiP: PJSIP owns the camera during calls (DirectShow
            // conflicts with Qt Camera), so render PJSIP frames directly.
            label->setPixmap(QPixmap::fromImage(
                img.scaled(label->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            return;
        }
        // Generic widget (VideoPanel remote video): store frame and repaint.
        widget->setProperty("_pjFrame", QVariant::fromValue(std::move(img)));
        widget->update();
    }, Qt::QueuedConnection);

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
    free(strm->convertBuf);
    strm->convertBuf = nullptr;
    delete strm->framePendingPtr;  // in-flight lambdas hold their own shared_ptr copies
    strm->framePendingPtr = nullptr;
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
