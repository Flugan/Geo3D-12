// proxydll.cpp
#include "stdafx.h"
#include "proxydll.h"
#include "..\log.h"
#include "..\Nektra\NktHookLib.h"

// global variables
#pragma data_seg (".d3d12_shared")
HINSTANCE			gl_hInstDLL = NULL;
HINSTANCE           gl_hOriginalDll = NULL;
UINT64				gl_Device_hooked = 0;
UINT64				gl_SwapChain_hooked = 0;
UINT64				gl_SwapChain1_hooked = 0;
UINT64				gl_GraphicsCommandQueue_hooked = 0;
UINT64				gl_GraphicsCommandList_hooked = 0;
UINT64				gl_GraphicsCommandList1_hooked = 0;
bool				gl_left = true;
FILE*				LogFile = NULL;
bool				gl_log = false;
bool				gl_dumpBin = false;
bool				gl_dumpASM = false;
bool				gl_dumpRAW = false;
bool				gl_debugAttach = false;
char				cwd[MAX_PATH];
CNktHookLib			cHookMgr;

float gSep;
float gConv;
float gEyeDist;
float gScreenSize;
float gFinalSep;
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

void readINI() {
	char setting[MAX_PATH];
	char iniFile[MAX_PATH];
	char LOGfile[MAX_PATH];

	_getcwd(iniFile, MAX_PATH);
	_getcwd(LOGfile, MAX_PATH);
	_getcwd(cwd, MAX_PATH);
	strcat_s(iniFile, MAX_PATH, "\\d3d12.ini");

	// If specified in Debug section, wait for Attach to Debugger.
	bool waitfordebugger = GetPrivateProfileInt("Debug", "attach", 0, iniFile) > 0;
	if (waitfordebugger) {
		do {
			Sleep(250);
		} while (!IsDebuggerPresent());
	}

	gl_log = GetPrivateProfileInt("Dump", "log", gl_log, iniFile) > 0;

	gl_dumpBin = GetPrivateProfileInt("Dump", "bin", gl_dumpBin, iniFile) > 0;

	gl_dumpRAW = GetPrivateProfileInt("Dump", "raw", gl_dumpRAW, iniFile) > 0;

	gl_dumpASM = GetPrivateProfileInt("Dump", "asm", gl_dumpASM, iniFile) > 0;

	if (GetPrivateProfileString("Stereo", "separation", "50", setting, MAX_PATH, iniFile)) {
		gSep = stof(setting);
	}
	if (GetPrivateProfileString("Stereo", "convergence", "1.0", setting, MAX_PATH, iniFile)) {
		gConv = stof(setting);
	}
	if (GetPrivateProfileString("Stereo", "eyedistance", "6.3", setting, MAX_PATH, iniFile)) {
		gEyeDist = stof(setting);
	}
	if (GetPrivateProfileString("Stereo", "screenSize", "55", setting, MAX_PATH, iniFile)) {
		gScreenSize = stof(setting);
	}
	float eyeSep = gEyeDist / (2.54f * gScreenSize * 16 / sqrtf(256 + 81));
	gFinalSep = eyeSep * gSep * 0.01f;

	if (gl_log) {
		strcat_s(LOGfile, MAX_PATH, "\\d3d12_log.txt");
		LogFile = _fsopen(LOGfile, "wb", _SH_DENYNO);
		setvbuf(LogFile, NULL, _IONBF, 0);
		LogInfo("Start Log:\n");
	}
}

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpvReserved)
{
	bool result = true;

	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		gl_hInstDLL = hinst;
		gl_hOriginalDll = ::LoadLibrary("c:\\windows\\system32\\d3d12.dll");
		readINI();
		break;

	case DLL_PROCESS_DETACH:
		if (gl_hOriginalDll) {
			::FreeLibrary(gl_hOriginalDll);
			gl_hOriginalDll = NULL;
		}
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

void dumpShaderRAW(char* type, const void* pData, SIZE_T length, UINT64 crc) {
	FILE* f;
	char path[MAX_PATH];
	path[0] = 0;
	if (length >= 0) {
		if (gl_dumpRAW) {
			strcat_s(path, MAX_PATH, cwd);
			strcat_s(path, MAX_PATH, "\\ShaderCache");
			CreateDirectory(path, NULL);
			string sPath(path);
			sprintf_s(path, MAX_PATH, (sPath + "\\%016llX-%s.txt").c_str(), crc, type);
			LogInfo("Dump RAW: %s\n", path);
			if (!fileExists(path)) {
				fopen_s(&f, path, "wb");
				if (f != 0) {
					fwrite(pData, 1, length, f);
					fclose(f);
				}
			}
		}
	}
}

UINT64 dumpShader(char* type, const void* pData, SIZE_T length) {
	UINT64 crc = fnv_64_buf(pData, length);
	FILE* f;
	char path[MAX_PATH];
	path[0] = 0;
	if (length > 0) {
		if (gl_dumpBin) {
			strcat_s(path, MAX_PATH, cwd);
			strcat_s(path, MAX_PATH, "\\ShaderCache");
			CreateDirectory(path, NULL);
			string sPath(path);
			sprintf_s(path, MAX_PATH, (sPath + "\\%016llX-%s.bin").c_str(), crc, type);
			LogInfo("Dump BIN: %s\n", path);
			if (!fileExists(path)) {
				fopen_s(&f, path, "wb");
				if (f != 0) {
					fwrite(pData, 1, length, f);
					fclose(f);
				}
			}
		}
		if (gl_dumpASM) {
			strcat_s(path, MAX_PATH, cwd);
			strcat_s(path, MAX_PATH, "\\ShaderCache");
			CreateDirectory(path, NULL);
			string sPath(path);
			sprintf_s(path, MAX_PATH, (sPath + "\\%016llX-%s.txt").c_str(), crc, type);
			LogInfo("Dump ASM: %s\n", path);
			if (!fileExists(path)) {
				vector<byte> v;
				byte* bArray = (byte*)pData;
				for (int i = 0; i < length; i++) {
					v.push_back(bArray[i]);
				}
				auto ASM = disassembler(v);

				if (ASM.size() > 0) {
					fopen_s(&f, path, "wb");
					if (f != 0) {
						fwrite(ASM.data(), 1, ASM.size(), f);
						fclose(f);
					}
				}
			}
		}
	}
	return crc;
}
#pragma endregion

#pragma region PipelineState
string changeASM(vector<byte> ASM, bool left) {
	auto lines = stringToLines((char*)ASM.data(), ASM.size());
	string shaderL;
	string shaderR;
	string shader;
	string oReg;
	bool dcl = false;
	bool dcl_ICB = false;
	int temp = 0;
	for (int i = 0; i < lines.size(); i++) {
		string s = lines[i];
		if (s.find("dcl") == 0) {
			dcl = true;
			dcl_ICB = false;
			if (s.find("dcl_output_siv") == 0 && s.find("position") != string::npos) {
				oReg = s.substr(15, 2);
				shader += s + "\n";
			}
			else if (s.find("dcl_temps") == 0) {
				string num = s.substr(10);
				temp = atoi(num.c_str()) + 2;
				shader += "dcl_temps " + to_string(temp) + "\n";
			}
			else if (s.find("dcl_immediateConstantBuffer") == 0) {
				dcl_ICB = true;
				shader += s + "\n";
			}
			else {
				shader += s + "\n";
			}
		}
		else if (dcl_ICB == true) {
			shader += s + "\n";
		}
		else if (dcl == true) {
			// after dcl
			if (s.find("ret") < s.size()) {
				char buf[80];
				sprintf_s(buf, 80, "%.8f", gFinalSep);
				string sep(buf);
				sprintf_s(buf, 80, "%.3f", gConv);
				string conv(buf);

				string changeSep = left ? "l(-" + sep + ")" : "l(" + sep + ")";
				shader +=
					"add r" + to_string(temp - 2) + ".x, r" + to_string(temp - 1) + ".w, l(-" + conv + ")\n" +
					"mad r" + to_string(temp - 2) + ".x, r" + to_string(temp - 2) + ".x, " + changeSep + ", r" + to_string(temp - 1) + ".x\n" +
					"mov " + oReg + ".x, r" + to_string(temp - 2) + ".x\n";
			}
			if (oReg.size() == 0) {
				// no output
				return "";
			}
			if (temp == 0) {
				// add temps
				temp = 2;
				shader += "dcl_temps 2\n";
			}
			shader += s + "\n";
			auto pos = s.find(oReg);
			if (pos != string::npos) {
				string reg = "r" + to_string(temp - 1);
				for (int i = 0; i < s.size(); i++) {
					if (i < pos) {
						shader += s[i];
					}
					else if (i == pos) {
						shader += reg;
					}
					else if (i > pos + 1) {
						shader += s[i];
					}
				}
				shader += "\n";
			}
		}
		else {
			// before dcl
			shader += s + "\n";
		}
	}
	return shader;
}

HRESULT STDMETHODCALLTYPE D3D12_CreateGraphicsPipelineState(ID3D12Device* This, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) {
	LogInfo("CreateGraphicsPipelineState\n");
	HRESULT hr;
	UINT64 crc = dumpShader("vs", pDesc->VS.pShaderBytecode, pDesc->VS.BytecodeLength);
	dumpShader("ps", pDesc->PS.pShaderBytecode, pDesc->PS.BytecodeLength);
	dumpShader("ds", pDesc->DS.pShaderBytecode, pDesc->DS.BytecodeLength);
	dumpShader("gs", pDesc->GS.pShaderBytecode, pDesc->GS.BytecodeLength);
	dumpShader("hs", pDesc->HS.pShaderBytecode, pDesc->HS.BytecodeLength);
	// Removed due to Hellblade, ok solution for now
	if (crc == 0xF66E2C466DD0C233)
		return sCreateGraphicsPipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);

	vector<byte> v;
	byte* bArray = (byte*)pDesc->VS.pShaderBytecode;
	for (int i = 0; i < pDesc->VS.BytecodeLength; i++) {
		v.push_back(bArray[i]);
	}
	vector<byte> ASM = disassembler(v);
	if (ASM.size() == 0) {
		return sCreateGraphicsPipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
	}
	if (ASM[0] == ';') {
		dumpShaderRAW("vsDXIL", ASM.data(), ASM.size(), crc);
		HRESULT hr = sCreateGraphicsPipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
		PSO pso = {};
		pso.crc = crc;
		pso.dxil = true;
		pso.Neutral = (ID3D12PipelineState*)*ppPipelineState;
		PSOmap[pso.Neutral] = pso;
		return hr;
	}
	else {
		dumpShaderRAW("vsNormal", ASM.data(), ASM.size(), crc);
	}

	string shaderL = changeASM(ASM, true);
	string shaderR = changeASM(ASM, false);

	if (shaderL == "") {
		dumpShaderRAW("vsNoOutput", ASM.data(), ASM.size(), crc);
		HRESULT hr = sCreateGraphicsPipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
		PSO pso = {};
		pso.crc = crc;
		pso.Neutral = (ID3D12PipelineState*)*ppPipelineState;
		PSOmap[pso.Neutral] = pso;
		return hr;
	}

	vector<byte> a;
	PSO pso = {};
	pso.crc = crc;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC* modDesc = (D3D12_GRAPHICS_PIPELINE_STATE_DESC*)pDesc;
	
	a.clear();
	for (int i = 0; i < shaderL.length(); i++) {
		a.push_back(shaderL[i]);
	}
	dumpShaderRAW("vsLeft", a.data(), a.size(), crc);
	auto compiled = assembler(a, v);
	modDesc->VS.pShaderBytecode = compiled.data();
	modDesc->VS.BytecodeLength = compiled.size(); 
	hr = sCreateGraphicsPipelineState_Hook.fn(This, modDesc, riid, ppPipelineState);
	pso.Left = (ID3D12PipelineState*)*ppPipelineState;

	a.clear();
	for (int i = 0; i < shaderR.length(); i++) {
		a.push_back(shaderR[i]);
	}
	dumpShaderRAW("vsRight", a.data(), a.size(), crc);
	compiled = assembler(a, v);
	modDesc->VS.pShaderBytecode = compiled.data();
	modDesc->VS.BytecodeLength = compiled.size();
	hr = sCreateGraphicsPipelineState_Hook.fn(This, modDesc, riid, ppPipelineState);
	pso.Right = (ID3D12PipelineState*)*ppPipelineState;
	PSOmap[pso.Right] = pso;
	return hr;
}

