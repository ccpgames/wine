/*
 * IXmlReader tests
 *
 * Copyright 2010, 2012-2013, 2016-2017 Nikolay Sivov
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
#define CONST_VTABLE

#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "initguid.h"
#include "ole2.h"
#include "xmllite.h"
#include "wine/test.h"

DEFINE_GUID(IID_IXmlReaderInput, 0x0b3ccc9b, 0x9214, 0x428b, 0xa2, 0xae, 0xef, 0x3a, 0xa8, 0x71, 0xaf, 0xda);

static WCHAR *a2w(const char *str)
{
    int len = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
    WCHAR *ret = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
    MultiByteToWideChar(CP_ACP, 0, str, -1, ret, len);
    return ret;
}

static void free_str(WCHAR *str)
{
    HeapFree(GetProcessHeap(), 0, str);
}

static int strcmp_wa(const WCHAR *str1, const char *stra)
{
    WCHAR *str2 = a2w(stra);
    int r = lstrcmpW(str1, str2);
    free_str(str2);
    return r;
}

static const char xmldecl_full[] = "\xef\xbb\xbf<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n";
static const char xmldecl_short[] = "<?xml version=\"1.0\"?><RegistrationInfo/>";

static IStream *create_stream_on_data(const void *data, unsigned int size)
{
    IStream *stream = NULL;
    HGLOBAL hglobal;
    void *ptr;
    HRESULT hr;

    hglobal = GlobalAlloc(GHND, size);
    ptr = GlobalLock(hglobal);

    memcpy(ptr, data, size);

    hr = CreateStreamOnHGlobal(hglobal, TRUE, &stream);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(stream != NULL, "Expected non-NULL stream\n");

    GlobalUnlock(hglobal);

    return stream;
}

static void test_reader_pos(IXmlReader *reader, UINT line, UINT pos, UINT line_broken,
        UINT pos_broken, int _line_)
{
    UINT l = ~0u, p = ~0u;
    BOOL broken_state;

    IXmlReader_GetLineNumber(reader, &l);
    IXmlReader_GetLinePosition(reader, &p);

    if (line_broken == ~0u && pos_broken == ~0u)
        broken_state = FALSE;
    else
        broken_state = broken((line_broken == ~0u ? line : line_broken) == l &&
                              (pos_broken == ~0u ? pos : pos_broken) == p);

    ok_(__FILE__, _line_)((l == line && pos == p) || broken_state,
            "Expected (%d,%d), got (%d,%d)\n", line, pos, l, p);
}
#define TEST_READER_POSITION(reader, line, pos) \
    test_reader_pos(reader, line, pos, ~0u, ~0u, __LINE__)
#define TEST_READER_POSITION2(reader, line, pos, line_broken, pos_broken) \
    test_reader_pos(reader, line, pos, line_broken, pos_broken, __LINE__)

typedef struct input_iids_t {
    IID iids[10];
    int count;
} input_iids_t;

static const IID *setinput_full[] = {
    &IID_IXmlReaderInput,
    &IID_IStream,
    &IID_ISequentialStream,
    NULL
};

/* this applies to early xmllite versions */
static const IID *setinput_full_old[] = {
    &IID_IXmlReaderInput,
    &IID_ISequentialStream,
    &IID_IStream,
    NULL
};

/* after ::SetInput(IXmlReaderInput*) */
static const IID *setinput_readerinput[] = {
    &IID_IStream,
    &IID_ISequentialStream,
    NULL
};

static const IID *empty_seq[] = {
    NULL
};

static input_iids_t input_iids;

static void ok_iids_(const input_iids_t *iids, const IID **expected, const IID **exp_broken, BOOL todo, int line)
{
    int i = 0, size = 0;

    while (expected[i++]) size++;

    todo_wine_if (todo)
        ok_(__FILE__, line)(iids->count == size, "Sequence size mismatch (%d), got (%d)\n", size, iids->count);

    if (iids->count != size) return;

    for (i = 0; i < size; i++) {
        ok_(__FILE__, line)(IsEqualGUID(&iids->iids[i], expected[i]) ||
            (exp_broken ? broken(IsEqualGUID(&iids->iids[i], exp_broken[i])) : FALSE),
            "Wrong IID(%d), got %s\n", i, wine_dbgstr_guid(&iids->iids[i]));
    }
}
#define ok_iids(got, exp, brk, todo) ok_iids_(got, exp, brk, todo, __LINE__)

static const char *state_to_str(XmlReadState state)
{
    static const char* state_names[] = {
        "XmlReadState_Initial",
        "XmlReadState_Interactive",
        "XmlReadState_Error",
        "XmlReadState_EndOfFile",
        "XmlReadState_Closed"
    };

    static const char unknown[] = "unknown";

    switch (state)
    {
    case XmlReadState_Initial:
    case XmlReadState_Interactive:
    case XmlReadState_Error:
    case XmlReadState_EndOfFile:
    case XmlReadState_Closed:
        return state_names[state];
    default:
        return unknown;
    }
}

static const char *type_to_str(XmlNodeType type)
{
    static const char* type_names[] = {
        "XmlNodeType_None",
        "XmlNodeType_Element",
        "XmlNodeType_Attribute",
        "XmlNodeType_Text",
        "XmlNodeType_CDATA",
        "", "",
        "XmlNodeType_ProcessingInstruction",
        "XmlNodeType_Comment",
        "",
        "XmlNodeType_DocumentType",
        "", "",
        "XmlNodeType_Whitespace",
        "",
        "XmlNodeType_EndElement",
        "",
        "XmlNodeType_XmlDeclaration"
    };

    static const char unknown[] = "unknown";

    switch (type)
    {
    case XmlNodeType_None:
    case XmlNodeType_Element:
    case XmlNodeType_Attribute:
    case XmlNodeType_Text:
    case XmlNodeType_CDATA:
    case XmlNodeType_ProcessingInstruction:
    case XmlNodeType_Comment:
    case XmlNodeType_DocumentType:
    case XmlNodeType_Whitespace:
    case XmlNodeType_EndElement:
    case XmlNodeType_XmlDeclaration:
        return type_names[type];
    default:
        return unknown;
    }
}

static void test_read_state(IXmlReader *reader, XmlReadState expected,
    XmlReadState exp_broken, int line)
{
    BOOL broken_state;
    LONG_PTR state;

    state = -1; /* invalid state value */
    IXmlReader_GetProperty(reader, XmlReaderProperty_ReadState, &state);

    if (exp_broken == -1)
        broken_state = FALSE;
    else
        broken_state = broken(exp_broken == state);

    ok_(__FILE__, line)(state == expected || broken_state, "Expected (%s), got (%s)\n",
            state_to_str(expected), state_to_str(state));
}

#define TEST_READER_STATE(reader, state) test_read_state(reader, state, -1, __LINE__)
#define TEST_READER_STATE2(reader, state, brk) test_read_state(reader, state, brk, __LINE__)

typedef struct _testinput
{
    IUnknown IUnknown_iface;
    LONG ref;
} testinput;

static inline testinput *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, testinput, IUnknown_iface);
}

static HRESULT WINAPI testinput_QueryInterface(IUnknown *iface, REFIID riid, void** ppvObj)
{
    if (IsEqualGUID( riid, &IID_IUnknown ))
    {
        *ppvObj = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }

    input_iids.iids[input_iids.count++] = *riid;

    *ppvObj = NULL;

    return E_NOINTERFACE;
}

static ULONG WINAPI testinput_AddRef(IUnknown *iface)
{
    testinput *This = impl_from_IUnknown(iface);
    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI testinput_Release(IUnknown *iface)
{
    testinput *This = impl_from_IUnknown(iface);
    LONG ref;

    ref = InterlockedDecrement(&This->ref);
    if (ref == 0)
    {
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static const struct IUnknownVtbl testinput_vtbl =
{
    testinput_QueryInterface,
    testinput_AddRef,
    testinput_Release
};

static HRESULT testinput_createinstance(void **ppObj)
{
    testinput *input;

    input = HeapAlloc(GetProcessHeap(), 0, sizeof (*input));
    if(!input) return E_OUTOFMEMORY;

    input->IUnknown_iface.lpVtbl = &testinput_vtbl;
    input->ref = 1;

    *ppObj = &input->IUnknown_iface;

    return S_OK;
}

static HRESULT WINAPI teststream_QueryInterface(ISequentialStream *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ISequentialStream))
    {
        *obj = iface;
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI teststream_AddRef(ISequentialStream *iface)
{
    return 2;
}

static ULONG WINAPI teststream_Release(ISequentialStream *iface)
{
    return 1;
}

static int stream_readcall;

static HRESULT WINAPI teststream_Read(ISequentialStream *iface, void *pv, ULONG cb, ULONG *pread)
{
    static const char xml[] = "<!-- comment -->";

    if (stream_readcall++)
    {
        *pread = 0;
        return E_PENDING;
    }

    *pread = sizeof(xml) / 2;
    memcpy(pv, xml, *pread);
    return S_OK;
}

static HRESULT WINAPI teststream_Write(ISequentialStream *iface, const void *pv, ULONG cb, ULONG *written)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static const ISequentialStreamVtbl teststreamvtbl =
{
    teststream_QueryInterface,
    teststream_AddRef,
    teststream_Release,
    teststream_Read,
    teststream_Write
};

static HRESULT WINAPI resolver_QI(IXmlResolver *iface, REFIID riid, void **obj)
{
    ok(0, "unexpected call, riid %s\n", wine_dbgstr_guid(riid));

    if (IsEqualIID(riid, &IID_IXmlResolver) || IsEqualIID(riid, &IID_IUnknown))
    {
        *obj = iface;
        IXmlResolver_AddRef(iface);
        return S_OK;
    }

    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI resolver_AddRef(IXmlResolver *iface)
{
    return 2;
}

static ULONG WINAPI resolver_Release(IXmlResolver *iface)
{
    return 1;
}

static HRESULT WINAPI resolver_ResolveUri(IXmlResolver *iface, const WCHAR *base_uri,
    const WCHAR *public_id, const WCHAR *system_id, IUnknown **input)
{
    ok(0, "unexpected call\n");
    return E_NOTIMPL;
}

static const IXmlResolverVtbl resolvervtbl =
{
    resolver_QI,
    resolver_AddRef,
    resolver_Release,
    resolver_ResolveUri
};

static IXmlResolver testresolver = { &resolvervtbl };

static void test_reader_create(void)
{
    IXmlResolver *resolver;
    HRESULT hr;
    IXmlReader *reader;
    IUnknown *input;
    DtdProcessing dtd;
    XmlNodeType nodetype;

    /* crashes native */
    if (0)
    {
        CreateXmlReader(&IID_IXmlReader, NULL, NULL);
        CreateXmlReader(NULL, (void**)&reader, NULL);
    }

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    TEST_READER_STATE(reader, XmlReadState_Closed);

    nodetype = XmlNodeType_Element;
    hr = IXmlReader_GetNodeType(reader, &nodetype);
    ok(hr == S_FALSE, "got %08x\n", hr);
    ok(nodetype == XmlNodeType_None, "got %d\n", nodetype);

    /* crashes on XP, 2k3, works on newer versions */
    if (0)
    {
        hr = IXmlReader_GetNodeType(reader, NULL);
        ok(hr == E_INVALIDARG, "got %08x\n", hr);
    }

    resolver = (void*)0xdeadbeef;
    hr = IXmlReader_GetProperty(reader, XmlReaderProperty_XmlResolver, (LONG_PTR*)&resolver);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(resolver == NULL, "got %p\n", resolver);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_XmlResolver, 0);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_XmlResolver, (LONG_PTR)&testresolver);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    resolver = NULL;
    hr = IXmlReader_GetProperty(reader, XmlReaderProperty_XmlResolver, (LONG_PTR*)&resolver);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(resolver == &testresolver, "got %p\n", resolver);
    IXmlResolver_Release(resolver);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_XmlResolver, 0);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    dtd = 2;
    hr = IXmlReader_GetProperty(reader, XmlReaderProperty_DtdProcessing, (LONG_PTR*)&dtd);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(dtd == DtdProcessing_Prohibit, "got %d\n", dtd);

    dtd = 2;
    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_DtdProcessing, dtd);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_DtdProcessing, -1);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);

    /* Null input pointer, releases previous input */
    hr = IXmlReader_SetInput(reader, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    TEST_READER_STATE2(reader, XmlReadState_Initial, XmlReadState_Closed);

    /* test input interface selection sequence */
    hr = testinput_createinstance((void**)&input);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    if (hr == S_OK)
    {
        input_iids.count = 0;
        hr = IXmlReader_SetInput(reader, input);
        ok(hr == E_NOINTERFACE, "Expected E_NOINTERFACE, got %08x\n", hr);
        ok_iids(&input_iids, setinput_full, setinput_full_old, FALSE);
        IUnknown_Release(input);
    }
    IXmlReader_Release(reader);
}

