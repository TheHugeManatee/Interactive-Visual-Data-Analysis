#include "ParticleTracer.h"

#include "SimpleMesh.h"

#include "util/util.h"
#include "Globals.h"

#include <iostream>
#include <random>
#include <sstream>
#include <float.h>

ID3DX11Effect			* ParticleTracer::pEffect = nullptr;
ID3DX11EffectTechnique	* ParticleTracer::pTechnique = nullptr;
ID3D11Device			* ParticleTracer::pd3dDevice = nullptr;
ID3DX11EffectPass		* ParticleTracer::pPasses[NUM_PASSES];
TwBar					* ParticleTracer::pParametersBar = nullptr;
TwType					ParticleTracer::particleTracerType;

TransparencyModuleShaderEnvironment ParticleTracer::transparencyEnvironment;

// pass names in the effect file
char * ParticleTracer::passNames[] = {
		"PASS_RENDER",
		"PASS_RENDER_COLORED",
		"PASS_ADVECT",
		"PASS_RENDER_CL_LINE",
		"PASS_RENDER_CL_RIBBON",
		"PASS_RENDER_TUBE_CORNERS",
		"PASS_RENDER_TUBE_CYLINDERS",
		"PASS_RENDER_SURFACE",
		"PASS_RENDER_SURFACE_WIREFRAME",
		"PASS_COMPUTE_STREAMLINE",
		"PASS_COMPUTE_STREAMRIBBON",
		"PASS_COMPUTE_STREAKLINE",
		"PASS_COMPUTE_STREAKRIBBON",
		"PASS_INIT_CHARACTERISTIC_LINE",
		"PASS_COMPUTE_SURFACE_VERTEX_METRICS",
		"PASS_COMPUTE_TRIANGLE_PROPERTIES"
};

//Effect Variables
ID3DX11EffectMatrixVariable	* ParticleTracer::pViewProjEV = nullptr;
ID3DX11EffectMatrixVariable	* ParticleTracer::pModelWorldViewProjEV = nullptr;
ID3DX11EffectMatrixVariable	* ParticleTracer::pModelWorldEV = nullptr;
ID3DX11EffectMatrixVariable	* ParticleTracer::pModelWorldViewEV = nullptr;
ID3DX11EffectMatrixVariable	* ParticleTracer::pModelWorldInvTranspEV = nullptr;

ID3DX11EffectVectorVariable * ParticleTracer::pCamRightEV = nullptr;
ID3DX11EffectVectorVariable * ParticleTracer::pCamUpEV = nullptr;
ID3DX11EffectVectorVariable * ParticleTracer::pCamForwardEV = nullptr;

ID3DX11EffectScalarVariable * ParticleTracer::pTimestepTEV = nullptr;
ID3DX11EffectScalarVariable * ParticleTracer::pTimeDeltaEV = nullptr;
ID3DX11EffectVectorVariable * ParticleTracer::pVelocityScalingEV = nullptr;
ID3DX11EffectScalarVariable * ParticleTracer::pMaxParticleLifetimeEV = nullptr;
ID3DX11EffectVectorVariable * ParticleTracer::pRndEV = nullptr;
ID3DX11EffectScalarVariable * ParticleTracer::pNumParticlesEV = nullptr;
ID3DX11EffectScalarVariable * ParticleTracer::pParticleSizeEV = nullptr;
ID3DX11EffectScalarVariable * ParticleTracer::pColorParticlesEV = nullptr;

ID3DX11EffectVectorVariable	* ParticleTracer::pSpawnRegionMinEV = nullptr;
ID3DX11EffectVectorVariable	* ParticleTracer::pSpawnRegionMaxEV = nullptr;
ID3DX11EffectVectorVariable	* ParticleTracer::pParticleColorEV = nullptr;

ID3DX11EffectVectorVariable	* ParticleTracer::pLightColorEV = nullptr;
ID3DX11EffectVectorVariable	* ParticleTracer::pLightPosEV = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pAmbientEV = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pDiffuseEV = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pSpecularEV = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pSpecularExpEV = nullptr;

ID3DX11EffectScalarVariable	* ParticleTracer::pCLStepsizeEV = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLLengthEV = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLWidthEV = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLReseedIntervalEV = nullptr;
ID3DX11EffectVectorVariable	* ParticleTracer::pCLRibbonBaseOrientationEV = nullptr;

ID3DX11EffectScalarVariable	* ParticleTracer::pCLEnableSurfaceLighting = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLEnableAlphaDensity = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLAlphaDensityCoeff = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLEnableAlphaFade = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLEnableAlphaShape = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLAlphaShapeCoeff = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLEnableAlphaCurvature = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLAlphaCurvatureCoeff = nullptr;
ID3DX11EffectScalarVariable	* ParticleTracer::pCLNumTimeSurfaces = nullptr;
ID3DX11EffectScalarVariable * ParticleTracer::pCLEnableTimeSurfaces = nullptr;
ID3DX11EffectVectorVariable * ParticleTracer::pCLTimeSurfaceOffset = nullptr;

ID3DX11EffectShaderResourceVariable	* ParticleTracer::pTexVolume0EV = nullptr;
ID3DX11EffectShaderResourceVariable	* ParticleTracer::pTexVolume1EV = nullptr;
ID3DX11EffectShaderResourceVariable	* ParticleTracer::pParticleBufferEV = nullptr;
ID3DX11EffectUnorderedAccessViewVariable * ParticleTracer::pParticleBufferRWEV = nullptr;

ID3DX11EffectShaderResourceVariable	* ParticleTracer::pTransferFunctionEV = nullptr;
ID3DX11EffectShaderResourceVariable	* ParticleTracer::pScalarVolumeEV = nullptr;

ID3DX11EffectShaderResourceVariable	* ParticleTracer::pCharacteristicLineBufferEV = nullptr;
ID3DX11EffectUnorderedAccessViewVariable	* ParticleTracer::pCharacteristicLineBufferRWEV = nullptr;
ID3DX11EffectShaderResourceVariable	* ParticleTracer::pTrianglePropertiesEV = nullptr;
ID3DX11EffectUnorderedAccessViewVariable	* ParticleTracer::pTrianglePropertiesRWEV = nullptr;

std::list<ParticleTracer *> ParticleTracer::probeInstances;
unsigned int ParticleTracer::instanceCounter;