HRESULT STDMETHODCALLTYPE D3D12_CreateComputePipelineState(ID3D12Device* This, const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, const IID& riid, void** ppPipelineState) {
	LogInfo("CreateComputePipelineState\n");
	UINT64 crc = dumpShader("cs", pDesc->CS.pShaderBytecode, pDesc->CS.BytecodeLength);
	HRESULT hr = sCreateComputePipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
	PSO pso = {};
	pso.crc = crc;
	pso.cs = true;
	pso.Neutral = (ID3D12PipelineState*)*ppPipelineState;
	PSOmap[pso.Neutral] = pso;
	return hr;
}

HRESULT STDMETHODCALLTYPE D3D12_CreatePipelineState(ID3D12Device2* This, const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc, REFIID riid, void** ppPipelineState) {
	LogInfo("CreatePipelineState\n");
	HRESULT hr;
	DWORD64* stream64 = (DWORD64*)pDesc->pPipelineStateSubobjectStream;
	UINT64 crc = 0;
	void* vsPtr = nullptr;
	DWORD64 vsSize = 0;
	int vsOffset = 0;
	
	if ((stream64[3] & 0xF) == 6) {
		UINT64 crc = dumpShader("cs", (void*)stream64[4], stream64[5]);
		HRESULT hr = sCreatePipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
		PSO pso = {};
		pso.crc = crc;
		pso.cs = true;
		pso.Neutral = (ID3D12PipelineState*)*ppPipelineState;
		PSOmap[pso.Neutral] = pso;
		return hr;
	}
	else if ((stream64[0] & 0xF) == 1 &&
		(stream64[3] & 0xF) == 2 &&
		(stream64[6] & 0xF) == 3 &&
		(stream64[9] & 0xF) == 4 &&
		(stream64[12] & 0xF) == 5) {
		vsOffset = 0;
		vsPtr = (void*)stream64[vsOffset + 1];
		vsSize = stream64[vsOffset + 2];
		crc = dumpShader("vs", vsPtr, vsSize);
		dumpShader("ps", (void*)stream64[4], stream64[5]);
		dumpShader("ds", (void*)stream64[7], stream64[8]);
		dumpShader("gs", (void*)stream64[10], stream64[11]);
		dumpShader("hs", (void*)stream64[13], stream64[14]);
	}
	else if ((stream64[8] & 0xF) == 1 && 
			(stream64[25] & 0xF) == 2 &&
			(stream64[22] & 0xF) == 3 &&
			(stream64[19] & 0xF) == 4 &&
			(stream64[11] & 0xF) == 5) {
		vsOffset = 8;
		vsPtr = (void*)stream64[vsOffset + 1];
		vsSize = stream64[vsOffset + 2];
		crc = dumpShader("vs", vsPtr, vsSize);
		dumpShader("ps", (void*)stream64[26], stream64[27]);
		dumpShader("ds", (void*)stream64[23], stream64[24]);
		dumpShader("gs", (void*)stream64[20], stream64[21]);
		dumpShader("hs", (void*)stream64[12], stream64[13]);
	}
	else if ((stream64[8] & 0xF) == 1 &&
			(stream64[20] & 0xF) == 2 &&
			(stream64[17] & 0xF) == 3 &&
			(stream64[14] & 0xF) == 4 &&
			(stream64[11] & 0xF) == 5) {
		vsOffset = 8;
		vsPtr = (void*)stream64[vsOffset + 1];
		vsSize = stream64[vsOffset + 2];
		crc = dumpShader("vs", vsPtr, vsSize);
		dumpShader("ps", (void*)stream64[21], stream64[22]);
		dumpShader("ds", (void*)stream64[18], stream64[19]);
		dumpShader("gs", (void*)stream64[15], stream64[16]);
		dumpShader("hs", (void*)stream64[12], stream64[13]);
	}
	else if ((stream64[2] & 0xF) == 1 &&
			(stream64[5] & 0xF) == 2 &&
			(stream64[8] & 0xF) == 3 &&
			(stream64[11] & 0xF) == 4 &&
			(stream64[14] & 0xF) == 5) {
		vsOffset = 2;
		vsPtr = (void*)stream64[vsOffset + 1];
		vsSize = stream64[vsOffset + 2];
		crc = dumpShader("vs", vsPtr, vsSize);
		dumpShader("ps", (void*)stream64[6], stream64[7]);
		dumpShader("ds", (void*)stream64[9], stream64[10]);
		dumpShader("gs", (void*)stream64[12], stream64[13]);
		dumpShader("hs", (void*)stream64[15], stream64[16]);
	}
	else if ((stream64[9] & 0xF) == 1 &&
			(stream64[26] & 0xF) == 2 &&
			(stream64[23] & 0xF) == 3 &&
			(stream64[20] & 0xF) == 4 &&
			(stream64[12] & 0xF) == 5 &&
			(stream64[29] & 0xF) == 6) {
		vsOffset = 9;
		vsPtr = (void*)stream64[vsOffset + 1];
		vsSize = stream64[vsOffset + 2];
		crc = dumpShader("vs", vsPtr, vsSize);
		dumpShader("ps", (void*)stream64[27], stream64[28]);
		dumpShader("ds", (void*)stream64[24], stream64[25]);
		dumpShader("gs", (void*)stream64[21], stream64[22]);
		dumpShader("hs", (void*)stream64[13], stream64[14]);
		dumpShader("cs", (void*)stream64[30], stream64[31]);
	}
	else if ((stream64[5] & 0xF) == 1 &&
			(stream64[8] & 0xF) == 2 &&
			(stream64[11] & 0xF) == 3 &&
			(stream64[14] & 0xF) == 4 &&
			(stream64[17] & 0xF) == 5) {
		vsOffset = 5;
		vsPtr = (void*)stream64[vsOffset + 1];
		vsSize = stream64[vsOffset + 2];
		crc = dumpShader("vs", vsPtr, vsSize);
		dumpShader("ps", (void*)stream64[9], stream64[10]);
		dumpShader("ds", (void*)stream64[12], stream64[13]);
		dumpShader("gs", (void*)stream64[15], stream64[16]);
		dumpShader("hs", (void*)stream64[18], stream64[19]);
	}
	else {
		dumpShader("CPS", pDesc->pPipelineStateSubobjectStream, pDesc->SizeInBytes);
		return sCreatePipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
	}

	vector<byte> v;
	byte* bArray = (byte*)vsPtr;
	for (int i = 0; i < vsSize; i++) {
		v.push_back(bArray[i]);
	}
	vector<byte> ASM = disassembler(v);
	if (ASM.size() == 0 || ASM[0] == ';') {
		dumpShaderRAW("vsDXIL", ASM.data(), ASM.size(), crc);
		HRESULT hr = sCreatePipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
		PSO pso = {};
		pso.crc = crc;
		pso.dxil = true;
		pso.Neutral = (ID3D12PipelineState*)*ppPipelineState;
		PSOmap[pso.Neutral] = pso;
		return hr;
	}
	else {
		dumpShaderRAW("vsNormal", ASM.data(), ASM.size(), crc);
	}

	string shaderL = changeASM(ASM, true);
	string shaderR = changeASM(ASM, false);

	if (shaderL == "") {
		dumpShaderRAW("vsNoOutput", ASM.data(), ASM.size(), crc);
		hr = sCreatePipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
		PSO pso = {};
		pso.crc = crc;
		pso.Neutral = (ID3D12PipelineState*)*ppPipelineState;
		PSOmap[pso.Neutral] = pso;
		return hr;
	}

	vector<byte> a;
	PSO pso = {};
	pso.crc = crc;

	a.clear();
	for (int i = 0; i < shaderL.length(); i++) {
		a.push_back(shaderL[i]);
	}
	dumpShaderRAW("vsLeft", a.data(), a.size(), crc);
	auto compiled = assembler(a, v);
	stream64[vsOffset + 1] = (DWORD64)compiled.data();
	stream64[vsOffset + 2] = compiled.size();
	hr = sCreatePipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
	pso.Left = (ID3D12PipelineState*)*ppPipelineState;
	
	a.clear();
	for (int i = 0; i < shaderR.length(); i++) {
		a.push_back(shaderR[i]);
	}
	dumpShaderRAW("vsRight", a.data(), a.size(), crc);
	compiled = assembler(a, v);
	stream64[vsOffset + 1] = (DWORD64)compiled.data();
	stream64[vsOffset + 2] = compiled.size();
	hr = sCreatePipelineState_Hook.fn(This, pDesc, riid, ppPipelineState);
	pso.Right = (ID3D12PipelineState*)*ppPipelineState;
	PSOmap[pso.Right] = pso;
	return hr;
}