static void test_readerinput(void)
{
    IXmlReaderInput *reader_input;
    IXmlReader *reader, *reader2;
    IUnknown *obj, *input;
    IStream *stream, *stream2;
    XmlNodeType nodetype;
    HRESULT hr;
    LONG ref;

    hr = CreateXmlReaderInputWithEncodingName(NULL, NULL, NULL, FALSE, NULL, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);
    hr = CreateXmlReaderInputWithEncodingName(NULL, NULL, NULL, FALSE, NULL, &reader_input);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);

    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    ref = IStream_AddRef(stream);
    ok(ref == 2, "Expected 2, got %d\n", ref);
    IStream_Release(stream);
    hr = CreateXmlReaderInputWithEncodingName((IUnknown*)stream, NULL, NULL, FALSE, NULL, &reader_input);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = IUnknown_QueryInterface(reader_input, &IID_IStream, (void**)&stream2);
    ok(hr == E_NOINTERFACE, "Expected S_OK, got %08x\n", hr);

    hr = IUnknown_QueryInterface(reader_input, &IID_ISequentialStream, (void**)&stream2);
    ok(hr == E_NOINTERFACE, "Expected S_OK, got %08x\n", hr);

    /* IXmlReaderInput grabs a stream reference */
    ref = IStream_AddRef(stream);
    ok(ref == 3, "Expected 3, got %d\n", ref);
    IStream_Release(stream);

    /* try ::SetInput() with valid IXmlReaderInput */
    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    ref = IUnknown_AddRef(reader_input);
    ok(ref == 2, "Expected 2, got %d\n", ref);
    IUnknown_Release(reader_input);

    hr = IXmlReader_SetInput(reader, reader_input);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    TEST_READER_STATE(reader, XmlReadState_Initial);

    nodetype = XmlNodeType_Element;
    hr = IXmlReader_GetNodeType(reader, &nodetype);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(nodetype == XmlNodeType_None, "got %d\n", nodetype);

    /* IXmlReader grabs a IXmlReaderInput reference */
    ref = IUnknown_AddRef(reader_input);
    ok(ref == 3, "Expected 3, got %d\n", ref);
    IUnknown_Release(reader_input);

    ref = IStream_AddRef(stream);
    ok(ref == 4, "Expected 4, got %d\n", ref);
    IStream_Release(stream);

    /* reset input and check state */
    hr = IXmlReader_SetInput(reader, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    TEST_READER_STATE2(reader, XmlReadState_Initial, XmlReadState_Closed);

    IXmlReader_Release(reader);

    ref = IStream_AddRef(stream);
    ok(ref == 3, "Expected 3, got %d\n", ref);
    IStream_Release(stream);

    ref = IUnknown_AddRef(reader_input);
    ok(ref == 2, "Expected 2, got %d\n", ref);
    IUnknown_Release(reader_input);

    /* IID_IXmlReaderInput */
    /* it returns a kind of private undocumented vtable incompatible with IUnknown,
       so it's not a COM interface actually.
       Such query will be used only to check if input is really IXmlReaderInput */
    obj = (IUnknown*)0xdeadbeef;
    hr = IUnknown_QueryInterface(reader_input, &IID_IXmlReaderInput, (void**)&obj);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ref = IUnknown_AddRef(reader_input);
    ok(ref == 3, "Expected 3, got %d\n", ref);
    IUnknown_Release(reader_input);

    IUnknown_Release(reader_input);
    IUnknown_Release(reader_input);
    IStream_Release(stream);

    /* test input interface selection sequence */
    input = NULL;
    hr = testinput_createinstance((void**)&input);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    input_iids.count = 0;
    ref = IUnknown_AddRef(input);
    ok(ref == 2, "Expected 2, got %d\n", ref);
    IUnknown_Release(input);
    hr = CreateXmlReaderInputWithEncodingName(input, NULL, NULL, FALSE, NULL, &reader_input);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok_iids(&input_iids, empty_seq, NULL, FALSE);
    /* IXmlReaderInput stores stream interface as IUnknown */
    ref = IUnknown_AddRef(input);
    ok(ref == 3, "Expected 3, got %d\n", ref);
    IUnknown_Release(input);

    hr = CreateXmlReader(&IID_IXmlReader, (LPVOID*)&reader, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    input_iids.count = 0;
    ref = IUnknown_AddRef(reader_input);
    ok(ref == 2, "Expected 2, got %d\n", ref);
    IUnknown_Release(reader_input);
    ref = IUnknown_AddRef(input);
    ok(ref == 3, "Expected 3, got %d\n", ref);
    IUnknown_Release(input);
    hr = IXmlReader_SetInput(reader, reader_input);
    ok(hr == E_NOINTERFACE, "Expected E_NOINTERFACE, got %08x\n", hr);
    ok_iids(&input_iids, setinput_readerinput, NULL, FALSE);

    TEST_READER_STATE(reader, XmlReadState_Closed);

    ref = IUnknown_AddRef(input);
    ok(ref == 3, "Expected 3, got %d\n", ref);
    IUnknown_Release(input);

    ref = IUnknown_AddRef(reader_input);
    ok(ref == 3 || broken(ref == 2) /* versions 1.0.x and 1.1.x - XP, Vista */,
          "Expected 3, got %d\n", ref);
    IUnknown_Release(reader_input);
    /* repeat another time, no check or caching here */
    input_iids.count = 0;
    hr = IXmlReader_SetInput(reader, reader_input);
    ok(hr == E_NOINTERFACE, "Expected E_NOINTERFACE, got %08x\n", hr);
    ok_iids(&input_iids, setinput_readerinput, NULL, FALSE);

    /* another reader */
    hr = CreateXmlReader(&IID_IXmlReader, (LPVOID*)&reader2, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    /* resolving from IXmlReaderInput to IStream/ISequentialStream is done at
       ::SetInput() level, each time it's called */
    input_iids.count = 0;
    hr = IXmlReader_SetInput(reader2, reader_input);
    ok(hr == E_NOINTERFACE, "Expected E_NOINTERFACE, got %08x\n", hr);
    ok_iids(&input_iids, setinput_readerinput, NULL, FALSE);

    IXmlReader_Release(reader2);
    IXmlReader_Release(reader);

    IUnknown_Release(reader_input);
    IUnknown_Release(input);
}

static void test_reader_state(void)
{
    XmlNodeType nodetype;
    IXmlReader *reader;
    IStream *stream;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    /* invalid arguments */
    hr = IXmlReader_GetProperty(reader, XmlReaderProperty_ReadState, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);

    /* attempt to read on closed reader */
    TEST_READER_STATE(reader, XmlReadState_Closed);

if (0)
{
    /* newer versions crash here, probably because no input was set */
    hr = IXmlReader_Read(reader, &nodetype);
    ok(hr == S_FALSE, "got %08x\n", hr);
}

    stream = create_stream_on_data("xml", sizeof("xml"));

    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    TEST_READER_STATE(reader, XmlReadState_Initial);

    nodetype = XmlNodeType_Element;
    hr = IXmlReader_Read(reader, &nodetype);
todo_wine
    ok(FAILED(hr), "got %08x\n", hr);
    ok(nodetype == XmlNodeType_None, "Unexpected node type %d\n", nodetype);

todo_wine
    TEST_READER_STATE(reader, XmlReadState_Error);

    nodetype = XmlNodeType_Element;
    hr = IXmlReader_Read(reader, &nodetype);
todo_wine
    ok(FAILED(hr), "got %08x\n", hr);
    ok(nodetype == XmlNodeType_None, "Unexpected node type %d\n", nodetype);

    IStream_Release(stream);
    IXmlReader_Release(reader);
}

static void test_reader_depth(IXmlReader *reader, UINT depth, UINT brk, int line)
{
    BOOL condition;
    UINT d = ~0u;

    IXmlReader_GetDepth(reader, &d);

    condition = d == depth;
    if (brk != ~0u)
        condition |= broken(d == brk);
    ok_(__FILE__, line)(condition, "Unexpected nesting depth %u, expected %u\n", d, depth);
}

#define TEST_DEPTH(reader, depth) test_reader_depth(reader, depth, ~0u, __LINE__)
#define TEST_DEPTH2(reader, depth, brk) test_reader_depth(reader, depth, brk, __LINE__)

static void test_read_xmldeclaration(void)
{
    static const WCHAR xmlW[] = {'x','m','l',0};
    static const WCHAR RegistrationInfoW[] = {'R','e','g','i','s','t','r','a','t','i','o','n','I','n','f','o',0};
    static const struct
    {
        WCHAR name[12];
        WCHAR val[12];
    } name_val[] =
    {
        { {'v','e','r','s','i','o','n',0}, {'1','.','0',0} },
        { {'e','n','c','o','d','i','n','g',0}, {'U','T','F','-','8',0} },
        { {'s','t','a','n','d','a','l','o','n','e',0}, {'y','e','s',0} }
    };
    IXmlReader *reader;
    IStream *stream;
    HRESULT hr;
    XmlNodeType type;
    UINT count = 0, len, i;
    BOOL ret;
    const WCHAR *val;

    hr = CreateXmlReader(&IID_IXmlReader, (LPVOID*)&reader, NULL);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    stream = create_stream_on_data(xmldecl_full, sizeof(xmldecl_full));

    hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);

    hr = IXmlReader_GetAttributeCount(reader, &count);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(count == 0, "got %d\n", count);

    /* try to move without attributes */
    hr = IXmlReader_MoveToElement(reader);
    ok(hr == S_FALSE, "got %08x\n", hr);

    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_FALSE, "got %08x\n", hr);

    hr = IXmlReader_MoveToFirstAttribute(reader);
    ok(hr == S_FALSE, "got %08x\n", hr);

    TEST_READER_POSITION(reader, 0, 0);

    type = -1;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "Expected S_OK, got %08x\n", hr);
    ok(type == XmlNodeType_XmlDeclaration,
                     "Expected XmlNodeType_XmlDeclaration, got %s\n", type_to_str(type));
    /* new version 1.2.x and 1.3.x properly update position for <?xml ?> */
    TEST_READER_POSITION2(reader, 1, 3, ~0u, 55);

    TEST_DEPTH(reader, 0);
    TEST_READER_STATE(reader, XmlReadState_Interactive);

    hr = IXmlReader_GetValue(reader, &val, NULL);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(*val == 0, "got %s\n", wine_dbgstr_w(val));

    /* check attributes */
    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);

    TEST_DEPTH(reader, 1);

    type = XmlNodeType_None;
    hr = IXmlReader_GetNodeType(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Attribute, "got %d\n", type);

    TEST_READER_POSITION2(reader, 1, 7, ~0u, 55);

    /* try to move from last attribute */
    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);
    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);
    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_FALSE, "got %08x\n", hr);

    type = XmlNodeType_None;
    hr = IXmlReader_GetNodeType(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Attribute, "got %d\n", type);

    hr = IXmlReader_MoveToFirstAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);
    TEST_READER_POSITION2(reader, 1, 7, ~0u, 55);

    hr = IXmlReader_GetAttributeCount(reader, NULL);
    ok(hr == E_INVALIDARG, "got %08x\n", hr);

    hr = IXmlReader_GetAttributeCount(reader, &count);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(count == 3, "Expected 3, got %d\n", count);

    for (i = 0; i < count; i++)
    {
        len = 0;
        hr = IXmlReader_GetLocalName(reader, &val, &len);
        ok(hr == S_OK, "got %08x\n", hr);
        ok(len == lstrlenW(name_val[i].name), "expected %u, got %u\n", lstrlenW(name_val[i].name), len);
        ok(!lstrcmpW(name_val[i].name, val), "expected %s, got %s\n", wine_dbgstr_w(name_val[i].name), wine_dbgstr_w(val));

        len = 0;
        hr = IXmlReader_GetValue(reader, &val, &len);
        ok(hr == S_OK, "got %08x\n", hr);
        ok(len == lstrlenW(name_val[i].val), "expected %u, got %u\n", lstrlenW(name_val[i].val), len);
        ok(!lstrcmpW(name_val[i].val, val), "expected %s, got %s\n", wine_dbgstr_w(name_val[i].val), wine_dbgstr_w(val));

        hr = IXmlReader_MoveToNextAttribute(reader);
        ok(hr == ((i < count - 1) ? S_OK : S_FALSE), "got %08x\n", hr);
    }

    TEST_DEPTH(reader, 1);

    hr = IXmlReader_MoveToElement(reader);
    ok(hr == S_OK, "got %08x\n", hr);
    TEST_READER_POSITION2(reader, 1, 3, ~0u, 55);

    type = XmlNodeType_None;
    hr = IXmlReader_GetNodeType(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_XmlDeclaration, "got %d\n", type);

    type = XmlNodeType_XmlDeclaration;
    hr = IXmlReader_Read(reader, &type);
    /* newer versions return syntax error here cause document is incomplete,
       it makes more sense than invalid char error */
