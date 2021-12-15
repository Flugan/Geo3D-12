// proxydll.cpp
#include "stdafx.h"
#include "proxydll.h"
#include "resource.h"
#include "..\log.h"
#include "..\Nektra\NktHookLib.h"
#include <d3d9.h>

// include the Direct3D Library file
#pragma comment (lib, "d3d9.lib")

// global variables
#pragma data_seg (".d3d12_shared")
HINSTANCE			gl_hInstDLL = NULL;
HINSTANCE           gl_hOriginalDll = NULL;
bool				gl_hookedDevice = false;
bool				gl_Present_hooked = false;
FILE*				LogFile = NULL;
bool				gLogDebug = false;
bool				gl_dumpBin = true;
bool				gl_dumpASM = false;
CNktHookLib			cHookMgr;
CRITICAL_SECTION	gl_CS;
// global declarations
LPDIRECT3D9 d3d;    // the pointer to our Direct3D interface
LPDIRECT3DDEVICE9 d3ddev;    // the pointer to the device class
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
		LogFile = _fsopen("Log_dx12.txt", "w", _SH_DENYNO);
		setvbuf(LogFile, NULL, _IONBF, 0);
		LogInfo("Project Flugan loaded:\n");
		gl_hOriginalDll = ::LoadLibrary("c:\\windows\\system32\\d3d12.dll");
		LogInfo("Original DLL loaded\n");
		InitializeCriticalSection(&gl_CS);
		LogInfo("Critical Section\n");
		ShowStartupScreen();
		LogInfo("Startup screen\n");
		if (gl_dumpBin || gl_dumpASM) 
		{
			LogInfo("Dumping\n");
			CreateDirectory("ShaderCache", NULL);
		}
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
		if (gl_dumpBin) {
			sprintf_s(path, MAX_PATH, "ShaderCache\\%016llX-%s.bin", crc, type);
			EnterCriticalSection(&gl_CS);
			if (!fileExists(path)) {
				fopen_s(&f, path, "wb");
				if (f != 0) {
					fwrite(pData, 1, length, f);
					fclose(f);
				}
			}
			LeaveCriticalSection(&gl_CS);
		}
		if (gl_dumpASM) {
			sprintf_s(path, MAX_PATH, "ShaderCache\\%016llX-%s.txt", crc, type);
			EnterCriticalSection(&gl_CS);
			if (!fileExists(path)) {
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


#pragma region DX9
// define the screen resolution
#define SCREEN_WIDTH  2560
#define SCREEN_HEIGHT 1440

// this is the main message handler for the DX9 window
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		return 0;
	} break;
	case WM_CLOSE:
	{
		PostQuitMessage(0);
		return 0;
	} break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

void CreateDX9Window() {
	WNDCLASSEX wc;

	ZeroMemory(&wc, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = gl_hInstDLL;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = "WindowClass9";

	RegisterClassEx(&wc);

	HWND hWnd = CreateWindowEx(WS_EX_TOPMOST,
		"WindowClass9",
		"3D Vision DX9",
		WS_POPUP,
		0, 0,    // the starting x and y positions should be 0
		SCREEN_WIDTH, SCREEN_HEIGHT,    // set window to new resolution
		NULL,
		NULL,
		NULL,
		NULL);

	ShowWindow(hWnd, SW_SHOWDEFAULT);

	d3d = Direct3DCreate9(D3D_SDK_VERSION);    // create the Direct3D interface
	D3DPRESENT_PARAMETERS d3dpp;    // create a struct to hold various device information

	ZeroMemory(&d3dpp, sizeof(d3dpp));    // clear out the struct for use
	d3dpp.Windowed = FALSE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;    // discard old frames
	d3dpp.BackBufferCount = 1;
	d3dpp.hDeviceWindow = hWnd;    // set the window to be used by Direct3D
	d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;    // set the back buffer format to 32-bit
	d3dpp.BackBufferWidth = SCREEN_WIDTH;    // set the width of the buffer
	d3dpp.BackBufferHeight = SCREEN_HEIGHT;    // set the height of the buffer

	// create a device class using this information and the info from the d3dpp stuct
	HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		hWnd,
		D3DCREATE_SOFTWARE_VERTEXPROCESSING,
		&d3dpp,
		&d3ddev);
}

// this is the function used to render a single frame
void render_frame()
{
	d3ddev->BeginScene();    // begins the 3D scene

	IDirect3DSurface9* gImageSrc = NULL; // Source stereo image beeing created

	d3ddev->CreateOffscreenPlainSurface(
		SCREEN_WIDTH * 2, // Stereo width is twice the source width
		SCREEN_HEIGHT + 1, // Stereo height add one raw to encode signature
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, // Surface is in video memory
		&gImageSrc, NULL);
	// Blit SBS src image to both side of stereo

	IDirect3DSurface9* gImageLeft = NULL; // Source stereo image beeing created

	d3ddev->CreateOffscreenPlainSurface(
		SCREEN_WIDTH, // Stereo width is twice the source width
		SCREEN_HEIGHT, // Stereo height add one raw to encode signature
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, // Surface is in video memory
		&gImageLeft, NULL);
	d3ddev->ColorFill(gImageLeft, NULL, D3DCOLOR_XRGB(255, 255, 255));
	RECT left = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	d3ddev->StretchRect(gImageLeft, &left, gImageSrc, &left, D3DTEXF_NONE);

	// Stereo Blit defines
#define NVSTEREO_IMAGE_SIGNATURE 0x4433564e //NV3D

	typedef struct _Nv_Stereo_Image_Header
	{
		unsigned int dwSignature;
		unsigned int dwWidth;
		unsigned int dwHeight;
		unsigned int dwBPP;
		unsigned int dwFlags;
	} NVSTEREOIMAGEHEADER, * LPNVSTEREOIMAGEHEADER;

	// ORed flags in the dwFlags fiels of the _Nv_Stereo_Image_Header structure above
#define SIH_SWAP_EYES 0x00000001
#define SIH_SCALE_TO_FIT 0x00000002

	// Lock the stereo image
	D3DLOCKED_RECT lr;
	gImageSrc->LockRect(&lr, NULL, 0);

	// write stereo signature in the last raw of the stereo image
	LPNVSTEREOIMAGEHEADER pSIH =
		(LPNVSTEREOIMAGEHEADER)(((unsigned char*)lr.pBits) + (lr.Pitch * SCREEN_HEIGHT));

	// Update the signature header values
	pSIH->dwSignature = NVSTEREO_IMAGE_SIGNATURE;
	pSIH->dwBPP = 32;
	pSIH->dwFlags = SIH_SWAP_EYES; // Src image has left on left and right on right
	pSIH->dwWidth = SCREEN_WIDTH * 2;
	pSIH->dwHeight = SCREEN_HEIGHT;

	// Unlock surface
	gImageSrc->UnlockRect();

	IDirect3DSurface9* pBackBuffer;
	d3ddev->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);

	RECT codedRect = { 0, 0, SCREEN_WIDTH * 2, SCREEN_HEIGHT + 1 };
	RECT backRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	d3ddev->StretchRect(gImageSrc, &codedRect, pBackBuffer, &backRect, D3DTEXF_NONE);

	pBackBuffer->Release();
	gImageSrc->Release();

	d3ddev->EndScene();    // ends the 3D scene

	d3ddev->Present(NULL, NULL, NULL, NULL);   // displays the created frame on the screen
}
#pragma endregion

