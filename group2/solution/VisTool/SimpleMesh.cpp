#include "SimpleMesh.h"

#include "util/util.h"
#include "Globals.h"

#include <sstream>
#include <iostream>
#include <exception>

ID3DX11Effect			* SimpleMesh::pEffect = nullptr;
ID3DX11EffectTechnique	* SimpleMesh::pTechnique = nullptr;
ID3DX11EffectPass		* SimpleMesh::pPass = nullptr;
ID3D11InputLayout		* SimpleMesh::pInputLayout = nullptr;
ID3D11Device			* SimpleMesh::pd3dDevice = nullptr;
ID3DX11EffectVariable	* SimpleMesh::pWorldViewProjEV = nullptr;
ID3DX11EffectVariable	* SimpleMesh::pMeshColorEV = nullptr;
ID3DX11EffectVariable	* SimpleMesh::pLightPosEV = nullptr;

ID3DX11EffectVariable	* SimpleMesh::pLightColorEV = nullptr;
ID3DX11EffectVariable	* SimpleMesh::pAmbientEV = nullptr;
ID3DX11EffectVariable	* SimpleMesh::pDiffuseEV = nullptr;
ID3DX11EffectVariable	* SimpleMesh::pSpecularEV = nullptr;
ID3DX11EffectVariable	* SimpleMesh::pSpecularExpEV = nullptr;


ID3D11Buffer			* SimpleMesh::pBBoxIndexBuffer = nullptr;

TwBar					* SimpleMesh::pParametersBar = nullptr;

unsigned int SimpleMesh::bboxIndices[] = {
	0, 1,   2, 3,   4, 5,   6, 7,
	0, 2,   1, 3,   4, 6,   5, 7,
	0, 4,   1, 5,   2, 6,   3, 7
};
float SimpleMesh::red[] = {1.0, 0.0, 0.0, 1.0};
float SimpleMesh::green[] = {0.0, 1.0, 0.0, 1.0};
float SimpleMesh::blue[] = {0.0, 0.0, 1.0, 1.0};

unsigned int SimpleMesh::instanceCount = 0;

TwType SimpleMesh::meshType;

HRESULT SimpleMesh::Initialize(ID3D11Device * _pd3dDevice, TwBar * pParametersBar_)
{
	HRESULT hr;

	pd3dDevice = _pd3dDevice;

	SetupTwBar(pParametersBar_);

	std::wstring effectPath = GetExePath() + L"SimpleMesh.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &pEffect)))
	{
		std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

	pTechnique = pEffect->GetTechniqueByName("SimpleMesh");
	pPass = pTechnique->GetPassByName("P0");

	// create input layout
	auto AAE = D3D11_APPEND_ALIGNED_ELEMENT;
	auto IPVD = D3D11_INPUT_PER_VERTEX_DATA;
	// Array of descriptions for each vertexattribute
	D3D11_INPUT_ELEMENT_DESC layout[]= {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, AAE, IPVD, 0},
		{"NORMAL" , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, AAE, IPVD, 0},
	};
	UINT numElements=sizeof(layout)/sizeof(layout[0]);
	// Get input signature of pass using this layout
	D3DX11_PASS_DESC passDesc;
	pPass->GetDesc(&passDesc);
	// Create the input layout
	V_RETURN(pd3dDevice->CreateInputLayout(layout,numElements,
		passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &pInputLayout));

	pWorldViewProjEV = pEffect->GetVariableByName("g_worldViewProj");
	pMeshColorEV = pEffect->GetVariableByName("g_meshColor");

	assert(pWorldViewProjEV);
	assert(pMeshColorEV);

	SAFE_GET_VECTOR(pEffect, "g_lightPos", pLightPosEV);
	SAFE_GET_VECTOR(pEffect, "g_lightColor", pLightColorEV);
	SAFE_GET_SCALAR(pEffect, "k_a", pAmbientEV);
	SAFE_GET_SCALAR(pEffect, "k_d", pDiffuseEV);
	SAFE_GET_SCALAR(pEffect, "k_s", pSpecularEV);
	SAFE_GET_SCALAR(pEffect, "g_specularExp", pSpecularExpEV);

	// Create the index buffer for the bounding box edges
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.Usage           = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth       = sizeof( bboxIndices );
	bufferDesc.BindFlags       = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags  = 0;
	bufferDesc.MiscFlags       = 0;

	// Define the resource data.
	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = bboxIndices;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	// Create the buffer with the device.
	hr = pd3dDevice->CreateBuffer( &bufferDesc, &InitData, &pBBoxIndexBuffer );
	if( FAILED( hr ) )
		return hr;

	return S_OK;
}

