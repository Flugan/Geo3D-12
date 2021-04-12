// proxydll.cpp
#include "stdafx.h"
#include "proxydll.h"
#include "resource.h"
#include "..\log.h"
#include "..\Nektra\NktHookLib.h"

// global variables
#pragma data_seg (".d3d12_shared")
HINSTANCE           gl_hOriginalDll = NULL;
bool				gl_hookedDevice = false;
FILE*				LogFile = NULL;
bool				gLogDebug = false;
bool				gl_dumpBin = true;
CNktHookLib			cHookMgr;
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

void ShowStartupScreen(HINSTANCE hinstDLL)
{
	BOOL affinity = -1;
	DWORD_PTR one = 0x01;
	DWORD_PTR before = 0;
	DWORD_PTR before2 = 0;
	affinity = GetProcessAffinityMask(GetCurrentProcess(), &before, &before2);
	affinity = SetProcessAffinityMask(GetCurrentProcess(), one);
	HBITMAP hBM = ::LoadBitmap(hinstDLL, MAKEINTRESOURCE(IDB_STARTUP));
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
			::Beep(440, 500);
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
	_In_  HINSTANCE hinstDLL,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved)
{
	bool result = true;

	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		ShowStartupScreen(hinstDLL);
		::CreateDirectory("c:\\Flugan", NULL);
		LogFile = _fsopen("c:\\Flugan\\Log_dx12.txt", "w", _SH_DENYNO);
		setvbuf(LogFile, NULL, _IONBF, 0);
		LogInfo("Project Flugan loaded:\n");
		char cwd[MAX_PATH];
		_getcwd(cwd, MAX_PATH);
		LogInfo("%s\n", cwd);
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

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CGPS)(ID3D12Device* This, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, const IID &riid, void **ppPipelineState);
static struct {
	SIZE_T nHookId;
	D3D12_CGPS fnCreateGraphicsPipelineState;
} sCreateGraphicsPipelineState_Hook = { 0, NULL };
HRESULT STDMETHODCALLTYPE D3D12_CreateGraphicsPipelineState(ID3D12Device* This, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) {
	FILE* f;
	char path[MAX_PATH];
	if (gl_dumpBin) {
		strcat_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache");
		CreateDirectory(path, NULL);
	}
	if (pDesc->VS.BytecodeLength > 0) {
		UINT64 crc = fnv_64_buf(pDesc->VS.pShaderBytecode, pDesc->VS.BytecodeLength);
		LogInfo("VertexShader: %016llX\n", crc);
		
		if (gl_dumpBin) {
			sprintf_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache\\%016llX-vs.bin", crc);
			fopen_s(&f, path, "wb");
			fwrite(pDesc->VS.pShaderBytecode, 1, pDesc->VS.BytecodeLength, f);
			fclose(f);
		}
	}
	if (pDesc->PS.BytecodeLength > 0) {
		UINT64 crc = fnv_64_buf(pDesc->PS.pShaderBytecode, pDesc->PS.BytecodeLength);
		LogInfo("PixelShader: %016llX\n", crc);
		if (gl_dumpBin) {
			sprintf_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache\\%016llX-ps.bin", crc);
			fopen_s(&f, path, "wb");
			fwrite(pDesc->PS.pShaderBytecode, 1, pDesc->PS.BytecodeLength, f);
			fclose(f);
		}
	}
	if (pDesc->DS.BytecodeLength > 0) {
		UINT64 crc = fnv_64_buf(pDesc->DS.pShaderBytecode, pDesc->DS.BytecodeLength);
		LogInfo("DomainShader: %016llX\n", crc);
		if (gl_dumpBin) {
			sprintf_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache\\%016llX-ds.bin", crc);
			fopen_s(&f, path, "wb");
			fwrite(pDesc->DS.pShaderBytecode, 1, pDesc->DS.BytecodeLength, f);
			fclose(f);
		}
	}
	if (pDesc->GS.BytecodeLength > 0) {
		UINT64 crc = fnv_64_buf(pDesc->GS.pShaderBytecode, pDesc->GS.BytecodeLength);
		LogInfo("GeometryShader: %016llX\n", crc);
		if (gl_dumpBin) {
			sprintf_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache\\%016llX-gs.bin", crc);
			fopen_s(&f, path, "wb");
			fwrite(pDesc->GS.pShaderBytecode, 1, pDesc->GS.BytecodeLength, f);
			fclose(f);
		}
	}
	if (pDesc->HS.BytecodeLength > 0) {
		UINT64 crc = fnv_64_buf(pDesc->HS.pShaderBytecode, pDesc->HS.BytecodeLength);
		LogInfo("HullShader: %016llX\n", crc);
		if (gl_dumpBin) {
			sprintf_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache\\%016llX-hs.bin", crc);
			fopen_s(&f, path, "wb");
			fwrite(pDesc->HS.pShaderBytecode, 1, pDesc->HS.BytecodeLength, f);
			fclose(f);
		}
	}
	return sCreateGraphicsPipelineState_Hook.fnCreateGraphicsPipelineState(This, pDesc, riid, ppPipelineState);
}

