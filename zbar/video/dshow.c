/*------------------------------------------------------------------------
 *  Copyright 2012 (c) Klaus Triendl <klaus@triendl.eu>
 *
 *  This file is part of the ZBar Bar Code Reader.
 *
 *  The ZBar Bar Code Reader is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU Lesser Public License as
 *  published by the Free Software Foundation; either version 2.1 of
 *  the License, or (at your option) any later version.
 *
 *  The ZBar Bar Code Reader is distributed in the hope that it will be
 *  useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser Public License
 *  along with the ZBar Bar Code Reader; if not, write to the Free
 *  Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *  Boston, MA  02110-1301  USA
 *
 *  http://sourceforge.net/projects/zbar
 *------------------------------------------------------------------------*/

#include "video.h"
#include "thread.h"
#include <objbase.h>
#include <strmif.h>
#include <control.h>
#include <qedit.h>
#include <amvideo.h>    // include after ddraw.h has been included from any dshow header
#include <assert.h>

#define ZBAR_DEFINE_STATIC_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        static const GUID DECLSPEC_SELECTANY name \
                = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

/* -------- */
/* define guids found in uuid.h, but for which we're missing the lib */

// e436ebb3-524f-11ce-9f53-0020af0ba770           Filter Graph
ZBAR_DEFINE_STATIC_GUID(CLSID_FilterGraph,
0xe436ebb3, 0x524f, 0x11ce, 0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
// 62BE5D10-60EB-11d0-BD3B-00A0C911CE86                 ICreateDevEnum
ZBAR_DEFINE_STATIC_GUID(CLSID_SystemDeviceEnum,
0x62BE5D10,0x60EB,0x11d0,0xBD,0x3B,0x00,0xA0,0xC9,0x11,0xCE,0x86);
// 860BB310-5D01-11d0-BD3B-00A0C911CE86                 Video capture category
ZBAR_DEFINE_STATIC_GUID(CLSID_VideoInputDeviceCategory,
0x860BB310,0x5D01,0x11d0,0xBD,0x3B,0x00,0xA0,0xC9,0x11,0xCE,0x86);
// BF87B6E1-8C27-11d0-B3F0-00AA003761C5     New Capture graph building
ZBAR_DEFINE_STATIC_GUID(CLSID_CaptureGraphBuilder2,
0xBF87B6E1, 0x8C27, 0x11d0, 0xB3, 0xF0, 0x0, 0xAA, 0x00, 0x37, 0x61, 0xC5);
// {301056D0-6DFF-11d2-9EEB-006008039E37}
ZBAR_DEFINE_STATIC_GUID(CLSID_MjpegDec,
0x301056d0, 0x6dff, 0x11d2, 0x9e, 0xeb, 0x0, 0x60, 0x8, 0x3, 0x9e, 0x37);

// 73646976-0000-0010-8000-00AA00389B71  'vids' == MEDIATYPE_Video
ZBAR_DEFINE_STATIC_GUID(MEDIATYPE_Video,
0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
// 05589f80-c356-11ce-bf01-00aa0055595a        FORMAT_VideoInfo
ZBAR_DEFINE_STATIC_GUID(FORMAT_VideoInfo,
0x05589f80, 0xc356, 0x11ce, 0xbf, 0x01, 0x00, 0xaa, 0x00, 0x55, 0x59, 0x5a);

// fb6c4282-0353-11d1-905f-0000c0cc16ba
ZBAR_DEFINE_STATIC_GUID(PIN_CATEGORY_PREVIEW,
0xfb6c4282, 0x0353, 0x11d1, 0x90, 0x5f, 0x00, 0x00, 0xc0, 0xcc, 0x16, 0xba);
// {AC798BE1-98E3-11d1-B3F1-00AA003761C5}
ZBAR_DEFINE_STATIC_GUID(LOOK_DOWNSTREAM_ONLY,
0xac798be1, 0x98e3, 0x11d1, 0xb3, 0xf1, 0x0, 0xaa, 0x0, 0x37, 0x61, 0xc5);

/* -------- */

// define a special guid that can be used for fourcc formats
// 00000000-0000-0010-8000-00AA00389B71   == MEDIASUBTYPE_FOURCC_PLACEHOLDER
ZBAR_DEFINE_STATIC_GUID(MEDIASUBTYPE_FOURCC_PLACEHOLDER,
0x00000000, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);


#define BIH_FMT "%ldx%ld @%dbpp (%lx) cmp=%.4s(%08lx) res=%ldx%ld clr=%ld/%ld (%lx)"
#define BIH_FIELDS(bih)                                                 \
    (bih)->biWidth, (bih)->biHeight, (bih)->biBitCount, (bih)->biSizeImage, \
        (char*)&(bih)->biCompression, (bih)->biCompression,             \
        (bih)->biXPelsPerMeter, (bih)->biYPelsPerMeter,                 \
        (bih)->biClrImportant, (bih)->biClrUsed, (bih)->biSize

#define COM_SAFE_RELEASE(ppIface) \
    if (*ppIface)\
        (*ppIface)->lpVtbl->Release(*ppIface)

#define CHECK_COM_ERROR(hr, msg, stmt)\
    if (FAILED(hr))\
    {\
        zprintf(6, msg, hr);\
        stmt;\
    }


static const REFERENCE_TIME _100ns_unit = 1*1000*1000*1000 / 100;



// Destroy (Release) the format block for a media type.
static void DestroyMediaType(AM_MEDIA_TYPE* mt)
{
    if (mt->cbFormat != 0)
        CoTaskMemFree(mt->pbFormat);

    if (mt->pUnk)
        IUnknown_Release(mt->pUnk);
}

// Destroy the format block for a media type and free the media type
static void DeleteMediaType(AM_MEDIA_TYPE* mt)
{
    if (!mt)
        return;
    DestroyMediaType(mt);
    CoTaskMemFree(mt);
}

static void make_fourcc_subtype(GUID* subtype, uint32_t fmt)
{
    memcpy(subtype, &MEDIASUBTYPE_FOURCC_PLACEHOLDER, sizeof(GUID));
    subtype->Data1 = fmt;
}

static int dshow_is_fourcc_guid(REFGUID subtype)
{
    // make up a fourcc guid in spe
    GUID clsid;
    make_fourcc_subtype(&clsid, subtype->Data1);

    return IsEqualGUID(subtype, &clsid);
}

/// Returns the index in <code>vdo->formats</code> of the given format.
/** If not found, returns -1 */
int get_format_index(uint32_t* fmts, uint32_t fmt0)
{
    uint32_t *fmt;
    int i = 0;
    for (fmt = fmts; *fmt; fmt++) {
        if (*fmt == fmt0)
            break;
        i++;
    }
    if (*fmt)
        return i;
    else
        return -1;
}

struct video_state_s
{
    zbar_thread_t thread;            /* capture message pump */
    HANDLE captured;
    HANDLE notify;                    /* capture thread status change */
    int bi_size;                    /* size of bih */
    BITMAPINFOHEADER* bih;            /* video format details */
    zbar_image_t* image;            /* currently capturing frame */

    IGraphBuilder* graph;            /* dshow graph manager */
    IMediaControl* mediacontrol;    /* dshow graph control */
    IBaseFilter* camera;            /* dshow source filter */
    ISampleGrabber* samplegrabber;    /* dshow intermediate filter */
    IBaseFilter* grabberbase;        /* samplegrabber's IBaseFilter interface */
    IBaseFilter* nullrenderer;
    ICaptureGraphBuilder2* builder;
    IAMStreamConfig* streamconfig;    /* dshow stream configuration interface */
    /// 0 terminated list of supported internal formats.
    /** The size of this
      * array matches the size of {@link zbar_video_s#formats} array and
      * the consecutive entries correspond to each other, making a mapping
      * between internal (dshow) formats and zbar formats
      * (presented to zbar processor). */
    uint32_t *int_formats;
};

static void dump_formats(zbar_video_t* vdo)
{
    video_state_t* state = vdo->state;
    uint32_t *fmt, *int_fmt;
    zprintf(8, "Detected formats: (internal) / (translated for zbar)\n");
    fmt = vdo->formats;
    int_fmt = state->int_formats;
    while (*fmt) {
        zprintf(8, "  %.4s / %.4s\n", (char*)int_fmt, (char*)fmt);
        fmt++; int_fmt++;
    }
}

/// Maps internal format MJPG (if found) to a format known to
/// zbar.
/** The conversion will be done by dshow mjpeg decompressor
  * before passing the image to zbar. */
static void prepare_mjpg_format_mapping(zbar_video_t* vdo)
{
    video_state_t* state = vdo->state;
    /// The format we will convert MJPG to
    uint32_t fmtConv = fourcc('B','G','R','4');

    int iMjpg = get_format_index(state->int_formats, fourcc('M','J','P','G'));
    if (iMjpg < 0)
        return;
    assert(vdo->formats[iMjpg] == fourcc('M','J','P','G'));

    // If we already have fmtConv, it will lead to duplicating it in
    // external formats.
    // We can't leave it this way, because when zbar wants to use fmtConv
    // we must have only one internal format for that.
    // It's better to drop MJPG then, as it's a compressed format and
    // we prefer better quality images.

    // The index of fmtConv before mapping mjpg to fmtConv
    int iMjpgConv = get_format_index(state->int_formats, fmtConv);

    if (iMjpgConv >= 0) {
        // remove the iMjpgConv entry by moving the following entries by 1
        int i;
        for (i=iMjpgConv; vdo->formats[i]; i++) {
            vdo->formats[i] = vdo->formats[i+1];
            state->int_formats[i] = state->int_formats[i+1];
        }
        // The number of formats is reduced by 1 now, but to realloc just
        // to save 2 times 4 bytes? Too much fuss.
    }
    else {
        vdo->formats[iMjpg] = fmtConv;
    }
}

static void dshow_destroy_video_state_t(video_state_t* state)
{
    COM_SAFE_RELEASE(&state->streamconfig);
    COM_SAFE_RELEASE(&state->builder);
    COM_SAFE_RELEASE(&state->nullrenderer);
    COM_SAFE_RELEASE(&state->grabberbase);
    COM_SAFE_RELEASE(&state->samplegrabber);
    COM_SAFE_RELEASE(&state->camera);
    COM_SAFE_RELEASE(&state->mediacontrol);
    COM_SAFE_RELEASE(&state->graph);

    if (state->captured)
        CloseHandle(state->captured);

    if(state->bih) { free(state->bih); state->bih = NULL; }
    if(state->int_formats) { free(state->int_formats);
                             state->int_formats = NULL; }
}


// sample grabber implementation (derived from ISampleGrabberCB)
typedef struct zbar_samplegrabber_cb
{
    // baseclass
    const struct ISampleGrabberCB _;
    // COM refcount
    ULONG refcount;

    zbar_video_t* vdo;

} zbar_samplegrabber_cb;

// ISampleGrabber methods (implementation below)

HRESULT __stdcall zbar_samplegrabber_cb_QueryInterface(ISampleGrabberCB* _This, REFIID riid, void** ppvObject);
ULONG __stdcall zbar_samplegrabber_cb_AddRef(ISampleGrabberCB* _This);
ULONG __stdcall zbar_samplegrabber_cb_Release(ISampleGrabberCB* _This);
HRESULT __stdcall zbar_samplegrabber_cb_SampleCB(ISampleGrabberCB* _This, double sampletime, IMediaSample* sample);
// note: original MS version expects a long for the buffer length, wine version a LONG
//HRESULT __stdcall zbar_samplegrabber_cb_BufferCB(ISampleGrabberCB* _This, double sampletime, BYTE* buffer, long bufferlen);
HRESULT __stdcall zbar_samplegrabber_cb_BufferCB(ISampleGrabberCB* _This, double sampletime, BYTE* buffer, LONG bufferlen);

static struct ISampleGrabberCBVtbl SampleGrabberCBVtbl = 
{
    &zbar_samplegrabber_cb_QueryInterface,
    &zbar_samplegrabber_cb_AddRef,
    &zbar_samplegrabber_cb_Release,
    &zbar_samplegrabber_cb_SampleCB,
    &zbar_samplegrabber_cb_BufferCB
};

static zbar_samplegrabber_cb* new_zbar_samplegrabber_cb(zbar_video_t* vdo)
{
    // allocate memory
    zbar_samplegrabber_cb* o = calloc(1, sizeof(zbar_samplegrabber_cb));
    
    // construct parent
    ISampleGrabberCB* base = (ISampleGrabberCB*)o;
    base->lpVtbl = &SampleGrabberCBVtbl;

    // construct object
    o->refcount = 1;
    o->vdo = vdo;
    
    return o;
}

static void delete_zbar_samplegrabber_cb(zbar_samplegrabber_cb* o)
{
    zprintf(16, "thr=%04lx\n", _zbar_thread_self());

    // no destruction necessary

    free(o);
}

HRESULT __stdcall zbar_samplegrabber_cb_QueryInterface(ISampleGrabberCB* _This, REFIID riid, void** ppvObject)
{
    if (IsEqualIID(riid, &IID_IUnknown)            || 
        IsEqualIID(riid, &IID_ISampleGrabberCB)    )
    {
        *ppvObject = _This;
        return NOERROR;
    }
    else
    {
        *ppvObject = NULL;
        return E_NOINTERFACE;
    }
}

ULONG __stdcall zbar_samplegrabber_cb_AddRef(ISampleGrabberCB* _This)
{
    zbar_samplegrabber_cb* This = (zbar_samplegrabber_cb*)_This;

    return ++This->refcount;
}

ULONG __stdcall zbar_samplegrabber_cb_Release(ISampleGrabberCB* _This)
{
    zbar_samplegrabber_cb* This = (zbar_samplegrabber_cb*)_This;

    ULONG refcnt = --This->refcount;
    if (!refcnt)
        delete_zbar_samplegrabber_cb(This);

    return refcnt;
}

HRESULT __stdcall zbar_samplegrabber_cb_SampleCB(ISampleGrabberCB* _This, double sampletime, IMediaSample* sample)
{
    return E_NOTIMPL;
}

//HRESULT __stdcall zbar_samplegrabber_cb_BufferCB(ISampleGrabberCB* _This, double sampletime, BYTE* buffer, long bufferlen)
HRESULT __stdcall zbar_samplegrabber_cb_BufferCB(ISampleGrabberCB* _This, double sampletime, BYTE* buffer, LONG bufferlen)
{
    if (!buffer || !bufferlen)
        return S_OK;


    zbar_samplegrabber_cb* This = (zbar_samplegrabber_cb*)_This;
    zbar_video_t* vdo = This->vdo;

    _zbar_mutex_lock(&vdo->qlock);

    zprintf(16, "got sample: %p (%ld), thr=%04lx\n", buffer, bufferlen, _zbar_thread_self());

    zbar_image_t* img = vdo->state->image;
    if (!img)
    {
        _zbar_mutex_lock(&vdo->qlock);
        img = video_dq_image(vdo);
        // note: video_dq_image() has unlocked the mutex
    }
    if (img)
    {
        zprintf(16, "copying into img: %p (srcidx: %d, data: %p, len: %ld)\n", img, img->srcidx, img->data, img->datalen);

        assert(img->datalen == bufferlen);
        memcpy((void*)img->data, buffer, img->datalen);
        vdo->state->image = img;
        SetEvent(vdo->state->captured);
    }

    _zbar_mutex_unlock(&vdo->qlock);

    return S_OK;
}


static ZTHREAD dshow_capture_thread(void* arg)
{
    zbar_video_t* vdo = arg;
    video_state_t* state = vdo->state;
    zbar_thread_t* thr = &state->thread;


    _zbar_mutex_lock(&vdo->qlock);

    _zbar_thread_init(thr);
    zprintf(4, "spawned dshow capture thread (thr=%04lx)\n",
            _zbar_thread_self());

    MSG msg;
    int rc = 0;
    while(thr->started && rc >= 0 && rc <= 1) {
        _zbar_mutex_unlock(&vdo->qlock);

        rc = MsgWaitForMultipleObjects(1, &thr->notify, 0,
                                       INFINITE, QS_ALLINPUT);
        if (rc == 1)
            while(PeekMessage(&msg, NULL, 0, 0, PM_NOYIELD | PM_REMOVE))
                if (rc > 0) {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }

        _zbar_mutex_lock(&vdo->qlock);
    }

//done:
    thr->running = 0;
    _zbar_event_trigger(&thr->activity);
    _zbar_mutex_unlock(&vdo->qlock);
    return 0;
}

static int dshow_nq(zbar_video_t* vdo, zbar_image_t* img)
{
    zprintf(16, "img: %p (srcidx: %d, data: %p, len: %ld), thr=%04lx\n", img, img->srcidx, img->data, img->datalen, _zbar_thread_self());
    return video_nq_image(vdo, img);
}

static zbar_image_t* dshow_dq(zbar_video_t* vdo)
{
    zbar_image_t* img = vdo->state->image;
    if (!img) {
        _zbar_mutex_unlock(&vdo->qlock);
        DWORD rc = WaitForSingleObject(vdo->state->captured, INFINITE);
        _zbar_mutex_lock(&vdo->qlock);
        if (rc == WAIT_OBJECT_0)
            img = vdo->state->image;
        else
            img = NULL;
        /*FIXME handle errors? */
    }
    else
        ResetEvent(vdo->state->captured);
    if (img)
        vdo->state->image = NULL;

    zprintf(16, "img: %p (srcidx: %d, data: %p, len: %ld), thr=%04lx\n", img, img ? img->srcidx : 0, img ? img->data : NULL, img ? img->datalen : 0, _zbar_thread_self());

    video_unlock(vdo);
    return img;
}

static int dshow_start(zbar_video_t* vdo)
{
    video_state_t* state = vdo->state;
    ResetEvent(state->captured);

    zprintf(16, "thr=%04lx\n", _zbar_thread_self());

    HRESULT hr = IMediaControl_Run(state->mediacontrol);
    CHECK_COM_ERROR(hr, "couldn't start video stream, hresult: 0x%lx\n", (void)0);
    if (FAILED(hr))
        return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                           "starting video stream"));
    return 0;
}

