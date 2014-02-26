#pragma once

#include "util/util.h"
#include "util/notification.h"

#include "SettingsStorage.h"
#include "VectorVolumeData.h"

#include <DirectXMath.h>
#include <d3dx11effect.h>
#include <DXUT.h>
#include <DXUTcamera.h>
using namespace DirectX;

#include "AntTweakBar.h"

class GlyphVisualizer : public Observer
{
public:
	//types
	enum PassType {
		PASS_RENDER,
		NUM_PASSES
	};

	//statics
	static TwBar		* pParametersBar;
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);

	//constructors & destructor
	GlyphVisualizer(VectorVolumeData & volData);
	virtual ~GlyphVisualizer(void);

	//methods
	virtual HRESULT Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	void SaveConfig(SettingsStorage &store);
	void LoadConfig(SettingsStorage &store);

	void FrameMove(double dTime, float fElapsedTime);

protected:
	// static functions
	static void TW_CALL SetSliceDirectionCB(const void *value, void *clientData);
	static void TW_CALL GetSliceDirectionCB(void *value, void *clientData);
	static void TW_CALL SetGlyphSpacingCB(const void *value, void *clientData);
	static void TW_CALL GetGlyphSpacingCB(void *value, void *clientData);
	static void TW_CALL SetSliceCB(const void *value, void *clientData);
	static void TW_CALL GetSliceCB(void *value, void *clientData);

	//statics
	static ID3DX11Effect * pEffect;
	static ID3DX11EffectTechnique * pTechnique;
	static char * passNames[NUM_PASSES];
	static ID3DX11EffectPass * pPasses[NUM_PASSES];
	static ID3D11Device * pd3dDevice;

	static TwType		vectfType;
	static TwType		vectiType;

	static ID3DX11EffectMatrixVariable	* pWorldViewProjEV;
	static ID3DX11EffectScalarVariable	* pTimestepTEV;
	static ID3DX11EffectVectorVariable	* pCamPosEV;
	static ID3DX11EffectVectorVariable	* pNumGlyphsEV;
	static ID3DX11EffectVectorVariable	* pGlyphSpacingEV;
	static ID3DX11EffectVectorVariable	* pSliceCenterEV;
	static ID3DX11EffectScalarVariable	* pGlyphSizeEV;
	
	static ID3DX11EffectShaderResourceVariable	* pTexVolume0EV;
	static ID3DX11EffectShaderResourceVariable	* pTexVolume1EV;

	XMINT3		m_numGlyphs;
	XMFLOAT3	m_glyphSpacing;
	XMFLOAT3	m_sliceCenter;
	XMMATRIX	m_modelTransform;
	float		m_glyphSize;

	VectorVolumeData & m_volumeData;
};