HRESULT ParticleTracer::Initialize(ID3D11Device * pd3dDevice_, TwBar* pParametersBar_)
{
	HRESULT hr;

	pd3dDevice = pd3dDevice_;
	SetupTwBar(pParametersBar_);

	std::wstring effectPath = GetExePath() + L"ParticleTracer.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &pEffect)))
	{
		std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

	SAFE_GET_TECHNIQUE(pEffect, "ParticleTracer", pTechnique);
	for(int i=0; i < NUM_PASSES; i++)
		SAFE_GET_PASS(pTechnique, passNames[i], pPasses[i]);

	SAFE_GET_MATRIX(pEffect, "g_modelWorldViewProj", pModelWorldViewProjEV);
	SAFE_GET_MATRIX(pEffect, "g_viewProj", pViewProjEV);
	SAFE_GET_MATRIX(pEffect, "g_modelWorld", pModelWorldEV);
	SAFE_GET_MATRIX(pEffect, "g_modelWorldView", pModelWorldViewEV);
	SAFE_GET_MATRIX(pEffect, "g_modelWorldInvTransp", pModelWorldInvTranspEV);
	
	SAFE_GET_VECTOR(pEffect, "g_camRight", pCamRightEV);
	SAFE_GET_VECTOR(pEffect, "g_camUp", pCamForwardEV);
	SAFE_GET_VECTOR(pEffect, "g_camForward", pCamUpEV);

	SAFE_GET_SCALAR(pEffect, "g_timestepT", pTimestepTEV);
	SAFE_GET_SCALAR(pEffect, "g_timeDelta", pTimeDeltaEV);
	SAFE_GET_VECTOR(pEffect, "g_velocityScaling", pVelocityScalingEV);
	SAFE_GET_VECTOR(pEffect, "g_rnd", pRndEV);
	SAFE_GET_SCALAR(pEffect, "g_maxParticleLifetime", pMaxParticleLifetimeEV);
	SAFE_GET_SCALAR(pEffect, "g_numParticles", pNumParticlesEV);
	SAFE_GET_SCALAR(pEffect, "g_particleSize", pParticleSizeEV);
	SAFE_GET_SCALAR(pEffect, "g_colorParticles", pColorParticlesEV);

	SAFE_GET_VECTOR(pEffect, "g_spawnRegionMin", pSpawnRegionMinEV);
	SAFE_GET_VECTOR(pEffect, "g_spawnRegionMax", pSpawnRegionMaxEV);
	SAFE_GET_VECTOR(pEffect, "g_particleColor", pParticleColorEV);
	
	SAFE_GET_VECTOR(pEffect, "g_lightPos", pLightPosEV);
	SAFE_GET_VECTOR(pEffect, "g_lightColor", pLightColorEV);
	SAFE_GET_SCALAR(pEffect, "k_a", pAmbientEV);
	SAFE_GET_SCALAR(pEffect, "k_d", pDiffuseEV);
	SAFE_GET_SCALAR(pEffect, "k_s", pSpecularEV);
	SAFE_GET_SCALAR(pEffect, "g_specularExp", pSpecularExpEV);

	SAFE_GET_RESOURCE(pEffect, "g_flowFieldTex0", pTexVolume0EV);
	SAFE_GET_RESOURCE(pEffect, "g_flowFieldTex1", pTexVolume1EV);
	SAFE_GET_RESOURCE(pEffect, "g_particleBuffer", pParticleBufferEV);
	SAFE_GET_UAV(pEffect, "g_particleBufferRW", pParticleBufferRWEV);

	SAFE_GET_RESOURCE(pEffect, "g_transferFunction", pTransferFunctionEV);
	SAFE_GET_RESOURCE(pEffect, "g_scalarVolume", pScalarVolumeEV);

	SAFE_GET_SCALAR(pEffect, "g_clLength", pCLLengthEV);
	SAFE_GET_SCALAR(pEffect, "g_clWidth", pCLWidthEV);
	SAFE_GET_SCALAR(pEffect, "g_clStepsize", pCLStepsizeEV);
	SAFE_GET_SCALAR(pEffect, "g_clReseedInterval", pCLReseedIntervalEV);
	SAFE_GET_VECTOR(pEffect, "g_clRibbonBaseOrientation", pCLRibbonBaseOrientationEV);

	SAFE_GET_SCALAR(pEffect, "g_enableSurfaceLighting", pCLEnableSurfaceLighting);
	SAFE_GET_SCALAR(pEffect, "g_clEnableAlphaDensity", pCLEnableAlphaDensity);
	SAFE_GET_SCALAR(pEffect, "g_alphaDensityCoefficient", pCLAlphaDensityCoeff);
	SAFE_GET_SCALAR(pEffect, "g_clEnableAlphaFade", pCLEnableAlphaFade);
	SAFE_GET_SCALAR(pEffect, "g_clEnableAlphaShape", pCLEnableAlphaShape);
	SAFE_GET_SCALAR(pEffect, "g_alphaShapeCoefficient", pCLAlphaShapeCoeff);
	SAFE_GET_SCALAR(pEffect, "g_clEnableAlphaCurvature", pCLEnableAlphaCurvature);
	SAFE_GET_SCALAR(pEffect, "g_alphaCurvatureCoefficient", pCLAlphaCurvatureCoeff);
	SAFE_GET_VECTOR(pEffect, "g_timeSurfaceLineOffset", pCLTimeSurfaceOffset);
	SAFE_GET_SCALAR(pEffect, "g_enableTimeSurfaces", pCLEnableTimeSurfaces);
	SAFE_GET_SCALAR(pEffect, "g_numTimeSurfaces", pCLNumTimeSurfaces);

	SAFE_GET_RESOURCE(pEffect, "g_characteristicLineBuffer", pCharacteristicLineBufferEV);
	SAFE_GET_UAV(pEffect, "g_characteristicLineBufferRW", pCharacteristicLineBufferRWEV);
	SAFE_GET_RESOURCE(pEffect, "g_triangleProperties", pTrianglePropertiesEV);
	SAFE_GET_UAV(pEffect, "g_trianglePropertiesRW", pTrianglePropertiesRWEV);

	transparencyEnvironment.LinkEffect(pEffect);

	return S_OK;
}

void ParticleTracer::SetupTwBar(TwBar * pParametersBar_) {
	
	pParametersBar = TwNewBar("Particle Tracer");
	TwDefine("'Particle Tracer' color='10 10 200' size='300 450' position='1600 200' visible=false");

	static TwStructMember posMembers[] = // array used to describe tweakable variables of the Light structure
    {
        { "x",    TW_TYPE_FLOAT, 0 * sizeof(float),    "min=0 max=1 step=0.02" }, 
        { "y",    TW_TYPE_FLOAT, 1 * sizeof(float),    "min=0 max=1 step=0.02" }, 
        { "z",    TW_TYPE_FLOAT, 2 * sizeof(float),    "min=0 max=1 step=0.02" },
    };
	TwType posType = TwDefineStruct("SpawnPosLimit", posMembers, 3, sizeof(float[4]), NULL, NULL);

	TwType lineModeType = TwDefineEnum("LineModeType", NULL, 0);
	TwType lineModeDrawType = TwDefineEnum("LineModeDrawType", NULL, 0);
	TwType seedingType = TwDefineEnum("SeedingType", NULL, 0);

	// Define a new struct type: light variables are embedded in this structure
    static TwStructMember tracerMembers[] = // array used to describe tweakable variables of the Light structure
    {
		{ "Enable Particles",			TW_TYPE_BOOLCPP,	offsetof(ParticleTracer, m_enableParticles),  "" }, 
		{ "Num. of Particles / Lines",	TW_TYPE_INT32,		offsetof(ParticleTracer, m_numParticlesGUI), "min=2 step=20"},
		{ "Particle Size",				TW_TYPE_FLOAT,		offsetof(ParticleTracer, m_particleSize),     "min=0.0001 step=0.0001" }, 
        { "Particle Lifetime",			TW_TYPE_FLOAT,		offsetof(ParticleTracer, m_maxParticleLifetime),    "min=0.1 step=0.1" },
		{ "Color By Metric",			TW_TYPE_BOOLCPP,	offsetof(ParticleTracer, m_colorParticlesByMetricGUI),    "" },
		{ "Particle Color",				TW_TYPE_COLOR4F,	offsetof(ParticleTracer, m_particleColorGUI), "" },
		{ "Characteristic Line Mode",	lineModeType,		offsetof(ParticleTracer, m_clModeGUI), "enum='0 {Disabled}, 1 {Pathlines}, 2 {Streaklines}, 3 {Streamlines}'"},
		{ "Characteristic Line Rendering", lineModeDrawType, offsetof(ParticleTracer, m_clRenderModeGUI), "enum='0 {Lines}, 1 {Ribbons}, 2 {Tubes}, 3 {Surface}'"},
		{ "Seeding",					seedingType,		offsetof(ParticleTracer, m_seedingModeGUI), "enum='0 {Random}, 1 {Line}, 2 {Time Surface}'"},
		{ "CL Length",					TW_TYPE_INT32,		offsetof(ParticleTracer, m_clLengthGUI), "min=1 step=10"},
		{ "CL Width",					TW_TYPE_FLOAT,		offsetof(ParticleTracer, m_clWidthGUI), "min=0.0001 step=0.0001"},
		{ "CL Stepsize",				TW_TYPE_FLOAT,		offsetof(ParticleTracer, m_clStepsizeGUI), "step=0.0005"},
		{ "CL Ribbon Orientation",		TW_TYPE_DIR3F,		offsetof(ParticleTracer, m_clRibbonBaseOrientationGUI), ""},
		{ "CL Reseed Interval",			TW_TYPE_FLOAT,		offsetof(ParticleTracer, m_clReseedIntervalGUI), ""},
		{ "Surface Lighting",			TW_TYPE_BOOLCPP,	offsetof(ParticleTracer, m_clEnableSurfaceLighting), ""},
		{ "Surface Density Transparency", TW_TYPE_BOOLCPP,	offsetof(ParticleTracer, m_clEnableAlphaDensityGUI), ""},
		{ "Surface Density Coefficient", TW_TYPE_FLOAT,		offsetof(ParticleTracer, m_clAlphaDensityCoeffGUI), "min=0 step=0.01"},
		{ "Surface Fade",				TW_TYPE_BOOLCPP,	offsetof(ParticleTracer, m_clEnableAlphaFadeGUI), ""},
		{ "Surface Shape Transparency",	TW_TYPE_BOOLCPP,	offsetof(ParticleTracer, m_clEnableAlphaShapeGUI), ""},
		{ "Surface Shape Coefficient",	TW_TYPE_FLOAT,		offsetof(ParticleTracer, m_clAlphaShapeCoeffGUI), "min=0.3 max=1.2 step=0.01"},
		{ "Surface Curvature Transparency", TW_TYPE_BOOLCPP, offsetof(ParticleTracer, m_clEnableAlphaCurvatureGUI), ""},
		{ "Surface Curvature Coefficient", TW_TYPE_FLOAT,	offsetof(ParticleTracer, m_clAlphaCurvatureCoeffGUI), "min=0.5 max=3 step=0.1"},
		{ "Num. of Time Surfaces",		TW_TYPE_INT32,		offsetof(ParticleTracer, m_numTimeSurfacesGUI), "min=1"},
		{ "Spawn Region Center",		posType,			offsetof(ParticleTracer, m_spawnRegionBox) + offsetof(BoxManipulationManager::ManipulationBox, center), ""},
		{ "Spawn Region Size",			posType,			offsetof(ParticleTracer, m_spawnRegionBox) + offsetof(BoxManipulationManager::ManipulationBox, size), ""},
		{ "Render Surface Wireframe",	TW_TYPE_BOOLCPP,	offsetof(ParticleTracer, m_surfaceWireframe), ""}
    };
    particleTracerType = TwDefineStruct("Particle Tracer", tracerMembers, 26, sizeof(ParticleTracer), NULL, NULL);  // create a new TwType associated to the struct defined by the lightMembers array

}


HRESULT ParticleTracer::Release(void)
{
	for(int i=0; i < NUM_PASSES; i++)
		SAFE_RELEASE(pPasses[i]);

	SAFE_RELEASE(pTechnique);
	SAFE_RELEASE(pEffect);
	
	return S_OK;
}

void TW_CALL ParticleTracer::SetNumParticlesCB(const void *value, void *clientData)
{ 
	ParticleTracer * me = static_cast<ParticleTracer*>(clientData);
	me->SetNumParticles(*(const unsigned int *)value);
}

void TW_CALL ParticleTracer::GetNumParticlesCB(void *value, void *clientData)
{ 
    *(unsigned int *)value = static_cast<ParticleTracer*>(clientData)->m_numParticles;
}

void ParticleTracer::SaveConfig(SettingsStorage & store)
{
	char idBuf[256];
	store.StoreInt("scene.particletracer.numProbes", (int)probeInstances.size());
	int i=0;
	for(auto it = probeInstances.begin(); it != probeInstances.end(); ++it, ++i) {
		_itoa(i, idBuf, 10);
		(*it)->SaveConfig(store, idBuf);
	};
}

void TW_CALL ParticleTracer::RemoveParticleProbeCB(void * clientData)
{
	ParticleTracer * me = reinterpret_cast<ParticleTracer *> (clientData);
	//me->DumpStreaklineInfo(me->m_numParticles);
	delete me;
}

void ParticleTracer::LoadConfig(SettingsStorage & store, VectorVolumeData & volData)
{
	char idBuf[256];
	int instances;
	store.GetInt("scene.particletracer.numProbes", instances);
	for(int i=0; i < instances; i++) {
		ParticleTracer *p = new ParticleTracer(volData);
		_itoa(i, idBuf, 10);
		p->LoadConfig(store, idBuf);
	};
}

HRESULT ParticleTracer::Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	HRESULT hr;
	for(auto it = probeInstances.begin(); it != probeInstances.end(); ++it) {
		V_RETURN((*it)->RenderInstance(pd3dImmediateContext, sceneMtcs));
	}
	return S_OK;
}

