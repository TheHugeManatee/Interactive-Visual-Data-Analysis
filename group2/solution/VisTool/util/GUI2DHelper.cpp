#include "GUI2DHelper.h"

#include "util.h"
#include "../Globals.h"

#include <sstream>
#include <iostream>
#include <exception>

ID3DX11Effect			* GUI2DHelper::pEffect = nullptr;
ID3DX11EffectTechnique	* GUI2DHelper::pTechnique = nullptr;
ID3DX11EffectPass		* GUI2DHelper::pPassRenderGUIOnTop = nullptr;
ID3D11Device			* GUI2DHelper::pd3dDevice = nullptr;

ID3DX11EffectVectorVariable		* GUI2DHelper::pMousePositionEV = nullptr;
ID3DX11EffectScalarVariable		* GUI2DHelper::pSideBySide3DEnabledEV = nullptr;
ID3DX11EffectScalarVariable		* GUI2DHelper::pViewportXEV = nullptr;

ID3D11Texture2D					* GUI2DHelper::pGUI2DTexture = nullptr;
ID3D11ShaderResourceView		* GUI2DHelper::pGUI2DTextureSRV = nullptr;
ID3D11RenderTargetView			* GUI2DHelper::pGUI2DTextureRTV = nullptr;

ID3DX11EffectShaderResourceVariable * GUI2DHelper::pGUI2DTextureEV = nullptr;

ID3D11RenderTargetView			* GUI2DHelper::pRTVBefore = nullptr;
ID3D11DepthStencilView			* GUI2DHelper::pDSVBefore = nullptr;
	
XMFLOAT2 GUI2DHelper::viewportSize;


HRESULT GUI2DHelper::Initialize(ID3D11Device * _pd3dDevice, TwBar * pParametersBar_)
{
	HRESULT hr;

	pd3dDevice = _pd3dDevice;


	std::wstring effectPath = GetExePath() + L"GUI2DHelper.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &pEffect)))
	{
		std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

	pTechnique = pEffect->GetTechniqueByName("TransparencyModule");
	SAFE_GET_TECHNIQUE(pEffect, "TransparencyModule", pTechnique);
	SAFE_GET_PASS(pTechnique, "RENDER_GUI_ON_TOP", pPassRenderGUIOnTop);

	SAFE_GET_VECTOR(pEffect, "g_mousePosition", pMousePositionEV);
	SAFE_GET_SCALAR(pEffect, "g_sideBySide3DEnabled", pSideBySide3DEnabledEV);
	SAFE_GET_SCALAR(pEffect, "g_viewportX", pViewportXEV);

	SAFE_GET_RESOURCE(pEffect, "g_tex2DGUI", pGUI2DTextureEV);

	return S_OK;
}

HRESULT GUI2DHelper::Release(void)
{

	SAFE_RELEASE(pEffect);
	SAFE_RELEASE(pGUI2DTexture);
	SAFE_RELEASE(pGUI2DTextureSRV);
	SAFE_RELEASE(pGUI2DTextureRTV);

	return S_OK;
}

HRESULT GUI2DHelper::Begin2DSection(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->OMGetRenderTargets(1, &pRTVBefore, &pDSVBefore);
	pd3dImmediateContext->OMSetRenderTargets(1, &pGUI2DTextureRTV, pDSVBefore);
	return S_OK;
}

HRESULT GUI2DHelper::End2DSection(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->OMSetRenderTargets(1, &pRTVBefore, pDSVBefore);
	SAFE_RELEASE(pRTVBefore);
	SAFE_RELEASE(pDSVBefore);
	return S_OK;
}

HRESULT GUI2DHelper::Draw2DGUI(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	pd3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	pGUI2DTextureEV->SetResource(pGUI2DTextureSRV);

	pPassRenderGUIOnTop->Apply(0, pd3dImmediateContext);

	pd3dImmediateContext->Draw(1, 0);


	pGUI2DTextureEV->SetResource(nullptr);
	pPassRenderGUIOnTop->Apply(0, pd3dImmediateContext);

	float clear[]  =  {0, 0, 0, 0};
	pd3dImmediateContext->ClearRenderTargetView(pGUI2DTextureRTV, clear);

	return S_OK;
}



HRESULT GUI2DHelper::OnResizeSwapChain(ID3D11Device * pd3dDevice, int width, int height)
{
	viewportSize = XMFLOAT2(width, height);

	HRESULT hr;

	// create the head buffer
	D3D11_TEXTURE2D_DESC tdesc;
	ZeroMemory(&tdesc, sizeof(tdesc));
	tdesc.Width = width;
	tdesc.Height = height;
	tdesc.ArraySize = 1;
	tdesc.MipLevels = 1;
	tdesc.SampleDesc.Count = 1;
	tdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	tdesc.Usage = D3D11_USAGE_DEFAULT;
	tdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	tdesc.CPUAccessFlags = 0;
	tdesc.MiscFlags = 0;
	tdesc.SampleDesc.Count = 1;
	V_RETURN(pd3dDevice->CreateTexture2D(&tdesc, nullptr, &pGUI2DTexture));
	
	D3D11_SHADER_RESOURCE_VIEW_DESC pDesc;
	ZeroMemory(&pDesc, sizeof(pDesc));
	pDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	pDesc.Texture2D.MipLevels = -1;
	pDesc.Texture2D.MostDetailedMip = 0;
	V_RETURN(pd3dDevice->CreateShaderResourceView(pGUI2DTexture, &pDesc, &pGUI2DTextureSRV));

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	ZeroMemory(&rtvDesc, sizeof(rtvDesc));
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	V_RETURN(pd3dDevice->CreateRenderTargetView(pGUI2DTexture, &rtvDesc, &pGUI2DTextureRTV));

	//initialize to defined value
	float clear[] = {0, 0, 0, 0};
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);
	pContext->ClearRenderTargetView(pGUI2DTextureRTV, clear);
	SAFE_RELEASE(pContext);

	pViewportXEV->SetFloat(width);

	return S_OK;
}

HRESULT GUI2DHelper::OnReleasingSwapChain(void)
{
	SAFE_RELEASE(pGUI2DTexture);
	SAFE_RELEASE(pGUI2DTextureRTV);
	SAFE_RELEASE(pGUI2DTextureSRV);

	return S_OK;
}