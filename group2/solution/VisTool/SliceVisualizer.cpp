#include "SliceVisualizer.h"

#include "SimpleMesh.h"

#include "util/util.h"
#include "Globals.h"

#include <iostream>
#include <random>
#include <sstream>
#include <float.h>

ID3DX11Effect			* SliceVisualizer::pEffect = nullptr;
ID3DX11EffectTechnique	* SliceVisualizer::pTechnique = nullptr;
ID3D11Device			* SliceVisualizer::pd3dDevice = nullptr;
ID3DX11EffectPass		* SliceVisualizer::pPasses[NUM_PASSES];
TwBar					* SliceVisualizer::pParametersBar = nullptr;
TwType					SliceVisualizer::sliceVisualizerType;

// pass names in the effect file
char * SliceVisualizer::passNames[] = {
		"PASS_RENDER",
		"PASS_RENDER_TRANSPARENT",
		"PASS_COMPUTE_TF_SLICE",
		"PASS_COMPUTE_2D_LIC",
		"PASS_COMPUTE_2D_LIC_COLORED"
};

//Effect Variables
ID3DX11EffectMatrixVariable	* SliceVisualizer::pModelWorldViewProjEV = nullptr;
ID3DX11EffectScalarVariable	* SliceVisualizer::pTimestepTEV = nullptr;
ID3DX11EffectVectorVariable	* SliceVisualizer::pVelocityScalingEV = nullptr;

ID3DX11EffectVectorVariable  * SliceVisualizer::pSliceDirectionUEV = nullptr;
ID3DX11EffectVectorVariable  * SliceVisualizer::pSliceDirectionVEV = nullptr;
ID3DX11EffectVectorVariable	* SliceVisualizer::pSliceCornerEV = nullptr;
ID3DX11EffectVectorVariable	* SliceVisualizer::pSliceResolutionEV = nullptr;
ID3DX11EffectVectorVariable	* SliceVisualizer::pPlaneNormalEV = nullptr;

ID3DX11EffectScalarVariable	* SliceVisualizer::pLICLengthEV = nullptr;
ID3DX11EffectScalarVariable	* SliceVisualizer::pLICStepsizeEV = nullptr;
ID3DX11EffectScalarVariable	* SliceVisualizer::pLICStepThreshold = nullptr;
ID3DX11EffectScalarVariable	* SliceVisualizer::pLICKernelSigmaEV = nullptr;

ID3DX11EffectShaderResourceVariable	* SliceVisualizer::pTexVolume0EV = nullptr;
ID3DX11EffectShaderResourceVariable	* SliceVisualizer::pTexVolume1EV = nullptr;
ID3DX11EffectShaderResourceVariable	* SliceVisualizer::pSliceTextureEV = nullptr;
ID3DX11EffectShaderResourceVariable	* SliceVisualizer::pNoiseTextureEV = nullptr;
ID3DX11EffectUnorderedAccessViewVariable	* SliceVisualizer::pSliceTextureRWEV = nullptr;
	
ID3DX11EffectShaderResourceVariable	* SliceVisualizer::pScalarVolumeEV = nullptr;
ID3DX11EffectShaderResourceVariable	* SliceVisualizer::pTransferFunctionEV = nullptr;

TransparencyModuleShaderEnvironment SliceVisualizer::transparencyEnvironment;

std::list<SliceVisualizer *> SliceVisualizer::sliceInstances;
unsigned int SliceVisualizer::instanceCounter;