HRESULT ParticleTracer::RenderTransparency(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	HRESULT hr;
	for(auto it = probeInstances.begin(); it != probeInstances.end(); ++it) {
		V_RETURN((*it)->RenderTransparencyInstance(pd3dImmediateContext, sceneMtcs));
	}
	return S_OK;
}

void ParticleTracer::FrameMove(double dTime, float fElapsedTime, float fElapsedLogicTime) {
	for(auto it = probeInstances.begin(); it != probeInstances.end(); ++it) {
		(*it)->FrameMoveInstance(dTime, fElapsedTime, fElapsedLogicTime);
	}
}

void ParticleTracer::DeleteInstances(void) {
	// we cant just iterate over because the destructor modifies the list and invalidates the iterators
	while(probeInstances.size()) {
		delete probeInstances.front();
	}
}

ParticleTracer::ParticleTracer(VectorVolumeData & volData) :
	m_volumeData(volData),
	m_numParticles(0),m_numParticlesGUI(100),
	m_enableParticles(true),
	m_pParticleBuffer(nullptr),
	m_pParticleBufferSRV(nullptr),
	m_pParticleBufferUAV(nullptr),
	m_maxParticleLifetime(5.f),
	m_particleSize(0.005f),
	m_colorParticlesByMetric(false), m_colorParticlesByMetricGUI(false),
	m_particleColor(1,1,1,1), m_particleColorGUI(1,1,1,1),
	m_probeIndex(instanceCounter++),
	m_seedingMode(SM_RANDOM), m_seedingModeGUI(SM_RANDOM),
	m_clMode(CL_DISABLED),	m_clModeGUI(CL_DISABLED),
	m_clRenderMode(CLRM_TUBE),	m_clRenderModeGUI(CLRM_TUBE),
	m_clStepsize(0.1f),
	m_clStepsizeGUI(0.1f),
	m_clLength(150),
	m_clLengthGUI(150),
	m_clWidth(0.005f),m_clWidthGUI(0.005f),
	m_clRibbonBaseOrientation(0, 0, 0),
	m_clRibbonBaseOrientationGUI(0, 0, 0),
	m_clReseedInterval(.05f), m_clReseedIntervalGUI(.05f),

	m_surfaceWireframe(false),

	m_pCharacteristicLineBuffer(nullptr),
	m_pCharacteristicLineBufferSRV(nullptr),
	m_pCharacteristicLineBufferUAV(nullptr),
	m_pTrianglePropertiesBuffer(nullptr),
	m_pTrianglePropertiesBufferSRV(nullptr),
	m_pTrianglePropertiesBufferUAV(nullptr),
	
	m_clEnableAlphaDensity(true), m_clEnableAlphaDensityGUI(true),
	m_clAlphaDensityCoeff(100), m_clAlphaDensityCoeffGUI(100),
	m_clEnableAlphaFade(true), m_clEnableAlphaFadeGUI(true),
	m_clEnableAlphaShape(true), m_clEnableAlphaShapeGUI(true),
	m_clAlphaShapeCoeff(0.75f), m_clAlphaShapeCoeffGUI(0.75f),
	m_clEnableAlphaCurvature(true), m_clEnableAlphaCurvatureGUI(true),
	m_clAlphaCurvatureCoeff(2.f), m_clAlphaCurvatureCoeffGUI(2.f),
	m_numTimeSurfaces(3), m_numTimeSurfacesGUI(3),
	m_clEnableSurfaceLighting(false),
	
	m_spawnRegionBox(XMFLOAT3(0.35f, 0.5f, 0.5f), XMFLOAT3(0.2f, 0.2f, 0.6f), true, true, true),
	m_volumeDataChanged(true)
{
	m_volumeData.RegisterObserver(this);
	g_boxManipulationManager->AddBox(&m_spawnRegionBox);

	XMFLOAT3 bbox = m_volumeData.GetBoundingBox();
	m_modelTransform = XMMatrixScaling(bbox.x, bbox.y, bbox.z);
	m_velocityScaling = XMFLOAT3(1.f/bbox.x, 1.f/bbox.y, 1.f/bbox.z);

	probeInstances.push_back(this);

	std::stringstream ss;
	ss << "[" << m_probeIndex << "]";
	TwAddVarRW(pParametersBar, (std::string("Particle Probe ")  + ss.str()).c_str(), particleTracerType, this, "");
	//TwAddVarCB(pParametersBar, (std::string("Number of Particles ")  + ss.str()).c_str(), TW_TYPE_INT32, SetNumParticlesCB, GetNumParticlesCB, this, ((std::string("group='Particle Probe ")  + ss.str() + "' min=2 step=100").c_str()));
	TwAddButton(pParametersBar,(std::string("Remove Probe ")  + ss.str()).c_str(), RemoveParticleProbeCB, this, "");//(std::string("group='Particle Probe ")  + ss.str() + "'").c_str());

	SetNumParticles(m_numParticlesGUI);

	int visible = 1;
	TwSetParam(pParametersBar, nullptr, "visible", TW_PARAM_INT32, 1, &visible);
}

ParticleTracer::~ParticleTracer(void)
{
	SAFE_RELEASE(m_pParticleBuffer);
	SAFE_RELEASE(m_pParticleBufferSRV);
	SAFE_RELEASE(m_pParticleBufferUAV);
	SAFE_RELEASE(m_pCharacteristicLineBuffer);
	SAFE_RELEASE(m_pCharacteristicLineBufferSRV);
	SAFE_RELEASE(m_pCharacteristicLineBufferUAV);
	SAFE_RELEASE(m_pTrianglePropertiesBuffer);
	SAFE_RELEASE(m_pTrianglePropertiesBufferSRV);
	SAFE_RELEASE(m_pTrianglePropertiesBufferUAV);

	std::stringstream ss;
	ss << "[" << m_probeIndex << "]";

	g_boxManipulationManager->RemoveBox(&m_spawnRegionBox);

	probeInstances.remove(this);

	int visible = probeInstances.size()?1:0;
	TwSetParam(pParametersBar, nullptr, "visible", TW_PARAM_INT32, 1, &visible);

	TwRemoveVar(pParametersBar, (std::string("Particle Probe ") + ss.str()).c_str());
	TwRemoveVar(pParametersBar, (std::string("Remove Probe ") + ss.str()).c_str());
	m_volumeData.UnregisterObserver(this);
	g_transferFunctionEditor->UnregisterObserver(this);
	if(m_colorParticlesByMetric)
		m_volumeData.GetScalarMetricVolume()->UnregisterObserver(this);
}

void ParticleTracer::SaveConfig(SettingsStorage &store, std::string id)
{
	std::string cfgName = "particletracer[" + id + "]";
	store.StoreBool(cfgName + ".enableParticles", m_enableParticles);
	store.StoreFloat(cfgName + ".maxParticleLifetime", m_maxParticleLifetime);
	store.StoreInt(cfgName + ".numParticles", m_numParticles);
	store.StoreFloat(cfgName + ".particleSize", m_particleSize);
	store.StoreBool(cfgName + ".colorByMetric", m_colorParticlesByMetric);
	store.StoreFloat4(cfgName + ".particleColor", &m_particleColor.x);
	store.StoreFloat3(cfgName + ".spawnRegion.center", &m_spawnRegionBox.center.x);
	store.StoreFloat3(cfgName + ".spawnRegion.size", &m_spawnRegionBox.size.x);
	store.StoreInt(cfgName + ".seedingMode", m_seedingMode);
	store.StoreInt(cfgName + ".characteristicLines.type", (int)m_clMode);
	store.StoreInt(cfgName + ".characteristicLines.renderMode", (int)m_clRenderMode);
	store.StoreInt(cfgName + ".characteristicLines.length", m_clLength);
	store.StoreFloat(cfgName + ".characteristicLines.width", m_clWidth);
	store.StoreFloat(cfgName + ".characteristicLines.stepsize", m_clStepsize);
	store.StoreFloat(cfgName + ".characteristicLines.reseedInterval", m_clReseedInterval);
	store.StoreFloat3(cfgName + ".characteristicLines.ribbonOrientation", &m_clRibbonBaseOrientation.x);

	store.StoreBool(cfgName + ".characteristicLines.surface.enableAlphaDensity", m_clEnableAlphaDensity);
	store.StoreFloat(cfgName + ".characteristicLines.surface.alphaDensityCoefficient", m_clAlphaDensityCoeffGUI);
	store.StoreBool(cfgName + ".characteristicLines.surface.enableAlphaFade", m_clEnableAlphaFade);
	store.StoreBool(cfgName + ".characteristicLines.surface.enableAlphaShape", m_clEnableAlphaShape);
	store.StoreFloat(cfgName + ".characteristicLines.surface.alphaShapeCoefficient", m_clAlphaShapeCoeff);
	store.StoreBool(cfgName + ".characteristicLines.surface.enableAlphaCurvature", m_clEnableAlphaCurvature);
	store.StoreFloat(cfgName + ".characteristicLines.surface.alphaCurvatureCoefficient", m_clAlphaCurvatureCoeff);
	store.StoreBool(cfgName + ".characteristicLines.surface.enableLighting", m_clEnableSurfaceLighting);
	store.StoreInt(cfgName + ".characteristicLines.surface.numTimeSurfaces", m_numTimeSurfaces);
}

