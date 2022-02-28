// proxydll.cpp
#include "stdafx.h"
#include "proxydll.h"
#include "..\log.h"
#include "resource.h"
#include "..\Nektra\NktHookLib.h"

// global variables
#pragma data_seg (".d3d9_shared")
HINSTANCE           gl_hOriginalDll;
HINSTANCE           gl_hThisInstance;
bool				gl_hooked = false;
bool				gl_hooked2 = false;
FILE*				LogFile = NULL;
#pragma data_seg ()

CNktHookLib cHookMgr;

// 64 bit magic FNV-0 and FNV-1 prime
#define FNV_64_PRIME ((UINT64)0x100000001b3ULL)
static UINT64 fnv_64_buf(const void *buf, size_t len)
{
	UINT64 hval = 0;
	unsigned const char *bp = (unsigned const char *)buf;	/* start of buffer */
	unsigned const char *be = bp + len;		/* beyond end of buffer */

											// FNV-1 hash each octet of the buffer
	while (bp < be) {
		// multiply by the 64 bit FNV magic prime mod 2^64 */
		hval *= FNV_64_PRIME;
		// xor the bottom with the current octet
		hval ^= (UINT64)*bp++;
	}
	return hval;
}

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	// to avoid compiler lvl4 warnings 
    LPVOID lpDummy = lpReserved;
    lpDummy = NULL;

    switch (ul_reason_for_call)
	{
	    case DLL_PROCESS_ATTACH:
			gl_hOriginalDll = NULL;
			LoadOriginalDll();
			gl_hThisInstance = (HINSTANCE)hModule;
			ShowStartupScreen();
			break;
	    case DLL_PROCESS_DETACH: ExitInstance(); break;
        case DLL_THREAD_ATTACH:  break;
	    case DLL_THREAD_DETACH:  break;
	}
    return TRUE;
}

HRESULT STDMETHODCALLTYPE D3D9_CreateVS(IDirect3DDevice9 * This, CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
	//HRESULT dRes = D3DXDisassembleShader(pFunction, FALSE, NULL, &pDisassembly);
	//D3DXAssembleShader(shader, size, NULL, NULL, 0, &pAssembly, NULL);
	return sCreateVS_Hook.fnCreateVS(This, pFunction, ppShader);
}

HRESULT STDMETHODCALLTYPE D3D9_VSSetShader(IDirect3DDevice9 * This, IDirect3DVertexShader9* pShader) {
	return sVSSS_Hook.fnVSSS(This, pShader);
}