void D3D12CL_SetPipelineState(ID3D12GraphicsCommandList* This, ID3D12PipelineState* pPipelineState) {
	if (PSOmap.count(pPipelineState) == 1) {
		PSO* pso = &PSOmap[pPipelineState];
		if (!pso->Left) {
			LogInfo("SetPipeineStage: %016llX cs:%d dxil:%d\n", pso->crc, pso->cs, pso->dxil);
			sSetPipelineState_Hook.fn(This, pso->Neutral);
		}
		else {
			LogInfo("SetPipeineStageSwapVS: %016llX\n", pso->crc);
			if (gl_left) {
				sSetPipelineState_Hook.fn(This, pso->Left);
			}
			else {
				sSetPipelineState_Hook.fn(This, pso->Right);
			}
		}
	}
	else {
		LogInfo("SetPipeineStage: unknown\n");
		sSetPipelineState_Hook.fn(This, pPipelineState);
	}
}
#pragma endregion

#pragma region Command
void D3D12CQ_ExecuteCommandLists(ID3D12CommandQueue* This, UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) {
	LogInfo("ExecuteCommandLists: %d\n", NumCommandLists);
	sExecuteCommandLists_Hook.fn(This, NumCommandLists, ppCommandLists);
}

HRESULT D3D12_CreateCommandQueue(ID3D12Device* This, const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID riid, void** ppCommandQueue) {
	HRESULT hr = sCreateCommandQueue_Hook.fn(This, pDesc, riid, ppCommandQueue);
	if (++gl_GraphicsCommandQueue_hooked == 1) {
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppCommandQueue;

		D3D12CQ_ECL origECL = (D3D12CQ_ECL)(*vTable)[10];
		cHookMgr.Hook(&(sExecuteCommandLists_Hook.nHookId), (LPVOID*)&(sExecuteCommandLists_Hook.fn), origECL, D3D12CQ_ExecuteCommandLists);
	}
	LogInfo("CreateCommandQueue: %d\n", pDesc->Type);
	return hr;
}

