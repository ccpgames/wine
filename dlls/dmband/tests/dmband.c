/*
 * Unit tests for dmband functions
 *
 * Copyright (C) 2012 Christian Costa
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

#include <stdio.h>

#include "wine/test.h"
#include "uuids.h"
#include "ole2.h"
#include "initguid.h"
#include "dmusici.h"
#include "dmusicf.h"
#include "dmplugin.h"

DEFINE_GUID(IID_IDirectMusicBandTrackPrivate, 0x53466056, 0x6dc4, 0x11d1, 0xbf, 0x7b, 0x00, 0xc0, 0x4f, 0xbf, 0x8f, 0xef);

static BOOL missing_dmband(void)
{
    IDirectMusicBand *dmb;
    HRESULT hr = CoCreateInstance(&CLSID_DirectMusicBand, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicBand, (void**)&dmb);

    if (hr == S_OK && dmb)
    {
        IDirectMusicBand_Release(dmb);
        return FALSE;
    }
    return TRUE;
}

static void test_COM(void)
{
    IDirectMusicBand *dmb = (IDirectMusicBand*)0xdeadbeef;
    IDirectMusicObject *dmo;
    IPersistStream *ps;
    IUnknown *unk;
    ULONG refcount;
    HRESULT hr;

    /* COM aggregation */
    hr = CoCreateInstance(&CLSID_DirectMusicBand, (IUnknown *)0xdeadbeef, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void**)&dmb);
    ok(hr == CLASS_E_NOAGGREGATION,
            "DirectMusicBand create failed: %08x, expected CLASS_E_NOAGGREGATION\n", hr);
    ok(!dmb, "dmb = %p\n", dmb);

    /* Invalid RIID */
    hr = CoCreateInstance(&CLSID_DirectMusicBand, NULL, CLSCTX_INPROC_SERVER, &IID_IClassFactory,
            (void**)&dmb);
    ok(hr == E_NOINTERFACE, "DirectMusicBand create failed: %08x, expected E_NOINTERFACE\n", hr);

    /* Same refcount for all DirectMusicBand interfaces */
    hr = CoCreateInstance(&CLSID_DirectMusicBand, NULL, CLSCTX_INPROC_SERVER, &IID_IDirectMusicBand,
            (void**)&dmb);
    ok(hr == S_OK, "DirectMusicBand create failed: %08x, expected S_OK\n", hr);
    refcount = IDirectMusicBand_AddRef(dmb);
    ok(refcount == 2, "refcount == %u, expected 2\n", refcount);

    hr = IDirectMusicBand_QueryInterface(dmb, &IID_IDirectMusicObject, (void**)&dmo);
    ok(hr == S_OK, "QueryInterface for IID_IDirectMusicObject failed: %08x\n", hr);
    refcount = IDirectMusicObject_AddRef(dmo);
    ok(refcount == 4, "refcount == %u, expected 4\n", refcount);
    refcount = IDirectMusicObject_Release(dmo);

    hr = IDirectMusicBand_QueryInterface(dmb, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    refcount = IPersistStream_AddRef(ps);
    ok(refcount == 5, "refcount == %u, expected 5\n", refcount);
    refcount = IPersistStream_Release(ps);

    hr = IDirectMusicBand_QueryInterface(dmb, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
    refcount = IUnknown_AddRef(unk);
    ok(refcount == 6, "refcount == %u, expected 6\n", refcount);
    refcount = IUnknown_Release(unk);

    while (IDirectMusicBand_Release(dmb));
}

static void test_COM_bandtrack(void)
{
    IDirectMusicTrack *dmbt = (IDirectMusicTrack*)0xdeadbeef;
    IPersistStream *ps;
    IUnknown *private;
    IUnknown *unk;
    ULONG refcount;
    HRESULT hr;

    /* COM aggregation */
    hr = CoCreateInstance(&CLSID_DirectMusicBandTrack, (IUnknown *)0xdeadbeef, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void**)&dmbt);
    ok(hr == CLASS_E_NOAGGREGATION,
            "DirectMusicBandTrack create failed: %08x, expected CLASS_E_NOAGGREGATION\n", hr);
    ok(!dmbt, "dmbt = %p\n", dmbt);

    /* Invalid RIID */
    hr = CoCreateInstance(&CLSID_DirectMusicBandTrack, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicObject, (void**)&dmbt);
    ok(hr == E_NOINTERFACE, "DirectMusicBandTrack create failed: %08x, expected E_NOINTERFACE\n", hr);

    /* Same refcount for all DirectMusicBandTrack interfaces */
    hr = CoCreateInstance(&CLSID_DirectMusicBandTrack, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicTrack, (void**)&dmbt);
    ok(hr == S_OK, "DirectMusicBandTrack create failed: %08x, expected S_OK\n", hr);
    refcount = IDirectMusicTrack_AddRef(dmbt);
    ok(refcount == 2, "refcount == %u, expected 2\n", refcount);

    hr = IDirectMusicTrack_QueryInterface(dmbt, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    refcount = IPersistStream_AddRef(ps);
    ok(refcount == 4, "refcount == %u, expected 4\n", refcount);
    IPersistStream_Release(ps);

    hr = IDirectMusicTrack_QueryInterface(dmbt, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
    refcount = IUnknown_AddRef(unk);
    ok(refcount == 5, "refcount == %u, expected 5\n", refcount);
    refcount = IUnknown_Release(unk);

    hr = IDirectMusicTrack_QueryInterface(dmbt, &IID_IDirectMusicBandTrackPrivate,
            (void**)&private);
    todo_wine ok(hr == S_OK, "QueryInterface for IID_IDirectMusicBandTrackPrivate failed: %08x\n", hr);
    if (hr == S_OK) {
        refcount = IUnknown_AddRef(private);
        ok(refcount == 6, "refcount == %u, expected 6\n", refcount);
        refcount = IUnknown_Release(private);
    }

    while (IDirectMusicTrack_Release(dmbt));
}

static void test_dmband(void)
{
    IDirectMusicBand *dmb;
    IPersistStream *ps;
    CLSID class;
    ULARGE_INTEGER size;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicBand, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicBand, (void**)&dmb);
    ok(hr == S_OK, "DirectMusicBand create failed: %08x, expected S_OK\n", hr);

    /* Unimplemented IPersistStream methods */
    hr = IDirectMusicBand_QueryInterface(dmb, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    hr = IPersistStream_GetClassID(ps, &class);
    ok(hr == E_NOTIMPL, "IPersistStream_GetClassID failed: %08x\n", hr);
    hr = IPersistStream_IsDirty(ps);
    ok(hr == S_FALSE, "IPersistStream_IsDirty failed: %08x\n", hr);
    hr = IPersistStream_GetSizeMax(ps, &size);
    ok(hr == E_NOTIMPL, "IPersistStream_GetSizeMax failed: %08x\n", hr);
    hr = IPersistStream_Save(ps, NULL, TRUE);
    ok(hr == E_NOTIMPL, "IPersistStream_Save failed: %08x\n", hr);

    while (IDirectMusicBand_Release(dmb));
}

static void test_bandtrack(void)
{
    IDirectMusicTrack8 *dmt8;
    IPersistStream *ps;
    CLSID class;
    ULARGE_INTEGER size;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicBandTrack, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicTrack8, (void**)&dmt8);
    ok(hr == S_OK, "DirectMusicBandTrack create failed: %08x, expected S_OK\n", hr);

    /* IDirectMusicTrack8 */
    todo_wine {
    hr = IDirectMusicTrack8_Init(dmt8, NULL);
    ok(hr == E_POINTER, "IDirectMusicTrack8_Init failed: %08x\n", hr);
    hr = IDirectMusicTrack8_InitPlay(dmt8, NULL, NULL, NULL, 0, 0);
    ok(hr == E_POINTER, "IDirectMusicTrack8_InitPlay failed: %08x\n", hr);
    }
    hr = IDirectMusicTrack8_EndPlay(dmt8, NULL);
    ok(hr == S_OK, "IDirectMusicTrack8_EndPlay failed: %08x\n", hr);
    todo_wine {
    hr = IDirectMusicTrack8_Play(dmt8, NULL, 0, 0, 0, 0, NULL, NULL, 0);
    ok(hr == DMUS_S_END, "IDirectMusicTrack8_Play failed: %08x\n", hr);
    hr = IDirectMusicTrack8_GetParam(dmt8, NULL, 0, NULL, NULL);
    ok(hr == E_POINTER, "IDirectMusicTrack8_GetParam failed: %08x\n", hr);
    hr = IDirectMusicTrack8_SetParam(dmt8, NULL, 0, NULL);
    ok(hr == E_POINTER, "IDirectMusicTrack8_SetParam failed: %08x\n", hr);
    }
    hr = IDirectMusicTrack8_IsParamSupported(dmt8, NULL);
    ok(hr == E_POINTER, "IDirectMusicTrack8_IsParamSupported failed: %08x\n", hr);
    hr = IDirectMusicTrack8_AddNotificationType(dmt8, NULL);
    ok(hr == E_NOTIMPL, "IDirectMusicTrack8_AddNotificationType failed: %08x\n", hr);
    hr = IDirectMusicTrack8_RemoveNotificationType(dmt8, NULL);
    ok(hr == E_NOTIMPL, "IDirectMusicTrack8_RemoveNotificationType failed: %08x\n", hr);
    todo_wine {
    hr = IDirectMusicTrack8_Clone(dmt8, 0, 0, NULL);
    ok(hr == E_POINTER, "IDirectMusicTrack8_Clone failed: %08x\n", hr);
    hr = IDirectMusicTrack8_PlayEx(dmt8, NULL, 0, 0, 0, 0, NULL, NULL, 0);
    ok(hr == DMUS_S_END, "IDirectMusicTrack8_PlayEx failed: %08x\n", hr);
    hr = IDirectMusicTrack8_GetParamEx(dmt8, NULL, 0, NULL, NULL, NULL, 0);
    ok(hr == E_POINTER, "IDirectMusicTrack8_GetParamEx failed: %08x\n", hr);
    hr = IDirectMusicTrack8_SetParamEx(dmt8, NULL, 0, NULL, NULL, 0);
    ok(hr == E_POINTER, "IDirectMusicTrack8_SetParamEx failed: %08x\n", hr);
    }
    hr = IDirectMusicTrack8_Compose(dmt8, NULL, 0, NULL);
    ok(hr == E_NOTIMPL, "IDirectMusicTrack8_Compose failed: %08x\n", hr);
    hr = IDirectMusicTrack8_Join(dmt8, NULL, 0, NULL, 0, NULL);
    todo_wine ok(hr == E_POINTER, "IDirectMusicTrack8_Join failed: %08x\n", hr);

    /* IPersistStream */
    hr = IDirectMusicTrack8_QueryInterface(dmt8, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    hr = IPersistStream_GetClassID(ps, &class);
    ok(hr == S_OK, "IPersistStream_GetClassID failed: %08x\n", hr);
    ok(IsEqualGUID(&class, &CLSID_DirectMusicBandTrack),
            "Expected class CLSID_DirectMusicBandTrack got %s\n", wine_dbgstr_guid(&class));

    /* Unimplemented IPersistStream methods */
    hr = IPersistStream_IsDirty(ps);
    ok(hr == S_FALSE, "IPersistStream_IsDirty failed: %08x\n", hr);
    hr = IPersistStream_GetSizeMax(ps, &size);
    ok(hr == E_NOTIMPL, "IPersistStream_GetSizeMax failed: %08x\n", hr);
    hr = IPersistStream_Save(ps, NULL, TRUE);
    ok(hr == E_NOTIMPL, "IPersistStream_Save failed: %08x\n", hr);

    while (IDirectMusicTrack8_Release(dmt8));
}

struct chunk {
    FOURCC id;
    DWORD size;
    FOURCC type;
};

#define CHUNK_HDR_SIZE (sizeof(FOURCC) + sizeof(DWORD))

/* Generate a RIFF file format stream from an array of FOURCC ids.
   RIFF and LIST need to be followed by the form type respectively list type,
   followed by the chunks of the list and terminated with 0. */
static IStream *gen_riff_stream(const FOURCC *ids)
{
    static const LARGE_INTEGER zero;
    int level = -1;
    DWORD *sizes[4];    /* Stack for the sizes of RIFF and LIST chunks */
    char riff[1024];
    char *p = riff;
    struct chunk *ck;
    IStream *stream;

    do {
        ck = (struct chunk *)p;
        ck->id = *ids++;
        switch (ck->id) {
            case 0:
                *sizes[level] = p - (char *)sizes[level] - sizeof(DWORD);
                level--;
                break;
            case FOURCC_LIST:
            case FOURCC_RIFF:
                level++;
                sizes[level] = &ck->size;
                ck->type = *ids++;
                p += sizeof(*ck);
                break;
            case DMUS_FOURCC_GUID_CHUNK:
                ck->size = sizeof(GUID_NULL);
                p += CHUNK_HDR_SIZE;
                memcpy(p, &GUID_NULL, sizeof(GUID_NULL));
                p += ck->size;
                break;
            case DMUS_FOURCC_VERSION_CHUNK:
            {
                DMUS_VERSION ver = {5, 8};

                ck->size = sizeof(ver);
                p += CHUNK_HDR_SIZE;
                memcpy(p, &ver, sizeof(ver));
                p += ck->size;
                break;
            }
            default:
            {
                /* Just convert the FOURCC id to a WCHAR string */
                WCHAR *s;

                ck->size = 5 * sizeof(WCHAR);
                p += CHUNK_HDR_SIZE;
                s = (WCHAR *)p;
                s[0] = (char)(ck->id);
                s[1] = (char)(ck->id >> 8);
                s[2] = (char)(ck->id >> 16);
                s[3] = (char)(ck->id >> 24);
                s[4] = 0;
                p += ck->size;
            }
        }
    } while (level >= 0);

    ck = (struct chunk *)riff;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    IStream_Write(stream, riff, ck->size + CHUNK_HDR_SIZE, NULL);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);

    return stream;
}

static void test_parsedescriptor(void)
{
    IDirectMusicObject *dmo;
    IStream *stream;
    DMUS_OBJECTDESC desc = {0};
    HRESULT hr;
    DWORD valid;
    const WCHAR s_inam[] = {'I','N','A','M','\0'};
    const WCHAR s_unam[] = {'U','N','A','M','\0'};
    const FOURCC alldesc[] =
    {
        FOURCC_RIFF, DMUS_FOURCC_BAND_FORM, DMUS_FOURCC_CATEGORY_CHUNK, FOURCC_LIST,
        DMUS_FOURCC_UNFO_LIST, DMUS_FOURCC_UNAM_CHUNK, DMUS_FOURCC_UCOP_CHUNK,
        DMUS_FOURCC_UCMT_CHUNK, DMUS_FOURCC_USBJ_CHUNK, 0, DMUS_FOURCC_VERSION_CHUNK,
        DMUS_FOURCC_GUID_CHUNK, 0
    };
    const FOURCC dupes[] =
    {
        FOURCC_RIFF, DMUS_FOURCC_BAND_FORM, DMUS_FOURCC_CATEGORY_CHUNK, DMUS_FOURCC_CATEGORY_CHUNK,
        DMUS_FOURCC_VERSION_CHUNK, DMUS_FOURCC_VERSION_CHUNK, DMUS_FOURCC_GUID_CHUNK,
        DMUS_FOURCC_GUID_CHUNK, FOURCC_LIST, DMUS_FOURCC_UNFO_LIST, DMUS_FOURCC_UNAM_CHUNK, 0,
        FOURCC_LIST, DMUS_FOURCC_UNFO_LIST, mmioFOURCC('I','N','A','M'), 0, 0
    };
    FOURCC empty[] = {FOURCC_RIFF, DMUS_FOURCC_BAND_FORM, 0};
    FOURCC catdate[] = {FOURCC_RIFF, DMUS_FOURCC_BAND_FORM, DMUS_FOURCC_CATEGORY_CHUNK, 0};
    FOURCC inam[] =
    {
        FOURCC_RIFF, DMUS_FOURCC_BAND_FORM, FOURCC_LIST, DMUS_FOURCC_UNFO_LIST,
        mmioFOURCC('I','N','A','M'), 0, 0
    };

    hr = CoCreateInstance(&CLSID_DirectMusicBand, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicObject, (void **)&dmo);
    ok(hr == S_OK, "DirectMusicBand create failed: %08x, expected S_OK\n", hr);

    /* Nothing loaded */
    hr = IDirectMusicObject_GetDescriptor(dmo, &desc);
    ok(hr == S_OK, "GetDescriptor failed: %08x, expected S_OK\n", hr);
    ok(desc.dwValidData == DMUS_OBJ_CLASS, "Got valid data %#x, expected DMUS_OBJ_OBJECT\n",
            desc.dwValidData);
    ok(IsEqualGUID(&desc.guidClass, &CLSID_DirectMusicBand),
            "Got class guid %s, expected CLSID_DirectMusicBand\n",
            wine_dbgstr_guid(&desc.guidClass));

    /* Empty RIFF stream */
    stream = gen_riff_stream(empty);
    memset(&desc, 0, sizeof(desc));
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, &desc);
    ok(hr == S_OK, "ParseDescriptor failed: %08x, expected S_OK\n", hr);
    ok(desc.dwValidData == DMUS_OBJ_CLASS, "Got valid data %#x, expected DMUS_OBJ_CLASS\n",
            desc.dwValidData);
    ok(IsEqualGUID(&desc.guidClass, &CLSID_DirectMusicBand),
            "Got class guid %s, expected CLSID_DirectMusicBand\n",
            wine_dbgstr_guid(&desc.guidClass));
    IStream_Release(stream);

    /* NULL pointers */
    memset(&desc, 0, sizeof(desc));
    hr = IDirectMusicObject_ParseDescriptor(dmo, NULL, &desc);
    ok(hr == E_POINTER, "ParseDescriptor failed: %08x, expected E_POINTER\n", hr);
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, NULL);
    ok(hr == E_POINTER, "ParseDescriptor failed: %08x, expected E_POINTER\n", hr);

    /* Wrong form */
    empty[1] = DMUS_FOURCC_CONTAINER_FORM;
    stream = gen_riff_stream(empty);
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, &desc);
    ok(hr == DMUS_E_INVALID_BAND,
            "ParseDescriptor failed: %08x, expected DMUS_E_INVALID_BAND\n", hr);

    /* A category chunk adds DMUS_OBJ_DATE too */
    stream = gen_riff_stream(catdate);
    memset(&desc, 0, sizeof(desc));
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, &desc);
    ok(hr == S_OK, "ParseDescriptor failed: %08x, expected S_OK\n", hr);
    valid = DMUS_OBJ_CLASS | DMUS_OBJ_CATEGORY | DMUS_OBJ_DATE;
    ok(desc.dwValidData == valid, "Got valid data %#x, expected %#x\n", desc.dwValidData, valid);
    IStream_Release(stream);

    /* All desc chunks, extra DMUS_OBJ_DATE */
    stream = gen_riff_stream(alldesc);
    memset(&desc, 0, sizeof(desc));
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, &desc);
    ok(hr == S_OK, "ParseDescriptor failed: %08x, expected S_OK\n", hr);
    valid = DMUS_OBJ_OBJECT|DMUS_OBJ_CLASS|DMUS_OBJ_NAME|DMUS_OBJ_CATEGORY|DMUS_OBJ_VERSION|DMUS_OBJ_DATE;
    ok(desc.dwValidData == valid, "Got valid data %#x, expected %#x\n", desc.dwValidData, valid);
    ok(IsEqualGUID(&desc.guidClass, &CLSID_DirectMusicBand),
            "Got class guid %s, expected CLSID_DirectMusicBand\n",
            wine_dbgstr_guid(&desc.guidClass));
    ok(IsEqualGUID(&desc.guidObject, &GUID_NULL), "Got object guid %s, expected GUID_NULL\n",
            wine_dbgstr_guid(&desc.guidClass));
    ok(!memcmp(desc.wszName, s_unam, sizeof(s_unam)), "Got name '%s', expected 'UNAM'\n",
            wine_dbgstr_w(desc.wszName));
    ok(!desc.ftDate.dwHighDateTime && !desc.ftDate.dwLowDateTime,
            "Got file time %08x %08x, expected 0\n", desc.ftDate.dwHighDateTime,
            desc.ftDate.dwLowDateTime);
    IStream_Release(stream);

    /* UNFO list with INAM */
    inam[3] = DMUS_FOURCC_UNFO_LIST;
    stream = gen_riff_stream(inam);
    memset(&desc, 0, sizeof(desc));
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, &desc);
    ok(hr == S_OK, "ParseDescriptor failed: %08x, expected S_OK\n", hr);
    ok(desc.dwValidData == (DMUS_OBJ_CLASS | DMUS_OBJ_NAME),
            "Got valid data %#x, expected DMUS_OBJ_CLASS | DMUS_OBJ_NAME\n", desc.dwValidData);
    ok(!memcmp(desc.wszName, s_inam, sizeof(s_inam)), "Got name '%s', expected 'INAM'\n",
            wine_dbgstr_w(desc.wszName));
    IStream_Release(stream);

    /* INFO list with INAM */
    inam[3] = DMUS_FOURCC_INFO_LIST;
    stream = gen_riff_stream(inam);
    memset(&desc, 0, sizeof(desc));
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, &desc);
    ok(hr == S_OK, "ParseDescriptor failed: %08x, expected S_OK\n", hr);
    ok(desc.dwValidData == DMUS_OBJ_CLASS, "Got valid data %#x, expected DMUS_OBJ_CLASS\n",
            desc.dwValidData);
    IStream_Release(stream);

    /* Duplicated chunks */
    stream = gen_riff_stream(dupes);
    memset(&desc, 0, sizeof(desc));
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, &desc);
    ok(hr == S_OK, "ParseDescriptor failed: %08x, expected S_OK\n", hr);
    valid = DMUS_OBJ_OBJECT|DMUS_OBJ_CLASS|DMUS_OBJ_NAME|DMUS_OBJ_CATEGORY|DMUS_OBJ_VERSION|DMUS_OBJ_DATE;
    ok(desc.dwValidData == valid, "Got valid data %#x, expected %#x\n", desc.dwValidData, valid);
    ok(!memcmp(desc.wszName, s_inam, sizeof(s_inam)), "Got name '%s', expected 'INAM'\n",
            wine_dbgstr_w(desc.wszName));
    IStream_Release(stream);

    IDirectMusicObject_Release(dmo);
}

START_TEST(dmband)
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if (missing_dmband())
    {
        skip("dmband not available\n");
        CoUninitialize();
        return;
    }

    test_COM();
    test_COM_bandtrack();
    test_dmband();
    test_bandtrack();
    test_parsedescriptor();

    CoUninitialize();
}