HRESULT SliceVisualizer::Initialize(ID3D11Device * pd3dDevice_, TwBar* pParametersBar_)
{
	HRESULT hr;

	pd3dDevice = pd3dDevice_;
	SetupTwBar(pParametersBar_);

	std::wstring effectPath = GetExePath() + L"SliceVisualizer.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &pEffect)))
	{
		std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

	SAFE_GET_TECHNIQUE(pEffect, "SliceVisualizer", pTechnique);
	for(int i=0; i < NUM_PASSES; i++)
		SAFE_GET_PASS(pTechnique, passNames[i], pPasses[i]);

	SAFE_GET_MATRIX(pEffect, "g_modelWorldViewProj", pModelWorldViewProjEV);

	SAFE_GET_SCALAR(pEffect, "g_timestepT", pTimestepTEV);
	SAFE_GET_VECTOR(pEffect, "g_velocityScaling", pVelocityScalingEV);
	
	
	SAFE_GET_RESOURCE(pEffect, "g_flowFieldTex0", pTexVolume0EV);
	SAFE_GET_RESOURCE(pEffect, "g_flowFieldTex1", pTexVolume1EV);
	SAFE_GET_RESOURCE(pEffect, "g_scalarVolume", pScalarVolumeEV);
	SAFE_GET_RESOURCE(pEffect, "g_sliceTexture", pSliceTextureEV);
	SAFE_GET_UAV(pEffect, "g_sliceTextureRW", pSliceTextureRWEV);
	SAFE_GET_RESOURCE(pEffect, "g_noiseTexture", pNoiseTextureEV);

	SAFE_GET_VECTOR(pEffect, "g_sliceCorner", pSliceCornerEV);
	SAFE_GET_VECTOR(pEffect, "g_sliceDirectionU", pSliceDirectionUEV);
	SAFE_GET_VECTOR(pEffect, "g_sliceDirectionV", pSliceDirectionVEV);
	SAFE_GET_VECTOR(pEffect, "g_sliceResolution", pSliceResolutionEV);

	SAFE_GET_SCALAR(pEffect, "g_LICLength", pLICLengthEV);
	SAFE_GET_SCALAR(pEffect, "g_LICStepsize", pLICStepsizeEV);
	SAFE_GET_SCALAR(pEffect, "g_LICStepThreshold", pLICStepThreshold);
	SAFE_GET_SCALAR(pEffect, "g_LICKernelSigma", pLICKernelSigmaEV);
	SAFE_GET_VECTOR(pEffect, "g_planeNormal", pPlaneNormalEV);

	SAFE_GET_RESOURCE(pEffect, "g_transferFunction", pTransferFunctionEV);
	SAFE_GET_RESOURCE(pEffect, "g_scalarVolume", pScalarVolumeEV);

	transparencyEnvironment.LinkEffect(pEffect);

	return S_OK;
}

void SliceVisualizer::SetupTwBar(TwBar * pParametersBar_) {
	
	pParametersBar = TwNewBar("Slice Visualization");
	TwDefine("'Slice Visualization' color='10 10 200' size='300 250' position='1600 665' visible=false");

	TwType licType = TwDefineEnum("LICType", NULL, 0);
	TwType licPlaneType = TwDefineEnum("LineModeDrawType", NULL, 0);
	TwType noiseType = TwDefineEnum("NoiseType", NULL, 0);

    static TwStructMember sliceMembers[] = 
    {
		{ "Type",    licType, offsetof(SliceVisualizer, m_currentPassTypeGUI),  "enum='2 {Color}, 3 {2D LIC}, 4 {2D Colored LIC}'" }, 
		{ "Transparency", TW_TYPE_BOOLCPP, offsetof(SliceVisualizer, m_enableTransparency), ""},
		{ "Direction",    licPlaneType, offsetof(SliceVisualizer, m_sliceDirectionGUI),  "enum='0 {XY}, 1 {XZ}, 2 {YZ}'" }, 
		{ "Resolution Scale",    TW_TYPE_FLOAT, offsetof(SliceVisualizer, m_sliceResolutionScaleGUI),  "min=0.01 step=0.1" }, 
		{ "LIC Threshold",    TW_TYPE_FLOAT, offsetof(SliceVisualizer, m_LICThresholdGUI),  "min=0.0 step=0.01" }, 
		{ "LIC Length",    TW_TYPE_FLOAT, offsetof(SliceVisualizer, m_LICLengthGUI),     "min=5 max=200 step=5" }, 
		{ "LIC Stepsize",	TW_TYPE_FLOAT,   offsetof(SliceVisualizer, m_LICStepsizeGUI),    "min=0.0 step=0.05" },
		{ "LIC Parameter",	TW_TYPE_FLOAT,   offsetof(SliceVisualizer, m_LICKernelSigmaGUI),    "min=0.0 step=0.1" },
		{ "Noise Type",	noiseType,   offsetof(SliceVisualizer, m_LICNoiseTypeGUI),    "enum='0 {Uniform}, 1 {Power}, 2 {Spot}, 3 {Bell}'" },
		{ "Noise Parameter",	TW_TYPE_FLOAT,   offsetof(SliceVisualizer, m_LICNoiseParamGUI),    "min=0.0 step=0.1" },
    };
	sliceVisualizerType = TwDefineStruct("Slice Visualizer", sliceMembers, 10, sizeof(SliceVisualizer), NULL, NULL); 
}


HRESULT SliceVisualizer::Release(void)
{
	for(int i=0; i < NUM_PASSES; i++)
		SAFE_RELEASE(pPasses[i]);

	SAFE_RELEASE(pTechnique);
	SAFE_RELEASE(pEffect);
	
	return S_OK;
}

void TW_CALL SliceVisualizer::DeleteSliceVisualizerCB(void *clientData)
{ 
	SliceVisualizer * me = static_cast<SliceVisualizer*>(clientData);
	delete me;
}

void SliceVisualizer::SaveConfig(SettingsStorage & store)
{
	char idBuf[256];
	store.StoreInt("scene.sliceVisualizer.numSlices", (int)sliceInstances.size());
	int i=0;
	for(auto it = sliceInstances.begin(); it != sliceInstances.end(); ++it, ++i) {
		_itoa(i, idBuf, 10);
		(*it)->SaveConfig(store, idBuf);
	};
}

void TW_CALL SliceVisualizer::RemoveSliceCB(void * clientData)
{
	SliceVisualizer * me = reinterpret_cast<SliceVisualizer *> (clientData);
	delete me;
}

void SliceVisualizer::LoadConfig(SettingsStorage & store, ScalarVolumeData * scalarData, VectorVolumeData * vectorData)
{
	char idBuf[256];
	int instances = 0;
	store.GetInt("scene.sliceVisualizer.numSlices", instances);
	for(int i=0; i < instances; i++) {
		SliceVisualizer *p = new SliceVisualizer(scalarData, vectorData);
		_itoa(i, idBuf, 10);
		p->LoadConfig(store, idBuf);
	};
}

HRESULT SliceVisualizer::Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	HRESULT hr;
	for(auto it = sliceInstances.begin(); it != sliceInstances.end(); ++it) {
		V_RETURN((*it)->RenderInstance(pd3dImmediateContext, sceneMtcs));
	}
	return S_OK;
}

