// Witcher 3 HDR fix for Mac (CrossOver / D3DMetal).
//
// On a Mac with an HDR (EDR) screen, D3DMetal reports the display to DXGI as HDR10, and
// The Witcher 3 (Next-Gen / 4.x / 5.x) then always renders in HDR, whatever HdrEnabled says,
// and has no menu option to turn it off. The result looks washed out.
//
// This DLL is loaded by the game as dinput8.dll. It forwards DirectInput to the real
// system dinput8.dll and hooks DXGI so the display is reported as SDR, the same as Windows
// does with "Use HDR" switched off. The game then picks SDR by itself.
//
// Log: %USERPROFILE%\witcher3-hdrfix.log (lines show the swap chain format and colour space).
// Env: W3HDRFIX_ALLOW_HDR=1 turns the fix off (HDR passes through) but keeps the log.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static FILE *logf;
static CRITICAL_SECTION lock;
static int allowHdr;

static void logmsg(const char *fmt, ...)
{
    if (!logf) return;
    va_list ap;
    va_start(ap, fmt);
    EnterCriticalSection(&lock);
    fprintf(logf, "[%lu] ", GetTickCount());
    vfprintf(logf, fmt, ap);
    fputc('\n', logf);
    fflush(logf);
    LeaveCriticalSection(&lock);
    va_end(ap);
}

/* ---- dinput8 forwarding ---- */

static HMODULE realDinput;

static FARPROC real_proc(const char *name)
{
    if (!realDinput) {
        char path[MAX_PATH];
        UINT n = GetSystemDirectoryA(path, MAX_PATH);
        if (n && n < MAX_PATH - 16) {
            strcat(path, "\\dinput8.dll");
            realDinput = LoadLibraryA(path);
        }
        if (!realDinput) {
            logmsg("could not load system dinput8.dll (err %lu)", GetLastError());
            return NULL;
        }
    }
    return GetProcAddress(realDinput, name);
}

long __stdcall proxy_DirectInput8Create(HINSTANCE inst, DWORD ver, const GUID *riid, void **out, void *outer)
{
    long (__stdcall *f)(HINSTANCE, DWORD, const GUID *, void **, void *) = (void *)real_proc("DirectInput8Create");
    return f ? f(inst, ver, riid, out, outer) : E_FAIL;
}

long __stdcall proxy_DllCanUnloadNow(void)
{
    return S_FALSE;
}

long __stdcall proxy_DllGetClassObject(const GUID *clsid, const GUID *riid, void **out)
{
    long (__stdcall *f)(const GUID *, const GUID *, void **) = (void *)real_proc("DllGetClassObject");
    return f ? f(clsid, riid, out) : E_FAIL;
}

long __stdcall proxy_DllRegisterServer(void)
{
    long (__stdcall *f)(void) = (void *)real_proc("DllRegisterServer");
    return f ? f() : E_FAIL;
}

long __stdcall proxy_DllUnregisterServer(void)
{
    long (__stdcall *f)(void) = (void *)real_proc("DllUnregisterServer");
    return f ? f() : E_FAIL;
}

/* ---- DXGI hooks ---- */

enum {
    CS_SDR = 0,     /* DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709 */
    CS_SCRGB = 1,   /* DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 */
    CS_HDR10 = 12,  /* DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 */
};

typedef struct {
    WCHAR deviceName[32];
    RECT desktop;
    BOOL attached;
    UINT rotation;
    HMONITOR monitor;
    UINT bitsPerColor;
    UINT colorSpace;
    float red[2], green[2], blue[2], white[2];
    float minLum, maxLum, maxFullFrameLum;
} OutputDesc1;

typedef struct { UINT w, h; UINT fmt; BOOL stereo; UINT samples, quality; UINT usage, count; UINT scaling, effect, alpha, flags; } SwapDesc1;
typedef struct { UINT w, h; UINT rateNum, rateDen; UINT fmt; UINT order, scaling; UINT samples, quality; UINT usage, count; HWND wnd; BOOL windowed; UINT effect, flags; } SwapDesc;