todo_wine {
    ok(hr == WC_E_SYNTAX || broken(hr == WC_E_XMLCHARACTER), "got 0x%08x\n", hr);
    ok(type == XmlNodeType_None, "got %d\n", type);
}
    IStream_Release(stream);

    /* test short variant */
    stream = create_stream_on_data(xmldecl_short, sizeof(xmldecl_short));

    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);

    type = -1;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(type == XmlNodeType_XmlDeclaration, "expected XmlDeclaration, got %s\n", type_to_str(type));
    TEST_READER_POSITION2(reader, 1, 3, ~0u, 21);
    TEST_READER_STATE(reader, XmlReadState_Interactive);

    hr = IXmlReader_GetAttributeCount(reader, &count);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(count == 1, "expected 1, got %d\n", count);

    ret = IXmlReader_IsEmptyElement(reader);
    ok(!ret, "element should not be empty\n");

    hr = IXmlReader_GetValue(reader, &val, NULL);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(*val == 0, "got %s\n", wine_dbgstr_w(val));

    val = NULL;
    hr = IXmlReader_GetLocalName(reader, &val, NULL);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(!lstrcmpW(val, xmlW), "got %s\n", wine_dbgstr_w(val));

    val = NULL;
    hr = IXmlReader_GetQualifiedName(reader, &val, NULL);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(!lstrcmpW(val, xmlW), "got %s\n", wine_dbgstr_w(val));

    /* check attributes */
    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);

    type = -1;
    hr = IXmlReader_GetNodeType(reader, &type);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(type == XmlNodeType_Attribute, "got %d\n", type);
    TEST_READER_POSITION2(reader, 1, 7, ~0u, 21);

    /* try to move from last attribute */
    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_FALSE, "expected S_FALSE, got %08x\n", hr);

    type = -1;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(type == XmlNodeType_Element, "expected Element, got %s\n", type_to_str(type));
    TEST_READER_POSITION2(reader, 1, 23, ~0u, 40);
    TEST_READER_STATE(reader, XmlReadState_Interactive);

    hr = IXmlReader_GetAttributeCount(reader, &count);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(count == 0, "expected 0, got %d\n", count);

    ret = IXmlReader_IsEmptyElement(reader);
    ok(ret, "element should be empty\n");

    hr = IXmlReader_GetValue(reader, &val, NULL);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(*val == 0, "got %s\n", wine_dbgstr_w(val));

    hr = IXmlReader_GetLocalName(reader, &val, NULL);
    ok(hr == S_OK, "expected S_OK, got %08x\n", hr);
    ok(!lstrcmpW(val, RegistrationInfoW), "got %s\n", wine_dbgstr_w(val));

    type = -1;
    hr = IXmlReader_Read(reader, &type);
todo_wine
    ok(hr == WC_E_SYNTAX || hr == WC_E_XMLCHARACTER /* XP */, "expected WC_E_SYNTAX, got %08x\n", hr);
    ok(type == XmlNodeType_None, "expected XmlNodeType_None, got %s\n", type_to_str(type));
    TEST_READER_POSITION(reader, 1, 41);
todo_wine
    TEST_READER_STATE(reader, XmlReadState_Error);

    IStream_Release(stream);
    IXmlReader_Release(reader);
}

struct test_entry {
    const char *xml;
    const char *name;
    const char *value;
    HRESULT hr;
    HRESULT hr_broken; /* this is set to older version results */
    BOOL todo;
};

static struct test_entry comment_tests[] = {
    { "<!-- comment -->", "", " comment ", S_OK },
    { "<!-- - comment-->", "", " - comment", S_OK },
    { "<!-- -- comment-->", NULL, NULL, WC_E_COMMENT, WC_E_GREATERTHAN },
    { "<!-- -- comment--->", NULL, NULL, WC_E_COMMENT, WC_E_GREATERTHAN },
    { NULL }
};

static void test_read_comment(void)
{
    static const char *teststr = "<a>text<!-- comment --></a>";
    struct test_entry *test = comment_tests;
    static const XmlNodeType types[] =
    {
        XmlNodeType_Element,
        XmlNodeType_Text,
        XmlNodeType_Comment,
        XmlNodeType_EndElement,
    };
    unsigned int i = 0;
    IXmlReader *reader;
    XmlNodeType type;
    IStream *stream;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    stream = create_stream_on_data(teststr, strlen(teststr));
    hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    while (IXmlReader_Read(reader, &type) == S_OK)
    {
        const WCHAR *value;

        ok(type == types[i], "%d: unexpected node type %d\n", i, type);

        if (type == XmlNodeType_Text || type == XmlNodeType_Comment)
        {
            hr = IXmlReader_GetValue(reader, &value, NULL);
            ok(hr == S_OK, "got %08x\n", hr);
            ok(*value != 0, "Expected node value\n");
        }
        i++;
    }

    IStream_Release(stream);

    while (test->xml)
    {
        stream = create_stream_on_data(test->xml, strlen(test->xml)+1);
        hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        type = XmlNodeType_None;
        hr = IXmlReader_Read(reader, &type);
        if (test->hr_broken)
            ok(hr == test->hr || broken(hr == test->hr_broken), "got %08x for %s\n", hr, test->xml);
        else
            ok(hr == test->hr, "got %08x for %s\n", hr, test->xml);
        if (hr == S_OK)
        {
            const WCHAR *str;
            WCHAR *str_exp;
            UINT len;

            ok(type == XmlNodeType_Comment, "got %d for %s\n", type, test->xml);

            len = 1;
            str = NULL;
            hr = IXmlReader_GetLocalName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == 0, "got %u\n", len);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            str = NULL;
            hr = IXmlReader_GetLocalName(reader, &str, NULL);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            len = 1;
            str = NULL;
            hr = IXmlReader_GetQualifiedName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == 0, "got %u\n", len);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            str = NULL;
            hr = IXmlReader_GetQualifiedName(reader, &str, NULL);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            /* value */
            len = 1;
            str = NULL;
            hr = IXmlReader_GetValue(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == strlen(test->value), "got %u\n", len);
            str_exp = a2w(test->value);
            ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            free_str(str_exp);
        }

        IStream_Release(stream);
        test++;
    }

    IXmlReader_Release(reader);
}