HRESULT SliceVisualizer::RenderTransparency(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	HRESULT hr;
	for(auto it = sliceInstances.begin(); it != sliceInstances.end(); ++it) {
		V_RETURN((*it)->RenderTransparencyInstance(pd3dImmediateContext, sceneMtcs));
	}
	return S_OK;
}

void SliceVisualizer::FrameMove(double dTime, float fElapsedTime, float fElapsedLogicTime) {
	for(auto it = sliceInstances.begin(); it != sliceInstances.end(); ++it) {
		(*it)->FrameMoveInstance(dTime, fElapsedTime, fElapsedLogicTime);
	}
}

void SliceVisualizer::DeleteInstances(void) {
	// we cant just iterate over because the destructor modifies the list and invalidates the iterators
	while(sliceInstances.size()) {
		delete sliceInstances.front();
	}
}

void SliceVisualizer::GenerateNoise(float * texture, int noiseType, int resX, int resY, float param)
{
	float r;
	std::default_random_engine g;
	std::uniform_real_distribution<float>	dist(0.0, 1.0);

	switch (noiseType) {
	default:
	case 0: //Uniform
		for(int i=0;i < resX * resY; i++) {
			texture[i] = dist(g);
		}
	break;
	case 1: //Power
		for(int i=0;i < resX * resY; i++) {
			texture[i] = pow(dist(g), param);
		}
	break;
	case 2: //Spot
		for(int i=0;i < resX * resY; i++) {
			texture[i] = (dist(g) > param) ? 0.0f : 1.0f;
		}
		break;
	case 3: //Bell
		for(int i=0;i < resX * resY; i++) {
			r = (2.f*dist(g) - 1.f);
			texture[i] = exp(1.f - 1 / (1.f - r*r));
		}
		break;
	}
}

