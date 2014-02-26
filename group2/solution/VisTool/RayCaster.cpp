#include "RayCaster.h"

#include "util/util.h"
#include "Globals.h"

#include <iostream>

ID3DX11Effect			* RayCaster::pEffect = nullptr;
ID3DX11EffectTechnique	* RayCaster::pTechnique = nullptr;
ID3D11Device			* RayCaster::pd3dDevice = nullptr;
ID3DX11EffectPass		* RayCaster::pPasses[NUM_PASSES];
TwBar					* RayCaster::pParametersBar = nullptr;

// pass names in the effect file
char * RayCaster::passNames[] = {
		"PASS_BOX",
		"PASS_BOX_FRONT",
		"PASS_BOX_BACK",
		"PASS_BOX_RAYCAST",
		"PASS_ISOSURFACE",
		"PASS_ISOSURFACE_ALPHA",
		"PASS_DVR",
		"PASS_MIP",
		"PASS_DUAL_ISOSURFACE",
		"PASS_ISOSURFACE_ALPHA_GLOBAL",
		"PASS_MIP2"
};

//Effect Variables
ID3DX11EffectMatrixVariable	* RayCaster::pWorldViewProjEV = nullptr;
ID3DX11EffectMatrixVariable	* RayCaster::pWorldViewProjInvEV = nullptr;
ID3DX11EffectVectorVariable	* RayCaster::pSurfaceColorEV = nullptr;
ID3DX11EffectVectorVariable	* RayCaster::pSurfaceColor2EV = nullptr;
ID3DX11EffectVectorVariable	* RayCaster::pLightPosEV = nullptr;
ID3DX11EffectVectorVariable	* RayCaster::pCamPosEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pIsoValueEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pIsoValue2EV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pRaycastStepsizeEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pRaycastPixelStepsizeEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pBinSearchStepsEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pTerminationAlphaEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pGlobalAlphaScaleEV = nullptr;

ID3DX11EffectVectorVariable	* RayCaster::pLightColorEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pAmbientEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pDiffuseEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pSpecularEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pSpecularExpEV = nullptr;
ID3DX11EffectScalarVariable	* RayCaster::pDVRLightingEV = nullptr;

ID3DX11EffectShaderResourceVariable	* RayCaster::pTexVolumeEV = nullptr;
ID3DX11EffectShaderResourceVariable	* RayCaster::pTexNormalVolumeEV = nullptr;
ID3DX11EffectShaderResourceVariable	* RayCaster::pDepthBufferEV = nullptr;
ID3DX11EffectShaderResourceVariable	* RayCaster::pRayEntryPointsEV = nullptr;
ID3DX11EffectShaderResourceVariable	* RayCaster::pTransferFunctionEV = nullptr;

ID3D11Buffer			* RayCaster::pBoxIndexBuffer = nullptr;
ID3D11Buffer			* RayCaster::pBoxVertexBuffer = nullptr;
ID3D11InputLayout		* RayCaster::pBoxInputLayout = nullptr;

TransparencyModuleShaderEnvironment RayCaster::transparencyEnvironment;

unsigned int RayCaster::boxIndices[] = 
//6 faces, two triangles each
{
	0, 3, 2,  1, 3, 0,
	4, 7, 5,  4, 6, 7,
	2, 3, 7,  2, 7, 6,
	0, 5, 1,  0, 4, 5,
	1, 5, 3,  3, 5, 7,
	0, 6, 4,  0, 2, 6
};

XMFLOAT4 boxVertices[] = {
	XMFLOAT4(0.0,  1.0,  0.0, 1.0),
	XMFLOAT4(1.0,  1.0,  0.0, 1.0),
	XMFLOAT4(0.0,  0.0,  0.0, 1.0),
	XMFLOAT4(1.0,  0.0,  0.0, 1.0),
	XMFLOAT4(0.0,  1.0,  1.0, 1.0),
	XMFLOAT4(1.0,  1.0,  1.0, 1.0),
	XMFLOAT4(0.0,  0.0,  1.0, 1.0),
	XMFLOAT4(1.0,  0.0,  1.0, 1.0)
};

