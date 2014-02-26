#pragma once

#include "util/notification.h"

#include "TransparencyModule.h"

#include "SettingsStorage.h"
#include "VectorVolumeData.h"
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

class ParticleTracer : public Observer
{
public:
	//types
	enum PassType {
		PASS_RENDER,
		PASS_RENDER_COLORED,
		PASS_ADVECT,
		PASS_RENDER_CL_LINE,
		PASS_RENDER_CL_RIBBON,
		PASS_RENDER_TUBE_CORNERS,
		PASS_RENDER_TUBE_CYLINDERS,
		PASS_RENDER_SURFACE,
		PASS_RENDER_SURFACE_WIREFRAME,
		PASS_COMPUTE_STREAMLINE,
		PASS_COMPUTE_STREAMRIBBON,
		PASS_COMPUTE_STREAKLINE,
		PASS_COMPUTE_STREAKRIBBON,
		PASS_INIT_CHARACTERISTIC_LINE,
		PASS_COMPUTE_SURFACE_VERTEX_METRICS,
		PASS_COMPUTE_TRIANGLE_PROPERTIES,
		NUM_PASSES
	};
	enum CharacteristicLineMode {
		CL_DISABLED,
		CL_PATHLINES,
		CL_STREAKLINES,
		CL_STREAMLINES
	};
	enum CharacteristicLineRenderMode {
		CLRM_LINEPRIMITIVE,
		CLRM_RIBBON,
		CLRM_TUBE,
		CLRM_SURFACE
	};
	enum SeedingMode {
		SM_RANDOM,
		SM_LINE,
		SM_SURFACE
	};

	// static variables
	static TwBar		* pParametersBar;