SliceVisualizer::SliceVisualizer(ScalarVolumeData * scalarData, VectorVolumeData * vectorData) :
	m_pScalarVolumeData(scalarData),
	m_pVectorVolumeData(vectorData),
	m_pSliceTextureBuffer(nullptr),
	m_pSliceTextureBufferUAV(nullptr),
	m_pSliceTextureBufferSRV(nullptr),
	m_pNoiseTextureBuffer(nullptr),
	m_pNoiseTextureBufferSRV(nullptr),
	m_slicePositionBox(XMFLOAT3(0.5, 0.5, 0.5), XMFLOAT3(1, 1, 0), true, false, true),
	m_sliceDirection(0), m_sliceDirectionGUI(0),
	m_sliceLastUpdate(-1), //set negative to enforce update on the first framemove
	m_LICKernelSigma(1), m_LICKernelSigmaGUI(1),
	m_LICLength(20), m_LICLengthGUI(20),
	m_LICStepsize(1), m_LICStepsizeGUI(1),
	m_LICThreshold(1e-10f), m_LICThresholdGUI(1e-10f),
	m_currentPassType(PASS_COMPUTE_TF_SLICE), m_currentPassTypeGUI(PASS_COMPUTE_TF_SLICE),
	m_sliceResolutionScale(2.f), m_sliceResolutionScaleGUI(2.f),
	m_LICNoiseType(1), m_LICNoiseTypeGUI(1),
	m_LICNoiseParam(2.2f), m_LICNoiseParamGUI(2.2f),
	m_dataChanged(true),
	m_enableTransparency(true)
{
	m_sliceIndex = instanceCounter ++;
	g_boxManipulationManager->AddBox(&m_slicePositionBox);

	g_transferFunctionEditor->RegisterObserver(this);

	if(m_pVectorVolumeData) {
		m_pVectorVolumeData->RegisterObserver(this);
		m_pVectorVolumeData->GetScalarMetricVolume()->RegisterObserver(this);
	}
	else {
		assert(m_pScalarVolumeData);
		m_pScalarVolumeData->RegisterObserver(this);
	}

	XMINT3 res = (vectorData?(VolumeData*)vectorData:(VolumeData*)scalarData)->GetResolution();
	m_sliceResolution = XMINT2(int(res.x*m_sliceResolutionScale), int(res.y*m_sliceResolutionScale));

	XMFLOAT3 bbox = (vectorData?(VolumeData*)vectorData:(VolumeData*)scalarData)->GetBoundingBox();
	m_modelTransform = XMMatrixScaling(bbox.x, bbox.y, bbox.z);
	m_velocityScaling = XMFLOAT3(1.f/bbox.x, 1.f/bbox.y, 1.f/bbox.z);

	sliceInstances.push_back(this);

	std::stringstream ss;
	ss << "[" << m_sliceIndex << "]";
	TwAddVarRW(pParametersBar, (std::string("Slice")  + ss.str()).c_str(), sliceVisualizerType, this, "");
	TwAddButton(pParametersBar,(std::string("Remove Slice ")  + ss.str()).c_str(), DeleteSliceVisualizerCB, this, "");//(std::string("group='Slice")  + ss.str() + "'").c_str());

	int visible = 1;
	TwSetParam(pParametersBar, nullptr, "visible", TW_PARAM_INT32, 1, &visible);
}

SliceVisualizer::~SliceVisualizer(void)
{
	SAFE_RELEASE(m_pSliceTextureBuffer);
	SAFE_RELEASE(m_pSliceTextureBufferSRV);
	SAFE_RELEASE(m_pSliceTextureBufferUAV);
	SAFE_RELEASE(m_pNoiseTextureBuffer);
	SAFE_RELEASE(m_pNoiseTextureBufferSRV);
	
	std::stringstream ss;
	ss << "[" << m_sliceIndex << "]";

	g_boxManipulationManager->RemoveBox(&m_slicePositionBox);

	g_transferFunctionEditor->UnregisterObserver(this);

	sliceInstances.remove(this);

	int visible = sliceInstances.size()?1:0;
	TwSetParam(pParametersBar, nullptr, "visible", TW_PARAM_INT32, 1, &visible);

	TwRemoveVar(pParametersBar, (std::string("Remove Slice ") + ss.str()).c_str());
	TwRemoveVar(pParametersBar, (std::string("Slice") + ss.str()).c_str());

	if(m_pVectorVolumeData) {
		m_pVectorVolumeData->UnregisterObserver(this);
		if(m_currentPassType != PASS_COMPUTE_2D_LIC)
			m_pVectorVolumeData->GetScalarMetricVolume()->UnregisterObserver(this);
	}
	else {
		m_pScalarVolumeData->UnregisterObserver(this);
	}
}

void SliceVisualizer::SaveConfig(SettingsStorage &store, std::string id)
{
	std::string cfgName = "slicevisualizer[" + id + "]";
	store.StoreInt(cfgName + ".sliceDirection", m_sliceDirection);
	store.StoreFloat(cfgName + ".resolutionScale", m_sliceResolutionScale);
	store.StoreBool(cfgName + ".enableTransparency", m_enableTransparency);
	store.StoreInt(cfgName + ".LIC.noiseType", m_LICNoiseType);
	store.StoreFloat(cfgName + ".LIC.length", m_LICLength);
	store.StoreFloat(cfgName + ".LIC.stepsize", m_LICStepsize);
	store.StoreFloat(cfgName + ".LIC.kernelSigma", m_LICKernelSigma);
	store.StoreFloat(cfgName + ".LIC.noiseParam", m_LICNoiseParam);
	store.StoreFloat(cfgName + ".LIC.threshold", m_LICThreshold);
	store.StoreFloat3(cfgName + ".center", &m_slicePositionBox.center.x);
	store.StoreFloat3(cfgName + ".size", &m_slicePositionBox.size.x);
	store.StoreInt(cfgName + ".passType", m_currentPassType);
}

