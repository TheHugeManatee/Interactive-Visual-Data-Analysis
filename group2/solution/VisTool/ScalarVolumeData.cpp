#include "ScalarVolumeData.h"

#include "util/util.h"

#include <iostream>
#include <fstream>
#include <algorithm>

const XMFLOAT3 ScalarVolumeData::CSGroupSize = XMFLOAT3(32, 2, 2);

ID3DX11Effect								* ScalarVolumeData::pEffect = nullptr;
ID3DX11EffectTechnique						* ScalarVolumeData::pTechnique = nullptr;
ID3DX11EffectPass							* ScalarVolumeData::pInterpolationPass = nullptr;
ID3DX11EffectPass							* ScalarVolumeData::pNormalsPass = nullptr;
ID3DX11EffectPass							* ScalarVolumeData::pMinMaxPass = nullptr;
ID3DX11EffectUnorderedAccessViewVariable	* ScalarVolumeData::pScalarTextureInterpolatedEV = nullptr;
ID3DX11EffectUnorderedAccessViewVariable	* ScalarVolumeData::pNormalTextureEV = nullptr;
ID3DX11EffectShaderResourceVariable			* ScalarVolumeData::pScalarTextureT0EV = nullptr;
ID3DX11EffectShaderResourceVariable			* ScalarVolumeData::pScalarTextureT1EV = nullptr;
ID3DX11EffectScalarVariable					* ScalarVolumeData::pTimestepTEV = nullptr;
ID3DX11EffectUnorderedAccessViewVariable	* ScalarVolumeData::pMinMaxTextureEV = nullptr;
ID3DX11EffectVectorVariable					* ScalarVolumeData::pVolumeResEV = nullptr;


