#pragma once

#include "util/util.h"

#include "SettingsStorage.h"

#include <DirectXMath.h>
#include <d3dx11effect.h>
#include <DXUT.h>
#include <DXUTcamera.h>
using namespace DirectX;

#include <AntTweakBar.h>

#include <string>
#include <vector>

struct SimpleVertex{
	XMFLOAT3 pos;// Position
	XMFLOAT3 nor; // Normal
};

class SimpleMesh
{
public:
	//statics
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);

	static HRESULT DrawBoundingBox(ID3D11DeviceContext* pd3dImmediateContext, XMMATRIX texToNDC, XMFLOAT3 size = XMFLOAT3(1.0f, 1.0f, 1.0f));

	static unsigned int instanceCount;

	//constructors & destructor
	SimpleMesh(std::string name = "");
	SimpleMesh(XMFLOAT3 bboxSize, std::string name = "");
	virtual ~SimpleMesh(void);

	//methods
	virtual void SaveConfig(SettingsStorage &store);
	virtual void LoadConfig(SettingsStorage &store);
	virtual HRESULT Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations camMtcs);

	//accessors
	void SetPosition(XMFLOAT3 &position);
	XMFLOAT3 GetPosition();

	void SetRotation(XMFLOAT3 &rotation);
	XMFLOAT3 GetRotation();

	void SetScale(XMFLOAT3 &scale);
	XMFLOAT3 GetScale();

	void SetColor(XMFLOAT4 &color);
	XMFLOAT4 GetColor();

	void SetBoundingBox(XMFLOAT3 &bbox);
	XMFLOAT3 GetBoundingBox();

	std::string GetName();
	void SetName(std::string);

	XMMATRIX GetModelTransform();

protected:
	//statics
	static void SetupTwBar(TwBar* pParametersBar);

	static ID3DX11Effect * pEffect;
	static ID3DX11EffectTechnique * pTechnique;
	static ID3DX11EffectPass * pPass;
	static ID3D11InputLayout * pInputLayout;
	static ID3D11Device * pd3dDevice;
	static ID3DX11EffectVariable * pWorldViewProjEV;
	static ID3DX11EffectVariable * pMeshColorEV;
	static ID3DX11EffectVariable * pLightPosEV;

	static ID3DX11EffectVariable	* pLightColorEV;
	static ID3DX11EffectVariable	* pAmbientEV;
	static ID3DX11EffectVariable	* pDiffuseEV;
	static ID3DX11EffectVariable	* pSpecularEV;
	static ID3DX11EffectVariable	* pSpecularExpEV;

	static TwBar *	pParametersBar;

	static ID3D11Buffer * pBBoxIndexBuffer;
	static unsigned int bboxIndices[24];
	static float red[4], green[4], blue[4];

	static TwType meshType;

	//methods
	HRESULT SetupGPUBuffers();

	//members
	XMFLOAT3 m_position;
	XMFLOAT3 m_rotation;
	XMFLOAT3 m_scale;
	XMFLOAT4 m_color;
	XMFLOAT3 m_bboxSize;

	std::string m_name;
	std::string m_barVarName;

	std::vector<SimpleVertex> m_vertices;
	std::vector<unsigned int> m_indices;
	ID3D11Buffer * m_pVertexBuffer;
	ID3D11Buffer * m_pIndexBuffer;
};

