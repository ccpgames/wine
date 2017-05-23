/*
 * Copyright 2015, 2016 Hans Leidekker for CodeWeavers
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "webservices.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "webservices_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(webservices);

static const struct prop_desc heap_props[] =
{
    { sizeof(SIZE_T), FALSE }, /* WS_HEAP_PROPERTY_MAX_SIZE */
    { sizeof(SIZE_T), FALSE }, /* WS_HEAP_PROPERTY_TRIM_SIZE */
    { sizeof(SIZE_T), TRUE },  /* WS_HEAP_PROPERTY_REQUESTED_SIZE */
    { sizeof(SIZE_T), TRUE }   /* WS_HEAP_PROPERTY_ACTUAL_SIZE */
};

struct heap
{
    ULONG            magic;
    CRITICAL_SECTION cs;
    HANDLE           handle;
    SIZE_T           max_size;
    SIZE_T           allocated;
    ULONG            prop_count;
    struct prop      prop[sizeof(heap_props)/sizeof(heap_props[0])];
};

#define HEAP_MAGIC (('H' << 24) | ('E' << 16) | ('A' << 8) | 'P')

static BOOL ensure_heap( struct heap *heap )
{
    SIZE_T size;
    if (heap->handle) return TRUE;
    prop_get( heap->prop, heap->prop_count, WS_HEAP_PROPERTY_MAX_SIZE, &size, sizeof(size) );
    if (!(heap->handle = HeapCreate( 0, 0, 0 ))) return FALSE;
    heap->max_size  = size;
    heap->allocated = 0;
    return TRUE;
}

void *ws_alloc( WS_HEAP *handle, SIZE_T size )
{
    struct heap *heap = (struct heap *)handle;
    void *ret = NULL;

    EnterCriticalSection( &heap->cs );

    if (heap->magic != HEAP_MAGIC) goto done;
    if (!ensure_heap( heap ) || size > heap->max_size - heap->allocated) goto done;
    if ((ret = HeapAlloc( heap->handle, 0, size ))) heap->allocated += size;

done:
    LeaveCriticalSection( &heap->cs );
    return ret;
}

void *ws_alloc_zero( WS_HEAP *handle, SIZE_T size )
{
    struct heap *heap = (struct heap *)handle;
    void *ret = NULL;

    EnterCriticalSection( &heap->cs );

    if (heap->magic != HEAP_MAGIC) goto done;
    if (!ensure_heap( heap ) || size > heap->max_size - heap->allocated) goto done;
    if ((ret = HeapAlloc( heap->handle, HEAP_ZERO_MEMORY, size ))) heap->allocated += size;

done:
    LeaveCriticalSection( &heap->cs );
    return ret;
}

void *ws_realloc( WS_HEAP *handle, void *ptr, SIZE_T old_size, SIZE_T new_size )
{
    struct heap *heap = (struct heap *)handle;
    void *ret = NULL;

    EnterCriticalSection( &heap->cs );

    if (heap->magic != HEAP_MAGIC || !ensure_heap( heap )) goto done;
    if (new_size >= old_size)
    {
        SIZE_T size = new_size - old_size;
        if (size > heap->max_size - heap->allocated) goto done;
        if ((ret = HeapReAlloc( heap->handle, 0, ptr, new_size ))) heap->allocated += size;
    }
    else
    {
        SIZE_T size = old_size - new_size;
        if ((ret = HeapReAlloc( heap->handle, 0, ptr, new_size ))) heap->allocated -= size;
    }

done:
    LeaveCriticalSection( &heap->cs );
    return ret;
}

