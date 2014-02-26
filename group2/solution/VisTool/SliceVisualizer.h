#pragma once

#include "TransparencyModule.h"

#include "SettingsStorage.h"
#include "VectorVolumeData.h"
#include "ScalarVolumeData.h"
#include "BoxManipulationManager.h"
#include "TransferFunctionEditor\TransferFunctionEditor.h"

#include <DirectXMath.h>
#include <d3dx11effect.h>
#include <DXUT.h>
#include <DXUTcamera.h>
using namespace DirectX;

#include "AntTweakBar.h"

#include <list>

extern TransferFunctionEditor	* g_transferFunctionEditor;
extern BoxManipulationManager	* g_boxManipulationManager;

class SliceVisualizer : public Observer
{
public:
	//types
	enum PassType {
		PASS_RENDER,
		PASS_RENDER_TRANSPARENT,
		PASS_COMPUTE_TF_SLICE,
		PASS_COMPUTE_2D_LIC,
		PASS_COMPUTE_2D_LIC_COLORED,
		NUM_PASSES
	};
	
	// static variables
	static TwBar		* pParametersBar;

	// static functions
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);
	static void SaveConfig(SettingsStorage &store);
	static void LoadConfig(SettingsStorage &store, ScalarVolumeData * scalarData, VectorVolumeData * vectorData);
	static HRESULT Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	static HRESULT RenderTransparency(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	static void FrameMove(double dTime, float fElapsedTime, float fElapsedLogicTime);
	static void DeleteInstances(void);
	static void TW_CALL CreateSliceVisualizerCB(void * clientData);
	static void TW_CALL DeleteSliceVisualizerCB(void * clientData);

	// constructors & destructor
	SliceVisualizer(ScalarVolumeData * scalarData, VectorVolumeData * vectorData);
	virtual ~SliceVisualizer(void);

	// methods
	void SetSliceDirection(int sliceDirection);
	virtual void notify(Observable * volumeData);

	// accessors
	void SetTextureSRV(ID3D11ShaderResourceView	*  debugTexture) {
		SAFE_RELEASE(m_pSliceTextureBufferSRV);
		m_pSliceTextureBufferSRV = debugTexture;
	};
	
protected:
	// types
	
	// static functions
	static void TW_CALL RemoveSliceCB(void *clientData);
	static void SetupTwBar(TwBar* pParametersBar);
	static void GenerateNoise(float * textureData, int noiseType, int resX, int resY, float param);

	// statics
	static std::list<SliceVisualizer *> sliceInstances;
	static unsigned int instanceCounter;

	static ID3DX11Effect * pEffect;
	static ID3DX11EffectTechnique * pTechnique;
	static char * passNames[NUM_PASSES];
	static ID3DX11EffectPass * pPasses[NUM_PASSES];
	static ID3D11Device * pd3dDevice;

	static TwType		sliceVisualizerType;
	static ID3DX11EffectMatrixVariable	* pModelWorldViewProjEV;
	
	static ID3DX11EffectScalarVariable	* pTimestepTEV;
	static ID3DX11EffectVectorVariable	* pVelocityScalingEV;

	static ID3DX11EffectVectorVariable  * pSliceDirectionUEV;
	static ID3DX11EffectVectorVariable  * pSliceDirectionVEV;
	static ID3DX11EffectVectorVariable	* pSliceCornerEV;
	static ID3DX11EffectVectorVariable	* pSliceResolutionEV;

	static ID3DX11EffectScalarVariable	* pLICLengthEV;
	static ID3DX11EffectScalarVariable	* pLICStepsizeEV;
	static ID3DX11EffectScalarVariable	* pLICStepThreshold;
	static ID3DX11EffectScalarVariable	* pLICKernelSigmaEV;
	static ID3DX11EffectVectorVariable	* pPlaneNormalEV;

	static ID3DX11EffectShaderResourceVariable	* pTexVolume0EV;
	static ID3DX11EffectShaderResourceVariable	* pTexVolume1EV;
	static ID3DX11EffectShaderResourceVariable	* pSliceTextureEV;
	static ID3DX11EffectShaderResourceVariable	* pNoiseTextureEV;
	static ID3DX11EffectUnorderedAccessViewVariable	* pSliceTextureRWEV;
	
	static ID3DX11EffectShaderResourceVariable	* pScalarVolumeEV;
	static ID3DX11EffectShaderResourceVariable	* pTransferFunctionEV;

	static TransparencyModuleShaderEnvironment transparencyEnvironment;

	// methods
	HRESULT CreateGPUBuffers();
	void SaveConfig(SettingsStorage &store, std::string id);
	void LoadConfig(SettingsStorage &store, std::string id);
	HRESULT RenderInstance(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	HRESULT RenderTransparencyInstance(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	void FrameMoveInstance(double dTime, float fElapsedTime, float fElapsedLogicTime);
	void ComputeTexture(ID3D11DeviceContext * pContext, double dTime);
	bool SVRequireRecompute(void);

	// members
	XMMATRIX		m_modelTransform;
	XMFLOAT3		m_velocityScaling;
	VectorVolumeData * m_pVectorVolumeData;
	ScalarVolumeData * m_pScalarVolumeData;

	bool m_dataChanged;

	int				m_sliceIndex;
	int				m_sliceDirection, m_sliceDirectionGUI;
	float			m_sliceResolutionScale, m_sliceResolutionScaleGUI;
	XMINT2			m_sliceResolution;
	bool			m_enableTransparency;
	int				m_LICNoiseType, m_LICNoiseTypeGUI;
	float			m_LICLength, m_LICLengthGUI;
	float			m_LICStepsize, m_LICStepsizeGUI;
	float			m_LICKernelSigma, m_LICKernelSigmaGUI;
	float			m_LICNoiseParam, m_LICNoiseParamGUI;
	float			m_LICThreshold, m_LICThresholdGUI;
	BoxManipulationManager::ManipulationBox m_slicePositionBox;
	float			m_sliceLastUpdate;
	
	PassType		m_currentPassType, m_currentPassTypeGUI;

	ID3D11Texture2D				* m_pNoiseTextureBuffer;
	ID3D11ShaderResourceView	* m_pNoiseTextureBufferSRV;

	ID3D11Texture2D				* m_pSliceTextureBuffer;
	ID3D11ShaderResourceView	* m_pSliceTextureBufferSRV;
	ID3D11UnorderedAccessView	* m_pSliceTextureBufferUAV;
};