void ParticleTracer::LoadConfig(SettingsStorage &store, std::string id)
{
	std::string cfgName = "particletracer[" + id + "]";

	store.GetBool(cfgName + ".enableParticles", m_enableParticles);

	store.GetFloat(cfgName + ".maxParticleLifetime", m_maxParticleLifetime);
	store.GetFloat(cfgName + ".particleSize", m_particleSize);

	store.GetBool(cfgName + ".colorByMetric", m_colorParticlesByMetricGUI);
	store.GetFloat4(cfgName + ".particleColor", &m_particleColorGUI.x);
	store.GetFloat3(cfgName + ".spawnRegion.center", &m_spawnRegionBox.center.x);
	store.GetFloat3(cfgName + ".spawnRegion.size", &m_spawnRegionBox.size.x);
	m_spawnRegionBox.SetChanged();

	int clMode = m_clMode, clRenderMode = m_clRenderMode, clLength = m_clLength, seedingMode = m_seedingMode;
	store.GetInt(cfgName + ".seedingMode", seedingMode);
	m_seedingModeGUI = (SeedingMode)seedingMode;
	store.GetInt(cfgName + ".characteristicLines.type", clMode);
	store.GetInt(cfgName + ".characteristicLines.renderMode", clRenderMode);
	store.GetInt(cfgName + ".characteristicLines.length", clLength);
	store.GetFloat(cfgName + ".characteristicLines.width", m_clWidthGUI);
	store.GetFloat(cfgName + ".characteristicLines.stepsize", m_clStepsizeGUI);
	store.GetFloat3(cfgName + ".characteristicLines.ribbonOrientation", &m_clRibbonBaseOrientation.x);
	store.GetFloat(cfgName + ".characteristicLines.reseedInterval", m_clReseedIntervalGUI);

	store.GetBool(cfgName + ".characteristicLines.surface.enableAlphaDensity", m_clEnableAlphaDensityGUI);
	store.GetFloat(cfgName + ".characteristicLines.surface.alphaDensityCoefficient", m_clAlphaDensityCoeffGUI);
	store.GetBool(cfgName + ".characteristicLines.surface.enableAlphaFade", m_clEnableAlphaFadeGUI);
	store.GetBool(cfgName + ".characteristicLines.surface.enableAlphaShape", m_clEnableAlphaShapeGUI);
	store.GetFloat(cfgName + ".characteristicLines.surface.alphaShapeCoefficient", m_clAlphaShapeCoeffGUI);
	store.GetBool(cfgName + ".characteristicLines.surface.enableLighting", m_clEnableSurfaceLighting);
	store.GetBool(cfgName + ".characteristicLines.surface.enableAlphaCurvature", m_clEnableAlphaCurvatureGUI);
	store.GetFloat(cfgName + ".characteristicLines.surface.alphaCurvatureCoefficient", m_clAlphaCurvatureCoeffGUI);
	store.GetInt(cfgName + ".characteristicLines.surface.numTimeSurfaces", m_numTimeSurfacesGUI);

	m_clModeGUI = (CharacteristicLineMode)clMode;
	m_clRenderModeGUI = (CharacteristicLineRenderMode)clRenderMode;
	m_clLength = m_clLengthGUI = clLength;
	m_clMode = CL_DISABLED; // set to disabled to enforce update

	int np = m_numParticles;
	m_numParticles = 0;
	store.GetInt(cfgName + ".numParticles", np);
	SetNumParticles(np);
	m_numParticlesGUI = np;
}

// sets up the shader variables
void ParticleTracer::PrepareRenderEnvironment(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	RenderTransformations modelMtcs = sceneMtcs.PremultModelMatrix(m_modelTransform);

	XMFLOAT4X4 mModelWorldViewProj, mWorldViewProjInv;

	XMMATRIX mWorldViewInv =	modelMtcs.viewInv;//mWorldViewProj * XMMatrixInverse(nullptr, cam->GetProjMatrix()));
	XMVECTOR	camRight = (mWorldViewInv.r[0] ),
				camUp = (mWorldViewInv.r[2] ),	//not sure why up and forward are switched but it works - do not touch ;)
				camForward = (mWorldViewInv.r[1]  );

	XMMATRIX toSpawnCenter = XMMatrixTranslation(m_spawnRegionBox.center.x - 0.5f*m_spawnRegionBox.size.x, 
		m_spawnRegionBox.center.y - 0.5f*m_spawnRegionBox.size.y, 
		m_spawnRegionBox.center.z - 0.5f*m_spawnRegionBox.size.z);

	m_spawnRegionBox.texToNDCSpace = modelMtcs.modelWorldViewProj;

	pCamRightEV->SetFloatVector(camRight.m128_f32);
	pCamUpEV->SetFloatVector(camUp.m128_f32);
	pCamForwardEV->SetFloatVector(camForward.m128_f32);

	pParticleSizeEV->SetFloat(m_particleSize);
	pParticleColorEV->SetFloatVector(&m_particleColor.x);
	pColorParticlesEV->SetBool(m_colorParticlesByMetric);

	pCLTimeSurfaceOffset->SetFloatVector(&m_timeSurfaceOffsetDirection.x);
	pCLNumTimeSurfaces->SetInt(m_numTimeSurfaces);
	pCLEnableTimeSurfaces->SetBool(m_seedingMode == SM_SURFACE);

	XMStoreFloat4x4(&mModelWorldViewProj, m_modelTransform * modelMtcs.worldViewProj);
	pModelWorldViewProjEV->SetMatrix((float*)mModelWorldViewProj.m);

	XMMATRIX mViewProj_ = modelMtcs.view * modelMtcs.proj;
	XMFLOAT4X4 mViewProj;
	XMStoreFloat4x4(&mViewProj, mViewProj_);
	pViewProjEV->SetMatrix((float*)mViewProj.m);

	XMMATRIX mViewProjInv = XMMatrixInverse(nullptr, mViewProj_);
	XMMATRIX mModelWorld = m_modelTransform * modelMtcs.worldViewProj * mViewProjInv;
	XMMATRIX mModelWorldInv = XMMatrixInverse(nullptr, mModelWorld);
	pLightPosEV->SetFloatVector(modelMtcs.camInWorld.m128_f32);

	XMFLOAT4X4 mModelWorld_, mModelWorldInvTransp;
	XMStoreFloat4x4(&mModelWorld_, mModelWorld);
	XMStoreFloat4x4(&mModelWorldInvTransp, XMMatrixTranspose(mModelWorld));
	pModelWorldEV->SetMatrix((float*)mModelWorld_.m);
	pModelWorldInvTranspEV->SetMatrix((float*)mModelWorldInvTransp.m);
	
	XMFLOAT4X4 mModelWorldView;
	XMStoreFloat4x4(&mModelWorldView, m_modelTransform * modelMtcs.worldViewProj * XMMatrixInverse(nullptr, modelMtcs.proj));
	pModelWorldViewEV->SetMatrix((float*)mModelWorldView.m);

	pLightColorEV->SetFloatVector(&g_globals.lightColor.x);
	pAmbientEV->SetFloat(g_globals.mat_ambient);
	pDiffuseEV->SetFloat(g_globals.mat_diffuse);
	pSpecularEV->SetFloat(g_globals.mat_specular);
	pSpecularExpEV->SetFloat(g_globals.mat_specular_exp);
}

HRESULT ParticleTracer::RenderInstance(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	PrepareRenderEnvironment(pd3dImmediateContext, sceneMtcs);

	pParticleBufferEV->SetResource(m_pParticleBufferSRV);
	if(m_colorParticlesByMetric) {
		pScalarVolumeEV->SetResource(m_volumeData.GetScalarMetricVolume()->GetInterpolatedTextureSRV());
		pTransferFunctionEV->SetResource(g_transferFunctionEditor->getSRV());
	}

	pd3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	pd3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	if(m_enableParticles) {
		if(m_colorParticlesByMetric) {
			pPasses[PASS_RENDER_COLORED]->Apply(0, pd3dImmediateContext);
		}
		else
			pPasses[PASS_RENDER]->Apply(0, pd3dImmediateContext);
			
		pd3dImmediateContext->Draw(m_numParticles, 0);
	}

	if(m_clMode == CL_STREAMLINES || m_clMode == CL_STREAKLINES) {
		pCLLengthEV->SetInt(m_clLength);
		pCLRibbonBaseOrientationEV->SetFloatVector(&m_clRibbonBaseOrientation.x);
		pCLWidthEV->SetFloat(m_clWidth);
		pCharacteristicLineBufferEV->SetResource(m_pCharacteristicLineBufferSRV);

		if(m_clRenderMode == CLRM_SURFACE) {
			if(m_surfaceWireframe) {
				pCLEnableSurfaceLighting->SetBool(false);
				pCLAlphaDensityCoeff->SetFloat(m_clAlphaDensityCoeff);
				pCLEnableAlphaDensity->SetBool(m_clEnableAlphaDensity);
				pTrianglePropertiesEV->SetResource(m_pTrianglePropertiesBufferSRV);
				pPasses[PASS_RENDER_SURFACE_WIREFRAME]->Apply(0, pd3dImmediateContext);
				pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
				pd3dImmediateContext->Draw((m_numParticles-1) * m_clLength, 0);
			}
		}
		else {
			if(m_clRenderMode == CLRM_LINEPRIMITIVE)
				pPasses[PASS_RENDER_CL_LINE]->Apply(0, pd3dImmediateContext);
			else if(m_clRenderMode == CLRM_RIBBON)
				pPasses[PASS_RENDER_CL_RIBBON]->Apply(0, pd3dImmediateContext);
			else {
				pPasses[PASS_RENDER_TUBE_CYLINDERS]->Apply(0, pd3dImmediateContext); 
			}

			pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
			pd3dImmediateContext->Draw(m_numParticles * (m_clLength + 1), 0);
		}
	}

	//remove the mappings from the shader inputs
	pParticleBufferEV->SetResource(nullptr);
	pCharacteristicLineBufferEV->SetResource(nullptr);
	pScalarVolumeEV->SetResource(nullptr);
	pTrianglePropertiesEV->SetResource(nullptr);

	for(int i=0; i < NUM_PASSES; i++) {
		if(i == PASS_RENDER_SURFACE) continue;
		pPasses[i]->Apply(0, pd3dImmediateContext);
	}

	return S_OK;
}