HRESULT SimpleMesh::Release(void)
{

	SAFE_RELEASE(pEffect);
	SAFE_RELEASE(pInputLayout);
	SAFE_RELEASE(pBBoxIndexBuffer);

	return S_OK;
}

HRESULT SimpleMesh::DrawBoundingBox(ID3D11DeviceContext* pd3dImmediateContext, XMMATRIX texToNDC, XMFLOAT3 size)
{
	if(!g_globals.showBoundingBoxes)
		return S_OK;

	//XMMATRIX modelTransform = GetModelTransform();
	XMFLOAT4X4 mWorldViewProj, mWorldViewProjInvTransp;
	XMStoreFloat4x4(&mWorldViewProj, XMMatrixScaling(size.x, size.y, size.z) * texToNDC);
	
	pWorldViewProjEV->AsMatrix()->SetMatrix((float*)mWorldViewProj.m);

	//setup the shading parameters
	pLightColorEV->AsVector()->SetFloatVector(&g_globals.lightColor.x);
	pAmbientEV->AsScalar()->SetFloat(g_globals.mat_ambient);
	pDiffuseEV->AsScalar()->SetFloat(g_globals.mat_diffuse);
	pSpecularEV->AsScalar()->SetFloat(g_globals.mat_specular);
	pSpecularExpEV->AsScalar()->SetFloat(g_globals.mat_specular_exp);

	pd3dImmediateContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
	pd3dImmediateContext->IASetIndexBuffer(pBBoxIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	pd3dImmediateContext->IASetInputLayout(nullptr);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);

	// assignment 1.2
	// probably not the most performant version, but it works with line colors
	// TODO: hardcode the vertex positions and colors and just render a static mesh with one draw call
	pMeshColorEV->AsVector()->SetFloatVector(red);
	pEffect->GetTechniqueByIndex(0)->GetPassByIndex(1)->Apply(0, pd3dImmediateContext);
	pd3dImmediateContext->DrawIndexed(8, 0, 0);
	pMeshColorEV->AsVector()->SetFloatVector(green);
	pEffect->GetTechniqueByIndex(0)->GetPassByIndex(1)->Apply(0, pd3dImmediateContext);
	pd3dImmediateContext->DrawIndexed(8, 8, 0);
	pMeshColorEV->AsVector()->SetFloatVector(blue);
	pEffect->GetTechniqueByIndex(0)->GetPassByIndex(1)->Apply(0, pd3dImmediateContext);
	pd3dImmediateContext->DrawIndexed(8, 16, 0);

	return S_OK;
}

void SimpleMesh::SetupTwBar(TwBar * pParametersBar_) {

	pParametersBar = TwNewBar("Meshes");
	TwDefine("Meshes color='30 30 150' size='300 160' position='15 625' iconified=true");

	static TwStructMember posMembers[] = // array used to describe tweakable variables of the Light structure
    {
        { "x",    TW_TYPE_FLOAT, 0 * sizeof(float),    "step=0.4" }, 
        { "y",    TW_TYPE_FLOAT, 1 * sizeof(float),     "step=0.4" }, 
        { "z",    TW_TYPE_FLOAT, 2 * sizeof(float),    "step=0.4" },
    };
	TwType posType = TwDefineStruct("Position", posMembers, 3, sizeof(float[4]), NULL, NULL);

	static TwStructMember rotMembers[] = // array used to describe tweakable variables of the Light structure
    {
        { "x",    TW_TYPE_FLOAT, 0 * sizeof(float),    "min=0 max=6.283 step=0.1" }, 
        { "y",    TW_TYPE_FLOAT, 1 * sizeof(float),     "min=0 max=6.283 step=0.1" }, 
        { "z",    TW_TYPE_FLOAT, 2 * sizeof(float),    "min=0 max=6.283 step=0.1" },
    };
	TwType rotType = TwDefineStruct("Rotation", rotMembers, 3, sizeof(float[4]), NULL, NULL);

	static TwStructMember scaleMembers[] = // array used to describe tweakable variables of the Light structure
    {
        { "x",    TW_TYPE_FLOAT, 0 * sizeof(float),    "step=0.1" }, 
        { "y",    TW_TYPE_FLOAT, 1 * sizeof(float),     "step=0.1" }, 
        { "z",    TW_TYPE_FLOAT, 2 * sizeof(float),    "step=0.1" },
    };
	TwType scaleType = TwDefineStruct("Vector", scaleMembers, 3, sizeof(float[4]), NULL, NULL);

	// Define a new struct type: light variables are embedded in this structure
    static TwStructMember meshMembers[] = // array used to describe tweakable variables of the Light structure
    {
        { "Position",    posType, offsetof(SimpleMesh, m_position),    "" }, 
        { "Rotation",    rotType, offsetof(SimpleMesh, m_rotation),     "" }, 
        { "Scale",		scaleType,   offsetof(SimpleMesh, m_scale),    "" },
        { "Color",		TW_TYPE_COLOR4F,  offsetof(SimpleMesh, m_color), "coloralpha=false" }
    };
    meshType = TwDefineStruct("Mesh", meshMembers, 4, sizeof(SimpleMesh), NULL, NULL);  // create a new TwType associated to the struct defined by the lightMembers array

}

