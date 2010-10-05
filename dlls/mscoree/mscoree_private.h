/*
 *
 * Copyright 2008 Alistair Leslie-Hughes
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

#ifndef __MSCOREE_PRIVATE__
#define __MSCOREE_PRIVATE__

extern LONG dll_refs;
static inline void MSCOREE_LockModule(void) { InterlockedIncrement(&dll_refs); }
static inline void MSCOREE_UnlockModule(void) { InterlockedDecrement(&dll_refs); }

extern char *WtoA(LPCWSTR wstr);

extern HRESULT CLRMetaHost_CreateInstance(REFIID riid, void **ppobj);

typedef struct tagASSEMBLY ASSEMBLY;

HRESULT assembly_create(ASSEMBLY **out, LPCWSTR file);
HRESULT assembly_release(ASSEMBLY *assembly);
HRESULT assembly_get_runtime_version(ASSEMBLY *assembly, LPSTR *version);

typedef struct RuntimeHost RuntimeHost;

typedef struct CLRRuntimeInfo
{
    const struct ICLRRuntimeInfoVtbl *ICLRRuntimeInfo_vtbl;
    LPCWSTR mono_libdir;
    DWORD major;
    DWORD minor;
    DWORD build;
    int mono_abi_version;
    WCHAR mono_path[MAX_PATH];
    WCHAR mscorlib_path[MAX_PATH];
    struct RuntimeHost *loaded_runtime;
} CLRRuntimeInfo;

extern HRESULT get_runtime_info(LPCWSTR exefile, LPCWSTR version, LPCWSTR config_file,
    DWORD startup_flags, DWORD runtimeinfo_flags, BOOL legacy, ICLRRuntimeInfo **result);

extern HRESULT force_get_runtime_info(ICLRRuntimeInfo **result);

extern HRESULT ICLRRuntimeInfo_GetRuntimeHost(ICLRRuntimeInfo *iface, RuntimeHost **result);

extern HRESULT MetaDataDispenser_CreateInstance(IUnknown **ppUnk);

typedef struct parsed_config_file
{
    struct list supported_runtimes;
} parsed_config_file;

typedef struct supported_runtime
{
    struct list entry;
    LPWSTR version;
} supported_runtime;

extern HRESULT parse_config_file(LPCWSTR filename, parsed_config_file *result);

extern void free_parsed_config_file(parsed_config_file *file);

/* Mono embedding */
typedef struct _MonoDomain MonoDomain;
typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoAssemblyName MonoAssemblyName;

typedef enum {
	MONO_IMAGE_OK,
	MONO_IMAGE_ERROR_ERRNO,
	MONO_IMAGE_MISSING_ASSEMBLYREF,
	MONO_IMAGE_IMAGE_INVALID
} MonoImageOpenStatus;

typedef MonoAssembly* (*MonoAssemblyPreLoadFunc)(MonoAssemblyName *aname, char **assemblies_path, void *user_data);

typedef struct loaded_mono
{
    HMODULE mono_handle;
    HMODULE glib_handle;

    MonoAssembly* (*mono_assembly_open)(const char *filename, MonoImageOpenStatus *status);
    void (*mono_config_parse)(const char *filename);
    MonoAssembly* (*mono_domain_assembly_open) (MonoDomain *domain, const char *name);
    void (*mono_free)(void *);
    void (*mono_install_assembly_preload_hook)(MonoAssemblyPreLoadFunc func, void *user_data);
    void (*mono_jit_cleanup)(MonoDomain *domain);
    int (*mono_jit_exec)(MonoDomain *domain, MonoAssembly *assembly, int argc, char *argv[]);
    MonoDomain* (*mono_jit_init)(const char *file);
    int (*mono_jit_set_trace_options)(const char* options);
    void (*mono_set_dirs)(const char *assembly_dir, const char *config_dir);
    char* (*mono_stringify_assembly_name)(MonoAssemblyName *aname);
} loaded_mono;

/* loaded runtime interfaces */
extern void unload_all_runtimes(void);

extern HRESULT RuntimeHost_Construct(const CLRRuntimeInfo *runtime_version,
    const loaded_mono *loaded_mono, RuntimeHost** result);

extern HRESULT RuntimeHost_GetInterface(RuntimeHost *This, REFCLSID clsid, REFIID riid, void **ppv);

extern HRESULT RuntimeHost_Destroy(RuntimeHost *This);

#endif   /* __MSCOREE_PRIVATE__ */