#pragma region DXGI
HRESULT STDMETHODCALLTYPE DXGIH_Present(IDXGISwapChain* This, UINT SyncInterval, UINT Flags) {
	//render_frame();
	HRESULT hr = sDXGI_Present_Hook.fnDXGI_Present(This, SyncInterval, Flags);
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGI_CreateSwapChain1(IDXGIFactory1* This, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) {
	LogInfo("CreateSwapChain1\n");
	HRESULT hr = sCreateSwapChain_Hook.fnCreateSwapChain1(This, pDevice, pDesc, ppSwapChain);
	if (!gl_Present_hooked) {
		LogInfo("Present hooked\n");
		gl_Present_hooked = true;
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppSwapChain;
		DXGI_Present origPresent = (DXGI_Present)(*vTable)[8];
		cHookMgr.Hook(&(sDXGI_Present_Hook.nHookId), (LPVOID*)&(sDXGI_Present_Hook.fnDXGI_Present), origPresent, DXGIH_Present);
	}
	return hr;
}

void HackedPresent() {
	IDXGIFactory1** ppFactory;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)ppFactory);
	DWORD_PTR*** vTable = (DWORD_PTR***)*ppFactory;
	DXGI_CSC1 origCSC1 = (DXGI_CSC1)(*vTable)[10];
	cHookMgr.Hook(&(sCreateSwapChain_Hook.nHookId), (LPVOID*)&(sCreateSwapChain_Hook.fnCreateSwapChain1), origCSC1, DXGI_CreateSwapChain1);
	(*ppFactory)->Release();
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

		//HackedPresent();
		//CreateDX9Window();
		gl_hookedDevice = true;
	}
}

