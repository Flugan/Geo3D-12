// stdafx.h 
#pragma once


#define WIN32_LEAN_AND_MEAN		
#include <windows.h>
#include <share.h>
#include <stdio.h>
#include <direct.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "d3d12.h"
#include "dxgi1_6.h"

using namespace std;

struct PSO {
	UINT64 crc;
	ID3D12PipelineState* Left;
	ID3D12PipelineState* Right;
};

map<ID3D12PipelineState*, PSO> PSOmap;

vector<byte> disassembler(vector<byte> buffer);
vector<byte> assembler(vector<byte> asmFile, vector<byte> buffer);
vector<byte> readFile(string fileName);
vector<string> stringToLines(const char* start, size_t size);