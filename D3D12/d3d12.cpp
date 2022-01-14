// proxydll.cpp
#include "stdafx.h"
#include "proxydll.h"
#include "resource.h"
#include "..\log.h"
#include "..\Nektra\NktHookLib.h"
#include <wrl.h>
using namespace Microsoft::WRL;

// global variables
#pragma data_seg (".d3d12_shared")
HINSTANCE			gl_hInstDLL = NULL;
HINSTANCE           gl_hOriginalDll = NULL;
bool				gl_hookedDevice = false;
bool				gl_SwapChain_hooked = false;
bool				gl_SwapChain1_hooked = false;
FILE*				LogFile = NULL;
bool				gLogDebug = false;
bool				gl_dumpBin = true;
bool				gl_dumpASM = false;
CNktHookLib			cHookMgr;
CRITICAL_SECTION	gl_CS;
#pragma data_seg ()

// 64 bit magic FNV-0 and FNV-1 prime
#define FNV_64_PRIME ((UINT64)0x100000001b3ULL)
static UINT64 fnv_64_buf(const void* buf, size_t len)
{
	UINT64 hval = 0;
	unsigned const char* bp = (unsigned const char*)buf;	/* start of buffer */
	unsigned const char* be = bp + len;		/* beyond end of buffer */

	// FNV-1 hash each octet of the buffer
	while (bp < be) {
		// multiply by the 64 bit FNV magic prime mod 2^64 */
		hval *= FNV_64_PRIME;
		// xor the bottom with the current octet
		hval ^= (UINT64)*bp++;
	}
	return hval;
}

void ShowStartupScreen()
{
	BOOL affinity = -1;
	DWORD_PTR one = 0x01;
	DWORD_PTR before = 0;
	DWORD_PTR before2 = 0;
	affinity = GetProcessAffinityMask(GetCurrentProcess(), &before, &before2);
	affinity = SetProcessAffinityMask(GetCurrentProcess(), one);
	HBITMAP hBM = ::LoadBitmap(gl_hInstDLL, MAKEINTRESOURCE(IDB_STARTUP));
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

			// Wait before proceeding
			::Beep(440, 1000);
		}
		::DeleteObject(hBM);
	}
	affinity = SetProcessAffinityMask(GetCurrentProcess(), before);
}

void ExitInstance()
{
	if (gl_hOriginalDll)
	{
		::FreeLibrary(gl_hOriginalDll);
		gl_hOriginalDll = NULL;
	}
}