HRESULT ScalarVolumeData::Initialize(ID3D11Device * pd3dDevice_, TwBar* pParametersBar_)
{
	HRESULT hr;

	std::wstring effectPath = GetExePath() + L"ScalarVolumeData.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &pEffect)))
	{
		std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

	SAFE_GET_TECHNIQUE(pEffect, "ScalarVolumeComputeShaders", pTechnique);
	SAFE_GET_PASS(pTechnique, "PASS_INTERPOLATE", pInterpolationPass);
	SAFE_GET_PASS(pTechnique, "PASS_COMPUTE_NORMALS", pNormalsPass);
	SAFE_GET_PASS(pTechnique, "PASS_MINMAX", pMinMaxPass);

	SAFE_GET_UAV(pEffect, "g_scalarTextureInterpolated", pScalarTextureInterpolatedEV);
	SAFE_GET_UAV(pEffect, "g_normalTexture", pNormalTextureEV);
	SAFE_GET_RESOURCE(pEffect, "g_scalarTextureT0", pScalarTextureT0EV);
	SAFE_GET_RESOURCE(pEffect, "g_scalarTextureT1", pScalarTextureT1EV);

	SAFE_GET_SCALAR(pEffect, "g_timestepT", pTimestepTEV);

	SAFE_GET_VECTOR(pEffect, "g_volumeRes", pVolumeResEV);
	SAFE_GET_UAV(pEffect, "g_minMaxTexture", pMinMaxTextureEV);

	return S_OK;
}

HRESULT ScalarVolumeData::Release(void)
{
	SAFE_RELEASE(pTechnique);
	SAFE_RELEASE(pInterpolationPass);
	SAFE_RELEASE(pNormalsPass);
	SAFE_RELEASE(pEffect);
	return S_OK;
}

ScalarVolumeData::ScalarVolumeData(std::string objectFileName, DataFormat format, XMFLOAT3 sliceThickness, XMINT3 resolution, float timestep, XMINT3 timestepIndices) :
	VolumeData(objectFileName, format, sliceThickness, resolution, timestep, timestepIndices),
	m_pNormalTexture(nullptr),
	m_pInterpolatedTexture(nullptr),
	m_pInterpolatedTextureSRV(nullptr),
	m_pInterpolatedTextureUAV(nullptr),
	m_pNormalTextureSRV(nullptr),
	m_pNormalTextureUAV(nullptr),
	m_pMinMaxTexture(nullptr),
	m_pMinMaxTextureDownload(nullptr),
	m_pMinMaxTextureUAV(nullptr),
	m_normalsRequired(false)
{
	LoadDataFiles(objectFileName);
	TwAddVarRO(pParametersBar, "Scalar Volume Data", volumeDataType, this, "");
}

/*
	Constructor for dependent data
	this is used by VectorVolumeData and other classes that derive scalar volumes from their data
*/
ScalarVolumeData::ScalarVolumeData(ID3D11ShaderResourceView * pVolumeDataSRV, DataFormat format, XMFLOAT3 sliceThickness, XMINT3 resolution) :
	VolumeData("[GENERATED]", format, sliceThickness, resolution, 1.f, XMINT3(0, 0, 1)),
	m_pNormalTexture(nullptr),
	m_pInterpolatedTexture(nullptr),
	m_pInterpolatedTextureSRV(nullptr),
	m_pInterpolatedTextureUAV(nullptr),
	m_pNormalTextureSRV(nullptr),
	m_pNormalTextureUAV(nullptr),
	m_pMinMaxTexture(nullptr),
	m_pMinMaxTextureDownload(nullptr),
	m_pMinMaxTextureUAV(nullptr),
	m_normalsRequired(false)
{
	m_pVolumeData0SRV = pVolumeDataSRV;
	m_externalData = true;
}

ScalarVolumeData::~ScalarVolumeData(void)
{
	// free each of the allocated arrays
	std::for_each(m_data.begin(), m_data.end(), [](void* p) {if(p) delete[] p;});
	std::for_each(m_histogram.begin(), m_histogram.end(), [](float* p) {delete[] p;});

	ReleaseGPUBuffers();

	if(!m_externalData)
		TwRemoveVar(pParametersBar, "Scalar Volume Data");
}

unsigned char ScalarVolumeData::SampleVolume(int volumeIdx, XMINT3 pos) {
	assert(!m_externalData);
	assert(m_format == DF_BYTE);

	unsigned int elemPitch = sizeof(char);
	unsigned int rowPitch = m_resolution.x * elemPitch;
	unsigned int slicePitch = m_resolution.y * rowPitch;

	pos.x = std::max(0, std::min(m_resolution.x - 1, pos.x));
	pos.y = std::max(0, std::min(m_resolution.y - 1, pos.y));
	pos.z = std::max(0, std::min(m_resolution.z - 1, pos.z));

	return static_cast<unsigned char*>(m_data[volumeIdx])[pos.x * elemPitch + pos.y * rowPitch + pos.z * slicePitch];
}

XMVECTOR ScalarVolumeData::GetGradient(int volumeIdx, XMINT3 pos) {
	assert(!m_externalData);
	XMFLOAT3 grad;

	grad.x = (float) SampleVolume(volumeIdx,XMINT3(pos.x + 1, pos.y, pos.z)) - SampleVolume(volumeIdx,XMINT3(pos.x - 1, pos.y, pos.z));
	grad.y = (float) SampleVolume(volumeIdx,XMINT3(pos.x, pos.y + 1, pos.z)) - SampleVolume(volumeIdx,XMINT3(pos.x, pos.y - 1, pos.z));
	grad.z = (float) SampleVolume(volumeIdx,XMINT3(pos.x, pos.y, pos.z + 1)) - SampleVolume(volumeIdx,XMINT3(pos.x, pos.y, pos.z - 1));
	
	return XMLoadFloat3(&grad);
}

void ScalarVolumeData::CalculateVolumeNormals() {
	//std::cout << "Calculating Volume Normals..." << std::endl;

	pScalarTextureT0EV->SetResource(GetInterpolatedTextureSRV());
	pNormalTextureEV->SetUnorderedAccessView(m_pNormalTextureUAV);

	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);
	pNormalsPass->Apply(0, pContext);

	XMUINT3 gs = XMUINT3(
		static_cast<uint32_t>(ceilf(static_cast<float>(m_resolution.x) / CSGroupSize.x)),
		static_cast<uint32_t>(ceilf(static_cast<float>(m_resolution.y) / CSGroupSize.y)),
		static_cast<uint32_t>(ceilf(static_cast<float>(m_resolution.z) / CSGroupSize.z))
	);

	pContext->Dispatch(gs.x, gs.y, gs.z);

	// remove input/output mapping
	pScalarTextureT0EV->SetResource(nullptr);
	pNormalTextureEV->SetUnorderedAccessView(nullptr);
	pNormalsPass->Apply(0, pContext);
	SAFE_RELEASE(pContext);

	//std::cout << "Normals calculated!" << std::endl;
}