	// static functions
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);
	static void SaveConfig(SettingsStorage &store);
	static void LoadConfig(SettingsStorage &store, VectorVolumeData & volData);
	static HRESULT Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	static HRESULT RenderTransparency(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	static void FrameMove(double dTime, float fElapsedTime, float fElapsedLogicTime);
	static void DeleteInstances(void);

	// constructors & destructor
	ParticleTracer(VectorVolumeData & volData);
	virtual ~ParticleTracer(void);

	// methods
	void DumpParticleInfo(unsigned int numParticles);	//Debugging helper
	void DumpStreaklineInfo(unsigned int numParticles);	//Debugging helper
	virtual void notify(Observable * volumeData);
	
	// accessors
	void SetNumParticles(unsigned int numParticles);
	unsigned int GetNumParticles() {return m_numParticles;};
	void SetCLLength(unsigned int clLength);

protected:
	// types
	struct ParticleDescriptor {
		float pos[3];
		float age;
		float seedPos[3];
		float ageSeed;
	} ParticleDescriptor;

	struct CharacteristicLineVertex {
		float pos[3];
		float tangent[3];
		float color[4];
		float age;
		float size;
	} LineStepDescriptor;

	struct TriangleProperties {
		float normal[3];
		float center[3];
		float area;
		float maxEdgeProduct;
	};

	// static functions
	static void TW_CALL SetNumParticlesCB(const void *value, void *clientData);
	static void TW_CALL GetNumParticlesCB(void *value, void *clientData);
	static void TW_CALL SetCLLengthCB(const void *value, void *clientData);
	static void TW_CALL GetCLLengthCB(void *value, void *clientData);
	static void TW_CALL RemoveParticleProbeCB(void * clientData);
	static void SetupTwBar(TwBar* pParametersBar);

	// statics
	static std::list<ParticleTracer *> probeInstances;
	static unsigned int instanceCounter;

	static ID3DX11Effect * pEffect;
	static ID3DX11EffectTechnique * pTechnique;
	static char * passNames[NUM_PASSES];
	static ID3DX11EffectPass * pPasses[NUM_PASSES];
	static ID3D11Device * pd3dDevice;

	static TwType		particleTracerType;
	static ID3DX11EffectMatrixVariable	* pModelWorldViewProjEV;
	static ID3DX11EffectMatrixVariable	* pModelWorldEV;
	static ID3DX11EffectMatrixVariable	* pModelWorldViewEV;
	static ID3DX11EffectMatrixVariable	* pModelWorldInvTranspEV;
	static ID3DX11EffectMatrixVariable	* pViewProjEV;
	
	static ID3DX11EffectVectorVariable	* pCamRightEV;
	static ID3DX11EffectVectorVariable	* pCamUpEV;
	static ID3DX11EffectVectorVariable	* pCamForwardEV;

	static ID3DX11EffectScalarVariable	* pTimestepTEV;
	static ID3DX11EffectScalarVariable	* pTimeDeltaEV;
	static ID3DX11EffectVectorVariable	* pVelocityScalingEV;
	static ID3DX11EffectVectorVariable	* pRndEV;
	static ID3DX11EffectScalarVariable	* pMaxParticleLifetimeEV;
	static ID3DX11EffectScalarVariable	* pNumParticlesEV;
	static ID3DX11EffectScalarVariable	* pParticleSizeEV;
	static ID3DX11EffectScalarVariable	* pColorParticlesEV;

	static ID3DX11EffectVectorVariable	* pSpawnRegionMinEV;
	static ID3DX11EffectVectorVariable	* pSpawnRegionMaxEV;
	static ID3DX11EffectVectorVariable	* pParticleColorEV;

	static ID3DX11EffectVectorVariable	* pLightColorEV;
	static ID3DX11EffectVectorVariable	* pLightPosEV;
	static ID3DX11EffectScalarVariable	* pAmbientEV;
	static ID3DX11EffectScalarVariable	* pDiffuseEV;
	static ID3DX11EffectScalarVariable	* pSpecularEV;
	static ID3DX11EffectScalarVariable	* pSpecularExpEV;

	static ID3DX11EffectScalarVariable	* pCLStepsizeEV;
	static ID3DX11EffectScalarVariable	* pCLLengthEV;
	static ID3DX11EffectScalarVariable	* pCLWidthEV;
	static ID3DX11EffectVectorVariable	* pCLRibbonBaseOrientationEV;
	static ID3DX11EffectScalarVariable	* pCLReseedIntervalEV;

	static ID3DX11EffectScalarVariable	* pCLEnableSurfaceLighting;
	static ID3DX11EffectScalarVariable	* pCLEnableAlphaDensity;
	static ID3DX11EffectScalarVariable	* pCLAlphaDensityCoeff;
	static ID3DX11EffectScalarVariable	* pCLEnableAlphaFade;
	static ID3DX11EffectScalarVariable	* pCLEnableAlphaShape;
	static ID3DX11EffectScalarVariable	* pCLAlphaShapeCoeff;
	static ID3DX11EffectScalarVariable	* pCLEnableAlphaCurvature;
	static ID3DX11EffectScalarVariable	* pCLAlphaCurvatureCoeff;
	static ID3DX11EffectVectorVariable	* pCLTimeSurfaceOffset;
	static ID3DX11EffectScalarVariable	* pCLEnableTimeSurfaces;
	static ID3DX11EffectScalarVariable	* pCLNumTimeSurfaces;

	static ID3DX11EffectShaderResourceVariable	* pTexVolume0EV;
	static ID3DX11EffectShaderResourceVariable	* pTexVolume1EV;
	static ID3DX11EffectShaderResourceVariable	* pParticleBufferEV;
	static ID3DX11EffectUnorderedAccessViewVariable	* pParticleBufferRWEV;
	static ID3DX11EffectShaderResourceVariable	* pCharacteristicLineBufferEV;
	static ID3DX11EffectUnorderedAccessViewVariable	* pCharacteristicLineBufferRWEV;
	static ID3DX11EffectShaderResourceVariable	* pTrianglePropertiesEV;
	static ID3DX11EffectUnorderedAccessViewVariable	* pTrianglePropertiesRWEV;
	
	static ID3DX11EffectShaderResourceVariable	* pScalarVolumeEV;
	static ID3DX11EffectShaderResourceVariable	* pTransferFunctionEV;

	static TransparencyModuleShaderEnvironment transparencyEnvironment;

	// methods
	HRESULT CreateParticleGPUBuffer();
	HRESULT CreateCharacteristicLineGPUBuffer();
	HRESULT CreateTrianglePropertiesBuffer();
	void SaveConfig(SettingsStorage &store, std::string id);
	void LoadConfig(SettingsStorage &store, std::string id);
	HRESULT RenderInstance(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	HRESULT RenderTransparencyInstance(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	void PrepareRenderEnvironment(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs);
	void FrameMoveInstance(double dTime, float fElapsedTime, float fElapsedLogicTime);
	void ComputeStreamlines(ID3D11DeviceContext * pContext);
	void ComputeStreaklines(ID3D11DeviceContext * pContext, float fElapsedTime);
	void ComputeTriangleProperties(ID3D11DeviceContext * pContext);
	void InitCharacteristicLineBuffer(void);
	bool CLRequireRecompute(void);

	// members
	unsigned int	m_probeIndex;
	bool			m_enableParticles;
	unsigned int	m_numParticles, m_numParticlesGUI;
	XMMATRIX		m_modelTransform;
	XMFLOAT3		m_velocityScaling;
	VectorVolumeData & m_volumeData;
	float			m_maxParticleLifetime;
	float			m_particleSize;
	BoxManipulationManager::ManipulationBox m_spawnRegionBox;
	XMFLOAT4		m_particleColor, m_particleColorGUI;
	bool			m_colorParticlesByMetric, m_colorParticlesByMetricGUI;
	XMMATRIX		m_texToNDCSpace;
	bool			m_volumeDataChanged;	//signals that volume data and current visualization are out of sync and should be updated
	SeedingMode		m_seedingMode, m_seedingModeGUI;
	bool			m_surfaceWireframe;

	CharacteristicLineMode m_clMode, m_clModeGUI; 
	CharacteristicLineRenderMode m_clRenderMode, m_clRenderModeGUI;
	float			m_clStepsize, m_clStepsizeGUI;
	unsigned int	m_clLength, m_clLengthGUI;
	float			m_clWidth, m_clWidthGUI;
	XMFLOAT3		m_clRibbonBaseOrientation, m_clRibbonBaseOrientationGUI;
	float			m_clReseedInterval, m_clReseedIntervalGUI;
	bool			m_clEnableSurfaceLighting;
	int				m_numTimeSurfaces, m_numTimeSurfacesGUI;
	XMFLOAT3		m_timeSurfaceOffsetDirection;

	bool			m_clEnableAlphaDensity, m_clEnableAlphaDensityGUI;
	float			m_clAlphaDensityCoeff, m_clAlphaDensityCoeffGUI;
	bool			m_clEnableAlphaFade, m_clEnableAlphaFadeGUI;
	bool			m_clEnableAlphaShape, m_clEnableAlphaShapeGUI;
	float			m_clAlphaShapeCoeff, m_clAlphaShapeCoeffGUI;
	bool			m_clEnableAlphaCurvature, m_clEnableAlphaCurvatureGUI;
	float			m_clAlphaCurvatureCoeff, m_clAlphaCurvatureCoeffGUI;

	ID3D11Buffer				* m_pParticleBuffer;
	ID3D11ShaderResourceView	* m_pParticleBufferSRV;
	ID3D11UnorderedAccessView	* m_pParticleBufferUAV;

	ID3D11Buffer				* m_pCharacteristicLineBuffer;
	ID3D11ShaderResourceView	* m_pCharacteristicLineBufferSRV;
	ID3D11UnorderedAccessView	* m_pCharacteristicLineBufferUAV;

	ID3D11Buffer				* m_pTrianglePropertiesBuffer;
	ID3D11ShaderResourceView	* m_pTrianglePropertiesBufferSRV;
	ID3D11UnorderedAccessView	* m_pTrianglePropertiesBufferUAV;
};

