/*
Order-independent transparency via GPU-side linked lists

Steps to integrate:
SHADER:
* include "TransparencyModuleInterface.hlsli" in the effect file
* change the pixel shader:
	- [earlydepthstencil] attribute
	- return void
	- use AddTransparentFragment(pos, col) to write the color value (pos unmodified from PS input)
* disable depth write (DepthNoWrite state)

CLASS:
* add static TransparencyShaderEnvironment transparencyEnvironment; to the class
* call transparencyEnvironment.LinkEffect after the effect has been loaded
* call transparencyEnvironment.BeginTransparency() before the transparency draw call
* call transparencyEnvironment.EndTransparency(...) with context and the effect pass after the draw call

WARNING: Effects having the modified pixel shader (returning void) will remove the active render target from the pipeline
	when effect->Apply(...) is called. Make sure to only call this inside the transparencyEnvironment.BeginTransparency() and
	transparencyEnvironment.EndTransparency() section
*/

#pragma once

#include "util/util.h"

#include <DirectXMath.h>
#include <d3dx11effect.h>
#include <DXUT.h>
#include <DXUTcamera.h>
using namespace DirectX;

#include <AntTweakBar.h>

#include <string>
#include <vector>


class TransparencyModule
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

	static HRESULT DrawAccumulatedTransparency(ID3D11DeviceContext* pd3dImmediateContext);
	
	// GPU Resources
	static ID3D11Buffer					* pFragmentBuffer;
	static ID3D11ShaderResourceView		* pFragmentBufferSRV;
	static ID3D11UnorderedAccessView	* pFragmentBufferUAV;

	static ID3D11Texture2D				* pListHeadBuffer;
	static ID3D11ShaderResourceView		* pListHeadBufferSRV;
	static ID3D11UnorderedAccessView	* pListHeadBufferUAV;

	//methods
	//accessors
	static int nodeBufferSize;

	static int transparencyRenderType;

protected:
	//statics
	static ID3DX11Effect * pEffect;
	static ID3DX11EffectTechnique * pTechnique;
	static ID3DX11EffectPass * pPassBlendAndRender;
	static ID3DX11EffectPass * pPassRenderFragmentCount;
	static ID3DX11EffectPass * pPassBlendDepth;
	static ID3D11Device * pd3dDevice;

	static ID3DX11EffectVectorVariable * pViewportResolutionEV;

	static ID3DX11EffectShaderResourceVariable * pListHeadEV;
	static ID3DX11EffectUnorderedAccessViewVariable * pListHeadRWEV;
	static ID3DX11EffectShaderResourceVariable * pFragmentBufferEV;
	static ID3DX11EffectUnorderedAccessViewVariable * pFragmentBufferRWEV;

	//methods
	static HRESULT SetupGPUBuffers();


};

struct TransparencyModuleShaderEnvironment {
	ID3D11RenderTargetView * pRTV;
	ID3D11DepthStencilView * pDSV;

	ID3DX11EffectUnorderedAccessViewVariable * pListHeadRWEV;
	ID3DX11EffectUnorderedAccessViewVariable * pFragmentBufferRWEV;
	ID3DX11EffectScalarVariable * pFragmentBufferSizeEV;

	void LinkEffect(ID3DX11Effect * pEffect) {
		SAFE_GET_UAV(pEffect, "g_fragmentBufferRW", pFragmentBufferRWEV);
		SAFE_GET_UAV(pEffect, "g_listHeadRW", pListHeadRWEV);
		SAFE_GET_SCALAR(pEffect, "g_fragmentBufferSize", pFragmentBufferSizeEV);
	};

	void BeginTransparency() {
		pRTV = DXUTGetD3D11RenderTargetView();
		pDSV = DXUTGetD3D11DepthStencilView();

		pFragmentBufferRWEV->SetUnorderedAccessView(TransparencyModule::pFragmentBufferUAV);
		pListHeadRWEV->SetUnorderedAccessView(TransparencyModule::pListHeadBufferUAV);
		pFragmentBufferSizeEV->SetInt(TransparencyModule::nodeBufferSize);
	};

	void EndTransparency(ID3D11DeviceContext * pd3dImmediateContext, ID3DX11EffectPass * pass) {
		pFragmentBufferRWEV->SetUnorderedAccessView(nullptr);
		pListHeadRWEV->SetUnorderedAccessView(nullptr);

		pass->Apply(0, pd3dImmediateContext);

		pd3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDSV);
	};
};