BOOL WINAPI DllMain(
	_In_  HINSTANCE hinst,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved)
{
	bool result = true;

	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		gl_hInstDLL = hinst;
		LogFile = _fsopen("d3d12.log.txt", "w", _SH_DENYNO);
		setvbuf(LogFile, NULL, _IONBF, 0);
		LogInfo("Project Flugan loaded:\n");
		gl_hOriginalDll = ::LoadLibrary("c:\\windows\\system32\\d3d12.dll");
		InitializeCriticalSection(&gl_CS);
		ShowStartupScreen();
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

void dumpShader(char* type, const void* pData, SIZE_T length) {
	FILE* f;
	char path[MAX_PATH];
	path[0] = 0;
	if (length > 0) {
		UINT64 crc = fnv_64_buf(pData, length);
		if (gl_dumpBin || gl_dumpASM) {
			CreateDirectory("ShaderCache", NULL);
			sprintf_s(path, MAX_PATH, "ShaderCache\\%016llX-%s.bin", crc, type);
			EnterCriticalSection(&gl_CS);
			if (!fileExists(path)) {
				LogInfo("Dump: %s\n", path);
				if (gl_dumpBin) {
					fopen_s(&f, path, "wb");
					if (f != 0) {
						fwrite(pData, 1, length, f);
						fclose(f);
					}
				}
				if (gl_dumpASM) {
					vector<byte> v;
					byte* bArray = (byte*)pData;
					for (int i = 0; i < length; i++) {
						v.push_back(bArray[i]);
					}
					auto ASM = disassembler(v);
					fopen_s(&f, path, "wb");
					if (f != 0) {
						fwrite(ASM.data(), 1, ASM.size(), f);
						fclose(f);
					}
				}
			}
			else {
				LogInfo("!!!Dump: %s\n", path);
			}
			LeaveCriticalSection(&gl_CS);
		}
	}
}

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CGPS)(ID3D12Device* This, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, const IID &riid, void **ppPipelineState);
static struct {
	SIZE_T nHookId;
	D3D12_CGPS fnCreateGraphicsPipelineState;
} sCreateGraphicsPipelineState_Hook = { 0, NULL };
HRESULT STDMETHODCALLTYPE D3D12_CreateGraphicsPipelineState(ID3D12Device* This, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) {
	dumpShader("vs", pDesc->VS.pShaderBytecode, pDesc->VS.BytecodeLength);
	dumpShader("ps", pDesc->PS.pShaderBytecode, pDesc->PS.BytecodeLength);
	dumpShader("ds", pDesc->DS.pShaderBytecode, pDesc->DS.BytecodeLength);
	dumpShader("gs", pDesc->GS.pShaderBytecode, pDesc->GS.BytecodeLength);
	dumpShader("hs", pDesc->HS.pShaderBytecode, pDesc->HS.BytecodeLength);
	return sCreateGraphicsPipelineState_Hook.fnCreateGraphicsPipelineState(This, pDesc, riid, ppPipelineState);
}

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CCPS)(ID3D12Device* This, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState);
static struct {
	SIZE_T nHookId;
	D3D12_CCPS fnCreateComputePipelineState;
} sCreateComputePipelineState_Hook = { 1, NULL };
HRESULT STDMETHODCALLTYPE D3D12_CreateComputePipelineState(ID3D12Device* This, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) {
	dumpShader("cs", pDesc->CS.pShaderBytecode, pDesc->CS.BytecodeLength);
	return sCreateComputePipelineState_Hook.fnCreateComputePipelineState(This, pDesc, riid, ppPipelineState);
}
#pragma endregion