static int dshow_stop(zbar_video_t* vdo)
{
    video_state_t* state = vdo->state;

    zprintf(16, "thr=%04lx\n", _zbar_thread_self());

    HRESULT hr = IMediaControl_Stop(state->mediacontrol);
    CHECK_COM_ERROR(hr, "couldn't stop video stream, hresult: 0x%lx\n", (void)0);
    if (FAILED(hr))
        return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                           "stopping video stream"));

    _zbar_mutex_lock(&vdo->qlock);
    if (state->image)
        state->image = NULL;
    SetEvent(state->captured);
    _zbar_mutex_unlock(&vdo->qlock);
    return 0;
}

static int dshow_set_format (zbar_video_t* vdo,
                           uint32_t fmt)
{
    const zbar_format_def_t* fmtdef = _zbar_format_lookup(fmt);
    int fmt_ind = get_format_index(vdo->formats, fmt);
    if (!fmtdef->format || fmt_ind < 0)
        return(err_capture_int(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                               "unsupported vfw format: %x", fmt));

    video_state_t* state = vdo->state;
    int int_fmt = state->int_formats[fmt_ind];

    AM_MEDIA_TYPE* currentmt = NULL;
    HRESULT hr = IAMStreamConfig_GetFormat(state->streamconfig, &currentmt);
    CHECK_COM_ERROR(hr, "queried currentmt, hresult: 0x%lx\n", return -1);
    
    VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*) currentmt->pbFormat;
    BITMAPINFOHEADER* bih = &vih->bmiHeader;

    assert(bih->biSize == state->bi_size);

    // must set fmt in subtype guid, because dshow doesn't consider the BMIH
    make_fourcc_subtype(&currentmt->subtype, int_fmt);

    assert(bih);
    bih->biWidth = vdo->width;
    bih->biHeight = vdo->height;
    switch(fmtdef->group) {
    case ZBAR_FMT_GRAY:
        bih->biBitCount = 8;
        break;
    case ZBAR_FMT_YUV_PLANAR:
    case ZBAR_FMT_YUV_PACKED:
    case ZBAR_FMT_YUV_NV:
        bih->biBitCount = 8 + (16 >> (fmtdef->p.yuv.xsub2 + fmtdef->p.yuv.ysub2));
        break;
    case ZBAR_FMT_RGB_PACKED:
        bih->biBitCount = fmtdef->p.rgb.bpp * 8;
        break;
    default:
        bih->biBitCount = 0;
    }
    bih->biClrUsed = bih->biClrImportant = 0;
    bih->biCompression = int_fmt;

    zprintf(4, "setting format: %.4s(%08x) " BIH_FMT "\n",
            (char*)&int_fmt, int_fmt, BIH_FIELDS(bih));

    hr = IAMStreamConfig_SetFormat(state->streamconfig, currentmt);
    CHECK_COM_ERROR(hr, "setting dshow format failed, hresult: 0x%lx\n", goto cleanup)

    DeleteMediaType(currentmt);
    
    
    // re-read format, image data size might have changed
    
    currentmt = NULL;
    hr = IAMStreamConfig_GetFormat(state->streamconfig, &currentmt);
    CHECK_COM_ERROR(hr, "queried currentmt, hresult: 0x%lx\n", return -1);
    
    vih = (VIDEOINFOHEADER*) currentmt->pbFormat;
    bih = &vih->bmiHeader;
    if(!dshow_is_fourcc_guid(&currentmt->subtype)
       || bih->biCompression != int_fmt)
        return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                           "video format set ignored"));

    vdo->format = fmt;
    vdo->width = bih->biWidth;
    vdo->height = bih->biHeight;
    vdo->datalen = bih->biSizeImage;

    // datalen was set based on the internal format, but sometimes it's
    // different from the format reported to zbar processor
    if (vdo->formats[fmt_ind] != state->int_formats[fmt_ind]) {
        // See prepare_mjpg_format_mapping for possible differences
        // between internal and zbar format.
        vdo->datalen = vdo->width * vdo->height * 4; //BGR4
    }

