// proxydll.cpp
#include "proxy10dll.h"
#include "Nektra\NktHookLib.h"

// global variables
#pragma data_seg (".d3d10_shared")
HINSTANCE           gl_hOriginalDll;
bool				gl_hookedDevice = false;
bool				gl_dump = true;
char				cwd[MAX_PATH];
#pragma data_seg ()

CNktHookLib cHookMgr;

BOOL WINAPI DllMain(
	_In_  HINSTANCE hinstDLL,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved)
{
	bool result = true;

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

	return result;
}

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

HRESULT STDMETHODCALLTYPE D3D10_CreateVertexShader(ID3D10Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10VertexShader **ppVertexShader) {
	dump("vs", pShaderBytecode, BytecodeLength);
	return sCreateVertexShader_Hook.fn(This, pShaderBytecode, BytecodeLength, ppVertexShader);
}

HRESULT STDMETHODCALLTYPE D3D10_CreatePixelShader(ID3D10Device* This, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D10PixelShader** ppPixelShader) {
	dump("ps", pShaderBytecode, BytecodeLength);
	return sCreatePixelShader_Hook.fn(This, pShaderBytecode, BytecodeLength, ppPixelShader);
}

HRESULT STDMETHODCALLTYPE D3D10_CreateGeometryShader(ID3D10Device* This, const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D10GeometryShader** ppGeometryShader) {
	dump("gs", pShaderBytecode, BytecodeLength);
	return sCreateGeometryShader_Hook.fn(This, pShaderBytecode, BytecodeLength, ppGeometryShader);
}
#pragma endregion

#pragma region exports
void hook(ID3D10Device** ppDevice) {
	if (ppDevice != NULL) {
		if (*ppDevice != NULL && !gl_hookedDevice) {
			gl_hookedDevice = true;
			DWORD*** vTable = (DWORD***)*ppDevice;
			D3D10_VS origVS = (D3D10_VS)(*vTable)[79];
			D3D10_PS origPS = (D3D10_PS)(*vTable)[82];
			D3D10_GS origGS = (D3D10_GS)(*vTable)[80];

			cHookMgr.Hook(&(sCreateVertexShader_Hook.nHookId), (LPVOID*)&(sCreateVertexShader_Hook.fn), origVS, D3D10_CreateVertexShader);
			cHookMgr.Hook(&(sCreatePixelShader_Hook.nHookId), (LPVOID*)&(sCreatePixelShader_Hook.fn), origPS, D3D10_CreatePixelShader);
			cHookMgr.Hook(&(sCreateGeometryShader_Hook.nHookId), (LPVOID*)&(sCreateGeometryShader_Hook.fn), origGS, D3D10_CreateGeometryShader);
		}
	}
}

// Exported function (faking d3d10.dll's export)
HRESULT WINAPI D3D10CreateDevice(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice) {
	if (!gl_hOriginalDll) LoadOriginalDll(); // looking for the "right d3d10.dll"
	
	// Hooking IDirect3D Object from Original Library
	typedef HRESULT (WINAPI* D3D10_Type)(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, ID3D10Device **ppDevice);
	D3D10_Type D3D10CreateDevice_fn = (D3D10_Type) GetProcAddress( gl_hOriginalDll, "D3D10CreateDevice");
    
    HRESULT res = D3D10CreateDevice_fn(pAdapter, DriverType, Software, Flags, SDKVersion, ppDevice);
	hook(ppDevice);
	return res;
}
HRESULT WINAPI D3D10CreateDeviceAndSwapChain(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice) {
	if (!gl_hOriginalDll) LoadOriginalDll(); // looking for the "right d3d10.dll"

	// Hooking IDirect3D Object from Original Library
	typedef HRESULT(WINAPI* D3D10_Type)(IDXGIAdapter *pAdapter, D3D10_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D10Device **ppDevice);
	D3D10_Type D3D10CreateDeviceAndSwapChain_fn = (D3D10_Type)GetProcAddress(gl_hOriginalDll, "D3D10CreateDeviceAndSwapChain");
	HRESULT res = D3D10CreateDeviceAndSwapChain_fn(pAdapter, DriverType, Software, Flags, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice);
	hook(ppDevice);
	return res;
}

void LoadOriginalDll(void)
{
    char buffer[MAX_PATH];
    
	::GetSystemDirectory(buffer, MAX_PATH);
	strcat_s(buffer, MAX_PATH, "\\d3d10.dll");

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