HRESULT ParticleTracer::RenderTransparencyInstance(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	//ID3D11RenderTargetView * pRTV = DXUTGetD3D11RenderTargetView();
	//ID3D11DepthStencilView * pDSV = DXUTGetD3D11DepthStencilView();
	//ID3D11RenderTargetView * pRTVNull = nullptr;
	//pd3dImmediateContext->OMSetRenderTargets(0, &pRTVNull, pDSV);

	PrepareRenderEnvironment(pd3dImmediateContext, sceneMtcs);
	transparencyEnvironment.BeginTransparency();

	pCLEnableSurfaceLighting->SetBool(m_clEnableSurfaceLighting);

	pParticleBufferEV->SetResource(m_pParticleBufferSRV);
	if(m_colorParticlesByMetric) {
		pScalarVolumeEV->SetResource(m_volumeData.GetScalarMetricVolume()->GetInterpolatedTextureSRV());
		pTransferFunctionEV->SetResource(g_transferFunctionEditor->getSRV());
	}

	pd3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	pd3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	if(m_clMode == CL_STREAMLINES || m_clMode == CL_STREAKLINES) {
		pCLLengthEV->SetInt(m_clLength);
		pCLRibbonBaseOrientationEV->SetFloatVector(&m_clRibbonBaseOrientation.x);
		pCLWidthEV->SetFloat(m_clWidth);
		pCharacteristicLineBufferEV->SetResource(m_pCharacteristicLineBufferSRV);
		if(m_clRenderMode == CLRM_SURFACE) {

			pCLEnableAlphaDensity->SetBool(m_clEnableAlphaDensity);
			pCLAlphaDensityCoeff->SetFloat(m_clAlphaDensityCoeff);
			pTrianglePropertiesEV->SetResource(m_pTrianglePropertiesBufferSRV);
			pPasses[PASS_RENDER_SURFACE]->Apply(0, pd3dImmediateContext);
			pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
			pd3dImmediateContext->Draw((m_numParticles-1) * m_clLength, 0);
		}
	
	}

	//remove the mappings from the shader inputs
	pParticleBufferEV->SetResource(nullptr);
	pCharacteristicLineBufferEV->SetResource(nullptr);
	pScalarVolumeEV->SetResource(nullptr);
	pTrianglePropertiesEV->SetResource(nullptr);
	pTransferFunctionEV->SetResource(nullptr);

	transparencyEnvironment.EndTransparency(pd3dImmediateContext, pPasses[PASS_RENDER_SURFACE]);

	return S_OK;
}

void ParticleTracer::FrameMoveInstance(double dTime, float fElapsedTime, float fElapsedLogicTime) {
	static std::default_random_engine g;
	static std::uniform_real_distribution<float> dist(0, 1);

	// Constrain spawn box to the region
	m_spawnRegionBox.size.x = std::min(1.f, std::max(0.02f/m_volumeData.GetBoundingBox().x, m_spawnRegionBox.size.x));
	m_spawnRegionBox.size.y = std::min(1.f, std::max(0.02f/m_volumeData.GetBoundingBox().y, m_spawnRegionBox.size.y));
	m_spawnRegionBox.size.z = std::min(1.f, std::max(0.02f/m_volumeData.GetBoundingBox().z, m_spawnRegionBox.size.z));
	m_spawnRegionBox.center.x = std::min(1.f - 0.5f * m_spawnRegionBox.size.x, std::max(0.5f * m_spawnRegionBox.size.x, m_spawnRegionBox.center.x));
	m_spawnRegionBox.center.y = std::min(1.f - 0.5f * m_spawnRegionBox.size.y, std::max(0.5f * m_spawnRegionBox.size.y, m_spawnRegionBox.center.y));
	m_spawnRegionBox.center.z = std::min(1.f - 0.5f * m_spawnRegionBox.size.z, std::max(0.5f * m_spawnRegionBox.size.z, m_spawnRegionBox.center.z));

	// update the particles
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);

	pTimeDeltaEV->SetFloat(fElapsedLogicTime);
	pTimestepTEV->SetFloat(m_volumeData.GetCurrentTimestepT());
	pVelocityScalingEV->SetFloatVector(&m_velocityScaling.x);
	pMaxParticleLifetimeEV->SetFloat(m_maxParticleLifetime);
	pNumParticlesEV->SetInt(m_numParticles);
	pColorParticlesEV->SetBool(m_colorParticlesByMetric);
	pParticleColorEV->SetFloatVector(&m_particleColor.x);

	XMFLOAT3 spawnRegionMin(m_spawnRegionBox.center.x - 0.5f*m_spawnRegionBox.size.x, 
		m_spawnRegionBox.center.y - 0.5f*m_spawnRegionBox.size.y, 
		m_spawnRegionBox.center.z - 0.5f*m_spawnRegionBox.size.z);
	XMFLOAT3 spawnRegionMax(m_spawnRegionBox.center.x + 0.5f*m_spawnRegionBox.size.x, 
		m_spawnRegionBox.center.y + 0.5f*m_spawnRegionBox.size.y, 
		m_spawnRegionBox.center.z + 0.5f*m_spawnRegionBox.size.z);
	pSpawnRegionMinEV->SetFloatVector(&spawnRegionMin.x);
	pSpawnRegionMaxEV->SetFloatVector(&spawnRegionMax.x);

	float rnd[3] = {dist(g), dist(g), dist(g)};
	pRndEV->SetFloatVector(rnd);
	
	pTexVolume0EV->SetResource(m_volumeData.GetTexture0SRV());
	pTexVolume1EV->SetResource(m_volumeData.GetTexture1SRV());
	pParticleBufferRWEV->SetUnorderedAccessView(m_pParticleBufferUAV);
	
	if(m_enableParticles) {
		pPasses[PASS_ADVECT]->Apply(0, pContext);

		uint32_t gx = uint32_t(ceilf(m_numParticles/(256.f))),// ceilf(m_numParticles / (32.f*2.f*2.f)),
			gy = 1,
			gz = 1;

		pContext->Dispatch(gx, gy, gz);
	
		pParticleBufferRWEV->SetUnorderedAccessView(nullptr);
		pPasses[PASS_ADVECT]->Apply(0, pContext);
	}

	//we don't update streamlines if paused
	bool needsRecompute = CLRequireRecompute();
	if( needsRecompute ) {
		switch (m_clMode) {
		case CL_STREAMLINES:
			ComputeStreamlines(pContext);
			break;
		case CL_STREAKLINES:
			ComputeStreaklines(pContext, fElapsedLogicTime);
			break;
		case CL_PATHLINES:
			std::cout << "Pathlines not implemented!" << std::endl;
			break;
		case CL_DISABLED:
		default:
			//release as early as we can to save GPU memory
			// we save ourselves an extra callback for m_clMode
			SAFE_RELEASE(m_pCharacteristicLineBuffer);
			SAFE_RELEASE(m_pCharacteristicLineBufferSRV);
			SAFE_RELEASE(m_pCharacteristicLineBufferUAV);
		}
	}
	// we need to recompute surface properties every frame even if we are paused if we have enabled alphaDensity since
	// it is view-dependent and we can't detect camera changes lazily (yet)
	if(m_clRenderMode == CLRM_SURFACE && (needsRecompute || (m_clMode == CL_STREAMLINES && m_clEnableAlphaDensity))) {
		ComputeTriangleProperties(pContext);
	}

	SAFE_RELEASE(pContext);
}

void ParticleTracer::SetNumParticles(unsigned int numParticles)
{
	if(m_numParticles == numParticles)
		return;
	m_numParticles = numParticles;
	assert(m_numParticles > 0);

	SAFE_RELEASE(m_pParticleBuffer);
	SAFE_RELEASE(m_pParticleBufferSRV);
	SAFE_RELEASE(m_pParticleBufferUAV);

	HRESULT hr = CreateParticleGPUBuffer();
	assert(SUCCEEDED(hr));

	SAFE_RELEASE(m_pCharacteristicLineBuffer);
	SAFE_RELEASE(m_pCharacteristicLineBufferSRV);
	SAFE_RELEASE(m_pCharacteristicLineBufferUAV);

	SAFE_RELEASE(m_pTrianglePropertiesBuffer);
	SAFE_RELEASE(m_pTrianglePropertiesBufferSRV);
	SAFE_RELEASE(m_pTrianglePropertiesBufferUAV);

	if(m_clMode != CL_DISABLED) {
		
		CreateCharacteristicLineGPUBuffer();
		m_clMode = CL_DISABLED; //set to disabled to enforce an update
	}
}

void ParticleTracer::SetCLLength(unsigned int clLength) 
{
	assert(m_clLength != 0);

	if(m_clLength == clLength)
		return;
	m_clLength = clLength;
	
	SAFE_RELEASE(m_pCharacteristicLineBuffer);
	SAFE_RELEASE(m_pCharacteristicLineBufferSRV);
	SAFE_RELEASE(m_pCharacteristicLineBufferUAV);

	SAFE_RELEASE(m_pTrianglePropertiesBuffer);
	SAFE_RELEASE(m_pTrianglePropertiesBufferSRV);
	SAFE_RELEASE(m_pTrianglePropertiesBufferUAV);
}

