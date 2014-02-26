#include "TransparencyModule.h"

#include "util/util.h"
#include "Globals.h"

#include <sstream>
#include <iostream>
#include <exception>

ID3DX11Effect			* TransparencyModule::pEffect = nullptr;
ID3DX11EffectTechnique	* TransparencyModule::pTechnique = nullptr;
ID3DX11EffectPass		* TransparencyModule::pPassBlendAndRender = nullptr;
ID3DX11EffectPass		* TransparencyModule::pPassRenderFragmentCount = nullptr;
ID3DX11EffectPass		* TransparencyModule::pPassBlendDepth = nullptr;
ID3D11Device			* TransparencyModule::pd3dDevice = nullptr;

ID3DX11EffectVectorVariable		* TransparencyModule::pViewportResolutionEV = nullptr;

ID3D11Buffer					* TransparencyModule::pFragmentBuffer = nullptr;
ID3D11ShaderResourceView		* TransparencyModule::pFragmentBufferSRV = nullptr;
ID3D11UnorderedAccessView		* TransparencyModule::pFragmentBufferUAV = nullptr;

ID3D11Texture2D					* TransparencyModule::pListHeadBuffer = nullptr;
ID3D11ShaderResourceView		* TransparencyModule::pListHeadBufferSRV = nullptr;
ID3D11UnorderedAccessView		* TransparencyModule::pListHeadBufferUAV = nullptr;

ID3DX11EffectShaderResourceVariable * TransparencyModule::pListHeadEV = nullptr;
ID3DX11EffectUnorderedAccessViewVariable * TransparencyModule::pListHeadRWEV = nullptr;
ID3DX11EffectShaderResourceVariable * TransparencyModule::pFragmentBufferEV = nullptr;
ID3DX11EffectUnorderedAccessViewVariable * TransparencyModule::pFragmentBufferRWEV = nullptr;

int TransparencyModule::nodeBufferSize = 0;
int TransparencyModule::transparencyRenderType = 0;

HRESULT TransparencyModule::Initialize(ID3D11Device * _pd3dDevice, TwBar * pParametersBar_)
{
	HRESULT hr;

	pd3dDevice = _pd3dDevice;


	std::wstring effectPath = GetExePath() + L"TransparencyModule.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &pEffect)))
	{
		std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

	pTechnique = pEffect->GetTechniqueByName("TransparencyModule");
	SAFE_GET_TECHNIQUE(pEffect, "TransparencyModule", pTechnique);
	SAFE_GET_PASS(pTechnique, "RENDER_TRANSPARENCY_TO_SCREEN", pPassBlendAndRender);
	SAFE_GET_PASS(pTechnique, "RENDER_FRAGMENT_COUNT", pPassRenderFragmentCount);
	SAFE_GET_PASS(pTechnique, "RENDER_BLENDED_DEPTH", pPassBlendDepth);

	SAFE_GET_VECTOR(pEffect, "g_viewportResolution", pViewportResolutionEV);

	SAFE_GET_RESOURCE(pEffect, "g_listHead", pListHeadEV);
	SAFE_GET_UAV(pEffect, "g_listHeadRW", pListHeadRWEV);

	SAFE_GET_RESOURCE(pEffect, "g_fragmentBuffer", pFragmentBufferEV);
	SAFE_GET_UAV(pEffect, "g_fragmentBufferRW", pFragmentBufferRWEV);

	return S_OK;
}

HRESULT TransparencyModule::Release(void)
{

	SAFE_RELEASE(pEffect);
	SAFE_RELEASE(pFragmentBuffer);
	SAFE_RELEASE(pFragmentBufferSRV);
	SAFE_RELEASE(pFragmentBufferUAV);

	SAFE_RELEASE(pListHeadBuffer);
	SAFE_RELEASE(pListHeadBufferSRV);
	SAFE_RELEASE(pListHeadBufferUAV);
	return S_OK;
}

HRESULT TransparencyModule::DrawAccumulatedTransparency(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	pd3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	pListHeadEV->SetResource(pListHeadBufferSRV);
	pFragmentBufferEV->SetResource(pFragmentBufferSRV);

	ID3DX11EffectPass * pPass;

	switch (transparencyRenderType) {
	case 1:
		pPass = pPassRenderFragmentCount;
		break;
	case 2:
		pPass = pPassBlendDepth;
	break;
	default:
		pPass = pPassBlendAndRender;
		break;
	}

	pPass->Apply(0, pd3dImmediateContext);

	pd3dImmediateContext->Draw(1, 0);

	pListHeadEV->SetResource(nullptr);
	pFragmentBufferEV->SetResource(nullptr);
	pPass->Apply(0, pd3dImmediateContext);

	unsigned int values[] = {0, 0, 0, 0};
	pd3dImmediateContext->ClearUnorderedAccessViewUint(pListHeadBufferUAV, values);

	static const UINT pZeros[] = {0,0,0,0,0,0,0,0};
	static ID3D11UnorderedAccessView* const pNulls[] = {nullptr, nullptr, nullptr, nullptr};
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, &pFragmentBufferUAV, pZeros);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 1, pNulls, pZeros);


	return S_OK;
}