static struct test_entry pi_tests[] = {
    { "<?pi?>", "pi", "", S_OK },
    { "<?pi ?>", "pi", "", S_OK },
    { "<?pi  ?>", "pi", "", S_OK },
    { "<?pi pi data?>", "pi", "pi data", S_OK },
    { "<?pi pi data  ?>", "pi", "pi data  ", S_OK },
    { "<?pi    data  ?>", "pi", "data  ", S_OK },
    { "<?pi:pi?>", NULL, NULL, NC_E_NAMECOLON, WC_E_NAMECHARACTER },
    { "<?:pi ?>", NULL, NULL, WC_E_PI, WC_E_NAMECHARACTER },
    { "<?-pi ?>", NULL, NULL, WC_E_PI, WC_E_NAMECHARACTER },
    { "<?xml-stylesheet ?>", "xml-stylesheet", "", S_OK },
    { NULL }
};

static void test_read_pi(void)
{
    struct test_entry *test = pi_tests;
    IXmlReader *reader;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    while (test->xml)
    {
        XmlNodeType type;
        IStream *stream;

        stream = create_stream_on_data(test->xml, strlen(test->xml)+1);
        hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        type = XmlNodeType_None;
        hr = IXmlReader_Read(reader, &type);
        if (test->hr_broken)
            ok(hr == test->hr || broken(hr == test->hr_broken), "got %08x for %s\n", hr, test->xml);
        else
            ok(hr == test->hr, "got %08x for %s\n", hr, test->xml);
        if (hr == S_OK)
        {
            const WCHAR *str;
            WCHAR *str_exp;
            UINT len;

            ok(type == XmlNodeType_ProcessingInstruction, "got %d for %s\n", type, test->xml);

            len = 0;
            str = NULL;
            hr = IXmlReader_GetLocalName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == strlen(test->name), "got %u\n", len);
            str_exp = a2w(test->name);
            ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            free_str(str_exp);

            len = 0;
            str = NULL;
            hr = IXmlReader_GetQualifiedName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == strlen(test->name), "got %u\n", len);
            str_exp = a2w(test->name);
            ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            free_str(str_exp);

            /* value */
            len = !strlen(test->value);
            str = NULL;
            hr = IXmlReader_GetValue(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == strlen(test->value), "got %u\n", len);
            str_exp = a2w(test->value);
            ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            free_str(str_exp);
        }

        IStream_Release(stream);
        test++;
    }

    IXmlReader_Release(reader);
}

struct nodes_test {
    const char *xml;
    struct {
        XmlNodeType type;
        const char *value;
    } nodes[20];
};

static const char misc_test_xml[] =
    "<!-- comment1 -->"
    "<!-- comment2 -->"
    "<?pi1 pi1body ?>"
    "<!-- comment3 -->"
    " \t \r \n"
    "<!-- comment4 -->"
    "<a>"
    "\r\n\t"
    "<b/>"
    "text"
    "<!-- comment -->"
    "text2"
    "<?pi pibody ?>"
    "\r\n"
    "</a>"
;

static struct nodes_test misc_test = {
    misc_test_xml,
    {
        {XmlNodeType_Comment, " comment1 "},
        {XmlNodeType_Comment, " comment2 "},
        {XmlNodeType_ProcessingInstruction, "pi1body "},
        {XmlNodeType_Comment, " comment3 "},
        {XmlNodeType_Whitespace, " \t \n \n"},
        {XmlNodeType_Comment, " comment4 "},
        {XmlNodeType_Element, ""},
        {XmlNodeType_Whitespace, "\n\t"},
        {XmlNodeType_Element, ""},
        {XmlNodeType_Text, "text"},
        {XmlNodeType_Comment, " comment "},
        {XmlNodeType_Text, "text2"},
        {XmlNodeType_ProcessingInstruction, "pibody "},
        {XmlNodeType_Whitespace, "\n"},
        {XmlNodeType_EndElement, ""},
        {XmlNodeType_None, ""}
    }
};

static void test_read_full(void)
{
    struct nodes_test *test = &misc_test;
    IXmlReader *reader;
    const WCHAR *value;
    XmlNodeType type;
    IStream *stream;
    ULONG len;
    HRESULT hr;
    int i;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    stream = create_stream_on_data(test->xml, strlen(test->xml));
    hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    i = 0;
    type = ~0u;
    do
    {
        hr = IXmlReader_Read(reader, &type);
        if (test->nodes[i].type != XmlNodeType_None)
            ok(hr == S_OK, "Read returned %08x\n", hr);
        else
            ok(hr == S_FALSE, "Read returned %08x\n", hr);

        ok(type == test->nodes[i].type, "%d: got wrong type %d, expected %d\n", i, type, test->nodes[i].type);

        len = 0xdeadbeef;
        hr = IXmlReader_GetValue(reader, &value, &len);
        ok(hr == S_OK, "GetValue failed: %08x\n", hr);
        if (test->nodes[i].value)
        {
            ok(!strcmp_wa(value, test->nodes[i].value), "value = %s\n", wine_dbgstr_w(value));
            ok(len == strlen(test->nodes[i].value), "len = %u\n", len);
        }
    } while(test->nodes[i++].type != XmlNodeType_None);

    IStream_Release(stream);
    IXmlReader_Release(reader);
}

static const char test_public_dtd[] =
    "<!DOCTYPE testdtd PUBLIC \"pubid\" \"externalid uri\" >";