HRESULT STDMETHODCALLTYPE D3D9_Present(IDirect3DDevice9 * This, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) {
	return sPresent_Hook.fnPresent(This, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

void hook(IDirect3DDevice9** ppDevice) {
	if (!gl_hooked2) {
		DWORD*** vTable = (DWORD***)*ppDevice;

		D3D9_P origPresent = (D3D9_P)(*vTable)[17];

		D3D9_VS origVS = (D3D9_VS)(*vTable)[91];
		D3D9_VSSS origVSSS = (D3D9_VSSS)(*vTable)[92];

		cHookMgr.Hook(&(sCreateVS_Hook.nHookId), (LPVOID*)&(sCreateVS_Hook.fnCreateVS), origVS, D3D9_CreateVS);
		cHookMgr.Hook(&(sVSSS_Hook.nHookId), (LPVOID*)&(sVSSS_Hook.fnVSSS), origVSSS, D3D9_VSSetShader);

		cHookMgr.Hook(&(sPresent_Hook.nHookId), (LPVOID*)&(sPresent_Hook.fnPresent), origPresent, D3D9_Present);

		gl_hooked2 = true;
	}
}

HRESULT STDMETHODCALLTYPE D3D9_CreateDevice(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) {
	HRESULT res = sCreate_Hook.fnCreate(This, Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
	if (res == S_OK)
		hook(ppReturnedDeviceInterface);
	return res;
}

void hook(IDirect3D9* direct9) {
	LogInfo("Hook direct9\n");
	if (direct9 != NULL && !gl_hooked) {
		DWORD*** vTable = (DWORD***)direct9;
		D3D9_Create origCreate = (D3D9_Create)(*vTable)[16];
		cHookMgr.Hook(&(sCreate_Hook.nHookId), (LPVOID*)&(sCreate_Hook.fnCreate), origCreate, D3D9_CreateDevice);
		gl_hooked = true;
	}
}

void ShowStartupScreen()
{
	HBITMAP hBM = ::LoadBitmap(gl_hThisInstance, MAKEINTRESOURCE(IDB_STARTUP));
	if (hBM) {
		HDC hDC = ::GetDC(NULL);
		if (hDC) {
			int iXPos = (::GetDeviceCaps(hDC, HORZRES) / 2) - (128 / 2);
			int iYPos = (::GetDeviceCaps(hDC, VERTRES) / 2) - (128 / 2);

			// paint the "GPP active" sign on desktop
			HDC hMemDC = ::CreateCompatibleDC(hDC);
			HBITMAP hBMold = (HBITMAP) ::SelectObject(hMemDC, hBM);
			::BitBlt(hDC, iXPos, iYPos, 128, 128, hMemDC, 0, 0, SRCCOPY);

			//Cleanup
			::SelectObject(hMemDC, hBMold);
			::DeleteDC(hMemDC);
			::ReleaseDC(NULL, hDC);

			// Wait 1 seconds before proceeding
			::Sleep(1000);
		}
		::DeleteObject(hBM);
	}
}

#pragma region export
// Exported function (faking d3d9.dll's export)
IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion) {
	typedef IDirect3D9* (WINAPI* D3D9_Type)(UINT SDKVersion);
	D3D9_Type Direct3DCreate9_fn = (D3D9_Type)GetProcAddress(gl_hOriginalDll, "Direct3DCreate9");

	IDirect3D9* direct9 = Direct3DCreate9_fn(SDKVersion);
	hook(direct9);
	return direct9;
}

int WINAPI D3DPERF_BeginEvent(D3DCOLOR col, LPCWSTR wszName) {
	typedef int (WINAPI* D3D9_Type)(D3DCOLOR col, LPCWSTR wszName);
	D3D9_Type D3DPERF_BeginEvent_fn = (D3D9_Type)GetProcAddress(gl_hOriginalDll, "D3DPERF_BeginEvent");
	return D3DPERF_BeginEvent_fn(col, wszName);
}

int WINAPI D3DPERF_EndEvent() {
	typedef int (WINAPI* D3D9_Type)();
	D3D9_Type D3DPERF_BeginEvent_fn = (D3D9_Type)GetProcAddress(gl_hOriginalDll, "D3DPERF_EndEvent");
	return D3DPERF_BeginEvent_fn();
}

void WINAPI D3DPERF_SetMarker(D3DCOLOR col, LPCWSTR wszName) {
	typedef void (WINAPI* D3D9_Type)(D3DCOLOR col, LPCWSTR wszName);
	D3D9_Type D3DPERF_SetMarker_fn = (D3D9_Type)GetProcAddress(gl_hOriginalDll, "D3DPERF_SetMarker");
	D3DPERF_SetMarker_fn(col, wszName);
}

void WINAPI D3DPERF_SetRegion(D3DCOLOR col, LPCWSTR wszName) {
	typedef void (WINAPI* D3D9_Type)(D3DCOLOR col, LPCWSTR wszName);
	D3D9_Type D3DPERF_SetRegion_fn = (D3D9_Type)GetProcAddress(gl_hOriginalDll, "D3DPERF_SetRegion");
	D3DPERF_SetRegion_fn(col, wszName);
}

BOOL WINAPI D3DPERF_QueryRepeatFrame() {
	typedef BOOL (WINAPI* D3D9_Type)();
	D3D9_Type D3DPERF_QueryRepeatFrame_fn = (D3D9_Type)GetProcAddress(gl_hOriginalDll, "D3DPERF_QueryRepeatFrame");

	return D3DPERF_QueryRepeatFrame_fn();
}

void WINAPI D3DPERF_SetOptions(DWORD dwOptions) {
	typedef void(WINAPI* D3D9_Type)(DWORD dwOptions);
	D3D9_Type D3DPERF_SetOptions_fn = (D3D9_Type)GetProcAddress(gl_hOriginalDll, "D3DPERF_SetOptions");
	D3DPERF_SetOptions_fn(dwOptions);
}

DWORD WINAPI D3DPERF_GetStatus() {
	typedef DWORD(WINAPI* D3D9_Type)();
	D3D9_Type D3DPERF_GetStatus_fn = (D3D9_Type)GetProcAddress(gl_hOriginalDll, "D3DPERF_GetStatus");
	return D3DPERF_GetStatus_fn();
}
#pragma endregion

void LoadOriginalDll(void)
{
    char buffer[MAX_PATH];
	::GetSystemDirectory(buffer,MAX_PATH);
	strcat_s(buffer, MAX_PATH, "\\d3d9.dll");
	// try to load the system's d3d9.dll, if pointer empty
	if (!gl_hOriginalDll) gl_hOriginalDll = ::LoadLibrary(buffer);
}

void ExitInstance() 
{    
	// Release the system's d3d9.dll
	if (gl_hOriginalDll)
	{
		::FreeLibrary(gl_hOriginalDll);
	    gl_hOriginalDll = NULL;  
	}
}