#include "GlyphVisualizer.h"

#include "util/util.h"
#include "Globals.h"

#include <iostream>

ID3DX11Effect			* GlyphVisualizer::pEffect = nullptr;
ID3DX11EffectTechnique	* GlyphVisualizer::pTechnique = nullptr;
ID3D11Device			* GlyphVisualizer::pd3dDevice = nullptr;
ID3DX11EffectPass		* GlyphVisualizer::pPasses[NUM_PASSES];
TwBar					* GlyphVisualizer::pParametersBar = nullptr;
TwType					GlyphVisualizer::vectfType;
TwType					GlyphVisualizer::vectiType;

// pass names in the effect file
char * GlyphVisualizer::passNames[] = {
		"PASS_RENDER"
};

//Effect Variables
ID3DX11EffectMatrixVariable	* GlyphVisualizer::pWorldViewProjEV = nullptr;
ID3DX11EffectScalarVariable	* GlyphVisualizer::pTimestepTEV = nullptr;
ID3DX11EffectVectorVariable	* GlyphVisualizer::pCamPosEV = nullptr;
ID3DX11EffectVectorVariable	* GlyphVisualizer::pNumGlyphsEV = nullptr;
ID3DX11EffectVectorVariable	* GlyphVisualizer::pGlyphSpacingEV = nullptr;
ID3DX11EffectVectorVariable	* GlyphVisualizer::pSliceCenterEV = nullptr;
ID3DX11EffectScalarVariable	* GlyphVisualizer::pGlyphSizeEV = nullptr;

ID3DX11EffectShaderResourceVariable	* GlyphVisualizer::pTexVolume0EV = nullptr;
ID3DX11EffectShaderResourceVariable	* GlyphVisualizer::pTexVolume1EV = nullptr;