SimpleMesh::SimpleMesh(std::string name) :
	m_position(0, 0, 0),
	m_rotation(0, 0, 0),
	m_scale(1, 1, 1),
	m_color(1, 1, 1, 1),
	m_bboxSize(1, 1, 1),
	m_pIndexBuffer(nullptr),
	m_pVertexBuffer(nullptr)
{
	instanceCount++;
	if(name.empty()) {
		std::stringstream ss;
		ss << "Mesh # " << instanceCount;
		m_name = ss.str();
	}
	else
		m_name = name;
	m_barVarName = std::string("Mesh [") + m_name + "]";

	TwAddVarRW(pParametersBar, m_barVarName.c_str(), meshType, this, "");
}



SimpleMesh::SimpleMesh(XMFLOAT3 bboxSize, std::string name) :
	m_position(0, 0, 0),
	m_rotation(0, 0, 0),
	m_scale(1, 1, 1),
	m_color(1, 1, 1, 1),
	m_bboxSize(bboxSize),
	m_pIndexBuffer(nullptr),
	m_pVertexBuffer(nullptr)
{
	instanceCount++;
	if(name.empty())
		m_name = "Mesh #" + instanceCount;
	else
		m_name = name;

	m_barVarName = "";//std::string("Mesh Color [") + m_name + "]";

	//TwAddVarRW(pParametersBar, m_barVarName.c_str(), TW_TYPE_COLOR4F, &m_color.x, "group='Mesh'");
}

SimpleMesh::~SimpleMesh(void)
{
	SAFE_RELEASE(m_pIndexBuffer);
	SAFE_RELEASE(m_pVertexBuffer);

	if(!m_barVarName.empty()) {
		TwRemoveVar(pParametersBar, m_barVarName.c_str());
	}
}

void SimpleMesh::LoadConfig(SettingsStorage &store)
{
	store.GetFloat3("meshes." + m_name + ".position", &m_position.x);
	store.GetFloat3("meshes." + m_name + ".rotation", &m_rotation.x);
	store.GetFloat3("meshes." + m_name + ".scale", &m_scale.x);
	store.GetFloat4("meshes." + m_name + ".color", &m_color.x);
	store.GetFloat3("meshes." + m_name + ".bboxSize", &m_bboxSize.x);
}

void SimpleMesh::SaveConfig(SettingsStorage &store)
{
	store.StoreFloat3("meshes." + m_name + ".position", &m_position.x);
	store.StoreFloat3("meshes." + m_name + ".rotation", &m_rotation.x);
	store.StoreFloat3("meshes." + m_name + ".scale", &m_scale.x);
	store.StoreFloat4("meshes." + m_name + ".color", &m_color.x);
	store.StoreFloat3("meshes." + m_name + ".bboxSize", &m_bboxSize.x);
}


