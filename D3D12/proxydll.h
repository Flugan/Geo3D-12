// proxydll.h
#pragma once
typedef HRESULT(STDMETHODCALLTYPE* DXGI_Present)(IDXGISwapChain* This, UINT SyncInterval, UINT Flags);
static struct {
	SIZE_T nHookId;
	DXGI_Present fnDXGI_Present;
} sDXGI_Present_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* DXGI_CSC)(IDXGIFactory1* This, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain);
static struct {
	SIZE_T nHookId;
	DXGI_CSC fnCreateSwapChain;
} sCreateSwapChain_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* DXGI_Present0)(IDXGISwapChain1* This, UINT SyncInterval, UINT Flags);
static struct {
	SIZE_T nHookId;
	DXGI_Present fnDXGI_Present0;
} sDXGI_Present0_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* DXGI_Present1)(IDXGISwapChain1* This, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters);
static struct {
	SIZE_T nHookId;
	DXGI_Present1 fnDXGI_Present1;
} sDXGI_Present1_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* DXGI_CSCFH)(IDXGIFactory2* This, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1** ppSwapChain);
static struct {
	SIZE_T nHookId;
	DXGI_CSCFH fnCreateSwapChainForHWND;
} sCreateSwapChainForHWND_Hook = { 0, NULL };