cleanup:
    DeleteMediaType(currentmt);

    if (FAILED(hr))
        return(err_capture_int(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                               "setting dshow format failed (hresult=%lx)", hr));
    else
        return 0;
}

static int dshow_init (zbar_video_t* vdo,
                     uint32_t fmt)
{
    if (dshow_set_format(vdo, fmt))
        return -1;

    video_state_t* state = vdo->state;
    int fmt_ind = get_format_index(vdo->formats, fmt);

    // query stream parameters;
    REFERENCE_TIME avgtime_perframe = 0;
    
    AM_MEDIA_TYPE* currentmt;
    HRESULT hr = IAMStreamConfig_GetFormat(state->streamconfig, &currentmt);
    CHECK_COM_ERROR(hr, "querying current format failed, hresult: 0x%lx\n", goto init)

    // check whether we're dealing with a VIDEOINFOHEADER
    if (IsEqualGUID(&currentmt->formattype, &FORMAT_VideoInfo) && 
        currentmt->cbFormat >= sizeof(VIDEOINFOHEADER) && 
        currentmt->pbFormat)
    {
        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*) currentmt->pbFormat;
        avgtime_perframe = vih->AvgTimePerFrame;

        state->bi_size = vih->bmiHeader.biSize;
        state->bih = realloc(state->bih, state->bi_size);
        memcpy(state->bih, &vih->bmiHeader, state->bi_size);

        zprintf(3, "new format: %.4s(%08x) " BIH_FMT "\n",
                (char*)&fmt, fmt, BIH_FIELDS(&vih->bmiHeader));
    }
    else
    {
        LPOLESTR str = NULL;
        hr = StringFromCLSID(&currentmt->formattype, &str);
        CHECK_COM_ERROR(hr, "StringFromCLSID() failed (hresult=%lx)\n", (void)0)
        zwprintf(1, L"encountered unsupported video format type: %s\n", str);
        CoTaskMemFree(str);
    }
    DeleteMediaType(currentmt);