void ScalarVolumeData::InterpolateTimesteps() {
	// we dont interpolate if we have only one fixed timestamp
	if(!m_timeSequenceLength || !HasObservers())
		return;


	pTimestepTEV->SetFloat(m_currentTimestepT);
	pScalarTextureT0EV->SetResource(m_pVolumeData0SRV);
	pScalarTextureT1EV->SetResource(m_pVolumeData1SRV);
	pScalarTextureInterpolatedEV->SetUnorderedAccessView(m_pInterpolatedTextureUAV);
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);
	pInterpolationPass->Apply(0, pContext);

	uint32_t	gx = static_cast<uint32_t>(ceilf(m_resolution.x / CSGroupSize.x)),
				gy = static_cast<uint32_t>(ceilf(m_resolution.y / CSGroupSize.y)), 
				gz = static_cast<uint32_t>(ceilf(m_resolution.z / CSGroupSize.z));
	pContext->Dispatch(gx, gy, gz);

	// remove input/output mapping
	pScalarTextureT0EV->SetResource(nullptr);
	pScalarTextureT1EV->SetResource(nullptr);
	pScalarTextureInterpolatedEV->SetUnorderedAccessView(nullptr);
	pNormalsPass->Apply(0, pContext);
	SAFE_RELEASE(pContext);

	NotifyAll(); //notify all visualizers that something has changed
}

HRESULT ScalarVolumeData::CreateGPUBuffers(void)
{
	HRESULT hr;

	DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
	if(m_format == DF_BYTE)
		fmt = DXGI_FORMAT_R8_UNORM;
	if(m_format == DF_FLOAT)
		fmt = DXGI_FORMAT_R32_FLOAT;

	assert(m_format != DXGI_FORMAT_UNKNOWN);

	D3D11_TEXTURE3D_DESC desc;
	D3D11_SUBRESOURCE_DATA initialData;
	D3D11_SHADER_RESOURCE_VIEW_DESC pDesc;
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;

	// create the data buffers only if not externally managed
	if(!m_externalData) {
		//create the scalar textures (raw data)
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = m_resolution.x;
		desc.Height = m_resolution.y;
		desc.Depth = m_resolution.z;
		desc.MipLevels = 1;
		desc.Format = fmt;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;


		initialData.pSysMem = m_data[0];
		initialData.SysMemPitch = m_resolution.x * m_elementSize;
		initialData.SysMemSlicePitch = m_resolution.x * m_resolution.y * m_elementSize;

		V_RETURN(pd3dDevice->CreateTexture3D(&desc, &initialData, &m_pVolumeData0));

	
		ZeroMemory(&pDesc, sizeof(pDesc));
		pDesc.Format = fmt;
		pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		pDesc.Texture3D.MipLevels = -1;
		pDesc.Texture3D.MostDetailedMip = 0;

		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pVolumeData0, &pDesc, &m_pVolumeData0SRV));

		if(m_timeSequenceLength) {
			ZeroMemory(&desc, sizeof(desc));
			desc.Width = m_resolution.x;
			desc.Height = m_resolution.y;
			desc.Depth = m_resolution.z;
			desc.MipLevels = 1;
			desc.Format = fmt;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			ZeroMemory(&initialData, sizeof(initialData));
			initialData.pSysMem = m_data[1];
			initialData.SysMemPitch = m_resolution.x * m_elementSize;
			initialData.SysMemSlicePitch = m_resolution.x * m_resolution.y * m_elementSize;

			V_RETURN(pd3dDevice->CreateTexture3D(&desc, &initialData, &m_pVolumeData1));

			ZeroMemory(&pDesc, sizeof(pDesc));
			pDesc.Format = fmt;
			pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
			pDesc.Texture3D.MipLevels = -1;
			pDesc.Texture3D.MostDetailedMip = 0;

			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pVolumeData1, &pDesc, &m_pVolumeData1SRV));

			m_currentDatasetSlot1 = 1;
		}

		// create the GPU buffer for the interpolated timestep
		// this buffer contains float data in all cases since it is filled by a CS
		if(m_timeSequenceLength) {
			// create the buffer itself
			ZeroMemory(&desc, sizeof(desc));
			desc.Width = m_resolution.x;
			desc.Height = m_resolution.y;
			desc.Depth = m_resolution.z;
			desc.MipLevels = 1;
			desc.Format = DXGI_FORMAT_R32_FLOAT;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
			desc.CPUAccessFlags = 0;
			desc.MiscFlags = 0;

			V_RETURN(pd3dDevice->CreateTexture3D(&desc, nullptr, &m_pInterpolatedTexture));

			//create the SRV
			ZeroMemory(&pDesc, sizeof(pDesc));
			pDesc.Format = DXGI_FORMAT_R32_FLOAT;
			pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
			pDesc.Texture3D.MipLevels = -1;
			pDesc.Texture3D.MostDetailedMip = 0;
			V_RETURN(pd3dDevice->CreateShaderResourceView(m_pInterpolatedTexture, &pDesc, &m_pInterpolatedTextureSRV));

			//create the UAV
			ZeroMemory(&uavDesc, sizeof(uavDesc));
			uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
			uavDesc.Texture3D.MipSlice = 0;
			uavDesc.Texture3D.FirstWSlice = 0;
			uavDesc.Texture3D.WSize = m_resolution.z;
			V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pInterpolatedTexture, &uavDesc, &m_pInterpolatedTextureUAV));
		}
	}

	return CreateHistoGPUBuffers();
}