HRESULT ParticleTracer::CreateParticleGPUBuffer()
{
	HRESULT hr;
	assert(pd3dDevice);

	if(m_pParticleBuffer)
		return S_OK;

	std::cout << "Creating Particle Buffers..." << std::endl;

	// create and fill the particle buffer on the cpu with meaningful data
	struct ParticleDescriptor * particleBuffer = new struct ParticleDescriptor[m_numParticles];
	std::default_random_engine g;
	std::uniform_real_distribution<float>	dist(0.0, 1.0);

	int longestAxis = m_spawnRegionBox.longestAxis;
	int largestFaceAxis = m_spawnRegionBox.largestFaceAxis;
	
	int surfaceAxis0, surfaceAxis1;

	switch(largestFaceAxis) {
	case 0: //x axis ->yz plane
		surfaceAxis0 = 1;
		surfaceAxis1 = 2;
		m_timeSurfaceOffsetDirection = XMFLOAT3(0, 0, 1.f/(m_clLength/m_numTimeSurfaces - 1.f));
		break;
	case 1:
		surfaceAxis0 = 0;
		surfaceAxis1 = 2;
		m_timeSurfaceOffsetDirection = XMFLOAT3(0, 0, 1.f/(m_clLength/m_numTimeSurfaces - 1.f));
		break;
	case 2:
		surfaceAxis0 = 0;
		surfaceAxis1 = 1;
		m_timeSurfaceOffsetDirection = XMFLOAT3(0, 1.f/(m_clLength/m_numTimeSurfaces - 1.f), 0);
		break;
	}
	if(m_seedingMode != SM_SURFACE)
		m_timeSurfaceOffsetDirection = XMFLOAT3(0,0,0); 

	for(unsigned int i=0;i < m_numParticles; i++) {
		particleBuffer[i].age = m_maxParticleLifetime * dist(g);
		particleBuffer[i].ageSeed = 0.5f*dist(g);

		//for surfaces, we seed along an axis
		if(m_seedingMode == SM_LINE) {
			particleBuffer[i].seedPos[0] = 0.5;
			particleBuffer[i].seedPos[1] = 0.5;
			particleBuffer[i].seedPos[2] = 0.5;
			particleBuffer[i].seedPos[longestAxis] = (float)i / (m_numParticles-1);
		}
		else if(m_seedingMode == SM_RANDOM) {
			particleBuffer[i].seedPos[0] = dist(g);
			particleBuffer[i].seedPos[1] = dist(g);
			particleBuffer[i].seedPos[2] = dist(g);
		}
		else if(m_seedingMode == SM_SURFACE) {
			particleBuffer[i].seedPos[surfaceAxis0] = (float)i / (m_numParticles - 1.f);
			particleBuffer[i].seedPos[surfaceAxis1] = 0;
			particleBuffer[i].seedPos[largestFaceAxis] = 0.5;
		}
		particleBuffer[i].pos[0] = (particleBuffer[i].seedPos[0] - .5f) * m_spawnRegionBox.size.x + m_spawnRegionBox.center.x;
		particleBuffer[i].pos[1] = (particleBuffer[i].seedPos[1] - .5f) * m_spawnRegionBox.size.y + m_spawnRegionBox.center.y;
		particleBuffer[i].pos[2] = (particleBuffer[i].seedPos[2] - .5f) * m_spawnRegionBox.size.z + m_spawnRegionBox.center.z;
	}

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.StructureByteStride = sizeof(struct ParticleDescriptor);
	desc.ByteWidth = m_numParticles * desc.StructureByteStride;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	D3D11_SUBRESOURCE_DATA initData;
	ZeroMemory(&initData, sizeof(initData));
	initData.pSysMem = particleBuffer;
	initData.SysMemPitch = 0;
	initData.SysMemSlicePitch = 0;

	V_RETURN(pd3dDevice->CreateBuffer(&desc, &initData, &m_pParticleBuffer));

	delete particleBuffer;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D_SRV_DIMENSION_BUFFEREX;
	srvDesc.BufferEx.FirstElement = 0;
	srvDesc.BufferEx.NumElements = m_numParticles;

	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pParticleBuffer, &srvDesc, &m_pParticleBufferSRV));


	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = m_numParticles;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pParticleBuffer, &uavDesc, &m_pParticleBufferUAV));

	return S_OK;
}

HRESULT ParticleTracer::CreateTrianglePropertiesBuffer()
{
	HRESULT hr;
	assert(pd3dDevice);

	if(m_pTrianglePropertiesBuffer)
		return S_OK;

	std::cout << "Creating Triangle GPU Buffers..." << std::endl;

	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.StructureByteStride = sizeof(struct TriangleProperties);
	desc.ByteWidth = (m_numParticles-1) * 2 * (m_clLength) * desc.StructureByteStride;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	V_RETURN(pd3dDevice->CreateBuffer(&desc, nullptr, &m_pTrianglePropertiesBuffer));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D_SRV_DIMENSION_BUFFEREX;
	srvDesc.BufferEx.FirstElement = 0;
	srvDesc.BufferEx.NumElements = (m_numParticles-1) * 2 * (m_clLength);
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pTrianglePropertiesBuffer, &srvDesc, &m_pTrianglePropertiesBufferSRV));


	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = (m_numParticles-1) * 2 * (m_clLength);
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pTrianglePropertiesBuffer, &uavDesc, &m_pTrianglePropertiesBufferUAV));

	return S_OK;
}

HRESULT ParticleTracer::CreateCharacteristicLineGPUBuffer()
{
	HRESULT hr;
	assert(pd3dDevice);

	if(m_pCharacteristicLineBuffer)
		return S_OK;

	std::cout << "Creating CL GPU Buffers..." << std::endl;

	//create the scalar texture
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.StructureByteStride = sizeof(struct CharacteristicLineVertex);
	desc.ByteWidth = m_numParticles * m_clLength * desc.StructureByteStride;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	V_RETURN(pd3dDevice->CreateBuffer(&desc, nullptr, &m_pCharacteristicLineBuffer));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D_SRV_DIMENSION_BUFFEREX;
	srvDesc.BufferEx.FirstElement = 0;
	srvDesc.BufferEx.NumElements = m_numParticles * m_clLength;

	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pCharacteristicLineBuffer, &srvDesc, &m_pCharacteristicLineBufferSRV));


	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = m_numParticles * m_clLength;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pCharacteristicLineBuffer, &uavDesc, &m_pCharacteristicLineBufferUAV));

	InitCharacteristicLineBuffer();

	return S_OK;
}

void ParticleTracer::ComputeStreamlines(ID3D11DeviceContext * pContext)
{
	//std::cout << "Computing streamlines for probe [" << m_probeIndex << "]" << std::endl;

	if(!m_pCharacteristicLineBuffer)
		CreateCharacteristicLineGPUBuffer();

	pCLLengthEV->SetInt(m_clLength);
	pCLStepsizeEV->SetFloat(m_clStepsize);
	pVelocityScalingEV->SetFloatVector(&m_velocityScaling.x);
	pNumParticlesEV->SetInt(m_numParticles);
	pColorParticlesEV->SetBool(m_colorParticlesByMetric);

	XMFLOAT3 spawnRegionMin(m_spawnRegionBox.center.x - 0.5f*m_spawnRegionBox.size.x, 
		m_spawnRegionBox.center.y - 0.5f*m_spawnRegionBox.size.y, 
		m_spawnRegionBox.center.z - 0.5f*m_spawnRegionBox.size.z);
	XMFLOAT3 spawnRegionMax(m_spawnRegionBox.center.x + 0.5f*m_spawnRegionBox.size.x, 
		m_spawnRegionBox.center.y + 0.5f*m_spawnRegionBox.size.y, 
		m_spawnRegionBox.center.z + 0.5f*m_spawnRegionBox.size.z);
	pSpawnRegionMinEV->SetFloatVector(&spawnRegionMin.x);
	pSpawnRegionMaxEV->SetFloatVector(&spawnRegionMax.x);
	
	pMaxParticleLifetimeEV->SetFloat((m_clLength - 1) * m_clStepsize);
	
	pTexVolume0EV->SetResource(m_volumeData.GetTexture0SRV());
	pTexVolume1EV->SetResource(m_volumeData.GetTexture1SRV());
	pParticleBufferEV->SetResource(m_pParticleBufferSRV);
	pCharacteristicLineBufferRWEV->SetUnorderedAccessView(m_pCharacteristicLineBufferUAV);

	pCLTimeSurfaceOffset->SetFloatVector(&m_timeSurfaceOffsetDirection.x);
	pCLNumTimeSurfaces->SetInt(m_numTimeSurfaces);
	pCLEnableTimeSurfaces->SetBool(m_seedingMode == SM_SURFACE);

	if(m_colorParticlesByMetric) {
		pScalarVolumeEV->SetResource(m_volumeData.GetScalarMetricVolume()->GetTexture0SRV());
		pTransferFunctionEV->SetResource(g_transferFunctionEditor->getSRV());
	}

	if(m_clRenderMode != CLRM_RIBBON)
		pPasses[PASS_COMPUTE_STREAMLINE]->Apply(0, pContext);
	else
		pPasses[PASS_COMPUTE_STREAMRIBBON]->Apply(0, pContext);
	
	uint32_t gx = uint32_t(ceilf(m_numParticles/(256.f))),
		gy = 1,
		gz = 1;

	pContext->Dispatch(gx, gy, gz);

	pCharacteristicLineBufferRWEV->SetUnorderedAccessView(nullptr);
	pParticleBufferEV->SetResource(nullptr);
	pScalarVolumeEV->SetResource(nullptr);
	pTransferFunctionEV->SetResource(nullptr);
	if(m_clRenderMode == CLRM_LINEPRIMITIVE)
		pPasses[PASS_COMPUTE_STREAMLINE]->Apply(0, pContext);
	else
		pPasses[PASS_COMPUTE_STREAMRIBBON]->Apply(0, pContext);

	m_volumeDataChanged = false;
}