#pragma region Exports
HRESULT _fastcall GetBehaviorValue(const char* a1, unsigned __int64* a2) {
	LogInfo("GetBehaviorValue\n");
	typedef HRESULT(_fastcall* D3D12_Type)(const char *a, unsigned __int64 *b);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "GetBehaviorValue");
	return fn(a1, a2);
}; // 100

HRESULT D3D12CreateDevice(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice) {
	LogInfo("CreateDevice\n");
	typedef HRESULT(*D3D12_Type)(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice);
	D3D12_Type fn = (D3D12_Type)NktHookLibHelpers::GetProcedureAddress(gl_hOriginalDll, "D3D12CreateDevice");
	HRESULT res = fn(pAdapter, MinimumFeatureLevel, riid, ppDevice);
	if (ppDevice)
		hookDevice(ppDevice);
	return res;
} // 101

HRESULT WINAPI D3D12GetDebugInterface(const IID* const riid, void** ppvDebug) {
	LogInfo("GetDebugInterface\n");
	typedef HRESULT(WINAPI* D3D12_Type)(const IID* const riid, void** ppvDebug);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12GetDebugInterface");
	return fn(riid, ppvDebug);
} // 102

void _fastcall SetAppCompatStringPointer(unsigned __int64 a1, const char* a2) {
	LogInfo("SetAppCompatStringPointer\n");
	typedef void(_fastcall* D3D12_Type)(unsigned __int64 a1, const char* a2);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "SetAppCompatStringPointer");
	return fn(a1, a2);
} // 103


HRESULT WINAPI D3D12CoreCreateLayeredDevice(const void *u0, DWORD u1, const void *u2, REFIID riid, void **ppvObj) {
	LogInfo("CoreCreateLayeredDevice\n");
	typedef HRESULT(WINAPI* D3D12_Type)(const void *u0, DWORD u1, const void *u2, REFIID riid, void **ppvObj);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreCreateLayeredDevice");
	return fn(u0, u1, u2, riid, ppvObj);
} // 104

HRESULT WINAPI D3D12CoreGetLayeredDeviceSize(const void *u0, DWORD u1) {
	LogInfo("CoreGetLayeredDeviceSize\n");
	typedef HRESULT(WINAPI* D3D12_Type)(const void *u0, DWORD u1);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreGetLayeredDeviceSize");
	return fn(u0, u1);
} // 105

