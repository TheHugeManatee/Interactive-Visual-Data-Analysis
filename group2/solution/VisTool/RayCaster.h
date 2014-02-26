#pragma once

#include "util/util.h"
#include "util/notification.h"

#include "TransparencyModule.h"

#include "SettingsStorage.h"
#include "ScalarVolumeData.h"
#include "SimpleMesh.h"

#include "BoxManipulationManager.h"

#include "TransferFunctionEditor\TransferFunctionEditor.h"

#include <DirectXMath.h>
#include <d3dx11effect.h>
#include <DXUT.h>
#include <DXUTcamera.h>
using namespace DirectX;

#include "AntTweakBar.h"

extern TransferFunctionEditor	* g_transferFunctionEditor;
extern BoxManipulationManager	* g_boxManipulationManager;
extern ID3D11Texture2D			* g_pDepthBufferCopy;
extern ID3D11ShaderResourceView * g_pDepthBufferCopySRV;
extern ID3D11RenderTargetView	* g_pSecondaryRenderTargetRTV;
extern ID3D11ShaderResourceView * g_pSecondaryRenderTargetSRV;

class RayCaster : public Observer
{
public:
	//types
	enum PassType {
		PASS_BOX,
		PASS_FRONT,
		PASS_BACK,
		PASS_BASIC_RAYCAST,
		PASS_ISOSURFACE,
		PASS_ISOSURFACE_ALPHA,
		PASS_DVR,
		PASS_MIP,
		PASS_DUAL_ISOSURFACE,
		PASS_ISOSURFACE_ALPHA_GLOBAL,
		PASS_MIP2,
		NUM_PASSES
	};
	//statics
	static TwBar		* pParametersBar;
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);

	//constructors & destructor
	RayCaster(ScalarVolumeData & volData);
	virtual ~RayCaster(void);

	//methods
	virtual HRESULT Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	virtual HRESULT RenderTransparency(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	void FrameMove(double dTime, float fElapsedTime, float fElapsedLogicTime);
	void SaveConfig(SettingsStorage &store);
	void LoadConfig(SettingsStorage &store);

protected:
	// static functions
	static void TW_CALL SetCurrentPassCB(const void *value, void *clientData);
	static void TW_CALL GetCurrentPassCB(void *value, void *clientData);
	static HRESULT CreateBoxVertexIndexBuffer();

	// static variables
	static ID3DX11Effect * pEffect;
	static ID3DX11EffectTechnique * pTechnique;
	static char * passNames[NUM_PASSES];
	static ID3DX11EffectPass * pPasses[NUM_PASSES];
	static ID3D11Device * pd3dDevice;

	static ID3DX11EffectMatrixVariable	* pWorldViewProjEV;
	static ID3DX11EffectMatrixVariable	* pWorldViewProjInvEV;
	static ID3DX11EffectVectorVariable	* pSurfaceColorEV;
	static ID3DX11EffectVectorVariable	* pSurfaceColor2EV;
	static ID3DX11EffectVectorVariable	* pLightPosEV;
	static ID3DX11EffectVectorVariable	* pCamPosEV;
	static ID3DX11EffectScalarVariable	* pIsoValueEV;
	static ID3DX11EffectScalarVariable	* pIsoValue2EV;
	static ID3DX11EffectScalarVariable	* pRaycastStepsizeEV;
	static ID3DX11EffectScalarVariable	* pRaycastPixelStepsizeEV;
	static ID3DX11EffectScalarVariable	* pBinSearchStepsEV;
	static ID3DX11EffectScalarVariable	* pTerminationAlphaEV;
	static ID3DX11EffectScalarVariable	* pGlobalAlphaScaleEV;

	static ID3DX11EffectVectorVariable	* pLightColorEV;
	static ID3DX11EffectScalarVariable	* pAmbientEV;
	static ID3DX11EffectScalarVariable	* pDiffuseEV;
	static ID3DX11EffectScalarVariable	* pSpecularEV;
	static ID3DX11EffectScalarVariable	* pSpecularExpEV;
	static ID3DX11EffectScalarVariable	* pDVRLightingEV;

	static ID3DX11EffectShaderResourceVariable	* pTexVolumeEV;
	static ID3DX11EffectShaderResourceVariable	* pTexNormalVolumeEV;
	static ID3DX11EffectShaderResourceVariable	* pDepthBufferEV;
	static ID3DX11EffectShaderResourceVariable	* pRayEntryPointsEV;
	static ID3DX11EffectShaderResourceVariable	* pTransferFunctionEV;

	static ID3D11Buffer				* pBoxIndexBuffer;
	static ID3D11Buffer				* pBoxVertexBuffer;
	static ID3D11InputLayout		* pBoxInputLayout;

	static TransparencyModuleShaderEnvironment transparencyEnvironment;

	static unsigned int boxIndices[36];
	static XMFLOAT4 boxDisplacements[8];

	bool			m_boxLocked;
	float			m_isoValue;
	float			m_isoValue2;
	float			m_raycastStepsize;
	float			m_rayTerminationAlpha;
	float			m_globalAlphaScale;
	int				m_binSearchSteps;
	XMFLOAT4		m_surfaceColor;
	XMFLOAT4		m_surfaceColor2;
	bool			m_DVRlighting;
	XMFLOAT4		m_boxVertices[8];

	BoxManipulationManager::ManipulationBox m_raycastClippingBox;

	PassType m_currentPassSelection;

	ScalarVolumeData & m_volumeData;

	XMMATRIX m_modelTransform;
};

