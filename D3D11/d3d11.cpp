// proxydll.cpp
#include "stdafx.h"

// global variables
#pragma data_seg (".d3d11_shared")
HINSTANCE           gl_hOriginalDll = NULL;
FILE* gl_logFile = NULL;
#pragma data_seg ()

void log(const char* message) {
	fputs(message, gl_logFile);
	fputs("\n", gl_logFile);
	fflush(gl_logFile);
}

void ExitInstance()
{
	if (gl_hOriginalDll)
	{
		::FreeLibrary(gl_hOriginalDll);
		gl_hOriginalDll = NULL;
	}
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	bool result = true;

	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		::CreateDirectory("c:\\Flugan", NULL);
		gl_logFile = _fsopen("c:\\Flugan\\Log_dx11.txt", "w", _SH_DENYNO);
		setvbuf(gl_logFile, NULL, _IONBF, 0);
		log("Project Flugan loaded:");
		char cwd[MAX_PATH];
		_getcwd(cwd, MAX_PATH);
		log(cwd);
		break;

	case DLL_PROCESS_DETACH:
		ExitInstance();
		break;

	case DLL_THREAD_ATTACH:
		// Do thread-specific initialization.
		break;

	case DLL_THREAD_DETACH:
		// Do thread-specific cleanup.
		break;
	}

	return result;
}

void LoadOriginalDll(void)
{
	wchar_t sysDir[MAX_PATH];
	::GetSystemDirectoryW(sysDir, MAX_PATH);
	wcscat_s(sysDir, MAX_PATH, L"\\D3D11.dll");
	if (!gl_hOriginalDll) gl_hOriginalDll = ::LoadLibraryExW(sysDir, NULL, NULL);
}

// Exported function (faking d3d11.dll's export)
HRESULT WINAPI D3D11CoreCreateDevice(__int32 a, int b, int c, LPCSTR lpModuleName, int e, int f, int g, int h, int i, int j) {
	if (!gl_hOriginalDll) LoadOriginalDll(); // looking for the "right d3d11.dll"
	typedef HRESULT(WINAPI* D3D11_Type)(__int32 a, int b, int c, LPCSTR lpModuleName, int e, int f, int g, int h, int i, int j);
	D3D11_Type D3D11CoreCreateDevice_fn = (D3D11_Type)GetProcAddress(gl_hOriginalDll, "D3D11CoreCreateDevice");
	HRESULT res = D3D11CoreCreateDevice_fn(a, b, c, lpModuleName, e, f, g, h, i, j);
	return res;
}
HRESULT WINAPI D3D11CoreCreateLayeredDevice(const void* unknown0, DWORD unknown1, const void* unknown2, REFIID riid, void** ppvObj) {
	if (!gl_hOriginalDll) LoadOriginalDll(); // looking for the "right d3d11.dll"
	typedef HRESULT(WINAPI* D3D11_Type)(const void* unknown0, DWORD unknown1, const void* unknown2, REFIID riid, void** ppvObj);
	D3D11_Type D3D11CoreCreateLayeredDevice_fn = (D3D11_Type)GetProcAddress(gl_hOriginalDll, "D3D11CoreCreateLayeredDevice");
	HRESULT res = D3D11CoreCreateLayeredDevice_fn(unknown0, unknown1, unknown2, riid, ppvObj);
	return res;
}
HRESULT WINAPI D3D11CoreGetLayeredDeviceSize(const void* unknown0, DWORD unknown1) {
	if (!gl_hOriginalDll) LoadOriginalDll(); // looking for the "right d3d11.dll"
	typedef HRESULT(WINAPI* D3D11_Type)(const void* unknown0, DWORD unknown1);
	D3D11_Type D3D11CoreGetLayeredDeviceSize_fn = (D3D11_Type)GetProcAddress(gl_hOriginalDll, "D3D11CoreGetLayeredDeviceSize");
	HRESULT res = D3D11CoreGetLayeredDeviceSize_fn(unknown0, unknown1);
	return res;
}
HRESULT WINAPI D3D11CoreRegisterLayers(const void* unknown0, DWORD unknown1) {
	if (!gl_hOriginalDll) LoadOriginalDll(); // looking for the "right d3d11.dll"
	typedef HRESULT(WINAPI* D3D11_Type)(const void* unknown0, DWORD unknown1);
	D3D11_Type D3D11CoreRegisterLayers_fn = (D3D11_Type)GetProcAddress(gl_hOriginalDll, "D3D11CoreRegisterLayers");
	HRESULT res = D3D11CoreRegisterLayers_fn(unknown0, unknown1);
	return res;
}
HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels,
	UINT FeatureLevels, UINT SDKVersion, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext) {
	if (!gl_hOriginalDll) LoadOriginalDll(); // looking for the "right d3d11.dll"
	typedef HRESULT(WINAPI* D3D11_Type)(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels,
		UINT FeatureLevels, UINT SDKVersion, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);
	D3D11_Type D3D11CreateDevice_fn = (D3D11_Type)GetProcAddress(gl_hOriginalDll, "D3D11CreateDevice");
	HRESULT res = D3D11CreateDevice_fn(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
	return res;
}
HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
	const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext) {
	if (!gl_hOriginalDll) LoadOriginalDll(); // looking for the "right d3d11.dll"
	typedef HRESULT(WINAPI* D3D11_Type)(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, INT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
		const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);
	D3D11_Type D3D11CreateDeviceAndSwapChain_fn = (D3D11_Type)GetProcAddress(gl_hOriginalDll, "D3D11CreateDeviceAndSwapChain");
	HRESULT res = D3D11CreateDeviceAndSwapChain_fn(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
	return res;
}
HRESULT D3D11On12CreateDevice(IUnknown* pDevice, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, IUnknown* const* ppCommandQueues, UINT NumQueues, UINT NodeMask,
	ID3D11Device** ppDevice, ID3D11DeviceContext** ppImmediateContext, D3D_FEATURE_LEVEL* pChosenFeatureLevel) {
	if (!gl_hOriginalDll) LoadOriginalDll(); // looking for the "right d3d11.dll"
	typedef HRESULT(WINAPI* D3D11_Type)(IUnknown* pDevice, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, IUnknown* const* ppCommandQueues, UINT NumQueues, UINT NodeMask,
		ID3D11Device** ppDevice, ID3D11DeviceContext** ppImmediateContext, D3D_FEATURE_LEVEL* pChosenFeatureLevel);
	D3D11_Type D3D11On12CreateDevice_fn = (D3D11_Type)GetProcAddress(gl_hOriginalDll, "D3D11On12CreateDevice");
	HRESULT res = D3D11On12CreateDevice_fn(pDevice, Flags, pFeatureLevels, FeatureLevels, ppCommandQueues, NumQueues, NodeMask, ppDevice, ppImmediateContext, pChosenFeatureLevel);
	return res;
}