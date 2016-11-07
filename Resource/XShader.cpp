
#include "d3dCompiler.h"
#include "XShader.h"

#include "..\d3dx12.h"
#include "..\DXSampleHelper.h"

#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)

extern XEngine *g_pEngine;

extern UINT	g_uRenderTargetCount[ESHADINGPATH_COUNT];
extern DXGI_FORMAT g_RenderTargetFortmat[ESHADINGPATH_COUNT][RENDERTARGET_MAXNUM];

//
std::map<std::wstring, XGraphicShader*> XGraphicShaderManager::m_mResource;
XGraphicShader* XGraphicShaderManager::CreateGraphicShaderFromFile(LPCWSTR pFileName, LPCSTR pVSEntryPoint, LPCSTR pVSTarget, LPCSTR pPSEntryPoint, LPCSTR pPSTarget, D3D12_INPUT_ELEMENT_DESC *pInputElementDescs, UINT uInputElementCount, ESHADINGPATH eShadingPath)
{
	return CreateGraphicShaderFromFile(pFileName, pVSEntryPoint, pVSTarget, pPSEntryPoint, pPSTarget, pInputElementDescs, uInputElementCount, g_uRenderTargetCount[eShadingPath], g_RenderTargetFortmat[eShadingPath]);
}

XGraphicShader* XGraphicShaderManager::CreatePostProcessGraphicShaderFromFile(LPCWSTR pFileName, LPCSTR pVSEntryPoint, LPCSTR pVSTarget, LPCSTR pPSEntryPoint, LPCSTR pPSTarget, D3D12_INPUT_ELEMENT_DESC *pInputElementDescs, UINT uInputElementCount)
{
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = FALSE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	//
	return CreateGraphicShaderFromFile(pFileName, depthStencilDesc, pVSEntryPoint, pVSTarget, pPSEntryPoint, pPSTarget, pInputElementDescs, uInputElementCount, ESHADINGPATH_POSTPROCESS);
}

//
XGraphicShader* XGraphicShaderManager::CreateGraphicShaderFromFile(LPCWSTR pFileName, LPCSTR pVSEntryPoint, LPCSTR pVSTarget, LPCSTR pPSEntryPoint, LPCSTR pPSTarget, D3D12_INPUT_ELEMENT_DESC *pInputElementDescs, UINT uInputElementCount, UINT uRenderTargetCount, DXGI_FORMAT RenderTargetFormat[])
{
	//
	XGraphicShader *pGraphicShader = GetResource(pFileName);
	if (pGraphicShader)
	{
		return pGraphicShader;
	}

	//
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

#ifdef _DEBUG
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = 0;//D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencilDesc.StencilEnable = FALSE;

	CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
	rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsoDesc = {};
	gpsoDesc.InputLayout = { pInputElementDescs, uInputElementCount };
	gpsoDesc.pRootSignature = g_pEngine->m_pGraphicRootSignature.Get();
	gpsoDesc.RasterizerState = rasterizerStateDesc;
	gpsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpsoDesc.DepthStencilState = depthStencilDesc;
	gpsoDesc.SampleMask = UINT_MAX;
	gpsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsoDesc.NumRenderTargets = uRenderTargetCount;
	for (unsigned int i = 0;i < gpsoDesc.NumRenderTargets;++i)
	{
		gpsoDesc.RTVFormats[i] = RenderTargetFormat[i];
	}
	gpsoDesc.SampleDesc.Count = 1;
	gpsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//
	{
		ComPtr<ID3DBlob> pError;
		HRESULT hr = D3DCompileFromFile(pFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, pVSEntryPoint, pVSTarget, compileFlags, 0, &vertexShader, &pError);
		if (hr != S_OK)
		{
			OutputDebugStringA((char*)(pError->GetBufferPointer()));
		}
		ThrowIfFailed(hr);
	}
	{
		ComPtr<ID3DBlob> pError;
		HRESULT hr = D3DCompileFromFile(pFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, pPSEntryPoint, pPSTarget, compileFlags, 0, &pixelShader, &pError);
		if (hr != S_OK)
		{
			OutputDebugStringA((char*)(pError->GetBufferPointer()));
		}
		ThrowIfFailed(hr);
	}
	
	gpsoDesc.VS = { reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
	gpsoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };

	pGraphicShader = new XGraphicShader(pFileName);
	AddResource(pFileName, pGraphicShader);
	ThrowIfFailed(g_pEngine->m_pDevice->CreateGraphicsPipelineState(&gpsoDesc, IID_PPV_ARGS(&pGraphicShader->m_pPipelineState)));
	
	return pGraphicShader;
}

