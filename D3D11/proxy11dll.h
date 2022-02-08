// proxydll.h
#pragma once
#include "stdafx.h"

typedef HRESULT(STDMETHODCALLTYPE* D3D11_VS)(ID3D11Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11VertexShader **ppVertexShader);
static struct {
	SIZE_T nHookId;
	D3D11_VS fn;
} sCreateVertexShader_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D11_PS)(ID3D11Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11PixelShader **ppPixelShader);
static struct {
	SIZE_T nHookId;
	D3D11_PS fn;
} sCreatePixelShader_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D11_GS)(ID3D11Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11GeometryShader **ppGeometryShader);
static struct {
	SIZE_T nHookId;
	D3D11_GS fn;
} sCreateGeometryShader_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D11_HS)(ID3D11Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11HullShader **ppHullShader);
static struct {
	SIZE_T nHookId;
	D3D11_HS fn;
} sCreateHullShader_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D11_DS)(ID3D11Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11DomainShader **ppDomainShader);
static struct {
	SIZE_T nHookId;
	D3D11_DS fn;
} sCreateDomainShader_Hook = { 0, NULL };

typedef HRESULT(STDMETHODCALLTYPE* D3D11_CS)(ID3D11Device * This, const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage, ID3D11ComputeShader **ppComputeShader);
static struct {
	SIZE_T nHookId;
	D3D11_CS fn;
} sCreateComputeShader_Hook = { 0, NULL };

void ExitInstance();
void LoadOriginalDll();