typedef HRESULT(STDMETHODCALLTYPE* D3D12_CCPS)(ID3D12Device* This, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState);
static struct {
	SIZE_T nHookId;
	D3D12_CCPS fnCreateComputePipelineState;
} sCreateComputePipelineState_Hook = { 1, NULL };
HRESULT STDMETHODCALLTYPE D3D12_CreateComputePipelineState(ID3D12Device* This, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) {
	FILE* f;
	char path[MAX_PATH];
	if (gl_dumpBin) {
		strcat_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache");
		CreateDirectory(path, NULL);
	}
	if (pDesc->CS.BytecodeLength > 0) {
		UINT64 crc = fnv_64_buf(pDesc->CS.pShaderBytecode, pDesc->CS.BytecodeLength);
		LogInfo("ComputeShader: %016llX\n", crc);
		if (gl_dumpBin) {
			sprintf_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache\\%016llX-cs.bin", crc);
			fopen_s(&f, path, "wb");
			fwrite(pDesc->CS.pShaderBytecode, 1, pDesc->CS.BytecodeLength, f);
			fclose(f);
		}
	}
	return sCreateComputePipelineState_Hook.fnCreateComputePipelineState(This, pDesc, riid, ppPipelineState);
}

void hookDevice(void** ppDevice) {
	if (!gl_hookedDevice) {
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppDevice;

		D3D12_CGPS origCGPS = (D3D12_CGPS)(*vTable)[10];
		D3D12_CCPS origCCPS = (D3D12_CCPS)(*vTable)[11];

		cHookMgr.Hook(&(sCreateGraphicsPipelineState_Hook.nHookId), (LPVOID*)&(sCreateGraphicsPipelineState_Hook.fnCreateGraphicsPipelineState), origCGPS, D3D12_CreateGraphicsPipelineState);
		cHookMgr.Hook(&(sCreateComputePipelineState_Hook.nHookId), (LPVOID*)&(sCreateComputePipelineState_Hook.fnCreateComputePipelineState), origCCPS, D3D12_CreateComputePipelineState);
	}
}

// Exports
signed int WINAPI GetBehaviorValue(const char *a1, unsigned __int64 *a2) {
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef signed int(WINAPI* D3D12_Type)(const char *a1, unsigned __int64 *a2);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "GetBehaviorValue");
	return fn(a1, a2);
}; // 100

HRESULT WINAPI D3D12CreateDevice(
	_In_opt_	IUnknown			*pAdapter,
				D3D_FEATURE_LEVEL	MinimumFeatureLevel,
	_In_		REFIID				riid,
	_Out_opt_	void				**ppDevice)
{
	if (!gl_hOriginalDll) LoadOriginalDll();	
	typedef HRESULT (WINAPI* D3D12_Type)(
	IUnknown			*pAdapter,
	D3D_FEATURE_LEVEL	MinimumFeatureLevel,
	REFIID				riid,
	void				**ppDevice);
	D3D12_Type fn = (D3D12_Type) GetProcAddress( gl_hOriginalDll, 
		"D3D12CreateDevice");
	HRESULT res = fn(pAdapter, MinimumFeatureLevel, riid, ppDevice);
	if (ppDevice != NULL)
		hookDevice(ppDevice);
	return res;
} // 101

HRESULT WINAPI D3D12GetDebugInterface(
	_In_      REFIID riid,
	_Out_opt_ void   **ppvDebug)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(
		_In_      REFIID riid,
		_Out_opt_ void   **ppvDebug);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, 
		"D3D12GetDebugInterface");
	HRESULT res = fn(riid, ppvDebug);
	return res;
} // 102

int WINAPI SetAppCompatStringPointer(unsigned __int32 a1, const char *a2)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(unsigned __int32 a1, const char *a2);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,
		"SetAppCompatStringPointer");
	int res = fn(a1, a2);
	return res;
} // 103

HRESULT WINAPI D3D12CoreCreateLayeredDevice(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, 
		"D3D12CoreCreateLayeredDevice");
	HRESULT res = fn(unknown0, unknown1, unknown2, riid, ppvObj);
	return res;
} // 104

HRESULT WINAPI D3D12CoreGetLayeredDeviceSize(const void *unknown0, DWORD unknown1)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(const void *unknown0, DWORD unknown1);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, 
		"D3D12CoreGetLayeredDeviceSize");
	HRESULT res = fn(unknown0, unknown1);
	return res;
} // 105

HRESULT WINAPI D3D12CoreRegisterLayers(const void *unknown0, DWORD unknown1)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(const void *unknown0, DWORD unknown1);	
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, 
		"D3D12CoreRegisterLayers");
	HRESULT res = fn(unknown0, unknown1);
	return res;
} // 106