void *ws_realloc_zero( WS_HEAP *handle, void *ptr, SIZE_T old_size, SIZE_T new_size )
{
    struct heap *heap = (struct heap *)handle;
    void *ret = NULL;

    EnterCriticalSection( &heap->cs );

    if (heap->magic != HEAP_MAGIC || !ensure_heap( heap )) goto done;
    if (new_size >= old_size)
    {
        SIZE_T size = new_size - old_size;
        if (size > heap->max_size - heap->allocated) goto done;
        if ((ret = HeapReAlloc( heap->handle, HEAP_ZERO_MEMORY, ptr, new_size ))) heap->allocated += size;
    }
    else
    {
        SIZE_T size = old_size - new_size;
        if ((ret = HeapReAlloc( heap->handle, HEAP_ZERO_MEMORY, ptr, new_size ))) heap->allocated -= size;
    }

done:
    LeaveCriticalSection( &heap->cs );
    return ret;
}

void ws_free( WS_HEAP *handle, void *ptr, SIZE_T size )
{
    struct heap *heap = (struct heap *)handle;

    EnterCriticalSection( &heap->cs );

    if (heap->magic == HEAP_MAGIC)
    {
        HeapFree( heap->handle, 0, ptr );
        heap->allocated -= size;
    }

    LeaveCriticalSection( &heap->cs );
}

/**************************************************************************
 *          WsAlloc		[webservices.@]
 */
HRESULT WINAPI WsAlloc( WS_HEAP *handle, SIZE_T size, void **ptr, WS_ERROR *error )
{
    void *mem;

    TRACE( "%p %u %p %p\n", handle, (ULONG)size, ptr, error );
    if (error) FIXME( "ignoring error parameter\n" );

    if (!handle || !ptr) return E_INVALIDARG;
    if (!(mem = ws_alloc( handle, size ))) return WS_E_QUOTA_EXCEEDED;
    *ptr = mem;
    return S_OK;
}

static struct heap *alloc_heap(void)
{
    static const ULONG count = sizeof(heap_props)/sizeof(heap_props[0]);
    struct heap *ret;
    ULONG size = sizeof(*ret) + prop_size( heap_props, count );

    if (!(ret = heap_alloc_zero( size ))) return NULL;

    ret->magic      = HEAP_MAGIC;
    InitializeCriticalSection( &ret->cs );
    ret->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": heap.cs");

    prop_init( heap_props, count, ret->prop, &ret[1] );
    ret->prop_count = count;
    return ret;
}

/**************************************************************************
 *          WsCreateHeap		[webservices.@]
 */
HRESULT WINAPI WsCreateHeap( SIZE_T max_size, SIZE_T trim_size, const WS_HEAP_PROPERTY *properties,
                             ULONG count, WS_HEAP **handle, WS_ERROR *error )
{
    struct heap *heap;

    TRACE( "%u %u %p %u %p %p\n", (ULONG)max_size, (ULONG)trim_size, properties, count, handle, error );
    if (error) FIXME( "ignoring error parameter\n" );

    if (!handle || count) return E_INVALIDARG;
    if (!(heap = alloc_heap())) return E_OUTOFMEMORY;

    prop_set( heap->prop, heap->prop_count, WS_HEAP_PROPERTY_MAX_SIZE, &max_size, sizeof(max_size) );
    prop_set( heap->prop, heap->prop_count, WS_HEAP_PROPERTY_TRIM_SIZE, &trim_size, sizeof(trim_size) );

    *handle = (WS_HEAP *)heap;
    return S_OK;
}

static void reset_heap( struct heap *heap )
{
    HeapDestroy( heap->handle );
    heap->handle   = NULL;
    heap->max_size = heap->allocated = 0;
}

/**************************************************************************
 *          WsFreeHeap		[webservices.@]
 */
void WINAPI WsFreeHeap( WS_HEAP *handle )
{
    struct heap *heap = (struct heap *)handle;

    TRACE( "%p\n", handle );

    if (!heap) return;

    EnterCriticalSection( &heap->cs );

    if (heap->magic != HEAP_MAGIC)
    {
        LeaveCriticalSection( &heap->cs );
        return;
    }

    reset_heap( heap );
    heap->magic = 0;

    LeaveCriticalSection( &heap->cs );

    heap->cs.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection( &heap->cs );
    heap_free( heap );
}