HRESULT WINAPI D3D12CoreRegisterLayers(const void *u0, DWORD u1) {
	LogInfo("CoreRegisterLayers\n");
	typedef HRESULT(WINAPI* D3D12_Type)(const void *u0, DWORD u1);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreRegisterLayers");
	return fn(u0, u1);
} // 106


HRESULT WINAPI D3D12CreateRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T  SrcDataSizeInBytes, REFIID  pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer) {
	LogInfo("CreateRootSignatureDeserializer\n");
	typedef HRESULT(WINAPI* D3D12_Type)(LPCVOID pSrcData, SIZE_T  SrcDataSizeInBytes, REFIID  pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CreateRootSignatureDeserializer");
	return fn(pSrcData, SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
} // 107

HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer) {
	LogInfo("CreateVersionedRootSignatureDeserializer\n");
	typedef HRESULT(WINAPI* D3D12_Type)(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CreateVersionedRootSignatureDeserializer");
	return fn(pSrcData,SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
} // 108

HRESULT WINAPI D3D12EnableExperimentalFeatures(UINT NumFeatures, const IID* pIIDs, void* pConfigurationStructs,	UINT* pConfigurationStructSizes) {
	LogInfo("EnableExperimentalFeatures\n");
	typedef HRESULT(WINAPI* D3D12_Type)(UINT NumFeatures, const IID* pIIDs,	void* pConfigurationStructs, UINT* pConfigurationStructSizes);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12EnableExperimentalFeatures");
	return fn(NumFeatures,pIIDs,pConfigurationStructs,pConfigurationStructSizes);
} // 110

HRESULT WINAPI D3D12GetInterface(struct _GUID* a1, const struct _GUID* a2, void** a3) {
	LogInfo("GetInterface\n");
	typedef HRESULT(WINAPI* D3D12_Type)(struct _GUID* a1, const struct _GUID* a2, void** a3);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12GetInterface");
	return fn(a1, a2, a3);
} // 111

HRESULT WINAPI D3D12PIXEventsReplaceBlock(bool getEarliestTime) {
	LogInfo("PIXEventsReplaceBlock\n");
	typedef HRESULT(WINAPI* D3D12_Type)(bool getEarliestTime);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXEventsReplaceBlock");
	return fn(getEarliestTime);
} // 112

HRESULT WINAPI D3D12PIXGetThreadInfo() {
	LogInfo("PIXGetThreadInfo\n");
	typedef HRESULT(WINAPI* D3D12_Type)();
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXGetThreadInfo");
	return fn();
} // 113

HRESULT WINAPI D3D12PIXNotifyWakeFromFenceSignal(HANDLE a, HANDLE b) {
	LogInfo("PIXNotifyWakeFromFenceSignal\n");
	typedef HRESULT(WINAPI* D3D12_Type)(HANDLE a, HANDLE b);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXNotifyWakeFromFenceSignal");
	return fn(a, b);
} // 114

HRESULT WINAPI D3D12PIXReportCounter(HANDLE a, HANDLE b, HANDLE c) {
	LogInfo("PIXReportCounter\n");
	typedef HRESULT(WINAPI* D3D12_Type)(HANDLE a, HANDLE b, HANDLE c);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXReportCounter");
	return fn(a, b, c);
} // 115

HRESULT WINAPI D3D12SerializeRootSignature(
	const D3D12_ROOT_SIGNATURE_DESC* pRootSignature,
	D3D_ROOT_SIGNATURE_VERSION Version,
	ID3DBlob** ppBlob,
	ID3DBlob** ppErrorBlob) {
	LogInfo("SerializeRootSignature\n");
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
	LogInfo("SerializeVersionedRootSignature\n");
	typedef HRESULT(WINAPI* D3D12_Type)(
		const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature,
		ID3DBlob** ppBlob,
		ID3DBlob** ppErrorBlob);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12SerializeVersionedRootSignature");
	return fn(pRootSignature, ppBlob, ppErrorBlob);
} // 117
#pragma endregion