HRESULT STDMETHODCALLTYPE D3D12_CreateCommandList(ID3D12Device* This, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* pCommandAllocator, ID3D12PipelineState* pInitialState, REFIID riid, void** ppCommandList) {
	HRESULT hr = sCreateCommandList_Hook.fn(This, nodeMask, type, pCommandAllocator, pInitialState, riid, ppCommandList);
	if (++gl_GraphicsCommandList_hooked == 1) {
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppCommandList;

		D3D12CL_SPS origSPS = (D3D12CL_SPS)(*vTable)[25];
		cHookMgr.Hook(&(sSetPipelineState_Hook.nHookId), (LPVOID*)&(sSetPipelineState_Hook.fn), origSPS, D3D12CL_SetPipelineState);
	}
	LogInfo("CreateCommandList: %d\n", type);
	return hr;
}

HRESULT STDMETHODCALLTYPE D3D12_CreateCommandList1(ID3D12Device4* This, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, D3D12_COMMAND_LIST_FLAGS flags, REFIID riid, void** ppCommandList) {
	HRESULT hr = sCreateCommandList1_Hook.fn(This, nodeMask, type, flags, riid, ppCommandList);
	if (++gl_GraphicsCommandList_hooked == 1) {
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppCommandList;

		D3D12CL_SPS origSPS = (D3D12CL_SPS)(*vTable)[25];
		cHookMgr.Hook(&(sSetPipelineState_Hook.nHookId), (LPVOID*)&(sSetPipelineState_Hook.fn), origSPS, D3D12CL_SetPipelineState);
	}
	LogInfo("CreateCommandList1: %d\n", type);
	return hr;
}
#pragma endregion