XMFLOAT4 RayCaster::boxDisplacements[] = {
	XMFLOAT4(-.5,  0.5,  -.5, 0.0),
	XMFLOAT4(0.5,  0.5,  -.5, 0.0),
	XMFLOAT4(-.5,  -.5,  -.5, 0.0),
	XMFLOAT4(0.5,  -.5,  -.5, 0.0),
	XMFLOAT4(-.5,  0.5,  0.5, 0.0),
	XMFLOAT4(0.5,  0.5,  0.5, 0.0),
	XMFLOAT4(-.5,  -.5,  0.5, 0.0),
	XMFLOAT4(0.5,  -.5,  0.5, 0.0)
};

HRESULT RayCaster::Initialize(ID3D11Device * pd3dDevice_, TwBar* pParametersBar_)
{
	HRESULT hr;

	pd3dDevice = pd3dDevice_;
	//pParametersBar = pParametersBar_;
	pParametersBar = TwNewBar("Ray Caster");
	TwDefine("'Ray Caster' color='200 10 10' size='300 180' position='1600 15' visible=false");
	
	std::wstring effectPath = GetExePath() + L"RayCaster.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &pEffect)))
	{
		std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

	SAFE_GET_TECHNIQUE(pEffect, "RayCaster", pTechnique);
	for(int i=0; i < NUM_PASSES; i++)
		SAFE_GET_PASS(pTechnique, passNames[i], pPasses[i]);


	SAFE_GET_MATRIX(pEffect, "g_worldViewProj", pWorldViewProjEV);
	SAFE_GET_MATRIX(pEffect, "g_worldViewProjInv", pWorldViewProjInvEV);
	SAFE_GET_VECTOR(pEffect, "g_lightPos", pLightPosEV);
	SAFE_GET_VECTOR(pEffect, "g_surfaceColor", pSurfaceColorEV);
	SAFE_GET_VECTOR(pEffect, "g_surfaceColor2", pSurfaceColor2EV);
	SAFE_GET_VECTOR(pEffect, "g_camPosInObjectSpace", pCamPosEV);
	SAFE_GET_SCALAR(pEffect, "g_isoValue", pIsoValueEV);
	SAFE_GET_SCALAR(pEffect, "g_isoValue2", pIsoValue2EV);
	SAFE_GET_SCALAR(pEffect, "g_raycastStepsize", pRaycastStepsizeEV);
	SAFE_GET_SCALAR(pEffect, "g_raycastPixelStepsize", pRaycastPixelStepsizeEV);
	SAFE_GET_SCALAR(pEffect, "g_binSearchSteps", pBinSearchStepsEV);
	SAFE_GET_SCALAR(pEffect, "g_DVRLighting", pDVRLightingEV);
	SAFE_GET_SCALAR(pEffect, "g_terminationAlphaThreshold", pTerminationAlphaEV);
	SAFE_GET_SCALAR(pEffect, "g_globalAlphaScale", pGlobalAlphaScaleEV);

	SAFE_GET_RESOURCE(pEffect, "g_texVolume", pTexVolumeEV);
	SAFE_GET_RESOURCE(pEffect, "g_texVolumeNormals", pTexNormalVolumeEV);
	SAFE_GET_RESOURCE(pEffect, "g_depthBuffer", pDepthBufferEV);
	SAFE_GET_RESOURCE(pEffect, "g_rayEntryPoints", pRayEntryPointsEV);
	SAFE_GET_RESOURCE(pEffect, "g_transferFunction", pTransferFunctionEV);

	SAFE_GET_VECTOR(pEffect, "g_lightColor", pLightColorEV);
	SAFE_GET_SCALAR(pEffect, "k_a", pAmbientEV);
	SAFE_GET_SCALAR(pEffect, "k_d", pDiffuseEV);
	SAFE_GET_SCALAR(pEffect, "k_s", pSpecularEV);
	SAFE_GET_SCALAR(pEffect, "g_specularExp", pSpecularExpEV);

	transparencyEnvironment.LinkEffect(pEffect);

	V_RETURN(CreateBoxVertexIndexBuffer());

	auto AAE = D3D11_APPEND_ALIGNED_ELEMENT;
	auto IPVD = D3D11_INPUT_PER_VERTEX_DATA;
	// Array of descriptions for each vertexattribute
	D3D11_INPUT_ELEMENT_DESC layout[]= {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, AAE, IPVD, 0}
	};
	UINT numElements=sizeof(layout)/sizeof(layout[0]);
	// Get input signature of pass using this layout
	D3DX11_PASS_DESC passDesc;
	pPasses[PASS_BOX]->GetDesc(&passDesc);
	// Create the input layout
	V_RETURN(pd3dDevice->CreateInputLayout(layout,numElements,
		passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &pBoxInputLayout));

	return S_OK;
}