init:
    ;
    // install sample grabber callback
    ISampleGrabberCB* grabbercb = (ISampleGrabberCB*)new_zbar_samplegrabber_cb(vdo);
    hr = ISampleGrabber_SetCallback(vdo->state->samplegrabber, grabbercb, 1);
    ISampleGrabberCB_Release(grabbercb);
    if (FAILED(hr))
        return(err_capture(vdo, SEV_ERROR, ZBAR_ERR_BUSY, __func__,
                           "setting capture callbacks"));


    // special handling for MJPG streams:
    // we use the stock mjpeg decompressor filter
    if (state->int_formats[fmt_ind] == fourcc('M','J','P','G'))
    {
    // set up directshow graph
        IBaseFilter* mjpgdecompressor = NULL;
        hr = CoCreateInstance(&CLSID_MjpegDec, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void**)&mjpgdecompressor);
        CHECK_COM_ERROR(hr, "failed to create mjpeg decompressor filter (hresult=0x%lx)\n", return -1)

        // add mjpeg decompressor to graph
        hr = IGraphBuilder_AddFilter(state->graph, mjpgdecompressor, L"MJPEG decompressor");
        CHECK_COM_ERROR(hr, "adding MJPEG decompressor, hresult: 0x%lx\n", goto mjpg_cleanup)

        hr = ICaptureGraphBuilder2_RenderStream(state->builder, &PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, (IUnknown*)state->camera, NULL, mjpgdecompressor);
        CHECK_COM_ERROR(hr, "setting up capture graph 1, hresult: 0x%lx\n", goto mjpg_cleanup)
        hr = ICaptureGraphBuilder2_RenderStream(state->builder, NULL, &MEDIATYPE_Video, (IUnknown*)mjpgdecompressor, state->grabberbase, state->nullrenderer);
        CHECK_COM_ERROR(hr, "setting up capture graph 2, hresult: 0x%lx\n", goto mjpg_cleanup)

        // set input information for zbar.
        // 
        // note: the mjpeg decompressor outputs MediaSubtype_RGB32 by default,
        // its memory layout is BGR (see http://msdn.microsoft.com/en-us/library/windows/desktop/dd407253(v=vs.85).aspx);
        // 
        // TODO: on my dell latitude e6520 laptop the decompressor produces a bottom-up image, 
        // which zbar doesn't seem to recognize, so the images are displayed upside-down.
        // changing the format to MediaSubtype_RGB24 or MediaSubtype_RGB565 doesn't help either
        vdo->format = fourcc('B','G','R','4');
        vdo->datalen = vdo->width * vdo->height * 4;