HRESULT TransparencyModule::OnResizeSwapChain(ID3D11Device * pd3dDevice, int width, int height)
{
	HRESULT hr;

	nodeBufferSize = width * height * 10;

	// create the node buffer
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.StructureByteStride = sizeof(struct LinkedListNode);
	desc.ByteWidth = nodeBufferSize * desc.StructureByteStride;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	V_RETURN(pd3dDevice->CreateBuffer(&desc, nullptr, &pFragmentBuffer));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D_SRV_DIMENSION_BUFFEREX;
	srvDesc.BufferEx.FirstElement = 0;
	srvDesc.BufferEx.NumElements = nodeBufferSize;
	V_RETURN(pd3dDevice->CreateShaderResourceView(pFragmentBuffer, &srvDesc, &pFragmentBufferSRV));


	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
	uavDesc.Buffer.NumElements = nodeBufferSize;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(pFragmentBuffer, &uavDesc, &pFragmentBufferUAV));


	// create the head buffer
	D3D11_TEXTURE2D_DESC tdesc;
	ZeroMemory(&tdesc, sizeof(tdesc));
	tdesc.Width = width;
	tdesc.Height = height;
	tdesc.ArraySize = 1;
	tdesc.MipLevels = 1;
	tdesc.SampleDesc.Count = 1;
	tdesc.Format = DXGI_FORMAT_R32_UINT;
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	tdesc.CPUAccessFlags = 0;
	tdesc.MiscFlags = 0;
	tdesc.SampleDesc.Count = 1;
	V_RETURN(pd3dDevice->CreateTexture2D(&tdesc, nullptr, &pListHeadBuffer));
	
	D3D11_SHADER_RESOURCE_VIEW_DESC pDesc;
	ZeroMemory(&pDesc, sizeof(pDesc));
	pDesc.Format = DXGI_FORMAT_R32_UINT;
	pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	pDesc.Texture2D.MipLevels = -1;
	pDesc.Texture2D.MostDetailedMip = 0;
	V_RETURN(pd3dDevice->CreateShaderResourceView(pListHeadBuffer, &pDesc, &pListHeadBufferSRV));

	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_R32_UINT;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(pListHeadBuffer, &uavDesc, &pListHeadBufferUAV));

	//initialize to defined value
	unsigned int values[] = {-1, -1, -1, -1};
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);
	pContext->ClearUnorderedAccessViewUint(pFragmentBufferUAV, values);
	SAFE_RELEASE(pContext);

	return S_OK;
}

HRESULT TransparencyModule::OnReleasingSwapChain(void)
{
		//first, release tentative resources
	SAFE_RELEASE(pFragmentBuffer);
	SAFE_RELEASE(pFragmentBufferSRV);
	SAFE_RELEASE(pFragmentBufferUAV);

	SAFE_RELEASE(pListHeadBuffer);
	SAFE_RELEASE(pListHeadBufferSRV);
	SAFE_RELEASE(pListHeadBufferUAV);

	return S_OK;
}