HRESULT ScalarVolumeData::CreateNormalDataGPUBuffers(void)
{
	HRESULT hr;

	//std::cout << "Creating Normal Data GPU Buffers" << std::endl;
	if(m_pNormalTexture)
		return S_OK;

	//create the normals texture
	D3D11_TEXTURE3D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = m_resolution.x;
	desc.Height = m_resolution.y;
	desc.Depth = m_resolution.z;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	V_RETURN(pd3dDevice->CreateTexture3D(&desc, nullptr, &m_pNormalTexture));

	D3D11_SHADER_RESOURCE_VIEW_DESC pDesc;
	ZeroMemory(&pDesc, sizeof(pDesc));
	pDesc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
	pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	pDesc.Texture3D.MipLevels = -1;
	pDesc.Texture3D.MostDetailedMip = 0;
	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pNormalTexture, &pDesc, &m_pNormalTextureSRV));

	//create the UAV
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_SNORM;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = m_resolution.z;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pNormalTexture, &uavDesc, &m_pNormalTextureUAV));
	
	CalculateVolumeNormals();

	return S_OK;
}

HRESULT ScalarVolumeData::CreateMinMaxGPUBuffers(void)
{
	//don't do if we already have textures
	if(m_pMinMaxTexture)
		return S_OK;

	HRESULT hr;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = m_resolution.y;
	desc.Height = m_resolution.z;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	V_RETURN(pd3dDevice->CreateTexture2D(&desc, nullptr, &m_pMinMaxTexture));

	
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = m_resolution.y;
	desc.Height = m_resolution.z;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.MiscFlags = 0;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;

	V_RETURN(pd3dDevice->CreateTexture2D(&desc, nullptr, &m_pMinMaxTextureDownload));

	//create the UAV
	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(m_pMinMaxTexture, &uavDesc, &m_pMinMaxTextureUAV));

	return S_OK;
}

HRESULT ScalarVolumeData::CreateHistoGPUBuffers(void)
{
	HRESULT hr;

	// create resources for the histogram
	D3D11_TEXTURE1D_DESC hdesc;
	ZeroMemory(&hdesc, sizeof(hdesc));
	hdesc.Width = 255;
	hdesc.MipLevels = 1;					
	hdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	hdesc.Usage = D3D11_USAGE_DEFAULT;
	hdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	hdesc.CPUAccessFlags = 0;
	hdesc.MiscFlags = 0;
	hdesc.ArraySize = 1;

	if(m_histogram.size()) {
		D3D11_SUBRESOURCE_DATA initialData;
		ZeroMemory(&initialData, sizeof(initialData));
		initialData.pSysMem = m_histogram[0];
		initialData.SysMemPitch = 255 * 4 * sizeof(float);
		initialData.SysMemSlicePitch = 0;

		V_RETURN(pd3dDevice->CreateTexture1D(&hdesc, &initialData, &m_pHistoTexture));
	}
	else 
		V_RETURN(pd3dDevice->CreateTexture1D(&hdesc, nullptr, &m_pHistoTexture));

	D3D11_SHADER_RESOURCE_VIEW_DESC pHistoDesc;
	ZeroMemory(&pHistoDesc, sizeof(pHistoDesc));
	pHistoDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	pHistoDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	pHistoDesc.Texture1D.MipLevels = -1;
	pHistoDesc.Texture1D.MostDetailedMip = 0;

	return pd3dDevice->CreateShaderResourceView(m_pHistoTexture, &pHistoDesc, &m_pHistogramSRV);
}

void ScalarVolumeData::ReleaseGPUBuffers() {
	if(!m_externalData) {
		//only release if not managed by other class
		SAFE_RELEASE(m_pVolumeData0SRV);
		SAFE_RELEASE(m_pVolumeData1SRV);
		SAFE_RELEASE(m_pVolumeData0);
		SAFE_RELEASE(m_pVolumeData1);
	}
	SAFE_RELEASE(m_pHistogramSRV);
	SAFE_RELEASE(m_pHistoTexture);
	
	SAFE_RELEASE(m_pNormalTexture);
	SAFE_RELEASE(m_pInterpolatedTexture);
	SAFE_RELEASE(m_pInterpolatedTextureSRV);
	SAFE_RELEASE(m_pInterpolatedTextureUAV);
	SAFE_RELEASE(m_pNormalTextureSRV);
	SAFE_RELEASE(m_pNormalTextureUAV);

	SAFE_RELEASE(m_pMinMaxTexture);
	SAFE_RELEASE(m_pMinMaxTextureDownload);
	SAFE_RELEASE(m_pMinMaxTextureUAV);
}