mjpg_cleanup:
        IBaseFilter_Release(mjpgdecompressor);
        if (FAILED(hr))
            return -1;
    }
    else
    {
        hr = ICaptureGraphBuilder2_RenderStream(state->builder, NULL, &MEDIATYPE_Video, (IUnknown*)state->camera, state->grabberbase, state->nullrenderer);
        CHECK_COM_ERROR(hr, "setting up capture graph, hresult: 0x%lx\n", return -1)
    }

    // directshow keeps ownership of the image passed to the callback,
    // hence, keeping a pointer to the original data (iomode VIDEO_MMAP) is a bad idea
    // in a multi-threaded environment.
    // real case szenario on a windows 7 tablet with intel atom:
    // with vdo->iomode=VIDEO_MMAP and vdo->num_images=1:
    // 1) directshow graph provides a sample, sets the data pointer of vdo->state->img
    // 2) dshow_dq (called from proc_video_thread()/zbar_video_next_image()) fetches the image and makes a copy of it
    // 3) directshow graph provides the next sample, sets the data pointer of vdo->state->img
    // 4) dshow_nq (called from ) resets the data pointer of vdo->state->img (nullptr)
    // 5) dshow_dq returns vdo->state->img without data (nullptr)
    // 
    // now, we could deal with this special case, but zbar_video_next_image() makes a copy of the sample anyway when 
    // vdo->num_images==1 (thus VIDEO_MMAP won't save us anything); therefore rather use image buffers provided 
    // by zbar (see video_init_images())
    vdo->iomode = VIDEO_USERPTR;
    // keep zbar's default
    //vdo->num_images = ZBAR_VIDEO_IMAGES_MAX;

    zprintf(3, "initialized video capture: %x format, %"PRId64" frames/s\n",
            fmt, _100ns_unit / avgtime_perframe);


    return 0;
}