// called by subclasses after the m_vertices and m_indices arrays have been assigned
HRESULT SimpleMesh::SetupGPUBuffers()
{
	HRESULT hr;

	//setup vertex buffer
	D3D11_BUFFER_DESC bd = {};//init to binary zero = defaultvalues
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(SimpleVertex)* (UINT)m_vertices.size();//size in bytes
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	//Define initial data
	D3D11_SUBRESOURCE_DATA initData = {};
	initData.pSysMem = &m_vertices[0];//pointer to the array
	//Create vertex buffer
	V_RETURN(pd3dDevice->CreateBuffer(&bd, &initData, &m_pVertexBuffer));

	//Createandfillthedescription
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(uint32_t) * (UINT)m_indices.size(); //sizeinbytes
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	//Define initial data
	initData.pSysMem = &m_indices[0]; //pointer to the array
	//Create index buffer
	ID3D11Buffer* pIndexBuffer= nullptr;
	V_RETURN(pd3dDevice->CreateBuffer(&bd,&initData,&m_pIndexBuffer));

	return S_OK;
}

HRESULT SimpleMesh::Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations camMtcs)
{
	XMMATRIX modelTransform = GetModelTransform();

	RenderTransformations modelMtcs = camMtcs.PremultModelMatrix(modelTransform);

	DrawBoundingBox(pd3dImmediateContext, modelMtcs.modelWorldViewProj, GetBoundingBox());

	//no mesh -> don't draw
	if(!m_pIndexBuffer)
		return S_OK;

	XMFLOAT4X4 mModelWorldViewProj;//, mWorldViewProjInvTransp, mModelTransform;
	XMStoreFloat4x4(&mModelWorldViewProj, modelMtcs.modelWorldViewProj);

	pWorldViewProjEV->AsMatrix()->SetMatrix((float*)mModelWorldViewProj.m);
	pMeshColorEV->AsVector()->SetFloatVector((float*)&m_color);

	//transform light into object coordinate system
	// because we do lighting in object space for
	XMVECTOR p = XMLoadFloat4(&XMFLOAT4(0, 0, 0, 1));
	auto camPos = XMVector4Transform(p, modelMtcs.modelWorldViewInv);
	pLightPosEV->AsVector()->SetFloatVector(camPos.m128_f32);

	// setup the shading parameters
	pLightColorEV->AsVector()->SetFloatVector(&g_globals.lightColor.x);
	pAmbientEV->AsScalar()->SetFloat(g_globals.mat_ambient);
	pDiffuseEV->AsScalar()->SetFloat(g_globals.mat_diffuse);
	pSpecularEV->AsScalar()->SetFloat(g_globals.mat_specular);
	pSpecularExpEV->AsScalar()->SetFloat(g_globals.mat_specular_exp);

	UINT stride=sizeof(SimpleVertex);
	UINT offset=0;

	pd3dImmediateContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
	pd3dImmediateContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	pd3dImmediateContext->IASetInputLayout(pInputLayout);
	pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	
	pPass->Apply(0, pd3dImmediateContext);
	pd3dImmediateContext->DrawIndexed((UINT)m_indices.size(), 0, 0);
	return S_OK;
}

void SimpleMesh::SetPosition(XMFLOAT3 &position)
{
	m_position = position;
}
XMFLOAT3 SimpleMesh::GetPosition()
{
	return m_position;
}

void SimpleMesh::SetRotation(XMFLOAT3 &rotation)
{
	m_rotation = rotation;
}
XMFLOAT3 SimpleMesh::GetRotation()
{
	return m_rotation;
}

void SimpleMesh::SetScale(XMFLOAT3 &scale)
{
	m_scale = scale;
}
XMFLOAT3 SimpleMesh::GetScale()
{
	return m_scale;
}

void SimpleMesh::SetBoundingBox(XMFLOAT3 &bbox)
{
	m_bboxSize = bbox;
}
XMFLOAT3 SimpleMesh::GetBoundingBox()
{
	return m_bboxSize;
}

void SimpleMesh::SetColor(XMFLOAT4 &color)
{
	m_color = color;
}

XMFLOAT4 SimpleMesh::GetColor()
{
	return m_color;
}

void SimpleMesh::SetName(std::string name)
{
	m_name = name;
}

std::string SimpleMesh::GetName(void)
{
	return m_name;
}

XMMATRIX SimpleMesh::GetModelTransform()
{
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_rotation.x, m_rotation.y, m_rotation.z);
	XMMATRIX scale = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
	XMMATRIX trans = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
	
	return scale * rot * trans;
}