typedef long (__stdcall *PfnQueryInterface)(void *self, const GUID *riid, void **out);
typedef ULONG (__stdcall *PfnRelease)(void *self);
typedef long (__stdcall *PfnEnum)(void *self, UINT i, void **out);
typedef long (__stdcall *PfnGetDesc1)(void *output, OutputDesc1 *desc);
typedef long (__stdcall *PfnCreateSwapChain)(void *f, void *dev, SwapDesc *d, void **out);
typedef long (__stdcall *PfnCreateSwapChainForHwnd)(void *f, void *dev, HWND wnd, const SwapDesc1 *d, const void *fs, void *output, void **out);
typedef long (__stdcall *PfnCheckColorSpaceSupport)(void *sc, UINT cs, UINT *support);
typedef long (__stdcall *PfnSetColorSpace1)(void *sc, UINT cs);
typedef long (__stdcall *PfnResizeBuffers)(void *sc, UINT count, UINT w, UINT h, UINT fmt, UINT flags);
typedef long (__stdcall *PfnCreateDXGIFactory2)(UINT flags, const GUID *riid, void **out);

static const GUID IID_Factory2 = {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
static const GUID IID_Output6 = {0x068346e8, 0xaaec, 0x4b84, {0xad, 0xd7, 0x13, 0x7f, 0x51, 0x3f, 0x77, 0xa1}};
static const GUID IID_SwapChain3 = {0x94d99bdb, 0xf1f8, 0x4ab0, {0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1}};

static PfnGetDesc1 origGetDesc1;
static PfnCreateSwapChain origCreateSwapChain;
static PfnCreateSwapChainForHwnd origCreateSwapChainForHwnd;
static PfnCheckColorSpaceSupport origCheckColorSpaceSupport;
static PfnSetColorSpace1 origSetColorSpace1;
static PfnResizeBuffers origResizeBuffers;

static const char *fmt_name(UINT f)
{
    switch (f) {
    case 0: return "unchanged";
    case 10: return "16-bit float (HDR scRGB)";
    case 24: return "10-bit (HDR10)";
    case 28: case 29: case 87: case 91: return "8-bit (SDR)";
    default: return "other";
    }
}

static const char *cs_name(UINT cs)
{
    switch (cs) {
    case CS_SDR: return "SDR";
    case CS_SCRGB: return "HDR scRGB";
    case CS_HDR10: return "HDR10";
    default: return "other";
    }
}

static void patch_slot(void **vtbl, int idx, void *hook, void **orig)
{
    DWORD old;
    if (vtbl[idx] == hook) return;
    VirtualProtect(&vtbl[idx], sizeof(void *), PAGE_EXECUTE_READWRITE, &old);
    *orig = vtbl[idx];
    vtbl[idx] = hook;
    VirtualProtect(&vtbl[idx], sizeof(void *), old, &old);
}

static long __stdcall hkGetDesc1(void *output, OutputDesc1 *desc)
{
    long hr = origGetDesc1(output, desc);
    if (hr >= 0 && desc && !allowHdr && desc->colorSpace != CS_SDR) {
        logmsg("display reports %s -> telling the game it is SDR", cs_name(desc->colorSpace));
        desc->colorSpace = CS_SDR;
        desc->bitsPerColor = 8;
    }
    return hr;
}

static long __stdcall hkCheckColorSpaceSupport(void *sc, UINT cs, UINT *support)
{
    long hr = origCheckColorSpaceSupport(sc, cs, support);
    if (hr >= 0 && support && !allowHdr && cs != CS_SDR)
        *support = 0;
    return hr;
}

static long __stdcall hkSetColorSpace1(void *sc, UINT cs)
{
    long hr = origSetColorSpace1(sc, cs);
    logmsg("game set colour space %u (%s) -> RESULT: HDR is %s", cs, cs_name(cs), cs == CS_SDR ? "OFF" : "ON");
    return hr;
}

static long __stdcall hkResizeBuffers(void *sc, UINT count, UINT w, UINT h, UINT fmt, UINT flags)
{
    long hr = origResizeBuffers(sc, count, w, h, fmt, flags);
    logmsg("swap chain resized %ux%u, format %u (%s)", w, h, fmt, fmt_name(fmt));
    return hr;
}

static void hook_swapchain(void *sc)
{
    static LONG done;
    void *sc3 = NULL;
    if ((*(PfnQueryInterface **)sc)[0](sc, &IID_SwapChain3, &sc3) < 0 || !sc3) return;
    void **vtbl = *(void ***)sc3;
    if (!InterlockedExchange(&done, 1)) {
        patch_slot(vtbl, 13, (void *)hkResizeBuffers, (void **)&origResizeBuffers);
        patch_slot(vtbl, 37, (void *)hkCheckColorSpaceSupport, (void **)&origCheckColorSpaceSupport);
        patch_slot(vtbl, 38, (void *)hkSetColorSpace1, (void **)&origSetColorSpace1);
    }
    ((PfnRelease)vtbl[2])(sc3);
}

static long __stdcall hkCreateSwapChain(void *f, void *dev, SwapDesc *d, void **out)
{
    long hr = origCreateSwapChain(f, dev, d, out);
    if (d) logmsg("swap chain created %ux%u, format %u (%s)", d->w, d->h, d->fmt, fmt_name(d->fmt));
    if (hr >= 0 && out && *out) hook_swapchain(*out);
    return hr;
}

static long __stdcall hkCreateSwapChainForHwnd(void *f, void *dev, HWND wnd, const SwapDesc1 *d, const void *fs, void *output, void **out)
{
    long hr = origCreateSwapChainForHwnd(f, dev, wnd, d, fs, output, out);
    if (d) logmsg("swap chain created %ux%u, format %u (%s)", d->w, d->h, d->fmt, fmt_name(d->fmt));
    if (hr >= 0 && out && *out) hook_swapchain(*out);
    return hr;
}

static void hook_outputs(void *factory)
{
    void **fv = *(void ***)factory;
    void *adapter = NULL;
    for (UINT a = 0; ((PfnEnum)fv[12])(factory, a, &adapter) >= 0; a++) {   /* EnumAdapters1 */
        void **av = *(void ***)adapter;
        void *output = NULL;
        for (UINT o = 0; ((PfnEnum)av[7])(adapter, o, &output) >= 0; o++) {  /* EnumOutputs */
            void *out6 = NULL;
            void **ov = *(void ***)output;
            if (((PfnQueryInterface)ov[0])(output, &IID_Output6, &out6) >= 0 && out6) {
                void **o6v = *(void ***)out6;
                patch_slot(o6v, 27, (void *)hkGetDesc1, (void **)&origGetDesc1);
                ((PfnRelease)o6v[2])(out6);
            }
            ((PfnRelease)ov[2])(output);
        }
        ((PfnRelease)av[2])(adapter);
    }
}

/* Runs on its own thread: creating a DXGI factory inside DllMain would deadlock on the loader lock. */
static DWORD WINAPI hook_dxgi(LPVOID unused)
{
    HMODULE dxgi = LoadLibraryA("dxgi.dll");
    PfnCreateDXGIFactory2 create = dxgi ? (PfnCreateDXGIFactory2)GetProcAddress(dxgi, "CreateDXGIFactory2") : NULL;
    void *factory = NULL;
    if (!create || create(0, &IID_Factory2, &factory) < 0 || !factory) {
        logmsg("could not create a DXGI factory, fix not active");
        return 0;
    }
    void **vtbl = *(void ***)factory;
    patch_slot(vtbl, 10, (void *)hkCreateSwapChain, (void **)&origCreateSwapChain);
    patch_slot(vtbl, 15, (void *)hkCreateSwapChainForHwnd, (void **)&origCreateSwapChainForHwnd);
    hook_outputs(factory);
    logmsg("DXGI hooked, %s", allowHdr ? "HDR allowed (W3HDRFIX_ALLOW_HDR)" : "forcing SDR");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        InitializeCriticalSection(&lock);
        char path[MAX_PATH];
        DWORD n = GetEnvironmentVariableA("USERPROFILE", path, MAX_PATH);
        if (n && n < MAX_PATH - 32) {
            strcat(path, "\\witcher3-hdrfix.log");
            logf = fopen(path, "w");
        }
        allowHdr = GetEnvironmentVariableA("W3HDRFIX_ALLOW_HDR", NULL, 0) > 0;
        logmsg("Witcher 3 HDR fix loaded");
        HANDLE t = CreateThread(NULL, 0, hook_dxgi, NULL, 0, NULL);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