static void test_read_public_dtd(void)
{
    static const WCHAR sysvalW[] = {'e','x','t','e','r','n','a','l','i','d',' ','u','r','i',0};
    static const WCHAR pubvalW[] = {'p','u','b','i','d',0};
    static const WCHAR dtdnameW[] = {'t','e','s','t','d','t','d',0};
    static const WCHAR sysW[] = {'S','Y','S','T','E','M',0};
    static const WCHAR pubW[] = {'P','U','B','L','I','C',0};
    IXmlReader *reader;
    const WCHAR *str;
    XmlNodeType type;
    IStream *stream;
    UINT len, count;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_DtdProcessing, DtdProcessing_Parse);
    ok(hr == S_OK, "got 0x%8x\n", hr);

    stream = create_stream_on_data(test_public_dtd, sizeof(test_public_dtd));
    hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    type = XmlNodeType_None;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got 0x%8x\n", hr);
    ok(type == XmlNodeType_DocumentType, "got type %d\n", type);

    count = 0;
    hr = IXmlReader_GetAttributeCount(reader, &count);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(count == 2, "got %d\n", count);

    hr = IXmlReader_MoveToFirstAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);

    type = XmlNodeType_None;
    hr = IXmlReader_GetNodeType(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Attribute, "got %d\n", type);

    len = 0;
    str = NULL;
    hr = IXmlReader_GetLocalName(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(len == lstrlenW(pubW), "got %u\n", len);
    ok(!lstrcmpW(str, pubW), "got %s\n", wine_dbgstr_w(str));

    len = 0;
    str = NULL;
    hr = IXmlReader_GetValue(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(len == lstrlenW(pubvalW), "got %u\n", len);
    ok(!lstrcmpW(str, pubvalW), "got %s\n", wine_dbgstr_w(str));

    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);

    type = XmlNodeType_None;
    hr = IXmlReader_GetNodeType(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Attribute, "got %d\n", type);

    len = 0;
    str = NULL;
    hr = IXmlReader_GetLocalName(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(len == lstrlenW(sysW), "got %u\n", len);
    ok(!lstrcmpW(str, sysW), "got %s\n", wine_dbgstr_w(str));

    len = 0;
    str = NULL;
    hr = IXmlReader_GetValue(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(len == lstrlenW(sysvalW), "got %u\n", len);
    ok(!lstrcmpW(str, sysvalW), "got %s\n", wine_dbgstr_w(str));

    hr = IXmlReader_MoveToElement(reader);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    len = 0;
    str = NULL;
    hr = IXmlReader_GetLocalName(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
todo_wine {
    ok(len == lstrlenW(dtdnameW), "got %u\n", len);
    ok(!lstrcmpW(str, dtdnameW), "got %s\n", wine_dbgstr_w(str));
}
    len = 0;
    str = NULL;
    hr = IXmlReader_GetQualifiedName(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
todo_wine {
    ok(len == lstrlenW(dtdnameW), "got %u\n", len);
    ok(!lstrcmpW(str, dtdnameW), "got %s\n", wine_dbgstr_w(str));
}
    IStream_Release(stream);
    IXmlReader_Release(reader);
}

static const char test_system_dtd[] =
    "<!DOCTYPE testdtd SYSTEM \"externalid uri\" >"
    "<!-- comment -->";

static void test_read_system_dtd(void)
{
    static const WCHAR sysvalW[] = {'e','x','t','e','r','n','a','l','i','d',' ','u','r','i',0};
    static const WCHAR dtdnameW[] = {'t','e','s','t','d','t','d',0};
    static const WCHAR sysW[] = {'S','Y','S','T','E','M',0};
    IXmlReader *reader;
    const WCHAR *str;
    XmlNodeType type;
    IStream *stream;
    UINT len, count;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_DtdProcessing, DtdProcessing_Parse);
    ok(hr == S_OK, "got 0x%8x\n", hr);

    stream = create_stream_on_data(test_system_dtd, sizeof(test_system_dtd));
    hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    type = XmlNodeType_None;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got 0x%8x\n", hr);
    ok(type == XmlNodeType_DocumentType, "got type %d\n", type);

    count = 0;
    hr = IXmlReader_GetAttributeCount(reader, &count);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(count == 1, "got %d\n", count);

    hr = IXmlReader_MoveToFirstAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);

    type = XmlNodeType_None;
    hr = IXmlReader_GetNodeType(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Attribute, "got %d\n", type);

    len = 0;
    str = NULL;
    hr = IXmlReader_GetLocalName(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(len == lstrlenW(sysW), "got %u\n", len);
    ok(!lstrcmpW(str, sysW), "got %s\n", wine_dbgstr_w(str));

    len = 0;
    str = NULL;
    hr = IXmlReader_GetValue(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
    ok(len == lstrlenW(sysvalW), "got %u\n", len);
    ok(!lstrcmpW(str, sysvalW), "got %s\n", wine_dbgstr_w(str));

    hr = IXmlReader_MoveToElement(reader);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    len = 0;
    str = NULL;
    hr = IXmlReader_GetLocalName(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
todo_wine {
    ok(len == lstrlenW(dtdnameW), "got %u\n", len);
    ok(!lstrcmpW(str, dtdnameW), "got %s\n", wine_dbgstr_w(str));
}
    len = 0;
    str = NULL;
    hr = IXmlReader_GetQualifiedName(reader, &str, &len);
    ok(hr == S_OK, "got 0x%08x\n", hr);
todo_wine {
    ok(len == lstrlenW(dtdnameW), "got %u\n", len);
    ok(!lstrcmpW(str, dtdnameW), "got %s\n", wine_dbgstr_w(str));
}
    type = XmlNodeType_None;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got 0x%8x\n", hr);
    ok(type == XmlNodeType_Comment, "got type %d\n", type);

    IStream_Release(stream);
    IXmlReader_Release(reader);
}

static struct test_entry element_tests[] = {
    { "<a/>", "a", "", S_OK },
    { "<a />", "a", "", S_OK },
    { "<a:b/>", "a:b", "", NC_E_UNDECLAREDPREFIX },
    { "<:a/>", NULL, NULL, NC_E_QNAMECHARACTER },
    { "< a/>", NULL, NULL, NC_E_QNAMECHARACTER },
    { "<a>", "a", "", S_OK },
    { "<a >", "a", "", S_OK },
    { "<a \r \t\n>", "a", "", S_OK },
    { "</a>", NULL, NULL, NC_E_QNAMECHARACTER },
    { "<a:b:c />", NULL, NULL, NC_E_QNAMECOLON },
    { "<:b:c />", NULL, NULL, NC_E_QNAMECHARACTER },
    { NULL }
};

static void test_read_element(void)
{
    struct test_entry *test = element_tests;
    static const char stag[] =
         "<a attr1=\"_a\">"
             "<b attr2=\"_b\">"
                 "text"
                 "<c attr3=\"_c\"/>"
                 "<d attr4=\"_d\"></d>"
             "</b>"
         "</a>";
    static const UINT depths[] = { 0, 1, 2, 2, 2, 3, 2, 1 };
    static const char mismatch[] = "<a></b>";
    IXmlReader *reader;
    XmlNodeType type;
    IStream *stream;
    unsigned int i;
    UINT depth;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    while (test->xml)
    {
        stream = create_stream_on_data(test->xml, strlen(test->xml)+1);
        hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        type = XmlNodeType_None;
        hr = IXmlReader_Read(reader, &type);
        if (test->hr_broken)
            ok(hr == test->hr || broken(hr == test->hr_broken), "got %08x for %s\n", hr, test->xml);
        else
            todo_wine_if(test->hr == NC_E_UNDECLAREDPREFIX)
                ok(hr == test->hr, "got %08x for %s\n", hr, test->xml);
        if (hr == S_OK)
        {
            const WCHAR *str;
            WCHAR *str_exp;
            UINT len;

            ok(type == XmlNodeType_Element, "got %d for %s\n", type, test->xml);

            len = 0;
            str = NULL;
            hr = IXmlReader_GetQualifiedName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == strlen(test->name), "got %u\n", len);
            str_exp = a2w(test->name);
            ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            free_str(str_exp);

            /* value */
            len = 1;
            str = NULL;
            hr = IXmlReader_GetValue(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == 0, "got %u\n", len);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));
        }

        IStream_Release(stream);
        test++;
    }

    /* test reader depth increment */
    stream = create_stream_on_data(stag, sizeof(stag));
    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    i = 0;
    while (IXmlReader_Read(reader, &type) == S_OK)
    {
        UINT count;

        ok(type == XmlNodeType_Element || type == XmlNodeType_EndElement ||
                type == XmlNodeType_Text, "Unexpected node type %d\n", type);

        depth = 123;
        hr = IXmlReader_GetDepth(reader, &depth);
        ok(hr == S_OK, "got %08x\n", hr);
        ok(depth == depths[i], "%u: got depth %u, expected %u\n", i, depth, depths[i]);

        if (type == XmlNodeType_Element || type == XmlNodeType_EndElement)
        {
            const WCHAR *prefix;

            prefix = NULL;
            hr = IXmlReader_GetPrefix(reader, &prefix, NULL);
            ok(hr == S_OK, "got %08x\n", hr);
            ok(prefix != NULL, "got %p\n", prefix);

            if (!*prefix)
            {
                const WCHAR *local, *qname;

                local = NULL;
                hr = IXmlReader_GetLocalName(reader, &local, NULL);
                ok(hr == S_OK, "got %08x\n", hr);
                ok(local != NULL, "got %p\n", local);

                qname = NULL;
                hr = IXmlReader_GetQualifiedName(reader, &qname, NULL);
                ok(hr == S_OK, "got %08x\n", hr);
                ok(qname != NULL, "got %p\n", qname);

                ok(local == qname, "expected same pointer\n");
            }
        }

        if (type == XmlNodeType_EndElement)
        {
            count = 1;
            hr = IXmlReader_GetAttributeCount(reader, &count);
            ok(hr == S_OK, "got %08x\n", hr);
            ok(count == 0, "got %u\n", count);
        }

        if (type == XmlNodeType_Element)
        {
            count = 0;
            hr = IXmlReader_GetAttributeCount(reader, &count);
            ok(hr == S_OK, "got %08x\n", hr);

            /* moving to attributes increases depth */
            if (count)
            {
                const WCHAR *value;

                hr = IXmlReader_GetValue(reader, &value, NULL);
                ok(*value == 0, "Unexpected value %s\n", wine_dbgstr_w(value));

                hr = IXmlReader_MoveToFirstAttribute(reader);
                ok(hr == S_OK, "got %08x\n", hr);

                hr = IXmlReader_GetValue(reader, &value, NULL);
                ok(*value != 0, "Unexpected value %s\n", wine_dbgstr_w(value));

                depth = 123;
                hr = IXmlReader_GetDepth(reader, &depth);
                ok(hr == S_OK, "got %08x\n", hr);
                ok(depth == depths[i] + 1, "%u: got depth %u, expected %u\n", i, depth, depths[i] + 1);

                hr = IXmlReader_MoveToElement(reader);
                ok(hr == S_OK, "got %08x\n", hr);

                hr = IXmlReader_GetValue(reader, &value, NULL);
                ok(*value == 0, "Unexpected value %s\n", wine_dbgstr_w(value));

                depth = 123;
                hr = IXmlReader_GetDepth(reader, &depth);
                ok(hr == S_OK, "got %08x\n", hr);
                ok(depth == depths[i], "%u: got depth %u, expected %u\n", i, depth, depths[i]);
            }
        }

        i++;
    }

    IStream_Release(stream);

    /* start/end tag mismatch */
    stream = create_stream_on_data(mismatch, sizeof(mismatch));
    hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    type = XmlNodeType_None;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Element, "got %d\n", type);

    type = XmlNodeType_Element;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == WC_E_ELEMENTMATCH, "got %08x\n", hr);
    ok(type == XmlNodeType_None, "got %d\n", type);
    TEST_READER_STATE(reader, XmlReadState_Error);

    IStream_Release(stream);

    IXmlReader_Release(reader);
}

static ISequentialStream teststream = { &teststreamvtbl };

static void test_read_pending(void)
{
    IXmlReader *reader;
    const WCHAR *value;
    XmlNodeType type;
    HRESULT hr;
    int c;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got 0x%08x\n", hr);

    hr = IXmlReader_SetInput(reader, (IUnknown*)&teststream);
    ok(hr == S_OK, "got 0x%08x\n", hr);

    /* first read call returns incomplete node, second attempt fails with E_PENDING */
    stream_readcall = 0;
    type = XmlNodeType_Element;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK || broken(hr == E_PENDING), "got 0x%08x\n", hr);
    /* newer versions are happy when it's enough data to detect node type,
       older versions keep reading until it fails to read more */
todo_wine
    ok(stream_readcall == 1 || broken(stream_readcall > 1), "got %d\n", stream_readcall);
    ok(type == XmlNodeType_Comment || broken(type == XmlNodeType_None), "got %d\n", type);

    /* newer versions' GetValue() makes an attempt to read more */
    c = stream_readcall;
    value = (void*)0xdeadbeef;
    hr = IXmlReader_GetValue(reader, &value, NULL);
    ok(hr == E_PENDING, "got 0x%08x\n", hr);
    ok(value == NULL || broken(value == (void*)0xdeadbeef) /* Win8 sets it to NULL */, "got %p\n", value);
    ok(c < stream_readcall || broken(c == stream_readcall), "got %d, expected %d\n", stream_readcall, c+1);

    IXmlReader_Release(reader);
}

static void test_readvaluechunk(void)
{
    static const char testA[] = "<!-- comment1 --><!-- comment2 -->";
    IXmlReader *reader;
    XmlNodeType type;
    IStream *stream;
    const WCHAR *value;
    WCHAR buf[64];
    WCHAR b;
    HRESULT hr;
    UINT c;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    stream = create_stream_on_data(testA, sizeof(testA));
    hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Comment, "type = %u\n", type);

    c = 0;
    b = 0;
    hr = IXmlReader_ReadValueChunk(reader, &b, 1, &c);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(c == 1, "got %u\n", c);
    ok(b == ' ', "got %x\n", b);

    c = 0;
    b = 0xffff;
    hr = IXmlReader_ReadValueChunk(reader, &b, 1, &c);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(c == 1, "got %u\n", c);
    ok(b == 'c', "got %x\n", b);

    /* portion read as chunk is skipped from resulting node value */
    value = NULL;
    hr = IXmlReader_GetValue(reader, &value, NULL);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!strcmp_wa(value, "omment1 "), "got %s\n", wine_dbgstr_w(value));

    /* once value is returned/allocated it's not possible to read by chunk */
    c = 0;
    b = 0;
    hr = IXmlReader_ReadValueChunk(reader, &b, 1, &c);
    ok(hr == S_FALSE, "got %08x\n", hr);
    ok(c == 0, "got %u\n", c);
    ok(b == 0, "got %x\n", b);

    c = 0xdeadbeef;
    hr = IXmlReader_ReadValueChunk(reader, buf, 0, &c);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!c, "c = %u\n", c);

    value = NULL;
    hr = IXmlReader_GetValue(reader, &value, NULL);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!strcmp_wa(value, "omment1 "), "got %s\n", wine_dbgstr_w(value));

    /* read comment2 */
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Comment, "type = %u\n", type);

    c = 0xdeadbeef;
    hr = IXmlReader_ReadValueChunk(reader, buf, 0, &c);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!c, "c = %u\n", c);

    c = 0xdeadbeef;
    memset(buf, 0xcc, sizeof(buf));
    hr = IXmlReader_ReadValueChunk(reader, buf, sizeof(buf)/sizeof(WCHAR), &c);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(c == 10, "got %u\n", c);
    ok(buf[c] == 0xcccc, "buffer overflow\n");
    buf[c] = 0;
    ok(!strcmp_wa(buf, " comment2 "), "buf = %s\n", wine_dbgstr_w(buf));

    c = 0xdeadbeef;
    memset(buf, 0xcc, sizeof(buf));
    hr = IXmlReader_ReadValueChunk(reader, buf, sizeof(buf)/sizeof(WCHAR), &c);
    ok(hr == S_FALSE, "got %08x\n", hr);
    ok(!c, "got %u\n", c);

    /* portion read as chunk is skipped from resulting node value */
    value = NULL;
    hr = IXmlReader_GetValue(reader, &value, NULL);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!*value, "got %s\n", wine_dbgstr_w(value));

    /* once value is returned/allocated it's not possible to read by chunk */
    c = 0xdeadbeef;
    b = 0xffff;
    hr = IXmlReader_ReadValueChunk(reader, &b, 1, &c);
    ok(hr == S_FALSE, "got %08x\n", hr);
    ok(c == 0, "got %u\n", c);
    ok(b == 0xffff, "got %x\n", b);

    value = NULL;
    hr = IXmlReader_GetValue(reader, &value, NULL);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!*value, "got %s\n", wine_dbgstr_w(value));

    IXmlReader_Release(reader);
    IStream_Release(stream);
}