HRESULT GlyphVisualizer::Initialize(ID3D11Device * pd3dDevice_, TwBar* pParametersBar_)
{
	HRESULT hr;

	pd3dDevice = pd3dDevice_;

	std::wstring effectPath = GetExePath() + L"GlyphVisualizer.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &pEffect)))
	{
		std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

	SAFE_GET_TECHNIQUE(pEffect, "GlyphVisualizer", pTechnique);
	for(int i=0; i < NUM_PASSES; i++)
		SAFE_GET_PASS(pTechnique, passNames[i], pPasses[i]);

	SAFE_GET_MATRIX(pEffect, "g_worldViewProj", pWorldViewProjEV);
	SAFE_GET_SCALAR(pEffect, "g_timestepT", pTimestepTEV);
	SAFE_GET_VECTOR(pEffect, "g_sliceCenter", pSliceCenterEV);
	SAFE_GET_VECTOR(pEffect, "g_numGlyphs", pNumGlyphsEV);
	SAFE_GET_VECTOR(pEffect, "g_glyphSpacing", pGlyphSpacingEV);
	SAFE_GET_VECTOR(pEffect, "g_camPos", pCamPosEV);
	SAFE_GET_SCALAR(pEffect, "g_glyphSize", pGlyphSizeEV);

	SAFE_GET_RESOURCE(pEffect, "g_flowFieldTex0", pTexVolume0EV);
	SAFE_GET_RESOURCE(pEffect, "g_flowFieldTex1", pTexVolume1EV);

	pParametersBar = TwNewBar("Glyph Visualizer");
	TwDefine("'Glyph Visualizer' color='10 200 200' size='300 180' position='1600 455' visible=false");

	static TwStructMember vectfTypeMemers[] = // array used to describe tweakable variables of the Light structure
    {
        { "x",    TW_TYPE_FLOAT, 0 * sizeof(float),    "" }, 
        { "y",    TW_TYPE_FLOAT, 1 * sizeof(float),    "" }, 
        { "z",    TW_TYPE_FLOAT, 2 * sizeof(float),    "" },
    };
	vectfType = TwDefineStruct("VectorF", vectfTypeMemers, 3, sizeof(float[3]), NULL, NULL);
	
	static TwStructMember vectiTypeMemers[] = // array used to describe tweakable variables of the Light structure
    {
		{ "x",    TW_TYPE_INT32, 0 * sizeof(int),    "min=1" }, 
        { "y",    TW_TYPE_INT32, 1 * sizeof(int),    "min=1" }, 
        { "z",    TW_TYPE_INT32, 2 * sizeof(int),    "min=1" },
    };
	vectiType = TwDefineStruct("VectorI", vectiTypeMemers, 3, sizeof(int[3]), NULL, NULL);
	return S_OK;
}

HRESULT GlyphVisualizer::Release(void)
{
	for(int i=0; i < NUM_PASSES; i++)
		SAFE_RELEASE(pPasses[i]);

	SAFE_RELEASE(pTechnique);
	SAFE_RELEASE(pEffect);
	return S_OK;
}


void TW_CALL GlyphVisualizer::SetSliceDirectionCB(const void *value, void *clientData)
{ 
	GlyphVisualizer * me = reinterpret_cast<GlyphVisualizer*>(clientData);
	int slice = *(const int *)value;

	if(slice == 0) {
		me->m_numGlyphs = XMINT3(1, int(ceilf(1.f/me->m_glyphSpacing.y)), int(ceilf(1.f/me->m_glyphSpacing.z)));
	}
	else if(slice == 1) {
		me->m_numGlyphs = XMINT3(int(ceilf(1.f/me->m_glyphSpacing.x)), 1, int(ceilf(1.f/me->m_glyphSpacing.z)));
	}
	else {
		me->m_numGlyphs = XMINT3(int(ceilf(1.f/me->m_glyphSpacing.x)), int(ceilf(1.f/me->m_glyphSpacing.y)), 1);
	};
}

void TW_CALL GlyphVisualizer::GetSliceDirectionCB(void *value, void *clientData)
{ 
	GlyphVisualizer * me = reinterpret_cast<GlyphVisualizer*>(clientData);
	if(me->m_numGlyphs.x == 1)
		*(int *)value = 0; 
	else if(me->m_numGlyphs.y == 1)
		*(int *)value = 1; 
	else
		*(int *)value = 2; 
}

void TW_CALL GlyphVisualizer::SetGlyphSpacingCB(const void *value, void *clientData)
{ 
	GlyphVisualizer * me = reinterpret_cast<GlyphVisualizer*>(clientData);
	float spacing = *(const float *)value;
	auto res = me->m_volumeData.GetResolution();
	me->m_glyphSpacing = XMFLOAT3(spacing / res.x, spacing / res.y, spacing / res.z);
	int sliceDir;
	GetSliceDirectionCB(&sliceDir, clientData);
	SetSliceDirectionCB(&sliceDir, clientData);
}

void TW_CALL GlyphVisualizer::GetGlyphSpacingCB(void *value, void *clientData)
{ 
	GlyphVisualizer * me = reinterpret_cast<GlyphVisualizer*>(clientData);
	auto res = me->m_volumeData.GetResolution();
	*(float *)value = (me->m_glyphSpacing.x * res.x + me->m_glyphSpacing.y * res.y + me->m_glyphSpacing.z * res.z)/3.f;
}

void TW_CALL GlyphVisualizer::SetSliceCB(const void *value, void *clientData)
{ 
	GlyphVisualizer * me = reinterpret_cast<GlyphVisualizer*>(clientData);
	int slice = *(const int *)value;

	int sliceDir;
	GetSliceDirectionCB(&sliceDir, clientData);
	auto res = me->m_volumeData.GetResolution();

	if(sliceDir == 0)
		me->m_sliceCenter = XMFLOAT3(std::min(1.f, std::max(0.f, slice / (float)res.x)), 0.5f, 0.5f); 
	else if(sliceDir == 1)
		me->m_sliceCenter = XMFLOAT3(0.5f, std::min(1.f, std::max(0.f, slice/(float)res.y)), 0.5f);
	else
		me->m_sliceCenter = XMFLOAT3(0.5, 0.5f, std::min(1.f, std::max(0.f, slice/(float)res.z)));
}

void TW_CALL GlyphVisualizer::GetSliceCB(void *value, void *clientData)
{ 
	GlyphVisualizer * me = reinterpret_cast<GlyphVisualizer*>(clientData);
	int sliceDir;
	GetSliceDirectionCB(&sliceDir, clientData);
	auto res = me->m_volumeData.GetResolution();


	if(sliceDir == 0)
		*(int *)value = int(res.x * me->m_sliceCenter.x); 
	else if(sliceDir == 1)
		*(int *)value = int(res.y * me->m_sliceCenter.y); 
	else
		*(int *)value = int(res.z * me->m_sliceCenter.z); 
}

GlyphVisualizer::GlyphVisualizer(VectorVolumeData & volData) :
	m_volumeData(volData),
	m_sliceCenter(XMFLOAT3(0.5f, 0.5f, 0.5f)),
	m_glyphSpacing(XMFLOAT3(3.f/volData.GetResolution().x, 3.f/volData.GetResolution().y, 3.f/volData.GetResolution().z)),
	m_numGlyphs(int(volData.GetResolution().x/3.f), int(volData.GetResolution().y/3.f), 1),
	m_glyphSize(3.f)
{
	m_volumeData.RegisterObserver(this);
	XMFLOAT3 bbox = m_volumeData.GetBoundingBox();
	m_modelTransform = XMMatrixScaling(bbox.x, bbox.y, bbox.z);

	TwType dirType = TwDefineEnum("DirType", NULL, 0);
	TwAddVarCB(pParametersBar, "Slice Direction", dirType, SetSliceDirectionCB, GetSliceDirectionCB, this, "enum='0 {X}, 1 {Y}, 2 {Z}'");
	TwAddVarCB(pParametersBar, "Slice Index", TW_TYPE_INT32, SetSliceCB, GetSliceCB, this, "");
	TwAddVarCB(pParametersBar, "Glyph Spacing [voxels]", TW_TYPE_FLOAT, SetGlyphSpacingCB, GetGlyphSpacingCB, this, "min=0.1 max=20 step=0.2");
	TwAddVarRW(pParametersBar, "Glyph Size", TW_TYPE_FLOAT, &m_glyphSize, "min=0.01 max=20 step=0.05");

	TwAddVarRW(pParametersBar, "Center [tex coords]", vectfType, &m_sliceCenter, "group='Advanced'");
	TwAddVarRW(pParametersBar, "Glyph Spacing [tex coords]", vectfType, &m_glyphSpacing, "group='Advanced'");
	TwAddVarRW(pParametersBar, "Number of Glyphs", vectiType, &m_numGlyphs, "group='Advanced'");

	int visible = 1;
	TwSetParam(pParametersBar, nullptr, "visible", TW_PARAM_INT32, 1, &visible);
}

GlyphVisualizer::~GlyphVisualizer(void)
{
	TwRemoveVar(pParametersBar, "Slice Direction");
	TwRemoveVar(pParametersBar, "Slice Index");
	TwRemoveVar(pParametersBar, "Glyph Spacing [voxels]");
	TwRemoveVar(pParametersBar, "Glyph Size");
	TwRemoveVar(pParametersBar, "Center [tex coords]");
	TwRemoveVar(pParametersBar, "Glyph Spacing [tex coords]");
	TwRemoveVar(pParametersBar, "Number of Glyphs");

	int visible = 0;
	TwSetParam(pParametersBar, nullptr, "visible", TW_PARAM_INT32, 1, &visible);
	m_volumeData.UnregisterObserver(this);
}

void GlyphVisualizer::SaveConfig(SettingsStorage &store)
{
	store.StoreFloat3("glyphvisualizer.sliceCenter", &m_sliceCenter.x);
	store.StoreFloat3("glyphvisualizer.glyphSpacing", &m_glyphSpacing.x);
	store.StoreInt3("glyphvisualizer.numGlyphs", &m_numGlyphs.x);
	store.StoreFloat("glyphvisualizer.glyphSize", m_glyphSize);
}

void GlyphVisualizer::LoadConfig(SettingsStorage &store)
{
	store.GetFloat3("glyphvisualizer.sliceCenter", &m_sliceCenter.x);
	store.GetFloat3("glyphvisualizer.glyphSpacing", &m_glyphSpacing.x);
	store.GetInt3("glyphvisualizer.numGlyphs", &m_numGlyphs.x);
	store.GetFloat("glyphvisualizer.glyphSize", m_glyphSize);
}

HRESULT GlyphVisualizer::Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations sceneMtcs)
{
	RenderTransformations modelMtcs = sceneMtcs.PremultModelMatrix(m_modelTransform);

	XMFLOAT4X4 mModelWorldViewProj;
	XMStoreFloat4x4(&mModelWorldViewProj, modelMtcs.modelWorldViewProj);
	pWorldViewProjEV->SetMatrix((float*)mModelWorldViewProj.m);

	//calculate camera position in object space
	//XMMATRIX mViewProjInv = XMMatrixInverse(nullptr, cam->GetViewMatrix() * cam->GetProjMatrix());
	//XMMATRIX mWorldView = m_modelTransform * mWorldViewProj * mViewProjInv;
	//XMMATRIX mWorldViewInv = XMMatrixInverse(nullptr, mWorldView);
	XMVECTOR p = XMLoadFloat4(&XMFLOAT4(0, 0, 0, 1));
	auto camPos = XMVector4Transform(p, modelMtcs.modelWorldViewInv);
	camPos /= XMVectorGetW(camPos);
	pCamPosEV->SetFloatVector(camPos.m128_f32);

	//set the other variables
	pTimestepTEV->SetFloat(m_volumeData.GetCurrentTimestepT());
	pNumGlyphsEV->SetIntVector(&m_numGlyphs.x);
	pSliceCenterEV->SetFloatVector(&m_sliceCenter.x);
	pGlyphSpacingEV->SetFloatVector(&m_glyphSpacing.x);
	pGlyphSizeEV->SetFloat(m_glyphSize);

	pTexVolume0EV->SetResource(m_volumeData.GetTexture0SRV());
	pTexVolume1EV->SetResource(m_volumeData.GetTexture1SRV());

	pd3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	pd3dImmediateContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

	pPasses[PASS_RENDER]->Apply(0, pd3dImmediateContext);
			
	pd3dImmediateContext->Draw(m_numGlyphs.x * m_numGlyphs.y * m_numGlyphs.z, 0);

	//remove the mappings from the shader inputs
	pTexVolume0EV->SetResource(nullptr);
	pTexVolume1EV->SetResource(nullptr);
	pPasses[PASS_RENDER]->Apply(0, pd3dImmediateContext);

	return S_OK;
}

void GlyphVisualizer::FrameMove(double dTime, float fElapsedTime) {

}