ID3D11ShaderResourceView * ScalarVolumeData::GetNormalTextureSRV() {
	//don't do if we already have textures
	if(!m_pNormalTexture) {
		if(FAILED(CreateNormalDataGPUBuffers()))
			return nullptr;
	}

	return m_pNormalTextureSRV;
};

ID3D11ShaderResourceView * ScalarVolumeData::GetInterpolatedTextureSRV(void)
{
	if(m_timeSequenceLength)
		return m_pInterpolatedTextureSRV;
	else
		return m_pVolumeData0SRV;
}

void ScalarVolumeData::UpdateHistogram(int timestep0, int timestep1, float timestepT) {
	if(!m_pHistoTexture)
		CreateHistoGPUBuffers();

	//std::cout << "Histo Timestep: " << timestepT << std::endl;
	float combinedHisto[4 * 256];
	if(m_externalData) {
		//we don't have a solution for histograms for metrics so we just set everything to a fixed value
		for(int i=0; i < 256; i++) {
			combinedHisto[4 * i] = .3f;
		}
	}
	else {
		for(int i=0; i < 256; i++) {
			combinedHisto[4 * i] = (1.f - timestepT) * m_histogram[timestep0][4*i] + timestepT * m_histogram[timestep1][4*i];
		}
	}
	//update the texture
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);
	pContext->UpdateSubresource(m_pHistoTexture, 0, nullptr, combinedHisto, 256 * 4 * sizeof(float), 0);
	SAFE_RELEASE(pContext);
}

void ScalarVolumeData::SetTime(float currentTime) 
{
	reinterpret_cast<VolumeData*>(this)->SetTime(currentTime);

	if(!m_externalData)
		InterpolateTimesteps();

	if(m_normalsRequired)
		CalculateVolumeNormals();
}

void ScalarVolumeData::SetNormalsRequired(bool required)
{
	m_normalsRequired = required;
	if(!required) {
		SAFE_RELEASE(m_pNormalTexture);
		SAFE_RELEASE(m_pNormalTextureSRV);
		SAFE_RELEASE(m_pNormalTextureUAV);
	}
	else
		//assure the buffers exist
		CreateNormalDataGPUBuffers();
}

void ScalarVolumeData::SaveConfig(SettingsStorage &store)
{
	
}

void ScalarVolumeData::LoadConfig(SettingsStorage &store)
{
}

XMFLOAT2 ScalarVolumeData::GetMinMax(void)
{
	if(!m_pMinMaxTexture)
		CreateMinMaxGPUBuffers();

	//std::cout << "GPU Buffers Exist!" << std::endl;

	pScalarTextureT0EV->SetResource(GetInterpolatedTextureSRV());
	pMinMaxTextureEV->SetUnorderedAccessView(m_pMinMaxTextureUAV);
	int vr[3] = {m_resolution.x - 1, m_resolution.y - 1, m_resolution.z - 1};
	pVolumeResEV->SetIntVector(vr);

	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);
	pMinMaxPass->Apply(0, pContext);

	pContext->Dispatch(1, m_resolution.y, m_resolution.z);

	pScalarTextureT0EV->SetResource(nullptr);
	pMinMaxTextureEV->SetUnorderedAccessView(nullptr);
	pMinMaxPass->Apply(0, pContext);

	//std::cout << "Shader finished." << std::endl;

	//read back buffer
	pContext->CopyResource(m_pMinMaxTextureDownload, m_pMinMaxTexture);
	D3D11_MAPPED_SUBRESOURCE ms;
	pContext->Map(m_pMinMaxTextureDownload, NULL, D3D11_MAP_READ, NULL, &ms);
	
	float * buf = (float*)ms.pData;
	float metricMin = buf[0];
	float metricMax = buf[1];
	//determine min and max from texture buffer
	for(int y=0; y < m_resolution.y; y++) {
		for(int z=0; z < m_resolution.z; z++) {
			metricMin = std::min(metricMin, buf[(z * m_resolution.y + y)*2]);
			metricMax = std::max(metricMax, buf[(z * m_resolution.y + y)*2 + 1]);
		}
	}
	
	pContext->Unmap(m_pMinMaxTextureDownload, NULL);

	SAFE_RELEASE(pContext);

	return XMFLOAT2(metricMin, metricMax);
}