HRESULT RayCaster::Release(void)
{
	SAFE_RELEASE(pEffect);
	SAFE_RELEASE(pBoxIndexBuffer);
	SAFE_RELEASE(pBoxVertexBuffer);
	SAFE_RELEASE(pBoxInputLayout);

	return S_OK;
}

HRESULT RayCaster::CreateBoxVertexIndexBuffer(void)
{
	HRESULT hr;

	// Create the index buffer for the bounding box edges
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage           = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth       = sizeof( boxIndices );
	bufferDesc.BindFlags       = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags  = 0;
	bufferDesc.MiscFlags       = 0;

	// Define the resource data.
	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = boxIndices;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	// Create the buffer with the device.
	V_RETURN(pd3dDevice->CreateBuffer( &bufferDesc, &InitData, &pBoxIndexBuffer ));

	ZeroMemory(&bufferDesc, sizeof(bufferDesc));
	bufferDesc.Usage           = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth       = sizeof( boxDisplacements );
	bufferDesc.BindFlags       = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags  = 0;
	bufferDesc.MiscFlags       = 0;

	// Define the resource data.
	/*ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = m_boxVertices;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;*/
	V_RETURN(pd3dDevice->CreateBuffer( &bufferDesc, nullptr, &pBoxVertexBuffer ));

	return S_OK;
}