void SliceVisualizer::LoadConfig(SettingsStorage &store, std::string id)
{
	std::string cfgName = "slicevisualizer[" + id + "]";

	store.GetInt(cfgName + ".sliceDirection", m_sliceDirectionGUI);
	store.GetFloat(cfgName + ".resolutionScale", m_sliceResolutionScaleGUI);
	store.GetBool(cfgName + ".enableTransparency", m_enableTransparency);
	store.GetInt(cfgName + ".LIC.noiseType", m_LICNoiseTypeGUI);
	store.GetFloat(cfgName + ".LIC.length", m_LICLengthGUI);
	store.GetFloat(cfgName + ".LIC.stepsize", m_LICStepsizeGUI);
	store.GetFloat(cfgName + ".LIC.kernelSigma", m_LICKernelSigmaGUI);
	store.GetFloat(cfgName + ".LIC.noiseParam", m_LICNoiseParamGUI);
	store.GetFloat(cfgName + ".LIC.threshold", m_LICThresholdGUI);
	store.GetFloat3(cfgName + ".center", &m_slicePositionBox.center.x);
	store.GetFloat3(cfgName + ".size", &m_slicePositionBox.size.x);
	int passType = m_currentPassType;
	store.GetInt(cfgName + ".passType", passType);
	m_currentPassTypeGUI = (PassType)passType;

}

HRESULT SliceVisualizer::RenderInstance(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{


	RenderTransformations modelMtcs = sceneMtcs.PremultModelMatrix(m_modelTransform);
	XMFLOAT4X4 mModelWorldViewProj, mWorldViewProjInv;
	
	XMMATRIX mModelWorldViewInv = modelMtcs.modelWorldViewInv;
	XMStoreFloat4x4(&mModelWorldViewProj, modelMtcs.modelWorldViewProj);
	pModelWorldViewProjEV->SetMatrix((float*)mModelWorldViewProj.m);

	m_slicePositionBox.texToNDCSpace = modelMtcs.modelWorldViewProj;
	
	if(m_enableTransparency) return S_OK;

	pSliceResolutionEV->SetIntVector(&m_sliceResolution.x);

	XMFLOAT3 sliceCorner;
	XMFLOAT3 dirU, dirV, normal;
	XMINT2 res = m_sliceResolution;
	switch(m_sliceDirection) {
	case 0: //XY
		dirU = XMFLOAT3(1.f/res.x, 0, 0);
		dirV = XMFLOAT3(0, 1.f/res.y, 0);
		sliceCorner = XMFLOAT3(0, 0, m_slicePositionBox.center.z);
	break;
	case 1: //XZ
		dirU = XMFLOAT3(1.f/res.x, 0, 0);
		dirV = XMFLOAT3(0, 0, 1.f/res.y);
		sliceCorner = XMFLOAT3(0, m_slicePositionBox.center.y, 0);
	break;
	case 2: //YZ
		dirU = XMFLOAT3(0, 1.f/res.x, 0);
		dirV = XMFLOAT3(0, 0, 1.f/res.y);
		sliceCorner = XMFLOAT3(m_slicePositionBox.center.x, 0, 0);
	break;
	}

	pSliceDirectionUEV->SetFloatVector(&dirU.x);
	pSliceDirectionVEV->SetFloatVector(&dirV.x);
	pSliceCornerEV->SetFloatVector(&sliceCorner.x);

	pSliceTextureEV->SetResource(m_pSliceTextureBufferSRV);

	pPasses[PASS_RENDER]->Apply(0, pd3dImmediateContext);

	pd3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	pd3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	pd3dImmediateContext->Draw(1, 0);
	

	//remove the mappings from the shader inputs
	pSliceTextureEV->SetResource(nullptr);
	pTexVolume0EV->SetResource(nullptr);
	pTexVolume1EV->SetResource(nullptr);
	pScalarVolumeEV->SetResource(nullptr);
	pPasses[PASS_RENDER]->Apply(0, pd3dImmediateContext);

	return S_OK;
}

HRESULT SliceVisualizer::RenderTransparencyInstance(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	if(!m_enableTransparency) return S_OK;

	transparencyEnvironment.BeginTransparency();

	RenderTransformations modelMtcs = sceneMtcs.PremultModelMatrix(m_modelTransform);
	XMFLOAT4X4 mModelWorldViewProj, mWorldViewProjInv;
	
	XMMATRIX mModelWorldViewInv = modelMtcs.modelWorldViewInv;
	XMStoreFloat4x4(&mModelWorldViewProj, modelMtcs.modelWorldViewProj);
	pModelWorldViewProjEV->SetMatrix((float*)mModelWorldViewProj.m);

	pSliceResolutionEV->SetIntVector(&m_sliceResolution.x);

	XMFLOAT3 sliceCorner;
	XMFLOAT3 dirU, dirV, normal;
	XMINT2 res = m_sliceResolution;
	switch(m_sliceDirection) {
	case 0: //XY
		dirU = XMFLOAT3(1.f/res.x, 0, 0);
		dirV = XMFLOAT3(0, 1.f/res.y, 0);
		sliceCorner = XMFLOAT3(0, 0, m_slicePositionBox.center.z);
	break;
	case 1: //XZ
		dirU = XMFLOAT3(1.f/res.x, 0, 0);
		dirV = XMFLOAT3(0, 0, 1.f/res.y);
		sliceCorner = XMFLOAT3(0, m_slicePositionBox.center.y, 0);
	break;
	case 2: //YZ
		dirU = XMFLOAT3(0, 1.f/res.x, 0);
		dirV = XMFLOAT3(0, 0, 1.f/res.y);
		sliceCorner = XMFLOAT3(m_slicePositionBox.center.x, 0, 0);
	break;
	}

	pSliceDirectionUEV->SetFloatVector(&dirU.x);
	pSliceDirectionVEV->SetFloatVector(&dirV.x);
	pSliceCornerEV->SetFloatVector(&sliceCorner.x);

	pSliceTextureEV->SetResource(m_pSliceTextureBufferSRV);



	pPasses[PASS_RENDER_TRANSPARENT]->Apply(0, pd3dImmediateContext);

	pd3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	pd3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	pd3dImmediateContext->Draw(1, 0);
	

	//remove the mappings from the shader inputs
	pSliceTextureEV->SetResource(nullptr);
	pTexVolume0EV->SetResource(nullptr);
	pTexVolume1EV->SetResource(nullptr);
	pScalarVolumeEV->SetResource(nullptr);

	transparencyEnvironment.EndTransparency(pd3dImmediateContext, pPasses[PASS_RENDER_TRANSPARENT]);

	return S_OK;
}

void SliceVisualizer::FrameMoveInstance(double dTime, float fElapsedTime, float fElapsedLogicTime) {
	static std::default_random_engine g;
	static std::uniform_real_distribution<float> dist(0, 1);

	// Constrain spawn box to the region
	m_slicePositionBox.center.x = std::min(1.f - 0.5f * m_slicePositionBox.size.x, std::max(0.5f * m_slicePositionBox.size.x, m_slicePositionBox.center.x));
	m_slicePositionBox.center.y = std::min(1.f - 0.5f * m_slicePositionBox.size.y, std::max(0.5f * m_slicePositionBox.size.y, m_slicePositionBox.center.y));
	m_slicePositionBox.center.z = std::min(1.f - 0.5f * m_slicePositionBox.size.z, std::max(0.5f * m_slicePositionBox.size.z, m_slicePositionBox.center.z));

	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);

	//we don't update texture if paused
	if( SVRequireRecompute() ) {
		ComputeTexture(pContext, dTime);
	}

	SAFE_RELEASE(pContext);
}


