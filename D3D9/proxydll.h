// proxydll.h
#pragma once

typedef HRESULT(STDMETHODCALLTYPE* D3D9_Create)(IDirect3D9* This, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface);
static struct {
	SIZE_T nHookId;
	D3D9_Create fnCreate;
} sCreate_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D9_VS)(IDirect3DDevice9 * This, CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader);
static struct {
	SIZE_T nHookId;
	D3D9_VS fnCreateVS;
} sCreateVS_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D9_VSSS)(IDirect3DDevice9 * This, IDirect3DVertexShader9* pShader);
static struct {
	SIZE_T nHookId;
	D3D9_VSSS fnVSSS;
} sVSSS_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D9_P)(IDirect3DDevice9 * This, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);
static struct {
	SIZE_T nHookId;
	D3D9_P fnPresent;
} sPresent_Hook = { 0, NULL };

void ExitInstance();
void LoadOriginalDll();
void ShowStartupScreen();