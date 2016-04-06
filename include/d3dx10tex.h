/*
 * Copyright 2016 Alistair Leslie-Hughes
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

#ifndef __D3DX10TEX_H__
#define __D3DX10TEX_H__

typedef enum D3DX10_IMAGE_FILE_FORMAT
{
    D3DX10_IFF_BMP         = 0,
    D3DX10_IFF_JPG         = 1,
    D3DX10_IFF_PNG         = 3,
    D3DX10_IFF_DDS         = 4,
    D3DX10_IFF_TIFF        = 10,
    D3DX10_IFF_GIF         = 11,
    D3DX10_IFF_WMP         = 12,
    D3DX10_IFF_FORCE_DWORD = 0x7fffffff
} D3DX10_IMAGE_FILE_FORMAT;

typedef struct D3DX10_IMAGE_INFO
{
    UINT                     Width;
    UINT                     Height;
    UINT                     Depth;
    UINT                     ArraySize;
    UINT                     MipLevels;
    UINT                     MiscFlags;
    DXGI_FORMAT              Format;
    D3D10_RESOURCE_DIMENSION ResourceDimension;
    D3DX10_IMAGE_FILE_FORMAT ImageFileFormat;
} D3DX10_IMAGE_INFO;

typedef struct D3DX10_IMAGE_LOAD_INFO
{
    UINT              Width;
    UINT              Height;
    UINT              Depth;
    UINT              FirstMipLevel;
    UINT              MipLevels;
    D3D10_USAGE       Usage;
    UINT              BindFlags;
    UINT              CpuAccessFlags;
    UINT              MiscFlags;
    DXGI_FORMAT       Format;
    UINT              Filter;
    UINT              MipFilter;
    D3DX10_IMAGE_INFO *pSrcInfo;

#ifdef __cplusplus
    D3DX10_IMAGE_LOAD_INFO()
    {
        Width          = D3DX10_DEFAULT;
        Height         = D3DX10_DEFAULT;
        Depth          = D3DX10_DEFAULT;
        FirstMipLevel  = D3DX10_DEFAULT;
        MipLevels      = D3DX10_DEFAULT;
        Usage          = (D3D10_USAGE)D3DX10_DEFAULT;
        BindFlags      = D3DX10_DEFAULT;
        CpuAccessFlags = D3DX10_DEFAULT;
        MiscFlags      = D3DX10_DEFAULT;
        Format         = DXGI_FORMAT_FROM_FILE;
        Filter         = D3DX10_DEFAULT;
        MipFilter      = D3DX10_DEFAULT;
        pSrcInfo       = NULL;
    }
#endif
} D3DX10_IMAGE_LOAD_INFO;

#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DX10CreateTextureFromMemory(ID3D10Device *device, const void *src_data, SIZE_T src_data_size,
        D3DX10_IMAGE_LOAD_INFO *loadinfo, ID3DX10ThreadPump *pump, ID3D10Resource **texture, HRESULT *hresult);


#ifdef __cplusplus
}
#endif

#endif