static struct test_entry cdata_tests[] = {
    { "<a><![CDATA[ ]]data ]]></a>", "", " ]]data ", S_OK },
    { "<a><![CDATA[<![CDATA[ data ]]]]></a>", "", "<![CDATA[ data ]]", S_OK },
    { "<a><![CDATA[\n \r\n \n\n ]]></a>", "", "\n \n \n\n ", S_OK, S_OK },
    { "<a><![CDATA[\r \r\r\n \n\n ]]></a>", "", "\n \n\n \n\n ", S_OK, S_OK },
    { "<a><![CDATA[\r\r \n\r \r \n\n ]]></a>", "", "\n\n \n\n \n \n\n ", S_OK },
    { NULL }
};

static void test_read_cdata(void)
{
    struct test_entry *test = cdata_tests;
    IXmlReader *reader;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    while (test->xml)
    {
        XmlNodeType type;
        IStream *stream;

        stream = create_stream_on_data(test->xml, strlen(test->xml)+1);
        hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        type = XmlNodeType_None;
        hr = IXmlReader_Read(reader, &type);

        /* read one more to get to CDATA */
        if (type == XmlNodeType_Element)
        {
            type = XmlNodeType_None;
            hr = IXmlReader_Read(reader, &type);
        }

        if (test->hr_broken)
            ok(hr == test->hr || broken(hr == test->hr_broken), "got %08x for %s\n", hr, test->xml);
        else
            ok(hr == test->hr, "got %08x for %s\n", hr, test->xml);
        if (hr == S_OK)
        {
            const WCHAR *str;
            WCHAR *str_exp;
            UINT len;

            ok(type == XmlNodeType_CDATA, "got %d for %s\n", type, test->xml);

            len = 1;
            str = NULL;
            hr = IXmlReader_GetLocalName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == 0, "got %u\n", len);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            str = NULL;
            hr = IXmlReader_GetLocalName(reader, &str, NULL);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            len = 1;
            str = NULL;
            hr = IXmlReader_GetQualifiedName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == 0, "got %u\n", len);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            str = NULL;
            hr = IXmlReader_GetQualifiedName(reader, &str, NULL);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            /* value */
            len = 1;
            str = NULL;
            hr = IXmlReader_GetValue(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);

            str_exp = a2w(test->value);
            todo_wine_if (test->todo)
            {
                ok(len == strlen(test->value), "got %u\n", len);
                ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            }
            free_str(str_exp);
        }

        IStream_Release(stream);
        test++;
    }

    IXmlReader_Release(reader);
}

static struct test_entry text_tests[] = {
    { "<a>simple text</a>", "", "simple text", S_OK },
    { "<a>text ]]> text</a>", "", "", WC_E_CDSECTEND },
    { "<a>\n \r\n \n\n text</a>", "", "\n \n \n\n text", S_OK, S_OK },
    { "<a>\r \r\r\n \n\n text</a>", "", "\n \n\n \n\n text", S_OK, S_OK },
    { NULL }
};

static void test_read_text(void)
{
    struct test_entry *test = text_tests;
    IXmlReader *reader;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    while (test->xml)
    {
        XmlNodeType type;
        IStream *stream;

        stream = create_stream_on_data(test->xml, strlen(test->xml)+1);
        hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        type = XmlNodeType_None;
        hr = IXmlReader_Read(reader, &type);

        /* read one more to get to text node */
        if (type == XmlNodeType_Element)
        {
            type = XmlNodeType_None;
            hr = IXmlReader_Read(reader, &type);
        }
        ok(hr == test->hr, "got %08x for %s\n", hr, test->xml);
        if (hr == S_OK)
        {
            const WCHAR *str;
            WCHAR *str_exp;
            UINT len;

            ok(type == XmlNodeType_Text, "got %d for %s\n", type, test->xml);

            len = 1;
            str = NULL;
            hr = IXmlReader_GetLocalName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == 0, "got %u\n", len);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            str = NULL;
            hr = IXmlReader_GetLocalName(reader, &str, NULL);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            len = 1;
            str = NULL;
            hr = IXmlReader_GetQualifiedName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == 0, "got %u\n", len);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            str = NULL;
            hr = IXmlReader_GetQualifiedName(reader, &str, NULL);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(*str == 0, "got %s\n", wine_dbgstr_w(str));

            /* value */
            len = 1;
            str = NULL;
            hr = IXmlReader_GetValue(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);

            str_exp = a2w(test->value);
            todo_wine_if (test->todo)
            {
                ok(len == strlen(test->value), "got %u\n", len);
                ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            }
            free_str(str_exp);
        }

        IStream_Release(stream);
        test++;
    }

    IXmlReader_Release(reader);
}

struct test_entry_empty {
    const char *xml;
    BOOL empty;
};

static struct test_entry_empty empty_element_tests[] = {
    { "<a></a>", FALSE },
    { "<a/>", TRUE },
    { NULL }
};

static void test_isemptyelement(void)
{
    struct test_entry_empty *test = empty_element_tests;
    IXmlReader *reader;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    while (test->xml)
    {
        XmlNodeType type;
        IStream *stream;
        BOOL ret;

        stream = create_stream_on_data(test->xml, strlen(test->xml)+1);
        hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        type = XmlNodeType_None;
        hr = IXmlReader_Read(reader, &type);
        ok(hr == S_OK, "got 0x%08x\n", hr);
        ok(type == XmlNodeType_Element, "got %d\n", type);

        ret = IXmlReader_IsEmptyElement(reader);
        ok(ret == test->empty, "got %d, expected %d. xml=%s\n", ret, test->empty, test->xml);

        IStream_Release(stream);
        test++;
    }

    IXmlReader_Release(reader);
}

static struct test_entry attributes_tests[] = {
    { "<a attr1=\"attrvalue\"/>", "attr1", "attrvalue", S_OK },
    { "<a attr1=\"a\'\'ttrvalue\"/>", "attr1", "a\'\'ttrvalue", S_OK },
    { "<a attr1=\'a\"ttrvalue\'/>", "attr1", "a\"ttrvalue", S_OK },
    { "<a attr1=\' \'/>", "attr1", " ", S_OK },
    { "<a attr1=\" \"/>", "attr1", " ", S_OK },
    { "<a attr1=\"\r\n \r \n \t\n\r\"/>", "attr1", "         ", S_OK },
    { "<a attr1=\" val \"/>", "attr1", " val ", S_OK },
    { "<a attr1=\"\r\n\tval\n\"/>", "attr1", "  val ", S_OK },
    { "<a attr1=\"val&#32;\"/>", "attr1", "val ", S_OK },
    { "<a attr1=\"val&#x20;\"/>", "attr1", "val ", S_OK },
    { "<a attr1=\"&lt;&gt;&amp;&apos;&quot;\"/>", "attr1", "<>&\'\"", S_OK },
    { "<a attr1=\"&entname;\"/>", NULL, NULL, WC_E_UNDECLAREDENTITY },
    { "<a attr1=\"val&#xfffe;\"/>", NULL, NULL, WC_E_XMLCHARACTER },
    { "<a attr1=\"val &#a;\"/>", NULL, NULL, WC_E_DIGIT, WC_E_SEMICOLON },
    { "<a attr1=\"val &#12a;\"/>", NULL, NULL, WC_E_SEMICOLON },
    { "<a attr1=\"val &#x12g;\"/>", NULL, NULL, WC_E_SEMICOLON },
    { "<a attr1=\"val &#xg;\"/>", NULL, NULL, WC_E_HEXDIGIT, WC_E_SEMICOLON },
    { "<a attr1=attrvalue/>", NULL, NULL, WC_E_QUOTE },
    { "<a attr1=\"attr<value\"/>", NULL, NULL, WC_E_LESSTHAN },
    { "<a attr1=\"&entname\"/>", NULL, NULL, WC_E_SEMICOLON },
    { NULL }
};

static void test_read_attribute(void)
{
    struct test_entry *test = attributes_tests;
    IXmlReader *reader;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    while (test->xml)
    {
        XmlNodeType type;
        IStream *stream;

        stream = create_stream_on_data(test->xml, strlen(test->xml)+1);
        hr = IXmlReader_SetInput(reader, (IUnknown*)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        hr = IXmlReader_Read(reader, NULL);

        if (test->hr_broken)
            ok(hr == test->hr || broken(hr == test->hr_broken), "got %08x for %s\n", hr, test->xml);
        else
            ok(hr == test->hr, "got %08x for %s\n", hr, test->xml);
        if (hr == S_OK)
        {
            const WCHAR *str;
            WCHAR *str_exp;
            UINT len;

            type = XmlNodeType_None;
            hr = IXmlReader_GetNodeType(reader, &type);
            ok(hr == S_OK, "Failed to get node type, %#x\n", hr);

            ok(type == XmlNodeType_Element, "got %d for %s\n", type, test->xml);

            hr = IXmlReader_MoveToFirstAttribute(reader);
            ok(hr == S_OK, "got 0x%08x\n", hr);

            len = 1;
            str = NULL;
            hr = IXmlReader_GetLocalName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == strlen(test->name), "got %u\n", len);
            str_exp = a2w(test->name);
            ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            free_str(str_exp);

            len = 1;
            str = NULL;
            hr = IXmlReader_GetQualifiedName(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == strlen(test->name), "got %u\n", len);
            str_exp = a2w(test->name);
            ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            free_str(str_exp);

            /* value */
            len = 1;
            str = NULL;
            hr = IXmlReader_GetValue(reader, &str, &len);
            ok(hr == S_OK, "got 0x%08x\n", hr);
            ok(len == strlen(test->value), "got %u\n", len);
            str_exp = a2w(test->value);
            ok(!lstrcmpW(str, str_exp), "got %s\n", wine_dbgstr_w(str));
            free_str(str_exp);
        }

        IStream_Release(stream);
        test++;
    }

    IXmlReader_Release(reader);
}