HRESULT SliceVisualizer::CreateGPUBuffers()
{
	HRESULT hr;
	assert(pd3dDevice);

	if(m_pSliceTextureBuffer)
		return S_OK;

	// create and fill the noise texture buffer on the cpu with noise
	float * noiseTextureData = new float[m_sliceResolution.x * m_sliceResolution.y];
	GenerateNoise(noiseTextureData, m_LICNoiseType, m_sliceResolution.x, m_sliceResolution.y, m_LICNoiseParam);

	//create the noise texture
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = m_sliceResolution.x;
	desc.Height = m_sliceResolution.y;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initData;
	ZeroMemory(&initData, sizeof(initData));
	initData.pSysMem = noiseTextureData;
	initData.SysMemPitch = m_sliceResolution.x * sizeof(float);
	initData.SysMemSlicePitch = 0;

	V_RETURN(pd3dDevice->CreateTexture2D(&desc, &initData, &m_pNoiseTextureBuffer));

	delete noiseTextureData;
		
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = -1;
	srvDesc.Texture2D.MostDetailedMip = 0;
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pNoiseTextureBuffer, &srvDesc, &m_pNoiseTextureBufferSRV));

	// Create the RW color texture
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	V_RETURN(pd3dDevice->CreateTexture2D(&desc, nullptr, &m_pSliceTextureBuffer));
	srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pSliceTextureBuffer, &srvDesc, &m_pSliceTextureBufferSRV));

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pSliceTextureBuffer, &uavDesc, &m_pSliceTextureBufferUAV));

	return S_OK;
}

