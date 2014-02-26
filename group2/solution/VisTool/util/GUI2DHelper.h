#pragma once

#include "util.h"

#include <DirectXMath.h>
#include <d3dx11effect.h>
#include <DXUT.h>
#include <DXUTcamera.h>
using namespace DirectX;

#include <AntTweakBar.h>

#include <string>
#include <vector>


class GUI2DHelper
{
public:
	struct LinkedListNode {
		XMFLOAT4 color;
		float depth;
		unsigned int nextNode;
	};

	//statics
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);
	static HRESULT OnResizeSwapChain(ID3D11Device * pd3dDevice, int width, int height);
	static HRESULT OnReleasingSwapChain(void);

	static HRESULT Begin2DSection(ID3D11DeviceContext* pd3dImmediateContext);
	static HRESULT End2DSection(ID3D11DeviceContext* pd3dImmediateContext);
	static HRESULT Draw2DGUI(ID3D11DeviceContext* pd3dImmediateContext);
	
	// GPU Resources
	static ID3D11Texture2D				* pGUI2DTexture;
	static ID3D11ShaderResourceView		* pGUI2DTextureSRV;
	static ID3D11RenderTargetView		* pGUI2DTextureRTV;




	//accessors
	static void SetMousePosition(XMFLOAT2 mousePos) {		
		pMousePositionEV->SetFloatVector(&mousePos.x);	
	};
	static void SetSideBySide3DEnabled(bool enabled) {		pSideBySide3DEnabledEV->SetBool(enabled);		};


protected:
	//statics
	static ID3DX11Effect * pEffect;
	static ID3DX11EffectTechnique * pTechnique;
	static ID3DX11EffectPass * pPassRenderGUIOnTop;
	static ID3D11Device * pd3dDevice;

	static ID3DX11EffectVectorVariable * pMousePositionEV;
	static ID3DX11EffectScalarVariable * pSideBySide3DEnabledEV;
	static ID3DX11EffectScalarVariable * pViewportXEV;

	static ID3DX11EffectShaderResourceVariable * pGUI2DTextureEV;

	static ID3D11RenderTargetView	* pRTVBefore;
	static ID3D11DepthStencilView	* pDSVBefore;
	
	static XMFLOAT2 viewportSize;

	//methods
	static HRESULT SetupGPUBuffers();


};