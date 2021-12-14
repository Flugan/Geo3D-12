// proxydll.h
#pragma once

void LoadOriginalDll(void);

typedef HRESULT(STDMETHODCALLTYPE* DXGI_Present)(IDXGISwapChain* This, UINT SyncInterval, UINT Flags);
static struct {
	SIZE_T nHookId;
	DXGI_Present fnDXGI_Present;
} sDXGI_Present_Hook = { 0, NULL };
typedef HRESULT(STDMETHODCALLTYPE* DXGI_CSC1)(IDXGIFactory1* This, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain);
static struct {
	SIZE_T nHookId;
	DXGI_CSC1 fnCreateSwapChain1;
} sCreateSwapChain_Hook = { 0, NULL };