static void test_reader_properties(void)
{
    IXmlReader *reader;
    LONG_PTR value;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    value = 0;
    hr = IXmlReader_GetProperty(reader, XmlReaderProperty_MaxElementDepth, &value);
    ok(hr == S_OK, "GetProperty failed: %08x\n", hr);
    ok(value == 256, "Unexpected default max depth value %ld\n", value);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_MultiLanguage, 0);
    ok(hr == S_OK, "SetProperty failed: %08x\n", hr);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_MaxElementDepth, 0);
    ok(hr == S_OK, "SetProperty failed: %08x\n", hr);

    value = 256;
    hr = IXmlReader_GetProperty(reader, XmlReaderProperty_MaxElementDepth, &value);
    ok(hr == S_OK, "GetProperty failed: %08x\n", hr);
    ok(value == 0, "Unexpected max depth value %ld\n", value);

    IXmlReader_Release(reader);
}

static void test_prefix(void)
{
    static const struct
    {
        const char *xml;
        const char *prefix1;
        const char *prefix2;
        const char *prefix3;
    } prefix_tests[] =
    {
        { "<b xmlns=\"defns\" xml:a=\"a ns\"/>", "", "", "xml" },
        { "<c:b xmlns:c=\"c ns\" xml:a=\"a ns\"/>", "c", "xmlns", "xml" },
    };
    IXmlReader *reader;
    unsigned int i;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    for (i = 0; i < sizeof(prefix_tests)/sizeof(prefix_tests[0]); i++) {
        const WCHAR *prefix;
        XmlNodeType type;
        WCHAR *expected;
        IStream *stream;

        stream = create_stream_on_data(prefix_tests[i].xml, strlen(prefix_tests[i].xml) + 1);
        hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        hr = IXmlReader_Read(reader, &type);
        ok(hr == S_OK, "Read() failed, %#x\n", hr);
        ok(type == XmlNodeType_Element, "Unexpected node type %d.\n", type);

        expected = a2w(prefix_tests[i].prefix1);
        hr = IXmlReader_GetPrefix(reader, &prefix, NULL);
        ok(hr == S_OK, "GetPrefix() failed, %#x.\n", hr);
        ok(!lstrcmpW(prefix, expected), "Unexpected prefix %s, expected %s.\n", wine_dbgstr_w(prefix),
            wine_dbgstr_w(expected));
        free_str(expected);

        hr = IXmlReader_MoveToFirstAttribute(reader);
        ok(hr == S_OK, "MoveToFirstAttribute() failed, %#x.\n", hr);

        hr = IXmlReader_GetNodeType(reader, &type);
        ok(hr == S_OK, "GetNodeType() failed, %#x.\n", hr);
        ok(type == XmlNodeType_Attribute, "Unexpected node type %d.\n", type);

        expected = a2w(prefix_tests[i].prefix2);
        hr = IXmlReader_GetPrefix(reader, &prefix, NULL);
        ok(hr == S_OK, "GetPrefix() failed, %#x.\n", hr);
        ok(!lstrcmpW(prefix, expected), "Unexpected prefix %s, expected %s.\n", wine_dbgstr_w(prefix),
            wine_dbgstr_w(expected));
        free_str(expected);

        hr = IXmlReader_MoveToNextAttribute(reader);
        ok(hr == S_OK, "MoveToNextAttribute() failed, %#x.\n", hr);

        hr = IXmlReader_GetNodeType(reader, &type);
        ok(hr == S_OK, "GetNodeType() failed, %#x.\n", hr);
        ok(type == XmlNodeType_Attribute, "Unexpected node type %d.\n", type);

        expected = a2w(prefix_tests[i].prefix3);
        hr = IXmlReader_GetPrefix(reader, &prefix, NULL);
        ok(hr == S_OK, "GetPrefix() failed, %#x.\n", hr);
        ok(!lstrcmpW(prefix, expected), "Unexpected prefix %s, expected %s.\n", wine_dbgstr_w(prefix),
            wine_dbgstr_w(expected));
        free_str(expected);

        /* back to the element, check prefix */
        hr = IXmlReader_MoveToElement(reader);
        ok(hr == S_OK, "MoveToElement() failed, %#x.\n", hr);

        expected = a2w(prefix_tests[i].prefix1);
        hr = IXmlReader_GetPrefix(reader, &prefix, NULL);
        ok(hr == S_OK, "GetPrefix() failed, %#x.\n", hr);
        ok(!lstrcmpW(prefix, expected), "Unexpected prefix %s, expected %s.\n", wine_dbgstr_w(prefix),
            wine_dbgstr_w(expected));
        free_str(expected);

        IStream_Release(stream);
    }

    IXmlReader_Release(reader);
}

static void test_namespaceuri(void)
{
    struct uri_test
    {
        const char *xml;
        const char *uri[5];
    } uri_tests[] =
    {
        { "<a xmlns=\"defns a\"><b xmlns=\"defns b\"><c xmlns=\"defns c\"/></b></a>",
                { "defns a", "defns b", "defns c", "defns b", "defns a" }},
        { "<r:a xmlns=\"defns a\" xmlns:r=\"ns r\"/>",
                { "ns r" }},
        { "<r:a xmlns=\"defns a\" xmlns:r=\"ns r\"><b/></r:a>",
                { "ns r", "defns a", "ns r" }},
        { "<a xmlns=\"defns a\" xmlns:r=\"ns r\"><r:b/></a>",
                { "defns a", "ns r", "defns a" }},
        { "<a><b><c/></b></a>",
                { "", "", "", "", "" }},
        { "<a>text</a>",
                { "", "", "" }},
        { "<a>\r\n</a>",
                { "", "", "" }},
        { "<a><![CDATA[data]]></a>",
                { "", "", "" }},
        { "<?xml version=\"1.0\" ?><a/>",
                { "", "" }},
        { "<a><?pi ?></a>",
                { "", "", "" }},
        { "<a><!-- comment --></a>",
                { "", "", "" }},
    };
    IXmlReader *reader;
    XmlNodeType type;
    unsigned int i;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void**)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    for (i = 0; i < sizeof(uri_tests)/sizeof(uri_tests[0]); i++) {
        IStream *stream = create_stream_on_data(uri_tests[i].xml, strlen(uri_tests[i].xml) + 1);
        unsigned int j = 0;

        hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        type = ~0u;
        while (IXmlReader_Read(reader, &type) == S_OK) {
            const WCHAR *uri, *local, *qname;
            UINT length, length2;
            WCHAR *uriW;

            ok(type == XmlNodeType_Element ||
                    type == XmlNodeType_Text ||
                    type == XmlNodeType_CDATA ||
                    type == XmlNodeType_ProcessingInstruction ||
                    type == XmlNodeType_Comment ||
                    type == XmlNodeType_Whitespace ||
                    type == XmlNodeType_EndElement ||
                    type == XmlNodeType_XmlDeclaration, "Unexpected node type %d.\n", type);

            local = NULL;
            length = 0;
            hr = IXmlReader_GetLocalName(reader, &local, &length);
            ok(hr == S_OK, "S_OK, got %08x\n", hr);
            ok(local != NULL, "Unexpected NULL local name pointer\n");

            qname = NULL;
            length2 = 0;
            hr = IXmlReader_GetQualifiedName(reader, &qname, &length2);
            ok(hr == S_OK, "S_OK, got %08x\n", hr);
            ok(qname != NULL, "Unexpected NULL qualified name pointer\n");

            if (type == XmlNodeType_Element ||
                    type == XmlNodeType_EndElement ||
                    type == XmlNodeType_ProcessingInstruction ||
                    type == XmlNodeType_XmlDeclaration)
            {
                ok(*local != 0, "Unexpected empty local name\n");
                ok(length > 0, "Unexpected local name length\n");

                ok(*qname != 0, "Unexpected empty qualified name\n");
                ok(length2 > 0, "Unexpected qualified name length\n");
            }

            uri = NULL;
            hr = IXmlReader_GetNamespaceUri(reader, &uri, NULL);
            ok(hr == S_OK, "S_OK, got %08x\n", hr);
            ok(uri != NULL, "Unexpected NULL uri pointer\n");

            uriW = a2w(uri_tests[i].uri[j]);
            ok(!lstrcmpW(uriW, uri), "%s: uri %s\n", wine_dbgstr_w(local), wine_dbgstr_w(uri));
            free_str(uriW);

            j++;
        }
        ok(type == XmlNodeType_None, "Unexpected node type %d\n", type);

        IStream_Release(stream);
    }

    IXmlReader_Release(reader);
}

static void test_read_charref(void)
{
    static const char testA[] = "<a b=\"c\">&#x1f3;&#x103;&gt;</a>";
    static const WCHAR chardataW[] = {0x01f3,0x0103,'>',0};
    const WCHAR *value;
    IXmlReader *reader;
    XmlNodeType type;
    IStream *stream;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    stream = create_stream_on_data(testA, sizeof(testA)-1);
    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Element, "Unexpected node type %d\n", type);

    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Text, "Unexpected node type %d\n", type);

    hr = IXmlReader_GetValue(reader, &value, NULL);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(!lstrcmpW(value, chardataW), "Text value : %s\n", wine_dbgstr_w(value));

    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_EndElement, "Unexpected node type %d\n", type);

    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_FALSE, "got %08x\n", hr);
    ok(type == XmlNodeType_None, "Unexpected node type %d\n", type);

    IXmlReader_Release(reader);
    IStream_Release(stream);
}

