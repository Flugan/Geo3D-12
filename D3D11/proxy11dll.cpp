// proxydll.cpp
#include "proxy11dll.h"
#include "Nektra\NktHookLib.h"

// global variables
#pragma data_seg (".d3d11_shared")
HINSTANCE           gl_hOriginalDll;
bool				gl_hookedDevice = false;
bool				gl_dump = true;
char				cwd[MAX_PATH];
#pragma data_seg ()

CNktHookLib cHookMgr;

INT WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		LoadOriginalDll();
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

	return 0;
}

// Primary hash calculation for all shader file names, all textures.
// 64 bit magic FNV-0 and FNV-1 prime
#define FNV_64_PRIME ((UINT64)0x100000001b3ULL)
static UINT64 fnv_64_buf(const void *buf, size_t len)
{
	UINT64 hval = 0;
	unsigned const char *bp = (unsigned const char *)buf;	/* start of buffer */
	unsigned const char *be = bp + len;		/* beyond end of buffer */

											// FNV-1 hash each octet of the buffer
	while (bp < be)
	{
		// multiply by the 64 bit FNV magic prime mod 2^64 */
		hval *= FNV_64_PRIME;
		// xor the bottom with the current octet
		hval ^= (UINT64)*bp++;
	}
	return hval;
}

#pragma region Dump
int fileExists(TCHAR* file) {
	WIN32_FIND_DATA FindFileData;
	HANDLE handle = FindFirstFile(file, &FindFileData);
	int found = handle != INVALID_HANDLE_VALUE;
	if (found) {
		FindClose(handle);
	}
	return found;
}

void dump(char* type, const void* pData, SIZE_T length) {
	if (!gl_dump)
		return;
	FILE* f;
	char path[MAX_PATH];
	path[0] = 0;
	if (length > 0) {
		UINT64 crc = fnv_64_buf(pData, length);
		CreateDirectory("ShaderCache", NULL);
		sprintf_s(path, MAX_PATH, "ShaderCache\\%016llX-%s.bin", crc, type);
		if (!fileExists(path)) {
			fopen_s(&f, path, "wb");
			if (f != 0) {
				fwrite(pData, 1, length, f);
				fclose(f);
			}
		}
	}
}

HRESULT STDMETHODCALLTYPE D3D11_CreateVertexShader(ID3D11Device *This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader) {
	dump("vs", pShaderBytecode, BytecodeLength);
	return sCreateVertexShader_Hook.fn(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}
HRESULT STDMETHODCALLTYPE D3D11_CreatePixelShader(ID3D11Device *This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader) {
	dump("ps", pShaderBytecode, BytecodeLength);
	return sCreatePixelShader_Hook.fn(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}
HRESULT STDMETHODCALLTYPE D3D11_CreateGeometryShader(ID3D11Device *This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader) {
	dump("gs", pShaderBytecode, BytecodeLength);
	return sCreateGeometryShader_Hook.fn(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
}

HRESULT STDMETHODCALLTYPE D3D11_CreateHullShader(ID3D11Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader) {
	dump("hs", pShaderBytecode, BytecodeLength);
	return sCreateHullShader_Hook.fn(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
}

HRESULT STDMETHODCALLTYPE D3D11_CreateDomainShader(ID3D11Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11DomainShader **ppDomainShader) {
	dump("ds", pShaderBytecode, BytecodeLength);
	return sCreateDomainShader_Hook.fn(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
}

HRESULT STDMETHODCALLTYPE D3D11_CreateComputeShader(ID3D11Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11ComputeShader **ppComputeShader) {
	dump("cs", pShaderBytecode, BytecodeLength);
	return sCreateComputeShader_Hook.fn(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
}
#pragma endregion

#pragma region exports
void hook(ID3D11Device** ppDevice) {
	if (gl_hookedDevice)
		return;
	gl_hookedDevice = true;
	DWORD_PTR*** vTable = (DWORD_PTR***)*ppDevice;
	D3D11_VS origVS = (D3D11_VS)(*vTable)[12];
	D3D11_PS origPS = (D3D11_PS)(*vTable)[15];
	D3D11_GS origGS = (D3D11_GS)(*vTable)[13];
	D3D11_HS origHS = (D3D11_HS)(*vTable)[16];
	D3D11_DS origDS = (D3D11_DS)(*vTable)[17];
	D3D11_CS origCS = (D3D11_CS)(*vTable)[18];

	cHookMgr.Hook(&(sCreateVertexShader_Hook.nHookId), (LPVOID*)&(sCreateVertexShader_Hook.fn), origVS, D3D11_CreateVertexShader);
	cHookMgr.Hook(&(sCreatePixelShader_Hook.nHookId), (LPVOID*)&(sCreatePixelShader_Hook.fn), origPS, D3D11_CreatePixelShader);
	cHookMgr.Hook(&(sCreateGeometryShader_Hook.nHookId), (LPVOID*)&(sCreateGeometryShader_Hook.fn), origGS, D3D11_CreateGeometryShader);
	cHookMgr.Hook(&(sCreateHullShader_Hook.nHookId), (LPVOID*)&(sCreateHullShader_Hook.fn), origHS, D3D11_CreateHullShader);
	cHookMgr.Hook(&(sCreateDomainShader_Hook.nHookId), (LPVOID*)&(sCreateDomainShader_Hook.fn), origDS, D3D11_CreateDomainShader);
	cHookMgr.Hook(&(sCreateComputeShader_Hook.nHookId), (LPVOID*)&(sCreateComputeShader_Hook.fn), origCS, D3D11_CreateComputeShader);
}

// Exported function (faking d3d11.dll's export)
HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext) {
	typedef HRESULT (WINAPI* D3D11_Type)(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext);
	D3D11_Type D3D11CreateDevice_fn = (D3D11_Type) GetProcAddress( gl_hOriginalDll, "D3D11CreateDevice");
	HRESULT res = D3D11CreateDevice_fn(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);
	if (res == S_OK) {
		hook(ppDevice);
	}
	return res;
}
HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice, D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext **ppImmediateContext) {
	typedef HRESULT(WINAPI* D3D11_Type)(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc, IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext);
	D3D11_Type D3D11CreateDeviceAndSwapChain_fn = (D3D11_Type)GetProcAddress(gl_hOriginalDll, "D3D11CreateDeviceAndSwapChain");
	HRESULT res = D3D11CreateDeviceAndSwapChain_fn(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
	if (res == S_OK) {
		hook(ppDevice);
	}
	return res;
}

void LoadOriginalDll(void)
{
	char buffer[MAX_PATH];

	::GetSystemDirectory(buffer, MAX_PATH);
	strcat_s(buffer, MAX_PATH, "\\d3d11.dll");

	if (!gl_hOriginalDll) gl_hOriginalDll = ::LoadLibrary(buffer);
}

void ExitInstance() 
{    
	if (gl_hOriginalDll)
	{
		::FreeLibrary(gl_hOriginalDll);
	    gl_hOriginalDll = NULL;  
	}
}
#pragma endregion