void SliceVisualizer::ComputeTexture(ID3D11DeviceContext * pContext, double dTime)
{
	//std::cout << "Computing slice texture..." << std::endl;

	//don't compute if currentPassType doesn't make sense
	if(m_currentPassType < PASS_RENDER || m_currentPassType >= NUM_PASSES)
		return;

	if(!m_pSliceTextureBuffer)
		CreateGPUBuffers();

	pVelocityScalingEV->SetFloatVector(&m_velocityScaling.x);
	pTimestepTEV->SetFloat((m_pVectorVolumeData?(VolumeData*)m_pVectorVolumeData:(VolumeData*)m_pScalarVolumeData)->GetCurrentTimestepT());
	pVelocityScalingEV->SetFloatVector(&m_velocityScaling.x);

	XMINT2 res = m_sliceResolution;
	pSliceResolutionEV->SetIntVector(&res.x);

	XMFLOAT3 dirU, dirV, normal, sliceCorner;
	switch(m_sliceDirection) {
	case 0: //XY
		dirU = XMFLOAT3(1.f/res.x, 0, 0);
		dirV = XMFLOAT3(0, 1.f/res.y, 0);
		normal = XMFLOAT3(0, 0, 1);
		sliceCorner = XMFLOAT3(0, 0, m_slicePositionBox.center.z);
	break;
	case 1: //XZ
		dirU = XMFLOAT3(1.f/res.x, 0, 0);
		dirV = XMFLOAT3(0, 0, 1.f/res.y);
		normal = XMFLOAT3(0, 1, 0);
		sliceCorner = XMFLOAT3(0, m_slicePositionBox.center.y, 0);
	break;
	case 2: //YZ
		dirU = XMFLOAT3(0, 1.f/res.x, 0);
		dirV = XMFLOAT3(0, 0, 1.f/res.y);
		normal = XMFLOAT3(1, 0, 0);
		sliceCorner = XMFLOAT3(m_slicePositionBox.center.x, 0, 0);
	break;
	}
	pSliceDirectionUEV->SetFloatVector(&dirU.x);
	pSliceDirectionVEV->SetFloatVector(&dirV.x);
	pPlaneNormalEV->SetFloatVector(&normal.x);
	pSliceCornerEV->SetFloatVector(&sliceCorner.x);

	float animationT = float(0.5*(1. + (dTime - floor(dTime))));
	pLICKernelSigmaEV->SetFloat(m_LICKernelSigma);
	pLICLengthEV->SetInt(int(m_LICLength)/**animationT*/);//"animation" works by using an asymmetric convolution kernel and an animated LIC length
	pLICStepsizeEV->SetFloat(m_LICStepsize*m_sliceResolutionScale);
	
	pTransferFunctionEV->SetResource(g_transferFunctionEditor->getSRV());
	pSliceTextureRWEV->SetUnorderedAccessView(m_pSliceTextureBufferUAV);
	pLICStepThreshold->SetFloat(m_LICThreshold);
	pNoiseTextureEV->SetResource(m_pNoiseTextureBufferSRV);

	if(m_currentPassType == PASS_COMPUTE_2D_LIC_COLORED || m_currentPassType == PASS_COMPUTE_TF_SLICE) {
		if(m_pScalarVolumeData)
			pScalarVolumeEV->SetResource(m_pScalarVolumeData->GetInterpolatedTextureSRV());
		else
			pScalarVolumeEV->SetResource(m_pVectorVolumeData->GetScalarMetricVolume()->GetInterpolatedTextureSRV());
	}

	if(m_pVectorVolumeData) {
		pTexVolume0EV->SetResource(m_pVectorVolumeData->GetTexture0SRV());
		pTexVolume1EV->SetResource(m_pVectorVolumeData->GetTexture1SRV());
	}

	pPasses[m_currentPassType]->Apply(0, pContext);
	
	uint32_t gx = uint32_t(ceilf(m_sliceResolution.x/(32.f))),
		gy = uint32_t(ceilf(m_sliceResolution.y/(32.f))),
		gz = 1;

	pContext->Dispatch(gx, gy, gz);

	pSliceTextureRWEV->SetUnorderedAccessView(nullptr);
	pScalarVolumeEV->SetResource(nullptr);
	pTexVolume0EV->SetResource(nullptr);
	pTexVolume1EV->SetResource(nullptr);

	pPasses[m_currentPassType]->Apply(0, pContext);

	//now we are in sync with the data again
	m_dataChanged = false;
}