HRESULT WINAPI D3D12CreateRootSignatureDeserializer(
	_In_  LPCVOID pSrcData,
	_In_  SIZE_T  SrcDataSizeInBytes,
	_In_  REFIID  pRootSignatureDeserializerInterface,
	_Out_ void    **ppRootSignatureDeserializer)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(
		_In_  LPCVOID pSrcData,
		_In_  SIZE_T  SrcDataSizeInBytes,
		_In_  REFIID  pRootSignatureDeserializerInterface,
		_Out_ void    **ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, 
		"D3D12CreateRootSignatureDeserializer");
	HRESULT res = fn(pSrcData, SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
	return res;
} // 107

HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(
	_In_  LPCVOID pSrcData,
	_In_  SIZE_T  SrcDataSizeInBytes,
	_In_  REFIID  pRootSignatureDeserializerInterface,
	_Out_ void    **ppRootSignatureDeserializer)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(
		_In_  LPCVOID pSrcData,
		_In_  SIZE_T  SrcDataSizeInBytes,
		_In_  REFIID  pRootSignatureDeserializerInterface,
		_Out_ void    **ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, 
		"D3D12CreateVersionedRootSignatureDeserializer");
	HRESULT res = fn(pSrcData,SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
	return res;
} // 108


HRESULT WINAPI D3D12DeviceRemovedExtendedData() {
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)();
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,
		"D3D12DeviceRemovedExtendedData");
	HRESULT res = fn();
	return res;
} // 109

HRESULT WINAPI D3D12EnableExperimentalFeatures(
	UINT NumFeatures,
	_In_ const IID  *pIIDs,
	_In_       void *pConfigurationStructs,
	_In_       UINT *pConfigurationStructSizes)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(
		UINT NumFeatures,
		_In_ const IID  *pIIDs,
		_In_       void *pConfigurationStructs,
		_In_       UINT *pConfigurationStructSizes);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, 
		"D3D12EnableExperimentalFeatures");
	HRESULT res = fn(NumFeatures,pIIDs,pConfigurationStructs,pConfigurationStructSizes);
	return res;
} // 110

HRESULT WINAPI D3D12GetInterface() {
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)();
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,
		"D3D12GetInterface");
	HRESULT res = fn();
	return res;
} // 111

HRESULT WINAPI D3D12PIXEventsReplaceBlock(bool getEarliestTime) {
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(bool getEarliestTime);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,
		"D3D12PIXEventsReplaceBlock");
	HRESULT res = fn(getEarliestTime);
	return res;
} // 112

HRESULT WINAPI D3D12PIXGetThreadInfo() {
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)();
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,
		"D3D12PIXGetThreadInfo");
	HRESULT res = fn();
	return res;
} // 113

HRESULT WINAPI D3D12PIXNotifyWakeFromFenceSignal(HANDLE event) {
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(HANDLE event);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,
		"D3D12PIXNotifyWakeFromFenceSignal");
	HRESULT res = fn(event);
	return res;
} // 114

HRESULT WINAPI D3D12PIXReportCounter(wchar_t const* name, float value) {
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(wchar_t const* name, float value);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,
		"D3D12PIXReportCounter");
	HRESULT res = fn(name, value);
	return res;
} // 115

HRESULT WINAPI D3D12SerializeRootSignature(
	_In_      const D3D12_ROOT_SIGNATURE_DESC  *pRootSignature,
	_In_            D3D_ROOT_SIGNATURE_VERSION Version,
	_Out_           ID3DBlob                   **ppBlob,
	_Out_opt_       ID3DBlob                   **ppErrorBlob)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(
		_In_      const D3D12_ROOT_SIGNATURE_DESC  *pRootSignature,
		_In_            D3D_ROOT_SIGNATURE_VERSION Version,
		_Out_           ID3DBlob                   **ppBlob,
		_Out_opt_       ID3DBlob                   **ppErrorBlob);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, 
		"D3D12SerializeRootSignature");
	HRESULT res = fn(pRootSignature,Version,ppBlob,ppErrorBlob);
	return res;
} // 116

HRESULT WINAPI D3D12SerializeVersionedRootSignature(
	_In_      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature,
	_Out_           ID3DBlob                            **ppBlob,
	_Out_opt_       ID3DBlob                            **ppErrorBlob)
{
	if (!gl_hOriginalDll) LoadOriginalDll();
	typedef HRESULT(WINAPI* D3D12_Type)(
		_In_      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature,
		_Out_           ID3DBlob                            **ppBlob,
		_Out_opt_       ID3DBlob                            **ppErrorBlob);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, 
		"D3D12SerializeVersionedRootSignature");
	HRESULT res = fn(pRootSignature, ppBlob, ppErrorBlob);
	return res;
} // 117

void LoadOriginalDll(void)
{
	wchar_t sysDir[MAX_PATH];
	::GetSystemDirectoryW(sysDir, MAX_PATH);
	wcscat_s(sysDir, MAX_PATH, L"\\D3D12.dll");
	if (!gl_hOriginalDll) gl_hOriginalDll = ::LoadLibraryExW(sysDir, NULL, NULL);
}