void ParticleTracer::ComputeStreaklines(ID3D11DeviceContext * pContext, float fElapsedTime)
{
	//std::cout << "Computing streaklines for probe [" << m_probeIndex << "]" << std::endl;

	if(!m_pCharacteristicLineBuffer)
		CreateCharacteristicLineGPUBuffer();

	pCLLengthEV->SetInt(m_clLength);
	pCLStepsizeEV->SetFloat(m_clStepsize);
	pVelocityScalingEV->SetFloatVector(&m_velocityScaling.x);
	pCLReseedIntervalEV->SetFloat(m_clReseedInterval);	
	pTimeDeltaEV->SetFloat(fElapsedTime);
	if(m_seedingMode == SM_SURFACE)
		pMaxParticleLifetimeEV->SetFloat(m_maxParticleLifetime);
	else
		pMaxParticleLifetimeEV->SetFloat((m_clLength - 1) * m_clReseedInterval);
	pNumParticlesEV->SetInt(m_numParticles);
	pColorParticlesEV->SetBool(m_colorParticlesByMetric);
	
	XMFLOAT3 spawnRegionMin(m_spawnRegionBox.center.x - 0.5f*m_spawnRegionBox.size.x, 
		m_spawnRegionBox.center.y - 0.5f*m_spawnRegionBox.size.y, 
		m_spawnRegionBox.center.z - 0.5f*m_spawnRegionBox.size.z);
	XMFLOAT3 spawnRegionMax(m_spawnRegionBox.center.x + 0.5f*m_spawnRegionBox.size.x, 
		m_spawnRegionBox.center.y + 0.5f*m_spawnRegionBox.size.y, 
		m_spawnRegionBox.center.z + 0.5f*m_spawnRegionBox.size.z);
	pSpawnRegionMinEV->SetFloatVector(&spawnRegionMin.x);
	pSpawnRegionMaxEV->SetFloatVector(&spawnRegionMax.x);
	
	pCLTimeSurfaceOffset->SetFloatVector(&m_timeSurfaceOffsetDirection.x);
	pCLNumTimeSurfaces->SetInt(m_numTimeSurfaces);
	pCLEnableTimeSurfaces->SetBool(m_seedingMode == SM_SURFACE);

	pTexVolume0EV->SetResource(m_volumeData.GetTexture0SRV());
	pTexVolume1EV->SetResource(m_volumeData.GetTexture1SRV());
	pParticleBufferEV->SetResource(m_pParticleBufferSRV);
	pCharacteristicLineBufferRWEV->SetUnorderedAccessView(m_pCharacteristicLineBufferUAV);

	if(m_colorParticlesByMetric) {
		pScalarVolumeEV->SetResource(m_volumeData.GetScalarMetricVolume()->GetTexture0SRV());
		pTransferFunctionEV->SetResource(g_transferFunctionEditor->getSRV());
	}


	if(m_clRenderMode != CLRM_RIBBON)
		pPasses[PASS_COMPUTE_STREAKLINE]->Apply(0, pContext);
	else
		pPasses[PASS_COMPUTE_STREAKRIBBON]->Apply(0, pContext);
	
	uint32_t gx = uint32_t(ceilf(m_numParticles/(256.f))),
		gy = m_clLength,
		gz = 1;

	pContext->Dispatch(gx, gy, gz);

	pCharacteristicLineBufferRWEV->SetUnorderedAccessView(nullptr);
	pParticleBufferEV->SetResource(nullptr);
	pScalarVolumeEV->SetResource(nullptr);
	pTransferFunctionEV->SetResource(nullptr);
	pPasses[PASS_COMPUTE_STREAKLINE]->Apply(0, pContext);

	m_volumeDataChanged = false;
}

void ParticleTracer::ComputeTriangleProperties(ID3D11DeviceContext * pContext)
{
	//std::cout << "Computing Triangle properties for probe [" << m_probeIndex << "]" << std::endl;

	if(!m_pCharacteristicLineBuffer)
		CreateCharacteristicLineGPUBuffer();
	if(!m_pTrianglePropertiesBuffer)
		CreateTrianglePropertiesBuffer();

	pCLLengthEV->SetInt(m_clLength);
	pCLStepsizeEV->SetFloat(m_clStepsize);
	pVelocityScalingEV->SetFloatVector(&m_velocityScaling.x);
	pCLReseedIntervalEV->SetFloat(m_clReseedInterval);	
	pNumParticlesEV->SetInt(m_numParticles);
	pColorParticlesEV->SetBool(m_colorParticlesByMetric);

	pLightPosEV->SetFloatVector(g_globals.currentlyActiveCamera->GetEyePt().m128_f32);

	pCLEnableAlphaCurvature->SetBool(m_clEnableAlphaCurvature);
	pCLEnableAlphaFade->SetBool(m_clEnableAlphaFade);
	pCLEnableAlphaShape->SetBool(m_clEnableAlphaShape);
	pCLEnableAlphaDensity->SetBool(m_clEnableAlphaDensity);

	if(m_clModeGUI == CL_STREAKLINES) {
		pMaxParticleLifetimeEV->SetFloat((m_clLength - 1) * m_clReseedInterval);
	}
	if(m_clMode == CL_STREAMLINES) {
		pMaxParticleLifetimeEV->SetFloat((m_clLength - 1) * m_clStepsize);
	}
	if(m_seedingMode == SM_SURFACE)
		pMaxParticleLifetimeEV->SetFloat(m_maxParticleLifetime);

	pCLAlphaDensityCoeff->SetFloat(m_clAlphaDensityCoeff);
	pCLAlphaCurvatureCoeff->SetFloat(m_clAlphaCurvatureCoeff);
	pCLAlphaShapeCoeff->SetFloat(m_clAlphaShapeCoeff);

	pCharacteristicLineBufferEV->SetResource(m_pCharacteristicLineBufferSRV);
	pTrianglePropertiesRWEV->SetUnorderedAccessView(m_pTrianglePropertiesBufferUAV);

	pPasses[PASS_COMPUTE_TRIANGLE_PROPERTIES]->Apply(0, pContext);
	
	uint32_t 
		gx = uint32_t(ceilf(2.f*(m_clLength)/32.f)),
		gy = uint32_t(ceilf((m_numParticles-1)/32.f)),
		gz = 1;

	pContext->Dispatch(gx, gy, gz);

	pCharacteristicLineBufferEV->SetResource(nullptr);
	pTrianglePropertiesRWEV->SetUnorderedAccessView(nullptr);
	pPasses[PASS_COMPUTE_TRIANGLE_PROPERTIES]->Apply(0, pContext);
	pCharacteristicLineBufferRWEV->SetUnorderedAccessView(m_pCharacteristicLineBufferUAV);
	pTrianglePropertiesEV->SetResource(m_pTrianglePropertiesBufferSRV);

	//now compute values for each vertex
	pPasses[PASS_COMPUTE_SURFACE_VERTEX_METRICS]->Apply(0, pContext);
		gx = uint32_t(ceilf(m_clLength/32.f)),
		gy = uint32_t(ceilf(m_numParticles/32.f)),
		gz = 1;
	pContext->Dispatch(gx, gy, gz);
	//std::cout << "Groups of [" << gx << "," << gy << "," << gz << "]"<< std::endl;

	pCharacteristicLineBufferRWEV->SetUnorderedAccessView(nullptr);
	pTrianglePropertiesEV->SetResource(nullptr);
	pPasses[PASS_COMPUTE_SURFACE_VERTEX_METRICS]->Apply(0, pContext);
}

void ParticleTracer::InitCharacteristicLineBuffer(void)
{
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);

	if(!m_pParticleBuffer)
		CreateParticleGPUBuffer();
	if(!m_pCharacteristicLineBuffer)
		CreateCharacteristicLineGPUBuffer();
	

	if(m_seedingMode == SM_SURFACE)
		pMaxParticleLifetimeEV->SetFloat(m_maxParticleLifetime);
	else
		pMaxParticleLifetimeEV->SetFloat((m_clLength - 1) * m_clReseedInterval);

	pCLLengthEV->SetInt(m_clLength);
	pCLReseedIntervalEV->SetFloat(m_clReseedInterval);
	pVelocityScalingEV->SetFloatVector(&m_velocityScaling.x);
	pNumParticlesEV->SetInt(m_numParticles);
	pParticleColorEV->SetFloatVector(&m_particleColor.x);
	pCLTimeSurfaceOffset->SetFloatVector(&m_timeSurfaceOffsetDirection.x);
	pCLNumTimeSurfaces->SetInt(m_numTimeSurfaces);
	pCLEnableTimeSurfaces->SetBool(m_seedingMode == SM_SURFACE);

	XMFLOAT3 spawnRegionMin(m_spawnRegionBox.center.x - 0.5f*m_spawnRegionBox.size.x, 
		m_spawnRegionBox.center.y - 0.5f*m_spawnRegionBox.size.y, 
		m_spawnRegionBox.center.z - 0.5f*m_spawnRegionBox.size.z);
	XMFLOAT3 spawnRegionMax(m_spawnRegionBox.center.x + 0.5f*m_spawnRegionBox.size.x, 
		m_spawnRegionBox.center.y + 0.5f*m_spawnRegionBox.size.y, 
		m_spawnRegionBox.center.z + 0.5f*m_spawnRegionBox.size.z);
	pSpawnRegionMinEV->SetFloatVector(&spawnRegionMin.x);
	pSpawnRegionMaxEV->SetFloatVector(&spawnRegionMax.x);

	pCharacteristicLineBufferRWEV->SetUnorderedAccessView(m_pCharacteristicLineBufferUAV);
	pParticleBufferEV->SetResource(m_pParticleBufferSRV);

	pPasses[PASS_INIT_CHARACTERISTIC_LINE]->Apply(0, pContext);
	
	uint32_t gx = uint32_t(ceilf(m_numParticles/(256.f))),
		gy = uint32_t(m_clLength),
		gz = 1;

	pContext->Dispatch(gx, gy, gz);

	pCharacteristicLineBufferRWEV->SetUnorderedAccessView(nullptr);
	pParticleBufferEV->SetResource(nullptr);
	pPasses[PASS_INIT_CHARACTERISTIC_LINE]->Apply(0, pContext);

	SAFE_RELEASE(pContext);
}