static int dshow_cleanup (zbar_video_t* vdo)
{
    zprintf(16, "thr=%04lx\n", _zbar_thread_self());

    /* close open device */
    video_state_t* state = vdo->state;

    _zbar_thread_stop(&state->thread, &vdo->qlock);

    dshow_destroy_video_state_t(state);
    free(state);
    vdo->state = NULL;

    CoUninitialize();

    return 0;
}

static int dshow_probe(zbar_video_t* vdo)
{
    video_state_t* state = vdo->state;
    HRESULT hr;


    // collect formats
    int resolutions, caps_size;
    hr = IAMStreamConfig_GetNumberOfCapabilities(state->streamconfig, &resolutions, &caps_size);
    CHECK_COM_ERROR(hr, "couldn't query camera capabilities (hresult=%lx)\n", return -1)
    zprintf(6, "number of supported formats/resolutions as reported by directshow: %d\n", resolutions);

    vdo->formats = calloc(resolutions+1, sizeof(uint32_t));
    state->int_formats = calloc(resolutions+1, sizeof(uint32_t));

    // this is actually a VIDEO_STREAM_CONFIG_CAPS structure, which is mostly deprecated anyway,
    // so we just reserve enough buffer but treat it as opaque otherwise
    BYTE* caps = malloc(caps_size);
    int n = 0, i;
    for (i = 0; i < resolutions; ++i)
    {
        AM_MEDIA_TYPE* mt;
        HRESULT hr = IAMStreamConfig_GetStreamCaps(state->streamconfig, i, &mt, caps);
        CHECK_COM_ERROR(hr, "querying stream capability failed, hresult: 0x%lx\n", continue)

        if (IsEqualGUID(&mt->formattype, &FORMAT_VideoInfo) && 
            mt->cbFormat >= sizeof(VIDEOINFOHEADER) && 
            mt->pbFormat)
        {
            VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*) mt->pbFormat;
            zprintf(6, "supported format/resolution: " BIH_FMT "\n", BIH_FIELDS(&vih->bmiHeader));
            if (dshow_is_fourcc_guid(&mt->subtype))
            {
                // first search for existing fourcc format
                int i;
                for (i = 0; i < n; ++i)
                {
                    if (state->int_formats[i] == vih->bmiHeader.biCompression)
                        break;
                }
                // push back if not found
                if (i == n)
                {
                    state->int_formats[n] = vih->bmiHeader.biCompression;
                    vdo->formats[n] = vih->bmiHeader.biCompression;
                    n++;
                }
            }
        }
        // note: other format types could be possible, e.g. VIDEOINFOHEADER2...

        DeleteMediaType(mt);
    }
    free(caps);

    zprintf(6, "number of supported fourcc formats: %d\n", n);

    vdo->formats = realloc(vdo->formats, (n + 1) * sizeof(uint32_t));
    state->int_formats = realloc(state->int_formats,
                                 (n + 1) * sizeof(uint32_t));
    prepare_mjpg_format_mapping(vdo);
    dump_formats(vdo);

    if (n == 0)
        return -1;


    // query current format,
    // set requested size
    BITMAPINFOHEADER* bih = NULL;

    AM_MEDIA_TYPE* currentmt;
    hr = IAMStreamConfig_GetFormat(state->streamconfig, &currentmt);
    CHECK_COM_ERROR(hr, "couldn't query media type from samplegrabber (hresult=%lx)\n", return -1)

    // query initial video format and size;
    // set requested video size
    if (IsEqualGUID(&currentmt->formattype, &FORMAT_VideoInfo) && 
        currentmt->cbFormat >= sizeof(VIDEOINFOHEADER) && 
        currentmt->pbFormat)
    {
        VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*) currentmt->pbFormat;
        zprintf(3, "initial format: " BIH_FMT " (bisz=%lx)\n",
                BIH_FIELDS(&vih->bmiHeader), vih->bmiHeader.biSize);

        if (dshow_is_fourcc_guid(&currentmt->subtype))
        {
            state->bi_size = vih->bmiHeader.biSize;
            bih = state->bih = realloc(state->bih, state->bi_size);
            memcpy(bih, &vih->bmiHeader, state->bi_size);

            // set requested video size
            if (vdo->width  &&  vdo->height)
            {
                zprintf(3, "set requested video size %dx%d\n", vdo->width, vdo->height);

                vih->bmiHeader.biWidth = vdo->width;
                vih->bmiHeader.biHeight = vdo->height;
                hr = IAMStreamConfig_SetFormat(state->streamconfig, currentmt);
                CHECK_COM_ERROR(hr, "couldn't set requested video size (hresult=%lx)\n", (void)0)
                if (SUCCEEDED(hr))
                    memcpy(bih, &vih->bmiHeader, state->bi_size);
            }
        }
        else
            zprintf(3, "unsupported initial format (no fourcc code)\n");
    }
    else
    {
        LPOLESTR str = NULL;
        hr = StringFromCLSID(&currentmt->formattype, &str);
        CHECK_COM_ERROR(hr, "StringFromCLSID() failed (hresult=%lx)\n", (void)0)
        zwprintf(1, L"encountered unsupported video format type: %s\n", str);
        CoTaskMemFree(str);
    }

    DeleteMediaType(currentmt);

    if (!bih)
        return -1;


    // no matter the size requested, we always set what the source filter defaults to
    vdo->width = bih->biWidth;
    vdo->height = bih->biHeight;
    vdo->datalen = bih->biSizeImage;
    vdo->intf = VIDEO_DSHOW;
    vdo->init = dshow_init;
    vdo->start = dshow_start;
    vdo->stop = dshow_stop;
    vdo->cleanup = dshow_cleanup;
    vdo->nq = dshow_nq;
    vdo->dq = dshow_dq;

    return 0;
}