#pragma region DXGI
HRESULT STDMETHODCALLTYPE DXGIH_Present(IDXGISwapChain* This, UINT SyncInterval, UINT Flags) {
	HRESULT hr = sDXGI_Present_Hook.fnDXGI_Present(This, SyncInterval, Flags);
	LogInfo("Present\n");
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIH_Present0(IDXGISwapChain1* This, UINT SyncInterval, UINT Flags) {
	HRESULT hr = sDXGI_Present0_Hook.fnDXGI_Present0(This, SyncInterval, Flags);
	LogInfo("Present0\n");
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGIH_Present1(IDXGISwapChain1* This, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
	HRESULT hr = sDXGI_Present1_Hook.fnDXGI_Present1(This, SyncInterval, Flags, pPresentParameters);
	LogInfo("Present1\n");
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGI_CreateSwapChain(IDXGIFactory1* This, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) {
	LogInfo("CreateSwapChain\n");
	HRESULT hr = sCreateSwapChain_Hook.fnCreateSwapChain(This, pDevice, pDesc, ppSwapChain);
	if (!gl_SwapChain_hooked) {
		LogInfo("SwapChain hooked\n");
		gl_SwapChain_hooked = true;
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppSwapChain;

		DXGI_Present origPresent = (DXGI_Present)(*vTable)[8];
		cHookMgr.Hook(&(sDXGI_Present_Hook.nHookId), (LPVOID*)&(sDXGI_Present_Hook.fnDXGI_Present), origPresent, DXGIH_Present);
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGI_CreateSwapChainForHWND(IDXGIFactory2* This, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain) {
	LogInfo("CreateSwapChainForHWND\n");
	HRESULT hr = sCreateSwapChainForHWND_Hook.fnCreateSwapChainForHWND(This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	if (!gl_SwapChain1_hooked) {
		LogInfo("SwapChain1 hooked\n");
		gl_SwapChain1_hooked = true;
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppSwapChain;

		DXGI_Present0 origPresent0 = (DXGI_Present0)(*vTable)[9];
		cHookMgr.Hook(&(sDXGI_Present0_Hook.nHookId), (LPVOID*)&(sDXGI_Present0_Hook.fnDXGI_Present0), origPresent0, DXGIH_Present0);

		DXGI_Present1 origPresent1 = (DXGI_Present1)(*vTable)[23];
		cHookMgr.Hook(&(sDXGI_Present1_Hook.nHookId), (LPVOID*)&(sDXGI_Present1_Hook.fnDXGI_Present1), origPresent1, DXGIH_Present1);
	}
	return hr;
}

void HackedPresent() {
	IDXGIFactory *pFactory1;
	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&pFactory1));
	DWORD_PTR*** vTable = (DWORD_PTR***)pFactory1;
	DXGI_CSC origCSC = (DXGI_CSC)(*vTable)[10];
	cHookMgr.Hook(&(sCreateSwapChain_Hook.nHookId), (LPVOID*)&(sCreateSwapChain_Hook.fnCreateSwapChain), origCSC, DXGI_CreateSwapChain);
	pFactory1->Release();

	IDXGIFactory2* pFactory2;
	hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory2));
	vTable = (DWORD_PTR***)pFactory2;
	DXGI_CSCFH origCSCFH = (DXGI_CSCFH)(*vTable)[15];
	cHookMgr.Hook(&(sCreateSwapChainForHWND_Hook.nHookId), (LPVOID*)&(sCreateSwapChainForHWND_Hook.fnCreateSwapChainForHWND), origCSCFH, DXGI_CreateSwapChainForHWND);
	pFactory2->Release();
}
#pragma endregion

void hookDevice(void** ppDevice) {
	if (!gl_hookedDevice) {
		LogInfo("Hooking device\n");

		DWORD_PTR*** vTable = (DWORD_PTR***)*ppDevice;

		D3D12_CGPS origCGPS = (D3D12_CGPS)(*vTable)[10];
		D3D12_CCPS origCCPS = (D3D12_CCPS)(*vTable)[11];

		cHookMgr.Hook(&(sCreateGraphicsPipelineState_Hook.nHookId), (LPVOID*)&(sCreateGraphicsPipelineState_Hook.fnCreateGraphicsPipelineState), origCGPS, D3D12_CreateGraphicsPipelineState);
		cHookMgr.Hook(&(sCreateComputePipelineState_Hook.nHookId), (LPVOID*)&(sCreateComputePipelineState_Hook.fnCreateComputePipelineState), origCCPS, D3D12_CreateComputePipelineState);

		HackedPresent();

		gl_hookedDevice = true;
	}
}

#pragma region Exports
HRESULT _fastcall GetBehaviorValue(const char* a1, unsigned __int64* a2) {
	typedef HRESULT(_fastcall* D3D12_Type)(const char *a, unsigned __int64 *b);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "GetBehaviorValue");
	return fn(a1, a2);
}; // 100

HRESULT D3D12CreateDevice(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, const IID riid, void** ppDevice) {
	LogInfo("CreateDevice\n");
	typedef HRESULT(*D3D12_Type)(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, const IID riid, void** ppDevice);
	D3D12_Type fn = (D3D12_Type)NktHookLibHelpers::GetProcedureAddress(gl_hOriginalDll, "D3D12CreateDevice");
	HRESULT res = fn(pAdapter, MinimumFeatureLevel, riid, ppDevice);
	if (ppDevice)
		hookDevice(ppDevice);
	return res;
} // 101

HRESULT WINAPI D3D12GetDebugInterface(const IID* const riid, void** ppvDebug) {
	typedef HRESULT(WINAPI* D3D12_Type)(const IID* const riid, void** ppvDebug);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12GetDebugInterface");
	return fn(riid, ppvDebug);
} // 102

void _fastcall SetAppCompatStringPointer(unsigned __int64 a1, const char* a2) {
	typedef void(_fastcall* D3D12_Type)(unsigned __int64 a1, const char* a2);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "SetAppCompatStringPointer");
	return fn(a1, a2);
} // 103


HRESULT WINAPI D3D12CoreCreateLayeredDevice(const void *u0, DWORD u1, const void *u2, REFIID riid, void **ppvObj) {
	typedef HRESULT(WINAPI* D3D12_Type)(const void *u0, DWORD u1, const void *u2, REFIID riid, void **ppvObj);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreCreateLayeredDevice");
	return fn(u0, u1, u2, riid, ppvObj);
} // 104

