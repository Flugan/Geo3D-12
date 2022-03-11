// proxydll.h
#pragma once
typedef void(STDMETHODCALLTYPE* D3D12CQ_ECL)(ID3D12CommandQueue* This, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists);
static struct {
	SIZE_T nHookId;
	D3D12CQ_ECL fn;
} sExecuteCommandLists_Hook = { 0, NULL };

typedef void(STDMETHODCALLTYPE* D3D12CL_SPS)(ID3D12GraphicsCommandList* This, ID3D12PipelineState* pPipelineState);
static struct {
	SIZE_T nHookId;
	D3D12CL_SPS fn;
} sSetPipelineState_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CCQ)(ID3D12Device* This, const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID riid, void** ppCommandQueue);
static struct {
	SIZE_T nHookId;
	D3D12_CCQ fn;
} sCreateCommandQueue_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CCL)(ID3D12Device* This, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* pCommandAllocator, ID3D12PipelineState* pInitialState, REFIID riid, void** ppCommandList);
static struct {
	SIZE_T nHookId;
	D3D12_CCL fn;
} sCreateCommandList_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CCL1)(ID3D12Device4* This, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_LIST_FLAGS flags, REFIID riid, void** ppCommandList);
static struct {
	SIZE_T nHookId;
	D3D12_CCL1 fn;
} sCreateCommandList1_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CGPS)(ID3D12Device* This, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState);
static struct {
	SIZE_T nHookId;
	D3D12_CGPS fn;
} sCreateGraphicsPipelineState_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CCPS)(ID3D12Device* This, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState);
static struct {
	SIZE_T nHookId;
	D3D12_CCPS fn;
} sCreateComputePipelineState_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CPS)(ID3D12Device2* This, const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc, REFIID riid, void** ppPipelineState);
static struct {
	SIZE_T nHookId;
	D3D12_CPS fn;
} sCreatePipelineState_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* DXGI_Present)(IDXGISwapChain* This, UINT SyncInterval, UINT Flags);
static struct {
	SIZE_T nHookId;
	DXGI_Present fn;
} sDXGI_Present_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* DXGI_CSC)(IDXGIFactory1* This, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain);
static struct {
	SIZE_T nHookId;
	DXGI_CSC fn;
} sCreateSwapChain_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* DXGI_Present1)(IDXGISwapChain1* This, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters);
static struct {
	SIZE_T nHookId;
	DXGI_Present1 fn;
} sDXGI_Present1_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* DXGI_CSCFH)(IDXGIFactory2* This, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc, IDXGIOutput *pRestrictToOutput, IDXGISwapChain1** ppSwapChain);
static struct {
	SIZE_T nHookId;
	DXGI_CSCFH fn;
} sCreateSwapChainForHWND_Hook = { 0, NULL };