bool ParticleTracer::CLRequireRecompute(void) {

	// for convenience, we disable some parameter combinations 
	if(m_clRenderModeGUI == CLRM_SURFACE && m_seedingModeGUI == SM_RANDOM)
		m_seedingModeGUI = SM_LINE;
	if(m_seedingModeGUI == SM_SURFACE) {
		m_clRenderModeGUI = CLRM_SURFACE;
		m_clModeGUI = CL_STREAKLINES;
	}

	float effectiveAlphaDensityGUI = m_clAlphaDensityCoeffGUI / (m_numParticles * 10000);
	if(m_clModeGUI == CL_STREAMLINES)
		effectiveAlphaDensityGUI *= m_clStepsize;
	if(m_clModeGUI == CL_STREAKLINES) 
		effectiveAlphaDensityGUI *= m_clReseedInterval;


	bool recompute =	m_numParticles != m_numParticlesGUI ||
						m_clMode != m_clModeGUI || 
						m_clMode == CL_STREAKLINES || 
						m_colorParticlesByMetric != m_colorParticlesByMetricGUI ||
						m_clLength != m_clLengthGUI || 
						m_clRenderMode != m_clRenderModeGUI ||
						(m_clRenderMode != CLRM_LINEPRIMITIVE && m_clWidth != m_clWidthGUI) ||
						m_clStepsize != m_clStepsizeGUI ||
						m_seedingMode != m_seedingModeGUI ||
						m_clReseedInterval != m_clReseedIntervalGUI ||

						m_clEnableAlphaDensity != m_clEnableAlphaDensityGUI ||
						m_clAlphaDensityCoeff != effectiveAlphaDensityGUI ||
						m_clEnableAlphaFade != m_clEnableAlphaFadeGUI ||
						m_clEnableAlphaShape != m_clEnableAlphaShapeGUI ||
						m_clAlphaShapeCoeff != m_clAlphaShapeCoeffGUI ||
						m_clEnableAlphaCurvature != m_clEnableAlphaCurvatureGUI ||
						m_clAlphaCurvatureCoeff != m_clAlphaCurvatureCoeffGUI ||
						((m_numTimeSurfaces != m_numTimeSurfacesGUI) && m_seedingMode == SM_SURFACE) ||

						!XMFLOAT3_EQUAL(m_clRibbonBaseOrientation, m_clRibbonBaseOrientationGUI) || 
						(!XMFLOAT4_EQUAL(m_particleColor, m_particleColorGUI) && !m_colorParticlesByMetric) ||
						m_volumeDataChanged || 
						m_spawnRegionBox.changed;

	if(m_numParticles != m_numParticlesGUI ||
		m_seedingMode != m_seedingModeGUI ||
		(m_seedingMode != SM_RANDOM && m_spawnRegionBox.axisSurfaceChanged)  ||
		((m_numTimeSurfaces != m_numTimeSurfacesGUI) && m_seedingMode == SM_SURFACE)
	  )
	{
		m_numTimeSurfaces = m_numTimeSurfacesGUI;
		m_seedingMode = m_seedingModeGUI;
		m_numParticles = 0;
		m_clLength = m_clLengthGUI;
		SetNumParticles(m_numParticlesGUI);
	}

	// handle changes to coloring policy to (un)register to metric changes
	if(m_colorParticlesByMetric != m_colorParticlesByMetricGUI) {
		if(m_colorParticlesByMetricGUI) {
			m_volumeData.GetScalarMetricVolume()->RegisterObserver(this);
			g_transferFunctionEditor->RegisterObserver(this);
		}
		else {
			m_volumeData.GetScalarMetricVolume()->UnregisterObserver(this);
			g_transferFunctionEditor->UnregisterObserver(this);
		}
		m_colorParticlesByMetric = m_colorParticlesByMetricGUI;
	}

	if( (m_clMode != m_clModeGUI && m_clModeGUI != CL_DISABLED) ||
		m_clReseedInterval != m_clReseedIntervalGUI  ||
		(!XMFLOAT4_EQUAL(m_particleColor, m_particleColorGUI) && !m_colorParticlesByMetric) 
		//|| (m_clModeGUI == CL_STREAKLINES && m_clRenderModeGUI == CLRM_RIBBON) //"streak-ribbons" and some parameter changes
		) 
	{
		m_seedingMode = m_seedingModeGUI;
		m_particleColor  = m_particleColorGUI;
		m_clMode = m_clModeGUI;
		m_clReseedInterval = m_clReseedIntervalGUI;
		InitCharacteristicLineBuffer();
	}

	m_seedingMode = m_seedingModeGUI;
	
	m_clEnableAlphaDensity = m_clEnableAlphaDensityGUI;
	m_clAlphaDensityCoeff = effectiveAlphaDensityGUI;
	m_clEnableAlphaFade = m_clEnableAlphaFadeGUI;
	m_clEnableAlphaShape = m_clEnableAlphaShapeGUI;
	m_clAlphaShapeCoeff = m_clAlphaShapeCoeffGUI;
	m_clEnableAlphaCurvature = m_clEnableAlphaCurvatureGUI;
	m_clAlphaCurvatureCoeff = m_clAlphaCurvatureCoeffGUI;
	
	SetCLLength(m_clLengthGUI);
	m_particleColor  = m_particleColorGUI;
	m_clMode = m_clModeGUI;
	m_clWidth = m_clWidthGUI;
	m_clRenderMode = m_clRenderModeGUI;
	m_clStepsize = m_clStepsizeGUI;
	m_clRibbonBaseOrientation = m_clRibbonBaseOrientationGUI;
	return recompute && m_clMode != CL_DISABLED;
}

void ParticleTracer::notify(Observable * observable)
{
	m_volumeDataChanged = true;
}

// reads the particle buffer from the GPU and dumps it to stdout
void ParticleTracer::DumpParticleInfo(unsigned int particles)
{

	HRESULT hr;
	unsigned int numParticles = std::min(particles, m_numParticles);
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.StructureByteStride = sizeof(struct ParticleDescriptor);
	desc.ByteWidth = numParticles * desc.StructureByteStride;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	ID3D11Buffer * pDLBuffer;
	if(!SUCCEEDED(pd3dDevice->CreateBuffer(&desc, nullptr, &pDLBuffer)))
	{
		std::cout << "Buffer Creation unsuccessful!" << std::endl;
		return;
	}

	//read back buffer
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);

	pContext->CopyResource(pDLBuffer, m_pParticleBuffer);
	
	D3D11_MAPPED_SUBRESOURCE ms;
	
	hr = pContext->Map(pDLBuffer, NULL, D3D11_MAP_READ, NULL, &ms);
	if(!SUCCEEDED(hr)) {
		std::cout << "Mapping Unsuccessful!" << std::endl;
		return;
	}
	
	struct ParticleDescriptor * par = static_cast<struct ParticleDescriptor*>(ms.pData);
	//dump particles to console
	
	std::cout << "Dumping first " << numParticles << " particles..." << std::endl;
	for(unsigned int i=0; i < numParticles; i++) {
		std::cout << "Pos: " << par[i].pos[0] << " " << par[i].pos[1] << " " << par[i].pos[2] << " | Age: " << par[i].age << " | Seed: " << 
			par[i].seedPos[0] << " " << par[i].seedPos[1] << " " << par[i].seedPos[2] << std::endl;
	}

	pContext->Unmap(pDLBuffer, NULL);
	SAFE_RELEASE(pContext);
	SAFE_RELEASE(pDLBuffer);
}

void ParticleTracer::DumpStreaklineInfo(unsigned int particles)
{

	HRESULT hr;
	unsigned int numParticles = std::min(particles, m_numParticles);
	D3D11_BUFFER_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.StructureByteStride = sizeof(struct CharacteristicLineVertex);
	desc.ByteWidth = m_numParticles * m_clLength * desc.StructureByteStride;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

	ID3D11Buffer * pDLBuffer;
	if(!SUCCEEDED(pd3dDevice->CreateBuffer(&desc, nullptr, &pDLBuffer)))
	{
		std::cout << "Buffer Creation unsuccessful!" << std::endl;
		return;
	}

	//read back buffer
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);

	pContext->CopyResource(pDLBuffer, m_pCharacteristicLineBuffer);
	
	D3D11_MAPPED_SUBRESOURCE ms;
	
	hr = pContext->Map(pDLBuffer, NULL, D3D11_MAP_READ, NULL, &ms);
	if(!SUCCEEDED(hr)) {
		std::cout << "Mapping Unsuccessful!" << std::endl;
		return;
	}
	
	struct CharacteristicLineVertex * par = static_cast<struct CharacteristicLineVertex*>(ms.pData);
	//dump particles to console
	
	std::cout << "Dumping first " << numParticles << " particles..." << std::endl;
	for(unsigned int i=0; i < numParticles; i++) {
		for(unsigned int j=0; j < m_clLength; j++) {
			std::cout << "Pos: " << par->pos[0] << " " << par->pos[1] << " " << par->pos[2] << " | Age: " << par->age << " ++ ";
			par++;
		}
		std::cout << std::endl;
	}

	pContext->Unmap(pDLBuffer, NULL);
	SAFE_RELEASE(pContext);
	SAFE_RELEASE(pDLBuffer);
}