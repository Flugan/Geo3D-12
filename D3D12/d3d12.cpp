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
bool				gl_dumpBin = false;
bool				gl_dumpASM = true;
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
	_In_  HINSTANCE hinstDLL,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved)
{
	bool result = true;

	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		LoadOriginalDll();
		InitializeCriticalSection(&gl_CS);
		ShowStartupScreen(hinstDLL);
		::CreateDirectory("c:\\Flugan", NULL);
		LogFile = _fsopen("c:\\Flugan\\Log_dx12.txt", "w", _SH_DENYNO);
		setvbuf(LogFile, NULL, _IONBF, 0);
		LogInfo("Project Flugan loaded:\n");
		char cwd[MAX_PATH];
		_getcwd(cwd, MAX_PATH);
		LogInfo("%s\n", cwd);
		if (gl_dumpBin)
			CreateDirectory("C:\\Flugan\\ShaderCache", NULL);
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
		if (gl_dumpBin) {
			sprintf_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache\\%016llX-%s.bin", crc, type);
			EnterCriticalSection(&gl_CS);
			if (!fileExists(path)) {
				fopen_s(&f, path, "wb");
				fwrite(pData, 1, length, f);
				fclose(f);
			}
			LeaveCriticalSection(&gl_CS);
		}
		if (gl_dumpASM) {
			sprintf_s(path, MAX_PATH, "C:\\Flugan\\ShaderCache\\%016llX-%s.txt", crc, type);
			EnterCriticalSection(&gl_CS);
			if (!fileExists(path)) {
				vector<byte> v;
				byte* bArray = (byte*)pData;
				for (int i = 0; i < length; i++) {
					v.push_back(bArray[i]);
				}
				auto ASM = disassembler(v);
				fopen_s(&f, path, "wb");
				fwrite(ASM.data(), 1, ASM.size(), f);
				fclose(f);
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

void hookDevice(void** ppDevice) {
	if (!gl_hookedDevice) {
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppDevice;

		D3D12_CGPS origCGPS = (D3D12_CGPS)(*vTable)[10];
		D3D12_CCPS origCCPS = (D3D12_CCPS)(*vTable)[11];

		cHookMgr.Hook(&(sCreateGraphicsPipelineState_Hook.nHookId), (LPVOID*)&(sCreateGraphicsPipelineState_Hook.fnCreateGraphicsPipelineState), origCGPS, D3D12_CreateGraphicsPipelineState);
		cHookMgr.Hook(&(sCreateComputePipelineState_Hook.nHookId), (LPVOID*)&(sCreateComputePipelineState_Hook.fnCreateComputePipelineState), origCCPS, D3D12_CreateComputePipelineState);

		gl_hookedDevice = true;
	}
}


// Exports
signed int WINAPI GetBehaviorValue(const char *a1, unsigned __int64 *a2) {
	typedef signed int(WINAPI* D3D12_Type)(const char *a1, unsigned __int64 *a2);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "GetBehaviorValue");
	return fn(a1, a2);
}; // 100

HRESULT WINAPI D3D12CreateDevice(
	IUnknown* pAdapter,
	D3D_FEATURE_LEVEL MinimumFeatureLevel,
	REFIID riid,
	void** ppDevice) {
	typedef HRESULT (WINAPI* D3D12_Type)(
		IUnknown* pAdapter,
		D3D_FEATURE_LEVEL MinimumFeatureLevel,
		REFIID riid,
		void** ppDevice);
	D3D12_Type fn = (D3D12_Type) GetProcAddress( gl_hOriginalDll, "D3D12CreateDevice");
	HRESULT res = fn(pAdapter, MinimumFeatureLevel, riid, ppDevice);
	//if (ppDevice != NULL)
	//	hookDevice(ppDevice);
	return res;
} // 101

HRESULT WINAPI D3D12GetDebugInterface(
	REFIID riid,
	void** ppvDebug) {
	typedef HRESULT(WINAPI* D3D12_Type)(
		REFIID riid,
		void** ppvDebug);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12GetDebugInterface");
	return fn(riid, ppvDebug);
} // 102

int WINAPI SetAppCompatStringPointer(unsigned __int32 a1, const char *a2) {
	typedef HRESULT(WINAPI* D3D12_Type)(unsigned __int32 a1, const char *a2);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "SetAppCompatStringPointer");
	return fn(a1, a2);
} // 103

HRESULT WINAPI D3D12CoreCreateLayeredDevice(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj) {
	typedef HRESULT(WINAPI* D3D12_Type)(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreCreateLayeredDevice");
	return fn(unknown0, unknown1, unknown2, riid, ppvObj);
} // 104

HRESULT WINAPI D3D12CoreGetLayeredDeviceSize(const void *unknown0, DWORD unknown1) {
	typedef HRESULT(WINAPI* D3D12_Type)(const void *unknown0, DWORD unknown1);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreGetLayeredDeviceSize");
	return fn(unknown0, unknown1);
} // 105

HRESULT WINAPI D3D12CoreRegisterLayers(const void *unknown0, DWORD unknown1) {
	typedef HRESULT(WINAPI* D3D12_Type)(const void *unknown0, DWORD unknown1);	
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreRegisterLayers");
	return fn(unknown0, unknown1);
} // 106

HRESULT WINAPI D3D12CreateRootSignatureDeserializer(
	LPCVOID pSrcData,
	SIZE_T SrcDataSizeInBytes,
	REFIID pRootSignatureDeserializerInterface,
	void** ppRootSignatureDeserializer) {
	typedef HRESULT(WINAPI* D3D12_Type)(
		LPCVOID pSrcData,
		SIZE_T SrcDataSizeInBytes,
		REFIID pRootSignatureDeserializerInterface,
		void** ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CreateRootSignatureDeserializer");
	return fn(pSrcData, SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
} // 107

HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(
	LPCVOID pSrcData,
	SIZE_T SrcDataSizeInBytes,
	REFIID pRootSignatureDeserializerInterface,
	void** ppRootSignatureDeserializer) {
	typedef HRESULT(WINAPI* D3D12_Type)(
		LPCVOID pSrcData,
		SIZE_T SrcDataSizeInBytes,
		REFIID pRootSignatureDeserializerInterface,
		void** ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CreateVersionedRootSignatureDeserializer");
	return fn(pSrcData,SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
} // 108


HRESULT WINAPI D3D12EnableExperimentalFeatures(
	UINT NumFeatures,
	const IID* pIIDs,
	void* pConfigurationStructs,
	UINT* pConfigurationStructSizes) {
	typedef HRESULT(WINAPI* D3D12_Type)(
		UINT NumFeatures,
		const IID* pIIDs,
		void* pConfigurationStructs,
		UINT* pConfigurationStructSizes);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12EnableExperimentalFeatures");
	return fn(NumFeatures,pIIDs,pConfigurationStructs,pConfigurationStructSizes);
} // 110

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

HRESULT WINAPI D3D12PIXNotifyWakeFromFenceSignal(HANDLE event) {
	typedef HRESULT(WINAPI* D3D12_Type)(HANDLE event);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXNotifyWakeFromFenceSignal");
	return fn(event);
} // 114

HRESULT WINAPI D3D12PIXReportCounter(wchar_t const* name, float value) {
	typedef HRESULT(WINAPI* D3D12_Type)(wchar_t const* name, float value);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXReportCounter");
	return fn(name, value);
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

void LoadOriginalDll(void)
{
	wchar_t sysDir[MAX_PATH];
	::GetSystemDirectoryW(sysDir, MAX_PATH);
	wcscat_s(sysDir, MAX_PATH, L"\\D3D12.dll");
	if (!gl_hOriginalDll) gl_hOriginalDll = ::LoadLibraryExW(sysDir, NULL, NULL);
}