//
XGraphicShader* XGraphicShaderManager::CreateGraphicShaderFromFile(LPCWSTR pFileName, D3D12_DEPTH_STENCIL_DESC &depthstencildesc, LPCSTR pVSEntryPoint, LPCSTR pVSTarget, LPCSTR pPSEntryPoint, LPCSTR pPSTarget, D3D12_INPUT_ELEMENT_DESC *pInputElementDescs, UINT uInputElementCount, ESHADINGPATH eShadingPath)
{
	//
	XGraphicShader *pGraphicShader = GetResource(pFileName);
	if (pGraphicShader)
	{
		return pGraphicShader;
	}

	//
	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

#ifdef _DEBUG
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = 0;//D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	CD3DX12_RASTERIZER_DESC rasterizerStateDesc(D3D12_DEFAULT);
	rasterizerStateDesc.CullMode = D3D12_CULL_MODE_NONE;

	// Describe and create the graphics pipeline state object (PSO).
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsoDesc = {};
	gpsoDesc.InputLayout = { pInputElementDescs, uInputElementCount };
	gpsoDesc.pRootSignature = g_pEngine->m_pGraphicRootSignature.Get();
	gpsoDesc.RasterizerState = rasterizerStateDesc;
	gpsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	gpsoDesc.DepthStencilState = depthstencildesc;
	gpsoDesc.SampleMask = UINT_MAX;
	gpsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	gpsoDesc.NumRenderTargets = g_uRenderTargetCount[eShadingPath];
	for (unsigned int i = 0;i < gpsoDesc.NumRenderTargets;++i)
	{
		gpsoDesc.RTVFormats[i] = g_RenderTargetFortmat[eShadingPath][i];
	}
	gpsoDesc.SampleDesc.Count = 1;
	gpsoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	//
	{
		ComPtr<ID3DBlob> pError;
		HRESULT hr = D3DCompileFromFile(pFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, pVSEntryPoint, pVSTarget, compileFlags, 0, &vertexShader, &pError);
		if (hr != S_OK)
		{
			OutputDebugStringA((char*)(pError->GetBufferPointer()));
		}
		ThrowIfFailed(hr);
	}
	{
		ComPtr<ID3DBlob> pError;
		HRESULT hr = D3DCompileFromFile(pFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, pPSEntryPoint, pPSTarget, compileFlags, 0, &pixelShader, &pError);
		if (hr != S_OK)
		{
			OutputDebugStringA((char*)(pError->GetBufferPointer()));
		}
		ThrowIfFailed(hr);
	}

	gpsoDesc.VS = { reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
	gpsoDesc.PS = { reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };

	pGraphicShader = new XGraphicShader(pFileName);
	AddResource(pFileName, pGraphicShader);
	ThrowIfFailed(g_pEngine->m_pDevice->CreateGraphicsPipelineState(&gpsoDesc, IID_PPV_ARGS(&pGraphicShader->m_pPipelineState)));

	return pGraphicShader;
}

std::map<std::wstring, XComputeShader*> XComputeShaderManager::m_mResource;
XComputeShader* XComputeShaderManager::CreateComputeShaderFromFile(LPCWSTR pFileName, LPCSTR pCSEntryPoint, LPCSTR pCSTarget)
{
	XComputeShader *pComputeShader = GetResource(pFileName);
	if (pComputeShader)
	{
		return pComputeShader;
	}

	//
	ComPtr<ID3DBlob> computeShader;

#ifdef _DEBUG
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	//
	{
		ComPtr<ID3DBlob> pError;
		HRESULT hr = D3DCompileFromFile(pFileName, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, pCSEntryPoint, pCSTarget, compileFlags, 0, &computeShader, &pError);
		if (hr != S_OK)
		{
			OutputDebugStringA((char*)(pError->GetBufferPointer()));
		}
		ThrowIfFailed(hr);
	}

	// Describe and create the compute pipeline state object (PSO).
	D3D12_COMPUTE_PIPELINE_STATE_DESC cpsoDesc = {};
	cpsoDesc.pRootSignature = g_pEngine->m_pComputeRootSignature.Get();
	cpsoDesc.CS = { reinterpret_cast<UINT8*>(computeShader->GetBufferPointer()), computeShader->GetBufferSize() };
	cpsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	pComputeShader = new XComputeShader(pFileName);
	AddResource(pFileName, pComputeShader);
	ThrowIfFailed(g_pEngine->m_pDevice->CreateComputePipelineState(&cpsoDesc, IID_PPV_ARGS(&pComputeShader->m_pPipelineState)));

	return pComputeShader;
}