// search camera by index or by device path
static IBaseFilter* dshow_search_camera(const char* dev)
{
    IBaseFilter* camera = NULL;
    ICreateDevEnum* devenumerator = NULL;
    IEnumMoniker* enummoniker = NULL;

    int reqid = -1;
    if ((!strncmp(dev, "/dev/video", 10) ||
        !strncmp(dev, "\\dev\\video", 10)) &&
       dev[10] >= '0' && dev[10] <= '9' && !dev[11])
        reqid = dev[10] - '0';
    else if (strlen(dev) == 1 &&
            dev[0] >= '0' && dev[0] <= '9')
        reqid = dev[0] - '0';

    zprintf(6, "searching for camera #%d: %s\n", reqid, dev);

    HRESULT hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, &IID_ICreateDevEnum, (void**)&devenumerator);
    CHECK_COM_ERROR(hr, "failed to create system device enumerator (hresult=0x%lx)\n", goto done)

    hr = ICreateDevEnum_CreateClassEnumerator(devenumerator, &CLSID_VideoInputDeviceCategory, &enummoniker, 0);
    CHECK_COM_ERROR(hr, "failed to create enumerator moniker (hresult=0x%lx)\n", goto done)

    if (hr != S_OK)
    {
        zprintf(6, "no video devices available");
        goto done;
    }


    // turn device name (the GUID) from char to wide char
    BSTR wdev = SysAllocStringLen(NULL, strlen(dev));
    if (!wdev)
        goto done;
    MultiByteToWideChar(CP_UTF8, 0, dev, -1, wdev, strlen(dev) + 1);

    // Go through and find capture device
    int docontinue;
    int devid;
    for (devid = 0, docontinue = 1; docontinue; ++devid)
    {
        IMoniker* moniker = NULL;
        IPropertyBag* propbag = NULL;
        VARIANT devpath_variant;
        VARIANT friendlyname_variant;

        hr = IEnumMoniker_Next(enummoniker, 1, &moniker, NULL);
        // end of monikers
        if (hr != S_OK)
            break;

        VariantInit(&devpath_variant);
        VariantInit(&friendlyname_variant);

        hr = IMoniker_BindToStorage(moniker, NULL, NULL, &IID_IPropertyBag, (void**)&propbag);
        CHECK_COM_ERROR(hr, "failed to get property bag from moniker, hresult: 0x%lx\n", goto breakout)

        hr = IPropertyBag_Read(propbag, L"DevicePath", &devpath_variant, NULL);
        CHECK_COM_ERROR(hr, "failed to read DevicePath from camera device, hresult: 0x%lx\n", goto breakout)
        hr = IPropertyBag_Read(propbag, L"FriendlyName", &friendlyname_variant, NULL);
        CHECK_COM_ERROR(hr, "failed to read FriendlyName from camera device, hresult: 0x%lx\n", goto breakout)

        if ((reqid >= 0)
           ? devid == reqid
           : !wcscmp(wdev, V_BSTR(&devpath_variant)))
        {
            // create camera from moniker
            hr = IMoniker_BindToObject(moniker, NULL, NULL, &IID_IBaseFilter, (void**)&camera);
            CHECK_COM_ERROR(hr, "failed to get camera device, hresult: 0x%lx\n", goto breakout)
        }

        if (camera)
        {
            zwprintf(1, L"using camera #%d: %s (%s)\n", devid, V_BSTR(&friendlyname_variant), V_BSTR(&devpath_variant));
            goto breakout;
        }
        else
            goto cleanup;


breakout:
        docontinue = 0;
cleanup:
        VariantClear(&friendlyname_variant);
        VariantClear(&devpath_variant);
        COM_SAFE_RELEASE(&propbag);
        COM_SAFE_RELEASE(&moniker);
    }
    SysFreeString(wdev);