/**************************************************************************
 *          WsResetHeap		[webservices.@]
 */
HRESULT WINAPI WsResetHeap( WS_HEAP *handle, WS_ERROR *error )
{
    struct heap *heap = (struct heap *)handle;

    TRACE( "%p %p\n", handle, error );
    if (error) FIXME( "ignoring error parameter\n" );

    if (!heap) return E_INVALIDARG;

    EnterCriticalSection( &heap->cs );

    if (heap->magic != HEAP_MAGIC)
    {
        LeaveCriticalSection( &heap->cs );
        return E_INVALIDARG;
    }

    reset_heap( heap );

    LeaveCriticalSection( &heap->cs );
    return S_OK;
}

/**************************************************************************
 *          WsGetHeapProperty		[webservices.@]
 */
HRESULT WINAPI WsGetHeapProperty( WS_HEAP *handle, WS_HEAP_PROPERTY_ID id, void *buf,
                                  ULONG size, WS_ERROR *error )
{
    struct heap *heap = (struct heap *)handle;
    HRESULT hr = S_OK;

    TRACE( "%p %u %p %u %p\n", handle, id, buf, size, error );
    if (error) FIXME( "ignoring error parameter\n" );

    if (!heap) return E_INVALIDARG;

    EnterCriticalSection( &heap->cs );

    if (heap->magic != HEAP_MAGIC)
    {
        LeaveCriticalSection( &heap->cs );
        return E_INVALIDARG;
    }

    switch (id)
    {
    case WS_HEAP_PROPERTY_REQUESTED_SIZE:
    case WS_HEAP_PROPERTY_ACTUAL_SIZE:
    {
        SIZE_T *heap_size = buf;
        if (!buf || size != sizeof(heap_size)) hr = E_INVALIDARG;
        else *heap_size = heap->allocated;
        break;
    }
    default:
        hr = prop_get( heap->prop, heap->prop_count, id, buf, size );
    }

    LeaveCriticalSection( &heap->cs );
    return hr;
}

#define XML_BUFFER_INITIAL_ALLOCATED_SIZE 256
struct xmlbuf *alloc_xmlbuf( WS_HEAP *heap )
{
    struct xmlbuf *ret;

    if (!(ret = ws_alloc( heap, sizeof(*ret) ))) return NULL;
    if (!(ret->bytes.bytes = ws_alloc( heap, XML_BUFFER_INITIAL_ALLOCATED_SIZE )))
    {
        ws_free( heap, ret, sizeof(*ret) );
        return NULL;
    }
    ret->heap         = heap;
    ret->size         = XML_BUFFER_INITIAL_ALLOCATED_SIZE;
    ret->bytes.length = 0;
    return ret;
}

void free_xmlbuf( struct xmlbuf *xmlbuf )
{
    if (!xmlbuf) return;
    ws_free( xmlbuf->heap, xmlbuf->bytes.bytes, xmlbuf->size );
    ws_free( xmlbuf->heap, xmlbuf, sizeof(*xmlbuf) );
}

/**************************************************************************
 *          WsCreateXmlBuffer		[webservices.@]
 */
HRESULT WINAPI WsCreateXmlBuffer( WS_HEAP *heap, const WS_XML_BUFFER_PROPERTY *properties,
                                  ULONG count, WS_XML_BUFFER **handle, WS_ERROR *error )
{
    struct xmlbuf *xmlbuf;

    if (!heap || !handle) return E_INVALIDARG;
    if (count) FIXME( "properties not implemented\n" );

    if (!(xmlbuf = alloc_xmlbuf( heap ))) return WS_E_QUOTA_EXCEEDED;

    *handle = (WS_XML_BUFFER *)xmlbuf;
    return S_OK;
}