HRESULT WINAPI D3D12CoreGetLayeredDeviceSize(const void *u0, DWORD u1) {
	typedef HRESULT(WINAPI* D3D12_Type)(const void *u0, DWORD u1);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreGetLayeredDeviceSize");
	return fn(u0, u1);
} // 105

HRESULT WINAPI D3D12CoreRegisterLayers(const void *u0, DWORD u1) {
	typedef HRESULT(WINAPI* D3D12_Type)(const void *u0, DWORD u1);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreRegisterLayers");
	return fn(u0, u1);
} // 106


HRESULT WINAPI D3D12CreateRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T  SrcDataSizeInBytes, REFIID  pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer) {
	typedef HRESULT(WINAPI* D3D12_Type)(LPCVOID pSrcData, SIZE_T  SrcDataSizeInBytes, REFIID  pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CreateRootSignatureDeserializer");
	return fn(pSrcData, SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
} // 107

HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer) {
	typedef HRESULT(WINAPI* D3D12_Type)(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CreateVersionedRootSignatureDeserializer");
	return fn(pSrcData,SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
} // 108

HRESULT WINAPI D3D12EnableExperimentalFeatures(UINT NumFeatures, const IID* pIIDs, void* pConfigurationStructs,	UINT* pConfigurationStructSizes) {
	typedef HRESULT(WINAPI* D3D12_Type)(UINT NumFeatures, const IID* pIIDs,	void* pConfigurationStructs, UINT* pConfigurationStructSizes);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12EnableExperimentalFeatures");
	return fn(NumFeatures,pIIDs,pConfigurationStructs,pConfigurationStructSizes);
} // 110

HRESULT WINAPI D3D12GetInterface(struct _GUID* a1, const struct _GUID* a2, void** a3) {
	typedef HRESULT(WINAPI* D3D12_Type)(struct _GUID* a1, const struct _GUID* a2, void** a3);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12GetInterface");
	return fn(a1, a2, a3);
} // 111

HRESULT WINAPI D3D12PIXEventsReplaceBlock(bool getEarliestTime) {
	typedef HRESULT(WINAPI* D3D12_Type)(bool getEarliestTime);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXEventsReplaceBlock");
	return fn(getEarliestTime);
} // 112

HRESULT WINAPI D3D12PIXGetThreadInfo() {
	typedef HRESULT(WINAPI* D3D12_Type)();
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXGetThreadInfo");
	return fn();
} // 113

HRESULT WINAPI D3D12PIXNotifyWakeFromFenceSignal(HANDLE a, HANDLE b) {
	typedef HRESULT(WINAPI* D3D12_Type)(HANDLE a, HANDLE b);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXNotifyWakeFromFenceSignal");
	return fn(a, b);
} // 114

HRESULT WINAPI D3D12PIXReportCounter(HANDLE a, HANDLE b, HANDLE c) {
	typedef HRESULT(WINAPI* D3D12_Type)(HANDLE a, HANDLE b, HANDLE c);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXReportCounter");
	return fn(a, b, c);
} // 115

HRESULT WINAPI D3D12SerializeRootSignature(
	const D3D12_ROOT_SIGNATURE_DESC* pRootSignature,
	D3D_ROOT_SIGNATURE_VERSION Version,
	ID3DBlob** ppBlob,
	ID3DBlob** ppErrorBlob) {
	typedef HRESULT(WINAPI* D3D12_Type)(
		const D3D12_ROOT_SIGNATURE_DESC* pRootSignature,
		D3D_ROOT_SIGNATURE_VERSION Version,
		ID3DBlob** ppBlob,
		ID3DBlob** ppErrorBlob);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12SerializeRootSignature");
	return fn(pRootSignature,Version,ppBlob,ppErrorBlob);
} // 116

HRESULT WINAPI D3D12SerializeVersionedRootSignature(
	const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature,
	ID3DBlob** ppBlob,
	ID3DBlob** ppErrorBlob) {
	typedef HRESULT(WINAPI* D3D12_Type)(
		const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature,
		ID3DBlob** ppBlob,
		ID3DBlob** ppErrorBlob);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12SerializeVersionedRootSignature");
	return fn(pRootSignature, ppBlob, ppErrorBlob);
} // 117
#pragma endregion