/*
 * Copyright 2020 Nikolay Sivov for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include "mf_private.h"
#include "uuids.h"
#include "evr.h"

WINE_DEFAULT_DEBUG_CHANNEL(mfplat);

enum video_renderer_flags
{
    EVR_SHUT_DOWN = 0x1,
};

struct video_renderer
{
    IMFMediaSink IMFMediaSink_iface;
    IMFMediaSinkPreroll IMFMediaSinkPreroll_iface;
    IMFVideoRenderer IMFVideoRenderer_iface;
    IMFMediaEventGenerator IMFMediaEventGenerator_iface;
    LONG refcount;

    IMFMediaEventQueue *event_queue;
    IMFTransform *mixer;
    IMFVideoPresenter *presenter;
    unsigned int flags;
    CRITICAL_SECTION cs;
};

static struct video_renderer *impl_from_IMFMediaSink(IMFMediaSink *iface)
{
    return CONTAINING_RECORD(iface, struct video_renderer, IMFMediaSink_iface);
}

static struct video_renderer *impl_from_IMFMediaSinkPreroll(IMFMediaSinkPreroll *iface)
{
    return CONTAINING_RECORD(iface, struct video_renderer, IMFMediaSinkPreroll_iface);
}

static struct video_renderer *impl_from_IMFVideoRenderer(IMFVideoRenderer *iface)
{
    return CONTAINING_RECORD(iface, struct video_renderer, IMFVideoRenderer_iface);
}

static struct video_renderer *impl_from_IMFMediaEventGenerator(IMFMediaEventGenerator *iface)
{
    return CONTAINING_RECORD(iface, struct video_renderer, IMFMediaEventGenerator_iface);
}

static HRESULT WINAPI video_renderer_sink_QueryInterface(IMFMediaSink *iface, REFIID riid, void **obj)
{
    struct video_renderer *renderer = impl_from_IMFMediaSink(iface);

    TRACE("%p, %s, %p.\n", iface, debugstr_guid(riid), obj);

    if (IsEqualIID(riid, &IID_IMFMediaSink) ||
            IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = &renderer->IMFMediaSink_iface;
    }
    else if (IsEqualIID(riid, &IID_IMFMediaSinkPreroll))
    {
        *obj = &renderer->IMFMediaSinkPreroll_iface;
    }
    else if (IsEqualIID(riid, &IID_IMFVideoRenderer))
    {
        *obj = &renderer->IMFVideoRenderer_iface;
    }
    else if (IsEqualIID(riid, &IID_IMFMediaEventGenerator))
    {
        *obj = &renderer->IMFMediaEventGenerator_iface;
    }
    else
    {
        WARN("Unsupported interface %s.\n", debugstr_guid(riid));
        *obj = NULL;
        return E_NOINTERFACE;
    }

    IUnknown_AddRef((IUnknown *)*obj);

    return S_OK;
}

static ULONG WINAPI video_renderer_sink_AddRef(IMFMediaSink *iface)
{
    struct video_renderer *renderer = impl_from_IMFMediaSink(iface);
    ULONG refcount = InterlockedIncrement(&renderer->refcount);
    TRACE("%p, refcount %u.\n", iface, refcount);
    return refcount;
}

static ULONG WINAPI video_renderer_sink_Release(IMFMediaSink *iface)
{
    struct video_renderer *renderer = impl_from_IMFMediaSink(iface);
    ULONG refcount = InterlockedDecrement(&renderer->refcount);

    TRACE("%p, refcount %u.\n", iface, refcount);

    if (!refcount)
    {
        if (renderer->event_queue)
            IMFMediaEventQueue_Release(renderer->event_queue);
        if (renderer->mixer)
            IMFTransform_Release(renderer->mixer);
        if (renderer->presenter)
            IMFVideoPresenter_Release(renderer->presenter);
        DeleteCriticalSection(&renderer->cs);
        heap_free(renderer);
    }

    return refcount;
}

static HRESULT WINAPI video_renderer_sink_GetCharacteristics(IMFMediaSink *iface, DWORD *flags)
{
    struct video_renderer *renderer = impl_from_IMFMediaSink(iface);

    TRACE("%p, %p.\n", iface, flags);

    if (renderer->flags & EVR_SHUT_DOWN)
        return MF_E_SHUTDOWN;

    *flags = MEDIASINK_CLOCK_REQUIRED | MEDIASINK_CAN_PREROLL;

    return S_OK;
}

static HRESULT WINAPI video_renderer_sink_AddStreamSink(IMFMediaSink *iface, DWORD stream_sink_id,
    IMFMediaType *media_type, IMFStreamSink **stream_sink)
{
    FIXME("%p, %#x, %p, %p.\n", iface, stream_sink_id, media_type, stream_sink);

    return E_NOTIMPL;
}

static HRESULT WINAPI video_renderer_sink_RemoveStreamSink(IMFMediaSink *iface, DWORD stream_sink_id)
{
    FIXME("%p, %#x.\n", iface, stream_sink_id);

    return E_NOTIMPL;
}

static HRESULT WINAPI video_renderer_sink_GetStreamSinkCount(IMFMediaSink *iface, DWORD *count)
{
    FIXME("%p, %p.\n", iface, count);

    return E_NOTIMPL;
}

static HRESULT WINAPI video_renderer_sink_GetStreamSinkByIndex(IMFMediaSink *iface, DWORD index,
        IMFStreamSink **stream)
{
    FIXME("%p, %u, %p.\n", iface, index, stream);

    return E_NOTIMPL;
}

static HRESULT WINAPI video_renderer_sink_GetStreamSinkById(IMFMediaSink *iface, DWORD stream_sink_id,
        IMFStreamSink **stream)
{
    FIXME("%p, %#x, %p.\n", iface, stream_sink_id, stream);

    return E_NOTIMPL;
}

static HRESULT WINAPI video_renderer_sink_SetPresentationClock(IMFMediaSink *iface, IMFPresentationClock *clock)
{
    FIXME("%p, %p.\n", iface, clock);

    return E_NOTIMPL;
}

static HRESULT WINAPI video_renderer_sink_GetPresentationClock(IMFMediaSink *iface, IMFPresentationClock **clock)
{
    FIXME("%p, %p.\n", iface, clock);

    return E_NOTIMPL;
}

static HRESULT WINAPI video_renderer_sink_Shutdown(IMFMediaSink *iface)
{
    struct video_renderer *renderer = impl_from_IMFMediaSink(iface);

    TRACE("%p.\n", iface);

    if (renderer->flags & EVR_SHUT_DOWN)
        return MF_E_SHUTDOWN;

    EnterCriticalSection(&renderer->cs);
    renderer->flags |= EVR_SHUT_DOWN;
    IMFMediaEventQueue_Shutdown(renderer->event_queue);
    LeaveCriticalSection(&renderer->cs);

    return S_OK;
}

static const IMFMediaSinkVtbl video_renderer_sink_vtbl =
{
    video_renderer_sink_QueryInterface,
    video_renderer_sink_AddRef,
    video_renderer_sink_Release,
    video_renderer_sink_GetCharacteristics,
    video_renderer_sink_AddStreamSink,
    video_renderer_sink_RemoveStreamSink,
    video_renderer_sink_GetStreamSinkCount,
    video_renderer_sink_GetStreamSinkByIndex,
    video_renderer_sink_GetStreamSinkById,
    video_renderer_sink_SetPresentationClock,
    video_renderer_sink_GetPresentationClock,
    video_renderer_sink_Shutdown,
};

static HRESULT WINAPI video_renderer_preroll_QueryInterface(IMFMediaSinkPreroll *iface, REFIID riid, void **obj)
{
    struct video_renderer *renderer = impl_from_IMFMediaSinkPreroll(iface);
    return IMFMediaSink_QueryInterface(&renderer->IMFMediaSink_iface, riid, obj);
}

static ULONG WINAPI video_renderer_preroll_AddRef(IMFMediaSinkPreroll *iface)
{
    struct video_renderer *renderer = impl_from_IMFMediaSinkPreroll(iface);
    return IMFMediaSink_AddRef(&renderer->IMFMediaSink_iface);
}

static ULONG WINAPI video_renderer_preroll_Release(IMFMediaSinkPreroll *iface)
{
    struct video_renderer *renderer = impl_from_IMFMediaSinkPreroll(iface);
    return IMFMediaSink_Release(&renderer->IMFMediaSink_iface);
}

static HRESULT WINAPI video_renderer_preroll_NotifyPreroll(IMFMediaSinkPreroll *iface, MFTIME start_time)
{
    FIXME("%p, %s.\n", iface, debugstr_time(start_time));

    return E_NOTIMPL;
}

static const IMFMediaSinkPrerollVtbl video_renderer_preroll_vtbl =
{
    video_renderer_preroll_QueryInterface,
    video_renderer_preroll_AddRef,
    video_renderer_preroll_Release,
    video_renderer_preroll_NotifyPreroll,
};

static HRESULT WINAPI video_renderer_QueryInterface(IMFVideoRenderer *iface, REFIID riid, void **obj)
{
    struct video_renderer *renderer = impl_from_IMFVideoRenderer(iface);
    return IMFMediaSink_QueryInterface(&renderer->IMFMediaSink_iface, riid, obj);
}

static ULONG WINAPI video_renderer_AddRef(IMFVideoRenderer *iface)
{
    struct video_renderer *renderer = impl_from_IMFVideoRenderer(iface);
    return IMFMediaSink_AddRef(&renderer->IMFMediaSink_iface);
}

static ULONG WINAPI video_renderer_Release(IMFVideoRenderer *iface)
{
    struct video_renderer *renderer = impl_from_IMFVideoRenderer(iface);
    return IMFMediaSink_Release(&renderer->IMFMediaSink_iface);
}

static HRESULT WINAPI video_renderer_InitializeRenderer(IMFVideoRenderer *iface, IMFTransform *mixer,
        IMFVideoPresenter *presenter)
{
    FIXME("%p, %p, %p.\n", iface, mixer, presenter);

    return E_NOTIMPL;
}

static const IMFVideoRendererVtbl video_renderer_vtbl =
{
    video_renderer_QueryInterface,
    video_renderer_AddRef,
    video_renderer_Release,
    video_renderer_InitializeRenderer,
};

static HRESULT WINAPI video_renderer_events_QueryInterface(IMFMediaEventGenerator *iface, REFIID riid, void **obj)
{
    struct video_renderer *renderer = impl_from_IMFMediaEventGenerator(iface);
    return IMFMediaSink_QueryInterface(&renderer->IMFMediaSink_iface, riid, obj);
}

static ULONG WINAPI video_renderer_events_AddRef(IMFMediaEventGenerator *iface)
{
    struct video_renderer *renderer = impl_from_IMFMediaEventGenerator(iface);
    return IMFMediaSink_AddRef(&renderer->IMFMediaSink_iface);
}

static ULONG WINAPI video_renderer_events_Release(IMFMediaEventGenerator *iface)
{
    struct video_renderer *renderer = impl_from_IMFMediaEventGenerator(iface);
    return IMFMediaSink_Release(&renderer->IMFMediaSink_iface);
}

static HRESULT WINAPI video_renderer_events_GetEvent(IMFMediaEventGenerator *iface, DWORD flags, IMFMediaEvent **event)
{
    struct video_renderer *renderer = impl_from_IMFMediaEventGenerator(iface);

    TRACE("%p, %#x, %p.\n", iface, flags, event);

    return IMFMediaEventQueue_GetEvent(renderer->event_queue, flags, event);
}

static HRESULT WINAPI video_renderer_events_BeginGetEvent(IMFMediaEventGenerator *iface, IMFAsyncCallback *callback,
        IUnknown *state)
{
    struct video_renderer *renderer = impl_from_IMFMediaEventGenerator(iface);

    TRACE("%p, %p, %p.\n", iface, callback, state);

    return IMFMediaEventQueue_BeginGetEvent(renderer->event_queue, callback, state);
}

static HRESULT WINAPI video_renderer_events_EndGetEvent(IMFMediaEventGenerator *iface, IMFAsyncResult *result,
        IMFMediaEvent **event)
{
    struct video_renderer *renderer = impl_from_IMFMediaEventGenerator(iface);

    TRACE("%p, %p, %p.\n", iface, result, event);

    return IMFMediaEventQueue_EndGetEvent(renderer->event_queue, result, event);
}

static HRESULT WINAPI video_renderer_events_QueueEvent(IMFMediaEventGenerator *iface, MediaEventType event_type,
        REFGUID ext_type, HRESULT hr, const PROPVARIANT *value)
{
    struct video_renderer *renderer = impl_from_IMFMediaEventGenerator(iface);

    TRACE("%p, %u, %s, %#x, %p.\n", iface, event_type, debugstr_guid(ext_type), hr, value);

    return IMFMediaEventQueue_QueueEventParamVar(renderer->event_queue, event_type, ext_type, hr, value);
}

static const IMFMediaEventGeneratorVtbl video_renderer_events_vtbl =
{
    video_renderer_events_QueryInterface,
    video_renderer_events_AddRef,
    video_renderer_events_Release,
    video_renderer_events_GetEvent,
    video_renderer_events_BeginGetEvent,
    video_renderer_events_EndGetEvent,
    video_renderer_events_QueueEvent,
};

static HRESULT video_renderer_create_mixer(IMFAttributes *attributes, IMFTransform **out)
{
    unsigned int flags = 0;
    IMFActivate *activate;
    CLSID clsid;
    HRESULT hr;

    if (SUCCEEDED(IMFAttributes_GetUnknown(attributes, &MF_ACTIVATE_CUSTOM_VIDEO_MIXER_ACTIVATE,
            &IID_IMFActivate, (void **)&activate)))
    {
        IMFAttributes_GetUINT32(attributes, &MF_ACTIVATE_CUSTOM_VIDEO_MIXER_FLAGS, &flags);
        hr = IMFActivate_ActivateObject(activate, &IID_IMFTransform, (void **)out);
        IMFActivate_Release(activate);
        if (FAILED(hr) && !(flags & MF_ACTIVATE_CUSTOM_MIXER_ALLOWFAIL))
            return hr;
    }

    if (FAILED(IMFAttributes_GetGUID(attributes, &MF_ACTIVATE_CUSTOM_VIDEO_MIXER_CLSID, &clsid)))
        memcpy(&clsid, &CLSID_MFVideoMixer9, sizeof(clsid));

    return CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IMFTransform, (void **)out);
}

static HRESULT video_renderer_create_presenter(IMFAttributes *attributes, IMFVideoPresenter **out)
{
    unsigned int flags = 0;
    IMFActivate *activate;
    CLSID clsid;
    HRESULT hr;

    if (SUCCEEDED(IMFAttributes_GetUnknown(attributes, &MF_ACTIVATE_CUSTOM_VIDEO_PRESENTER_ACTIVATE,
            &IID_IMFActivate, (void **)&activate)))
    {
        IMFAttributes_GetUINT32(attributes, &MF_ACTIVATE_CUSTOM_VIDEO_PRESENTER_FLAGS, &flags);
        hr = IMFActivate_ActivateObject(activate, &IID_IMFVideoPresenter, (void **)out);
        IMFActivate_Release(activate);
        if (FAILED(hr) && !(flags & MF_ACTIVATE_CUSTOM_PRESENTER_ALLOWFAIL))
            return hr;
    }

    if (FAILED(IMFAttributes_GetGUID(attributes, &MF_ACTIVATE_CUSTOM_VIDEO_PRESENTER_CLSID, &clsid)))
        memcpy(&clsid, &CLSID_MFVideoPresenter9, sizeof(clsid));

    return CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IMFVideoPresenter, (void **)out);
}

static HRESULT evr_create_object(IMFAttributes *attributes, void *user_context, IUnknown **obj)
{
    struct video_renderer *object;
    HRESULT hr;

    TRACE("%p, %p, %p.\n", attributes, user_context, obj);

    if (!(object = heap_alloc_zero(sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IMFMediaSink_iface.lpVtbl = &video_renderer_sink_vtbl;
    object->IMFMediaSinkPreroll_iface.lpVtbl = &video_renderer_preroll_vtbl;
    object->IMFVideoRenderer_iface.lpVtbl = &video_renderer_vtbl;
    object->IMFMediaEventGenerator_iface.lpVtbl = &video_renderer_events_vtbl;
    object->refcount = 1;
    InitializeCriticalSection(&object->cs);

    if (FAILED(hr = MFCreateEventQueue(&object->event_queue)))
        goto failed;

    /* Create mixer and presenter. */
    if (FAILED(hr = video_renderer_create_mixer(attributes, &object->mixer)))
        goto failed;

    if (FAILED(hr = video_renderer_create_presenter(attributes, &object->presenter)))
        goto failed;

    *obj = (IUnknown *)&object->IMFMediaSink_iface;

    return S_OK;

failed:

    IMFMediaSink_Release(&object->IMFMediaSink_iface);

    return hr;
}

static void evr_shutdown_object(void *user_context, IUnknown *obj)
{
    IMFMediaSink *sink;

    if (SUCCEEDED(IUnknown_QueryInterface(obj, &IID_IMFMediaSink, (void **)&sink)))
    {
        IMFMediaSink_Shutdown(sink);
        IMFMediaSink_Release(sink);
    }
}

static const struct activate_funcs evr_activate_funcs =
{
    .create_object = evr_create_object,
    .shutdown_object = evr_shutdown_object,
};

/***********************************************************************
 *      MFCreateVideoRendererActivate (mf.@)
 */
HRESULT WINAPI MFCreateVideoRendererActivate(HWND hwnd, IMFActivate **activate)
{
    HRESULT hr;

    TRACE("%p, %p.\n", hwnd, activate);

    if (!activate)
        return E_POINTER;

    hr = create_activation_object(hwnd, &evr_activate_funcs, activate);
    if (SUCCEEDED(hr))
        IMFActivate_SetUINT64(*activate, &MF_ACTIVATE_VIDEO_WINDOW, (ULONG_PTR)hwnd);

    return hr;
}