bool SliceVisualizer::SVRequireRecompute(void) {
	//float currentVolumeTime = m_pVectorVolumeData?m_pVectorVolumeData->GetTime():m_pScalarVolumeData->GetTime();
	bool recompute = false;

	//need update if metric/volume has changed and we need the metric for slice rendering
/*	if(m_pVectorVolumeData) {
		recompute = recompute || m_pVectorVolumeData->ChangedInLastFrame();
		if(m_currentPassType != PASS_COMPUTE_2D_LIC)
			recompute = recompute || m_pVectorVolumeData->GetScalarMetricVolume()->ChangedInLastFrame();
	}
	else {
		recompute = recompute || m_pScalarVolumeData->ChangedInLastFrame();
	}*/

	//other triggers for slice recomputation - parameter changes
	recompute = recompute ||
		m_sliceResolutionScale != m_sliceResolutionScaleGUI ||
		m_LICStepsize != m_LICStepsizeGUI ||
		m_LICKernelSigma != m_LICKernelSigmaGUI ||
		m_LICLength != m_LICLengthGUI ||
		m_currentPassType != m_currentPassTypeGUI ||
		m_LICThreshold != m_LICThresholdGUI ||
		m_sliceDirection != m_sliceDirectionGUI ||
		m_LICNoiseType != m_LICNoiseTypeGUI ||
		m_LICNoiseParam != m_LICNoiseParamGUI ||
		m_dataChanged ||
		m_slicePositionBox.changed;
	
	// handle updates to the pass type (register/unregister from metric updates)
	if(m_pVectorVolumeData && m_currentPassType != m_currentPassTypeGUI) {
		// signal that we don't need metric anymore
		if(m_currentPassTypeGUI == PASS_COMPUTE_2D_LIC)
			m_pVectorVolumeData->GetScalarMetricVolume()->UnregisterObserver(this);
		else {
			//if the pass one was pure LIC, we now need to register for the metric
			if(m_currentPassType == PASS_COMPUTE_2D_LIC)
				m_pVectorVolumeData->GetScalarMetricVolume()->RegisterObserver(this);
		}
	}

	if(m_currentPassType != m_currentPassTypeGUI) {
		if(m_currentPassTypeGUI == PASS_COMPUTE_2D_LIC) {
			g_transferFunctionEditor->UnregisterObserver(this);
		}
		else {
			g_transferFunctionEditor->RegisterObserver(this);
		}
	}

	if(	m_sliceResolutionScale != m_sliceResolutionScaleGUI ||
		m_sliceDirection != m_sliceDirectionGUI ||
		m_LICKernelSigma != m_LICKernelSigmaGUI ||
		m_LICNoiseType != m_LICNoiseTypeGUI ||
		m_LICNoiseParam != m_LICNoiseParamGUI)
	{
		m_LICNoiseType = m_LICNoiseTypeGUI;
		m_sliceResolutionScale = m_sliceResolutionScaleGUI;
		m_LICKernelSigma = m_LICKernelSigmaGUI;
		m_LICNoiseParam = m_LICNoiseParamGUI;
		SetSliceDirection(m_sliceDirectionGUI);
	}

	m_LICStepsize = m_LICStepsizeGUI;
	m_LICKernelSigma = m_LICKernelSigmaGUI;
	m_LICThreshold = m_LICThresholdGUI;
	m_LICLength = m_LICLengthGUI;
	m_currentPassType = m_currentPassTypeGUI;

	return recompute;
}

void SliceVisualizer::SetSliceDirection(int sliceDirection)
{
	m_sliceDirection = sliceDirection;
	SAFE_RELEASE(m_pNoiseTextureBuffer);
	SAFE_RELEASE(m_pNoiseTextureBufferSRV);
	SAFE_RELEASE(m_pSliceTextureBuffer);
	SAFE_RELEASE(m_pSliceTextureBufferSRV);
	SAFE_RELEASE(m_pSliceTextureBufferUAV);

	XMINT3 res = (m_pVectorVolumeData?(VolumeData*)m_pVectorVolumeData:(VolumeData*)m_pScalarVolumeData)->GetResolution();

	switch(m_sliceDirection)
	{
	case 0:
		m_sliceResolution = XMINT2(int(res.x*m_sliceResolutionScale), int(res.y*m_sliceResolutionScale));
		m_slicePositionBox.size = XMFLOAT3(1.f, 1.f, 0);
		break;
	case 1:
		m_sliceResolution = XMINT2(int(res.x*m_sliceResolutionScale), int(res.z*m_sliceResolutionScale));
		m_slicePositionBox.size = XMFLOAT3(1.f, 0, 1.f);
		break;
	case 2:
		m_sliceResolution = XMINT2(int(res.y*m_sliceResolutionScale), int(res.z*m_sliceResolutionScale));
		m_slicePositionBox.size = XMFLOAT3(0, 1.f, 1.f);
		break;
	}
}

void SliceVisualizer::notify(Observable * volumeData)
{
	m_dataChanged = true;
}