#pragma region DXGI
void beforePresent() {
	gl_left = !gl_left;
}

HRESULT STDMETHODCALLTYPE DXGIH_Present(IDXGISwapChain* This, UINT SyncInterval, UINT Flags) {
	beforePresent();
	LogInfo("Present\n");
	return sDXGI_Present_Hook.fn(This, SyncInterval, Flags);
}

HRESULT STDMETHODCALLTYPE DXGIH_Present1(IDXGISwapChain1* This, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters) {
	beforePresent();
	LogInfo("Present1\n");
	return sDXGI_Present1_Hook.fn(This, SyncInterval, Flags, pPresentParameters);
}

HRESULT STDMETHODCALLTYPE DXGI_CreateSwapChain(IDXGIFactory* This, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain) {
	LogInfo("CreateSwapChain\n");
	HRESULT hr = sCreateSwapChain_Hook.fn(This, pDevice, pDesc, ppSwapChain);
	if (++gl_SwapChain_hooked == 1) {
		LogInfo("SwapChain hooked\n");
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppSwapChain;

		DXGI_Present origPresent = (DXGI_Present)(*vTable)[8];
		cHookMgr.Hook(&(sDXGI_Present_Hook.nHookId), (LPVOID*)&(sDXGI_Present_Hook.fn), origPresent, DXGIH_Present);
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGI_CreateSwapChainForHWND(IDXGIFactory2* This, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain) {
	LogInfo("CreateSwapChainForHWND\n");
	HRESULT hr = sCreateSwapChainForHWND_Hook.fn(This, pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
	if (++gl_SwapChain1_hooked == 1) {
		LogInfo("SwapChain1 hooked\n");
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppSwapChain;

		DXGI_Present origPresent = (DXGI_Present)(*vTable)[8];
		cHookMgr.Hook(&(sDXGI_Present_Hook.nHookId), (LPVOID*)&(sDXGI_Present_Hook.fn), origPresent, DXGIH_Present);
		DXGI_Present1 origPresent1 = (DXGI_Present1)(*vTable)[23];
		cHookMgr.Hook(&(sDXGI_Present1_Hook.nHookId), (LPVOID*)&(sDXGI_Present1_Hook.fn), origPresent1, DXGIH_Present1);
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGI_CreateSwapChainForCoreWindow(IDXGIFactory2* This, IUnknown* pDevice, IUnknown* pWindow, const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain) {
	LogInfo("CreateSwapChainForCoreWindow\n");
	HRESULT hr = sCreateSwapChainForCoreWindow_Hook.fn(This, pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
	if (++gl_SwapChain1_hooked == 1) {
		LogInfo("SwapChain1 hooked\n");
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppSwapChain;

		DXGI_Present origPresent = (DXGI_Present)(*vTable)[8];
		cHookMgr.Hook(&(sDXGI_Present_Hook.nHookId), (LPVOID*)&(sDXGI_Present_Hook.fn), origPresent, DXGIH_Present);
		DXGI_Present1 origPresent1 = (DXGI_Present1)(*vTable)[23];
		cHookMgr.Hook(&(sDXGI_Present1_Hook.nHookId), (LPVOID*)&(sDXGI_Present1_Hook.fn), origPresent1, DXGIH_Present1);
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE DXGI_CreateSwapChainForComposition(IDXGIFactory2* This, IUnknown* pDevice, const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain) {
	LogInfo("CreateSwapChainForComposition\n");
	HRESULT hr = sCreateSwapChainForComposition_Hook.fn(This, pDevice, pDesc, pRestrictToOutput, ppSwapChain);
	if (++gl_SwapChain1_hooked == 1) {
		LogInfo("SwapChain1 hooked\n");
		DWORD_PTR*** vTable = (DWORD_PTR***)*ppSwapChain;

		DXGI_Present origPresent = (DXGI_Present)(*vTable)[8];
		cHookMgr.Hook(&(sDXGI_Present_Hook.nHookId), (LPVOID*)&(sDXGI_Present_Hook.fn), origPresent, DXGIH_Present);
		DXGI_Present1 origPresent1 = (DXGI_Present1)(*vTable)[23];
		cHookMgr.Hook(&(sDXGI_Present1_Hook.nHookId), (LPVOID*)&(sDXGI_Present1_Hook.fn), origPresent1, DXGIH_Present1);
	}
	return hr;
}
#pragma endregion

void hookDevice(void** ppDevice) {
	if (++gl_Device_hooked == 1) {
		LogInfo("Hooking device\n");

		DWORD_PTR*** vTable = (DWORD_PTR***)*ppDevice;
		
		D3D12_CCQ origCCQ = (D3D12_CCQ)(*vTable)[8];
		cHookMgr.Hook(&(sCreateCommandQueue_Hook.nHookId), (LPVOID*)&(sCreateCommandQueue_Hook.fn), origCCQ, D3D12_CreateCommandQueue);
		D3D12_CGPS origCGPS = (D3D12_CGPS)(*vTable)[10];
		cHookMgr.Hook(&(sCreateGraphicsPipelineState_Hook.nHookId), (LPVOID*)&(sCreateGraphicsPipelineState_Hook.fn), origCGPS, D3D12_CreateGraphicsPipelineState);
		D3D12_CCPS origCCPS = (D3D12_CCPS)(*vTable)[11];
		cHookMgr.Hook(&(sCreateComputePipelineState_Hook.nHookId), (LPVOID*)&(sCreateComputePipelineState_Hook.fn), origCCPS, D3D12_CreateComputePipelineState);
		D3D12_CCL origCCL = (D3D12_CCL)(*vTable)[12];
		cHookMgr.Hook(&(sCreateCommandList_Hook.nHookId), (LPVOID*)&(sCreateCommandList_Hook.fn), origCCL, D3D12_CreateCommandList);
		D3D12_CPS origCPS = (D3D12_CPS)(*vTable)[47];
		cHookMgr.Hook(&(sCreatePipelineState_Hook.nHookId), (LPVOID*)&(sCreatePipelineState_Hook.fn), origCPS, D3D12_CreatePipelineState);
		D3D12_CCL1 origCCL1 = (D3D12_CCL1)(*vTable)[51];
		cHookMgr.Hook(&(sCreateCommandList1_Hook.nHookId), (LPVOID*)&(sCreateCommandList1_Hook.fn), origCCL1, D3D12_CreateCommandList1);

		IDXGIFactory2* pFactory2;
		HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory2));
		vTable = (DWORD_PTR***)pFactory2;
		DXGI_CSC origCSC = (DXGI_CSC)(*vTable)[10];
		cHookMgr.Hook(&(sCreateSwapChain_Hook.nHookId), (LPVOID*)&(sCreateSwapChain_Hook.fn), origCSC, DXGI_CreateSwapChain);
		DXGI_CSCFH origCSCFH = (DXGI_CSCFH)(*vTable)[15];
		cHookMgr.Hook(&(sCreateSwapChainForHWND_Hook.nHookId), (LPVOID*)&(sCreateSwapChainForHWND_Hook.fn), origCSCFH, DXGI_CreateSwapChainForHWND);
		DXGI_CSCFCW origCSCFCW = (DXGI_CSCFCW)(*vTable)[16];
		cHookMgr.Hook(&(sCreateSwapChainForCoreWindow_Hook.nHookId), (LPVOID*)&(sCreateSwapChainForCoreWindow_Hook.fn), origCSCFCW, DXGI_CreateSwapChainForCoreWindow);
		DXGI_CSCFC origCSCFC = (DXGI_CSCFC)(*vTable)[24];
		cHookMgr.Hook(&(sCreateSwapChainForComposition_Hook.nHookId), (LPVOID*)&(sCreateSwapChainForComposition_Hook.fn), origCSCFC, DXGI_CreateSwapChainForComposition);
		pFactory2->Release();
	}
}

#pragma region Exports
HRESULT _fastcall GetBehaviorValue(const char* a1, unsigned __int64* a2) {
	LogInfo("GetBehaviorValue\n");
	typedef HRESULT(_fastcall* D3D12_Type)(const char *a, unsigned __int64 *b);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "GetBehaviorValue");
	return fn(a1, a2);
}; // 100

HRESULT D3D12CreateDevice(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, const IID riid, void** ppDevice) {
	LogInfo("D3D12CreateDevice\n");
	typedef HRESULT(*D3D12_Type)(IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, const IID riid, void** ppDevice);
	D3D12_Type fn = (D3D12_Type)NktHookLibHelpers::GetProcedureAddress(gl_hOriginalDll, "D3D12CreateDevice");
	HRESULT res = fn(pAdapter, MinimumFeatureLevel, riid, ppDevice);
	if (ppDevice)
		hookDevice(ppDevice);
	return res;
} // 101

HRESULT WINAPI D3D12GetDebugInterface(const IID* const riid, void** ppvDebug) {
	LogInfo("D3D12GetDebugInterface\n");
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
	LogInfo("D3D12CoreCreateLayeredDevice\n");
	typedef HRESULT(WINAPI* D3D12_Type)(const void *u0, DWORD u1, const void *u2, REFIID riid, void **ppvObj);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreCreateLayeredDevice");
	return fn(u0, u1, u2, riid, ppvObj);
} // 104

HRESULT WINAPI D3D12CoreGetLayeredDeviceSize(const void *u0, DWORD u1) {
	LogInfo("D3D12CoreCreateLayeredDeviceSize\n");
	typedef HRESULT(WINAPI* D3D12_Type)(const void *u0, DWORD u1);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreGetLayeredDeviceSize");
	return fn(u0, u1);
} // 105

HRESULT WINAPI D3D12CoreRegisterLayers(const void *u0, DWORD u1) {
	LogInfo("D3D12CoreRegisterLayers\n");
	typedef HRESULT(WINAPI* D3D12_Type)(const void *u0, DWORD u1);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CoreRegisterLayers");
	return fn(u0, u1);
} // 106

HRESULT WINAPI D3D12CreateRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T  SrcDataSizeInBytes, REFIID  pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer) {
	LogInfo("D3D12CreateRootSignatureDeserializer\n");
	typedef HRESULT(WINAPI* D3D12_Type)(LPCVOID pSrcData, SIZE_T  SrcDataSizeInBytes, REFIID  pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CreateRootSignatureDeserializer");
	return fn(pSrcData, SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
} // 107

HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer) {
	LogInfo("D3D12CreateVersionedRootSignatureDeserializer\n");
	typedef HRESULT(WINAPI* D3D12_Type)(LPCVOID pSrcData, SIZE_T SrcDataSizeInBytes, REFIID pRootSignatureDeserializerInterface, void** ppRootSignatureDeserializer);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12CreateVersionedRootSignatureDeserializer");
	return fn(pSrcData,SrcDataSizeInBytes,pRootSignatureDeserializerInterface,ppRootSignatureDeserializer);
} // 108

HRESULT WINAPI D3D12EnableExperimentalFeatures(UINT NumFeatures, const IID* pIIDs, void* pConfigurationStructs,	UINT* pConfigurationStructSizes) {
	LogInfo("D3D12EnableExperimentalFeatures\n");
	typedef HRESULT(WINAPI* D3D12_Type)(UINT NumFeatures, const IID* pIIDs,	void* pConfigurationStructs, UINT* pConfigurationStructSizes);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12EnableExperimentalFeatures");
	return fn(NumFeatures,pIIDs,pConfigurationStructs,pConfigurationStructSizes);
} // 110

HRESULT WINAPI D3D12GetInterface(struct _GUID* a1, const struct _GUID* a2, void** a3) {
	LogInfo("D3D12GetInterface\n");
	typedef HRESULT(WINAPI* D3D12_Type)(struct _GUID* a1, const struct _GUID* a2, void** a3);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12GetInterface");
	return fn(a1, a2, a3);
} // 111

HRESULT WINAPI D3D12PIXEventsReplaceBlock(bool getEarliestTime) {
	LogInfo("D3D12PIXEventsReplaceBlock\n");
	typedef HRESULT(WINAPI* D3D12_Type)(bool getEarliestTime);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXEventsReplaceBlock");
	return fn(getEarliestTime);
} // 112

HRESULT WINAPI D3D12PIXGetThreadInfo() {
	LogInfo("D3D12PIXGetThreadInfo\n");
	typedef HRESULT(WINAPI* D3D12_Type)();
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXGetThreadInfo");
	return fn();
} // 113

HRESULT WINAPI D3D12PIXNotifyWakeFromFenceSignal(HANDLE a, HANDLE b) {
	LogInfo("D3D12PIXNotifyWakeFromFenceSignal\n");
	typedef HRESULT(WINAPI* D3D12_Type)(HANDLE a, HANDLE b);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXNotifyWakeFromFenceSignal");
	return fn(a, b);
} // 114

HRESULT WINAPI D3D12PIXReportCounter(HANDLE a, HANDLE b, HANDLE c) {
	LogInfo("D3D12PIXReportCounter\n");
	typedef HRESULT(WINAPI* D3D12_Type)(HANDLE a, HANDLE b, HANDLE c);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll,	"D3D12PIXReportCounter");
	return fn(a, b, c);
} // 115

HRESULT WINAPI D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC* pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob** ppBlob, ID3DBlob** ppErrorBlob) {
	LogInfo("D3D12SerializeRootSignature\n");
	typedef HRESULT(WINAPI* D3D12_Type)(const D3D12_ROOT_SIGNATURE_DESC* pRootSignature, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob** ppBlob, ID3DBlob** ppErrorBlob);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12SerializeRootSignature");
	return fn(pRootSignature,Version,ppBlob,ppErrorBlob);
} // 116

HRESULT WINAPI D3D12SerializeVersionedRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature, ID3DBlob** ppBlob, ID3DBlob** ppErrorBlob) {
	LogInfo("D3D12SerializeVersionedRootSignature\n");
	typedef HRESULT(WINAPI* D3D12_Type)(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature, ID3DBlob** ppBlob, ID3DBlob** ppErrorBlob);
	D3D12_Type fn = (D3D12_Type)GetProcAddress(gl_hOriginalDll, "D3D12SerializeVersionedRootSignature");
	return fn(pRootSignature, ppBlob, ppErrorBlob);
} // 117
#pragma endregion