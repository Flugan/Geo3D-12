// proxydll.h
#pragma once
#include "stdafx.h"

typedef HRESULT(STDMETHODCALLTYPE* D3D10_VS)(ID3D10Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10VertexShader **ppVertexShader);
static struct {
	SIZE_T nHookId;
	D3D10_VS fn;
} sCreateVertexShader_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D10_PS)(ID3D10Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10PixelShader **ppPixelShader);
static struct {
	SIZE_T nHookId;
	D3D10_PS fn;
} sCreatePixelShader_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D10_GS)(ID3D10Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D10GeometryShader **ppGeometryShader);
static struct {
	SIZE_T nHookId;
	D3D10_GS fn;
} sCreateGeometryShader_Hook = { 0, NULL };

void ExitInstance();
void LoadOriginalDll();