void TW_CALL RayCaster::SetCurrentPassCB(const void *value, void *clientData)
{ 
	RayCaster * me = reinterpret_cast<RayCaster*>(clientData);
	me->m_currentPassSelection = *(const PassType *)value;

	int visible = 0;
	switch (me->m_currentPassSelection) {
	case PASS_ISOSURFACE:
	case PASS_ISOSURFACE_ALPHA:
	case PASS_ISOSURFACE_ALPHA_GLOBAL:
		visible = 1;
		TwSetParam(pParametersBar, "Surface Color", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Iso Value", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "BS-Steps", "visible", TW_PARAM_INT32, 1, &visible);
		visible = 0;
		TwSetParam(pParametersBar, "Termination Alpha", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Transparency Factor", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "DVR Lighting", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Surface Color (2)", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Iso Value (2)", "visible", TW_PARAM_INT32, 1, &visible);
		g_globals.showTransferFunctionEditor = false;
		break;
	case PASS_DUAL_ISOSURFACE:
		visible = 1;
		TwSetParam(pParametersBar, "Surface Color", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Iso Value", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "BS-Steps", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Surface Color (2)", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Iso Value (2)", "visible", TW_PARAM_INT32, 1, &visible);
		visible = 0;
		TwSetParam(pParametersBar, "Termination Alpha", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Transparency Factor", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "DVR Lighting", "visible", TW_PARAM_INT32, 1, &visible);
		g_globals.showTransferFunctionEditor = false;
		break;
	case PASS_DVR:
		visible = 0;
		TwSetParam(pParametersBar, "Surface Color", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Iso Value", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "BS-Steps", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Surface Color (2)", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Iso Value (2)", "visible", TW_PARAM_INT32, 1, &visible);
		visible = 1;
		TwSetParam(pParametersBar, "Termination Alpha", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Transparency Factor", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "DVR Lighting", "visible", TW_PARAM_INT32, 1, &visible);
		g_globals.showTransferFunctionEditor = true;
		break;
	case PASS_MIP:
	case PASS_MIP2:
		visible = 1;
		visible = 0;
		TwSetParam(pParametersBar, "Iso Value", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "BS-Steps", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Surface Color (2)", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Iso Value (2)", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Surface Color", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Termination Alpha", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "Transparency Factor", "visible", TW_PARAM_INT32, 1, &visible);
		TwSetParam(pParametersBar, "DVR Lighting", "visible", TW_PARAM_INT32, 1, &visible);
		g_globals.showTransferFunctionEditor = true;
	}
}

void TW_CALL RayCaster::GetCurrentPassCB(void *value, void *clientData)
{ 
	RayCaster * me = reinterpret_cast<RayCaster*>(clientData);
	*(int *)value = me->m_currentPassSelection; 
}

RayCaster::RayCaster(ScalarVolumeData & volData) :
	m_volumeData(volData),
	m_currentPassSelection(RayCaster::PASS_DVR),
	m_isoValue(0.5f),
	m_isoValue2(0.7f),
	m_raycastStepsize(1.f),
	m_rayTerminationAlpha(0.95f),
	m_globalAlphaScale(1.0f),
	m_binSearchSteps(5),
	m_surfaceColor(XMFLOAT4(1, .5f, .5f, .5)),
	m_surfaceColor2(XMFLOAT4(.5f, .5f, 1, .5)),
	m_DVRlighting(false),
	m_raycastClippingBox(XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(1.f, 1.f, 1.f), true, true, true),
	m_boxLocked(true)
{
	m_volumeData.RegisterObserver(this);

	g_boxManipulationManager->AddBox(&m_raycastClippingBox);

	g_transferFunctionEditor->setHistogramSRV(m_volumeData.GetHistogramSRV());
	g_transferFunctionEditor->setShowHistogram(true);

	XMFLOAT3 bbox = m_volumeData.GetBoundingBox();

	m_modelTransform = XMMatrixScaling(bbox.x, bbox.y, bbox.z);

	g_globals.showTransferFunctionEditor = true;

	TwType digitType; 
	digitType = TwDefineEnum("DigitType", NULL, 0);
	TwAddVarRW(pParametersBar, "Lock Clipping Box", TW_TYPE_BOOLCPP, &m_boxLocked, "");
	TwAddVarCB(pParametersBar, "RaycastPass", digitType, SetCurrentPassCB, GetCurrentPassCB, this, "enum='0 {Box}, 1 {Front}, 2 {Back}, 3 {RaycastBox}, 4 {Iso}, 5 {Iso Alpha}, 9 {Iso Alpha Global}, 8 {Dual Iso}, 6 {DVR}, 7 {MIP}, 10 {MIP2}'");
	TwAddVarRW(pParametersBar, "Surface Color", TW_TYPE_COLOR4F, &m_surfaceColor.x, "");
	TwAddVarRW(pParametersBar, "Step Size (px)", TW_TYPE_FLOAT, &m_raycastStepsize, "min=0.05 max=5 step=0.05");
	TwAddVarRW(pParametersBar, "Iso Value", TW_TYPE_FLOAT, &m_isoValue, "min=0 max=1 step=0.01");
	TwAddVarRW(pParametersBar, "BS-Steps", TW_TYPE_INT32, &m_binSearchSteps, "min=0 max=10 step=1");
	TwAddVarRW(pParametersBar, "Termination Alpha", TW_TYPE_FLOAT, &m_rayTerminationAlpha, "min=0.8 max=1.0 step=0.05");
	TwAddVarRW(pParametersBar, "Transparency Factor", TW_TYPE_FLOAT, &m_globalAlphaScale, "min=0.001 max=2.0 step=0.005");
	TwAddVarRW(pParametersBar, "DVR Lighting", TW_TYPE_BOOLCPP, &m_DVRlighting, "");
	TwAddVarRW(pParametersBar, "Show TF Editor", TW_TYPE_BOOLCPP, &g_globals.showTransferFunctionEditor, "");
	TwAddVarRW(pParametersBar, "Surface Color (2)", TW_TYPE_COLOR4F, &m_surfaceColor2.x, "");
	TwAddVarRW(pParametersBar, "Iso Value (2)", TW_TYPE_FLOAT, &m_isoValue2, "min=0 max=1 step=0.01");

	SetCurrentPassCB(&m_currentPassSelection, this);
	int visible = 1;
	TwSetParam(pParametersBar, nullptr, "visible", TW_PARAM_INT32, 1, &visible);

	//setup the vertex buffer
	XMVECTOR c = XMLoadFloat3(&m_raycastClippingBox.center);
	XMVECTOR s = XMLoadFloat3(&m_raycastClippingBox.size);

	for(int i=0; i < 8; i++) {
		XMVECTOR d = XMLoadFloat4(&boxDisplacements[i]);
		XMStoreFloat4(&m_boxVertices[i], c + d * s);
		m_boxVertices[i].w = 1.0f;
	}
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);
	pContext->UpdateSubresource(pBoxVertexBuffer, 0, nullptr, m_boxVertices, sizeof(m_boxVertices), sizeof(m_boxVertices));
	SAFE_RELEASE(pContext);
}

RayCaster::~RayCaster(void)
{
	g_globals.showTransferFunctionEditor = false;
	TwRemoveVar(pParametersBar, "Lock Clipping Box");
	TwRemoveVar(pParametersBar, "RaycastPass");
	TwRemoveVar(pParametersBar, "Surface Color");
	TwRemoveVar(pParametersBar, "Step Size (px)");
	TwRemoveVar(pParametersBar, "Iso Value");
	TwRemoveVar(pParametersBar, "BS-Steps");
	TwRemoveVar(pParametersBar, "Termination Alpha");
	TwRemoveVar(pParametersBar, "Transparency Factor");
	TwRemoveVar(pParametersBar, "DVR Lighting");
	TwRemoveVar(pParametersBar, "Show TF Editor");
	TwRemoveVar(pParametersBar, "Surface Color (2)");
	TwRemoveVar(pParametersBar, "Iso Value (2)");

	
	int visible = 0;
	TwSetParam(pParametersBar, nullptr, "visible", TW_PARAM_INT32, 1, &visible);
	m_volumeData.UnregisterObserver(this);
	
	g_boxManipulationManager->RemoveBox(&m_raycastClippingBox);
}

void RayCaster::SaveConfig(SettingsStorage &store)
{
	store.StoreInt("raycaster.currentPassSelection", m_currentPassSelection);
	store.StoreFloat("raycaster.isoValue", m_isoValue);
	store.StoreFloat("raycaster.isoValue2", m_isoValue2);
	store.StoreFloat("raycaster.raycastStepsize", m_raycastStepsize);
	store.StoreFloat("raycaster.rayTerminationAlpha", m_rayTerminationAlpha);
	store.StoreFloat("raycaster.globalAlphaScale", m_globalAlphaScale);
	store.StoreInt("raycaster.binSearchSteps", m_binSearchSteps);
	store.StoreFloat4("raycaster.surfaceColor", &m_surfaceColor.x);
	store.StoreFloat4("raycaster.surfaceColor2", &m_surfaceColor2.x);
	store.StoreBool("raycaster.DVRlighting", m_DVRlighting);

}

void RayCaster::LoadConfig(SettingsStorage &store)
{
	int pass = m_currentPassSelection;
	store.GetInt("raycaster.currentPassSelection", pass);
	SetCurrentPassCB(&pass, this);

	store.GetFloat("raycaster.isoValue", m_isoValue);
	store.GetFloat("raycaster.isoValue2", m_isoValue2);
	store.GetFloat("raycaster.raycastStepsize", m_raycastStepsize);
	store.GetFloat("raycaster.rayTerminationAlpha", m_rayTerminationAlpha);
	store.GetFloat("raycaster.globalAlphaScale", m_globalAlphaScale);
	store.GetInt("raycaster.binSearchSteps", m_binSearchSteps);
	store.GetFloat4("raycaster.surfaceColor", &m_surfaceColor.x);
	store.GetFloat4("raycaster.surfaceColor2", &m_surfaceColor2.x);
	store.GetBool("raycaster.DVRlighting", m_DVRlighting);
}

// The opaque render pass actually just saves the transformation matrix for the bounding box
HRESULT RayCaster::Render(ID3D11DeviceContext * pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	RenderTransformations modelMtcs = sceneMtcs.PremultModelMatrix(m_modelTransform);
	m_raycastClippingBox.texToNDCSpace = modelMtcs.modelWorldViewProj;
	return S_OK;
}

HRESULT RayCaster::RenderTransparency(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	RenderTransformations modelMtcs = sceneMtcs.PremultModelMatrix(m_modelTransform);

	XMFLOAT4X4 mModelWorldViewProj, mModelWorldViewProjInv;
	XMStoreFloat4x4(&mModelWorldViewProj, modelMtcs.modelWorldViewProj);
	pWorldViewProjEV->SetMatrix((float*)mModelWorldViewProj.m);


	XMStoreFloat4x4(&mModelWorldViewProjInv, modelMtcs.modelWorldViewProjInv);
	pWorldViewProjInvEV->SetMatrix((float*)mModelWorldViewProjInv.m);

	//calculate camera position in object space
	XMVECTOR p = XMLoadFloat4(&XMFLOAT4(0, 0, 0, 1));
	auto camPos = XMVector4Transform(p, modelMtcs.modelWorldViewInv);
	pCamPosEV->SetFloatVector(camPos.m128_f32);
	pLightPosEV->SetFloatVector(camPos.m128_f32);
	//std::cout << camPos.m128_f32[0] << " " << camPos.m128_f32[1] << " " << camPos.m128_f32[2] <<  " " << camPos.m128_f32[3] << std::endl;

	XMINT3 res = m_volumeData.GetResolution();
	float avgResolution = (res.x + res.y + res.z)/3.0f;
	float raycastStepRel = m_raycastStepsize / avgResolution;

	pIsoValueEV->SetFloat(m_isoValue);
	pIsoValue2EV->SetFloat(m_isoValue2);
	pRaycastStepsizeEV->SetFloat(raycastStepRel);
	pRaycastPixelStepsizeEV->SetFloat(m_raycastStepsize);
	pBinSearchStepsEV->SetInt(m_binSearchSteps);
	pSurfaceColorEV->SetFloatVector(&m_surfaceColor.x);
	pSurfaceColor2EV->SetFloatVector(&m_surfaceColor2.x);

	pDVRLightingEV->SetBool(m_DVRlighting);
	pTerminationAlphaEV->SetFloat(m_rayTerminationAlpha);
	pGlobalAlphaScaleEV->SetFloat(m_globalAlphaScale);

	// setup the shading parameters
	pLightColorEV->SetFloatVector(&g_globals.lightColor.x);
	pAmbientEV->SetFloat(g_globals.mat_ambient);
	pDiffuseEV->SetFloat(g_globals.mat_diffuse);
	pSpecularEV->SetFloat(g_globals.mat_specular);
	pSpecularExpEV->SetFloat(g_globals.mat_specular_exp);

	pTransferFunctionEV->SetResource(g_transferFunctionEditor->getSRV());
	g_transferFunctionEditor->setHistogramSRV(m_volumeData.GetHistogramSRV());
	g_transferFunctionEditor->setShowHistogram(true);

	pTexVolumeEV->SetResource(m_volumeData.GetInterpolatedTextureSRV());
	if((m_DVRlighting && m_currentPassSelection == PASS_DVR) 
		|| m_currentPassSelection == PASS_ISOSURFACE 
		|| m_currentPassSelection == PASS_ISOSURFACE_ALPHA 
		|| m_currentPassSelection == PASS_DUAL_ISOSURFACE
		|| m_currentPassSelection == PASS_ISOSURFACE_ALPHA_GLOBAL) { //if we have dvr lighting or isosurface rendering, we need normals

		m_volumeData.SetNormalsRequired(true);
		pTexNormalVolumeEV->SetResource(m_volumeData.GetNormalTextureSRV());
	}
	else
		m_volumeData.SetNormalsRequired(false);

	UINT stride=sizeof(float[4]);
	UINT offset=0;

	pd3dImmediateContext->IASetVertexBuffers(0, 1, &pBoxVertexBuffer, &stride, &offset);
	pd3dImmediateContext->IASetIndexBuffer(pBoxIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	pd3dImmediateContext->IASetInputLayout(pBoxInputLayout);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// first render front face into the secondary render target
	ID3D11RenderTargetView * pRTV = DXUTGetD3D11RenderTargetView();
	ID3D11DepthStencilView * pDSV = DXUTGetD3D11DepthStencilView();
	pd3dImmediateContext->OMSetRenderTargets(1, &g_pSecondaryRenderTargetRTV, pDSV);
	float black[4] = {0, 0, 0, 1};
	pd3dImmediateContext->ClearRenderTargetView(g_pSecondaryRenderTargetRTV, black);
	pPasses[PASS_FRONT]->Apply(0, pd3dImmediateContext);
	pd3dImmediateContext->DrawIndexed(36, 0, 0);
	pd3dImmediateContext->OMSetRenderTargets(1, &pRTV, pDSV);
	
	//link as a shader input
	pRayEntryPointsEV->SetResource(g_pSecondaryRenderTargetSRV);
	
	ID3D11Resource * pDepthStencilResource;

	switch(m_currentPassSelection) {
	case PASS_BOX:
	case PASS_FRONT:
	case PASS_BACK:
	case PASS_BASIC_RAYCAST:
		pPasses[m_currentPassSelection]->Apply(0, pd3dImmediateContext);
		break;
	case PASS_ISOSURFACE:
	case PASS_ISOSURFACE_ALPHA:
	case PASS_DUAL_ISOSURFACE:
	case PASS_DVR:
	case PASS_MIP:
	case PASS_MIP2:
		//for isosurface alpha rendering we also need depth buffer
		DXUTGetD3D11DepthStencilView()->GetResource(&pDepthStencilResource);
		pd3dImmediateContext->CopyResource(g_pDepthBufferCopy, pDepthStencilResource);
		pDepthBufferEV->AsShaderResource()->SetResource(g_pDepthBufferCopySRV);
		SAFE_RELEASE(pDepthStencilResource);

		pPasses[m_currentPassSelection]->Apply(0, pd3dImmediateContext);
		break;
	case PASS_ISOSURFACE_ALPHA_GLOBAL:
				//for isosurface alpha rendering we also need depth buffer
		DXUTGetD3D11DepthStencilView()->GetResource(&pDepthStencilResource);
		pd3dImmediateContext->CopyResource(g_pDepthBufferCopy, pDepthStencilResource);
		pDepthBufferEV->AsShaderResource()->SetResource(g_pDepthBufferCopySRV);
		SAFE_RELEASE(pDepthStencilResource);
		transparencyEnvironment.BeginTransparency();
		pPasses[PASS_ISOSURFACE_ALPHA_GLOBAL]->Apply(0, pd3dImmediateContext);
		break;
	default:
		//some default pass to render something
		std::cerr << "Raycaster Warning: Current Pass selection invalid. Rendering default Pass..." << std::endl;
		pPasses[0]->Apply(0, pd3dImmediateContext);
		break;
	}
	
	pd3dImmediateContext->DrawIndexed(36, 0, 0);

	//remove the mappings from the shader inputs
	pTexVolumeEV->SetResource(nullptr);
	pTexNormalVolumeEV->SetResource(nullptr);
	pRayEntryPointsEV->SetResource(nullptr);

	if(m_currentPassSelection == PASS_ISOSURFACE_ALPHA_GLOBAL)
		transparencyEnvironment.EndTransparency(pd3dImmediateContext, pPasses[PASS_ISOSURFACE_ALPHA_GLOBAL]);
	else
		pPasses[m_currentPassSelection]->Apply(0, pd3dImmediateContext);

	return S_OK;
}

void RayCaster::FrameMove(double dTime, float fElapsedTime, float fElapsedLogicTime)
{
	if(m_raycastClippingBox.changed) 
	{
		XMVECTOR c = XMLoadFloat3(&m_raycastClippingBox.center);
		XMVECTOR s = XMLoadFloat3(&m_raycastClippingBox.size);

		for(int i=0; i < 8; i++) {
			XMVECTOR d = XMLoadFloat4(&boxDisplacements[i]);
			XMStoreFloat4(&m_boxVertices[i], c + d * s);
			m_boxVertices[i].w = 1.0f;
		}
		ID3D11DeviceContext * pContext;
		pd3dDevice->GetImmediateContext(&pContext);
		pContext->UpdateSubresource(pBoxVertexBuffer, 0, nullptr, m_boxVertices, sizeof(m_boxVertices), sizeof(m_boxVertices));
		SAFE_RELEASE(pContext);
	}
	m_raycastClippingBox.moveable = !m_boxLocked;
	m_raycastClippingBox.scalable = !m_boxLocked;
}