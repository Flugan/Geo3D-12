// stdafx.h 
#pragma once

#include <direct.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <string>
#include "d3d11.h"
#include "dxgi1_6.h"

using namespace std;

struct VSO {
	ID3D11VertexShader* Left;
	ID3D11VertexShader* Right;
};

map<ID3D11VertexShader*, VSO> VSOmap;

vector<byte> disassembler(vector<byte> buffer);
vector<byte> assembler(vector<byte> asmFile, vector<byte> buffer);
vector<byte> readFile(string fileName);
vector<string> stringToLines(const char* start, size_t size);

void ExitInstance();
void LoadOriginalDll();