static void test_encoding_detection(void)
{
    static const struct encoding_testW
    {
        WCHAR text[16];
    }
    encoding_testsW[] =
    {
        { { '<','?','p','i',' ','?','>',0 } },
        { { '<','!','-','-',' ','c','-','-','>',0 } },
        { { 0xfeff,'<','a','/','>',0 } },
        { { '<','a','/','>',0 } },
    };
    static const char *encoding_testsA[] =
    {
        "<?pi ?>",
        "<!-- comment -->",
        "\xef\xbb\xbf<a/>", /* UTF-8 BOM */
        "<a/>",
    };
    IXmlReader *reader;
    XmlNodeType type;
    IStream *stream;
    unsigned int i;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    /* there's no way to query detected encoding back, so just verify that document is browsable */

    for (i = 0; i < sizeof(encoding_testsA)/sizeof(encoding_testsA[0]); i++)
    {
        stream = create_stream_on_data(encoding_testsA[i], strlen(encoding_testsA[i]));

        hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        type = XmlNodeType_None;
        hr = IXmlReader_Read(reader, &type);
        ok(hr == S_OK, "got %08x\n", hr);
        ok(type != XmlNodeType_None, "Unexpected node type %d\n", type);

        IStream_Release(stream);
    }

    for (i = 0; i < sizeof(encoding_testsW)/sizeof(encoding_testsW[0]); i++)
    {
        stream = create_stream_on_data(encoding_testsW[i].text, lstrlenW(encoding_testsW[i].text) * sizeof(WCHAR));

        hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
        ok(hr == S_OK, "got %08x\n", hr);

        type = XmlNodeType_None;
        hr = IXmlReader_Read(reader, &type);
        ok(hr == S_OK, "%u: got %08x\n", i, hr);
        ok(type != XmlNodeType_None, "%u: unexpected node type %d\n", i, type);

        IStream_Release(stream);
    }

    IXmlReader_Release(reader);
}

static void test_eof_state(IXmlReader *reader, BOOL eof)
{
    LONG_PTR state;
    HRESULT hr;

    ok(IXmlReader_IsEOF(reader) == eof, "Unexpected IsEOF() result\n");
    hr = IXmlReader_GetProperty(reader, XmlReaderProperty_ReadState, &state);
    ok(hr == S_OK, "GetProperty() failed, %#x\n", hr);
    ok((state == XmlReadState_EndOfFile) == eof, "Unexpected EndOfFile state %ld\n", state);
}

static void test_endoffile(void)
{
    static const char *xml = "<a/>";
    static const char *xml_garbageend = "<a/>text";
    IXmlReader *reader;
    XmlNodeType type;
    IStream *stream;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    test_eof_state(reader, FALSE);

    stream = create_stream_on_data(xml, strlen(xml));

    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    test_eof_state(reader, FALSE);

    type = XmlNodeType_None;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %#x\n", hr);
    ok(type == XmlNodeType_Element, "Unexpected type %d\n", type);

    test_eof_state(reader, FALSE);

    type = XmlNodeType_Element;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_FALSE, "got %#x\n", hr);
    ok(type == XmlNodeType_None, "Unexpected type %d\n", type);

    test_eof_state(reader, TRUE);

    hr = IXmlReader_SetInput(reader, NULL);
    ok(hr == S_OK, "got %08x\n", hr);

    test_eof_state(reader, FALSE);

    IStream_Release(stream);

    IXmlReader_Release(reader);

    hr = CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    stream = create_stream_on_data(xml_garbageend, strlen(xml_garbageend));
    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "got %08x\n", hr);
    IStream_Release(stream);

    type = XmlNodeType_None;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %#x\n", hr);
    ok(type == XmlNodeType_Element, "Unexpected type %d\n", type);

    test_eof_state(reader, FALSE);

    type = XmlNodeType_Element;
    hr = IXmlReader_Read(reader, &type);
    ok(hr == WC_E_SYNTAX, "got %#x\n", hr);
    ok(type == XmlNodeType_None, "Unexpected type %d\n", type);

    test_eof_state(reader, FALSE);

    hr = IXmlReader_SetInput(reader, NULL);
    ok(hr == S_OK, "got %08x\n", hr);

    IXmlReader_Release(reader);
}

static void test_max_element_depth(void)
{
    static const char *xml =
        "<a>"
            "<b attrb=\"_b\">"
                "<c>"
                   "<d></d>"
                "</c>"
            "</b>"
        "</a>";
    XmlNodeType nodetype;
    unsigned int count;
    IXmlReader *reader;
    IStream *stream;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    stream = create_stream_on_data(xml, strlen(xml));

    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_MaxElementDepth, 2);
    ok(hr == S_OK, "got %08x\n", hr);

    TEST_DEPTH(reader, 0);

    hr = IXmlReader_Read(reader, NULL);
    ok(hr == S_OK, "got %08x\n", hr);

    TEST_DEPTH(reader, 0);

    hr = IXmlReader_Read(reader, NULL);
    ok(hr == S_OK, "got %08x\n", hr);

    TEST_DEPTH(reader, 1);
    TEST_READER_STATE(reader, XmlReadState_Interactive);

    hr = IXmlReader_Read(reader, NULL);
    ok(hr == SC_E_MAXELEMENTDEPTH, "got %08x\n", hr);

    TEST_DEPTH2(reader, 0, 2);
    TEST_READER_STATE(reader, XmlReadState_Error);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_MaxElementDepth, 10);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = IXmlReader_Read(reader, NULL);
    ok(hr == SC_E_MAXELEMENTDEPTH, "got %08x\n", hr);

    TEST_DEPTH2(reader, 0, 2);
    TEST_READER_STATE(reader, XmlReadState_Error);
    IStream_Release(stream);

    /* test if stepping into attributes enforces depth limit too */
    stream = create_stream_on_data(xml, strlen(xml));

    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_MaxElementDepth, 2);
    ok(hr == S_OK, "got %08x\n", hr);

    TEST_DEPTH(reader, 0);

    hr = IXmlReader_Read(reader, NULL);
    ok(hr == S_OK, "got %08x\n", hr);

    TEST_DEPTH(reader, 0);

    hr = IXmlReader_Read(reader, NULL);
    ok(hr == S_OK, "got %08x\n", hr);

    TEST_DEPTH(reader, 1);

    hr = IXmlReader_MoveToFirstAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);

    TEST_DEPTH(reader, 2);
    TEST_READER_STATE(reader, XmlReadState_Interactive);

    nodetype = 123;
    hr = IXmlReader_Read(reader, &nodetype);
    ok(hr == SC_E_MAXELEMENTDEPTH, "got %08x\n", hr);
    ok(nodetype == XmlNodeType_None, "got node type %d\n", nodetype);

    nodetype = 123;
    hr = IXmlReader_Read(reader, &nodetype);
    ok(hr == SC_E_MAXELEMENTDEPTH, "got %08x\n", hr);
    ok(nodetype == XmlNodeType_None, "got node type %d\n", nodetype);

    TEST_DEPTH2(reader, 0, 2);
    TEST_READER_STATE(reader, XmlReadState_Error);

    IStream_Release(stream);

    /* set max depth to 0, this disables depth limit */
    stream = create_stream_on_data(xml, strlen(xml));

    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    hr = IXmlReader_SetProperty(reader, XmlReaderProperty_MaxElementDepth, 0);
    ok(hr == S_OK, "got %08x\n", hr);

    count = 0;
    while (IXmlReader_Read(reader, NULL) == S_OK)
        count++;
    ok(count == 8, "Unexpected node number %u\n", count);
    TEST_READER_STATE(reader, XmlReadState_EndOfFile);

    IStream_Release(stream);

    IXmlReader_Release(reader);
}

static void test_reader_position(void)
{
    static const char *xml = "<c:a xmlns:c=\"nsdef c\" b=\"attr b\">\n</c:a>";
    IXmlReader *reader;
    XmlNodeType type;
    IStream *stream;
    UINT position;
    HRESULT hr;

    hr = CreateXmlReader(&IID_IXmlReader, (void **)&reader, NULL);
    ok(hr == S_OK, "S_OK, got %08x\n", hr);

    TEST_READER_STATE(reader, XmlReadState_Closed);

    /* position methods with Null args */
    hr = IXmlReader_GetLineNumber(reader, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);

    hr = IXmlReader_GetLinePosition(reader, NULL);
    ok(hr == E_INVALIDARG, "Expected E_INVALIDARG, got %08x\n", hr);

    position = 123;
    hr = IXmlReader_GetLinePosition(reader, &position);
    ok(hr == S_FALSE, "got %#x\n", hr);
    ok(position == 0, "got %u\n", position);

    position = 123;
    hr = IXmlReader_GetLineNumber(reader, &position);
    ok(hr == S_FALSE, "got %#x\n", hr);
    ok(position == 0, "got %u\n", position);

    stream = create_stream_on_data(xml, strlen(xml));

    hr = IXmlReader_SetInput(reader, (IUnknown *)stream);
    ok(hr == S_OK, "got %08x\n", hr);

    TEST_READER_STATE(reader, XmlReadState_Initial);
    TEST_READER_POSITION(reader, 0, 0);
    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Element, "got type %d\n", type);
    TEST_READER_POSITION2(reader, 1, 2, ~0u, 34);

    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);
    TEST_READER_POSITION2(reader, 1, 6, ~0u, 34);

    hr = IXmlReader_MoveToNextAttribute(reader);
    ok(hr == S_OK, "got %08x\n", hr);
    TEST_READER_POSITION2(reader, 1, 24, ~0u, 34);

    hr = IXmlReader_MoveToElement(reader);
    ok(hr == S_OK, "got %08x\n", hr);
    TEST_READER_POSITION2(reader, 1, 2, ~0u, 34);

    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_Whitespace, "got type %d\n", type);
    TEST_READER_POSITION2(reader, 1, 35, 2, 6);

    hr = IXmlReader_Read(reader, &type);
    ok(hr == S_OK, "got %08x\n", hr);
    ok(type == XmlNodeType_EndElement, "got type %d\n", type);
    TEST_READER_POSITION2(reader, 2, 3, 2, 6);

    hr = IXmlReader_SetInput(reader, NULL);
    ok(hr == S_OK, "got %08x\n", hr);
    TEST_READER_STATE2(reader, XmlReadState_Initial, XmlReadState_Closed);
    TEST_READER_POSITION(reader, 0, 0);

    IStream_Release(stream);
    IXmlReader_Release(reader);
}

START_TEST(reader)
{
    test_reader_create();
    test_readerinput();
    test_reader_state();
    test_read_attribute();
    test_read_cdata();
    test_read_comment();
    test_read_pi();
    test_read_system_dtd();
    test_read_public_dtd();
    test_read_element();
    test_isemptyelement();
    test_read_text();
    test_read_full();
    test_read_pending();
    test_readvaluechunk();
    test_read_xmldeclaration();
    test_reader_properties();
    test_prefix();
    test_namespaceuri();
    test_read_charref();
    test_encoding_detection();
    test_endoffile();
    test_max_element_depth();
    test_reader_position();
}