done:
    COM_SAFE_RELEASE(&enummoniker);
    COM_SAFE_RELEASE(&devenumerator);

    if (!camera)
        zprintf(6, "no camera found\n");
    
    return camera;
}

int _zbar_video_open (zbar_video_t* vdo, const char* dev)
{
    // assume failure
    int ret = -1;


    video_state_t* state;
    state = vdo->state = calloc(1, sizeof(video_state_t));

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        goto done;
    }


    state->camera = dshow_search_camera(dev);
    if (!state->camera)
        goto done;


    if (!state->captured)
        state->captured = CreateEvent(NULL, 0, 0, NULL);
    else
        ResetEvent(state->captured);

    if (_zbar_thread_start(&state->thread, dshow_capture_thread, vdo, NULL))
        return -1;


    // create filter graph instance
    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (void**)&state->graph);
    CHECK_COM_ERROR(hr, "graph builder creation, hresult: 0x%lx\n", goto done)
    
    // query media control from filter graph
    hr = IGraphBuilder_QueryInterface(state->graph, &IID_IMediaControl, (void**)&state->mediacontrol);
    CHECK_COM_ERROR(hr, "querying media control, hresult: 0x%lx\n", goto done)

    // create sample grabber instance
    hr = CoCreateInstance(&CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, &IID_ISampleGrabber, (void**)&state->samplegrabber);
    CHECK_COM_ERROR(hr, "samplegrabber creation, hresult: 0x%lx\n", goto done)

    // query base filter interface from sample grabber
    hr = ISampleGrabber_QueryInterface(state->samplegrabber, &IID_IBaseFilter, (void**)&state->grabberbase);
    CHECK_COM_ERROR(hr, "grabberbase query, hresult: 0x%lx\n", goto done)

    // capture graph without preview window
    hr = CoCreateInstance(&CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void**)&state->nullrenderer);
    CHECK_COM_ERROR(hr, "null renderer creation, hresult: 0x%lx\n", goto done)


    // add camera to graph
    hr = IGraphBuilder_AddFilter(state->graph, state->camera, L"Camera");
    CHECK_COM_ERROR(hr, "adding camera, hresult: 0x%lx\n", goto done)

    // add sample grabber to graph
    hr = IGraphBuilder_AddFilter(state->graph, state->grabberbase, L"Sample Grabber");
    CHECK_COM_ERROR(hr, "adding samplegrabber, hresult: 0x%lx\n", goto done)

    // add nullrenderer to graph
    hr = IGraphBuilder_AddFilter(state->graph, state->nullrenderer, L"Null Renderer");
    CHECK_COM_ERROR(hr, "adding null renderer, hresult: 0x%lx\n", goto done)


    // Create the Capture Graph Builder.
    hr = CoCreateInstance(&CLSID_CaptureGraphBuilder2, NULL, CLSCTX_INPROC_SERVER, &IID_ICaptureGraphBuilder2, (void**)&state->builder);
    CHECK_COM_ERROR(hr, "capturegraph builder creation, hresult: 0x%lx\n", goto done)
 
    // tell graph builder about the filter graph
    hr = ICaptureGraphBuilder2_SetFiltergraph(state->builder, state->graph);
    CHECK_COM_ERROR(hr, "setting filtergraph, hresult: 0x%lx\n", goto done)
    

    // TODO: 
    // finding the streamconfig interface on the camera's preview output pin (specifying PIN_CATEGORY_PREVIEW, MEDIATYPE_Video)
    // should work according to the documentation but it doesn't.
    // Because devices may have separate pins for capture and preview or have a video port pin (PIN_CATEGORY_VIDEOPORT)
    // instead of a preview pin, I do hope that we get the streamconfig interface for the correct pin
    hr = ICaptureGraphBuilder2_FindInterface(state->builder, &LOOK_DOWNSTREAM_ONLY, NULL, state->camera, &IID_IAMStreamConfig, (void**)&state->streamconfig);
    CHECK_COM_ERROR(hr, "querying streamconfig interface, hresult: 0x%lx\n", goto done)


    if (dshow_probe(vdo))
        goto done;


    // success
    ret = 0;

done:
    if (ret)
    {
        if (state->camera)
            _zbar_thread_stop(&state->thread, NULL);
        dshow_destroy_video_state_t(state);
        free(state);
        vdo->state = NULL;
        CoUninitialize();
        
        return err_capture_str(vdo, SEV_ERROR, ZBAR_ERR_INVALID, __func__,
                               "failed to connect to camera '%s'", dev);
    }

    return ret;
}

// jedit file properties :noTabs=true:tabSize=4:indentSize=4: