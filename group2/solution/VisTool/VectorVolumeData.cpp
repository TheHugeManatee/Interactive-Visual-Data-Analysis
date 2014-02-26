#include "VectorVolumeData.h"

#include "util/util.h"

#include <iostream>

const XMFLOAT3 VectorVolumeData::CSGroupSize = XMFLOAT3(32, 2, 2);

char * VectorVolumeData::metricPassNames[] = {
	"PASS_VELOCITY_MAGNITUDE",
	"PASS_DIVERGENCE",
	"PASS_VORTICITY_MAGNITUDE",
	"PASS_Q_S_PARAMETER",
	"PASS_Q_OMEGA_PARAMETER",
	"PASS_ENSTROPHY_PRODUCTION",
	"PASS_V_SQUARED",
	"PASS_Q_PARAMETER",
	"PASS_LAMBDA2",
	"PASS_FOURTH_COMPONENT"
};

ID3DX11Effect			* VectorVolumeData::pEffect = nullptr;
ID3DX11EffectTechnique	* VectorVolumeData::pTechnique = nullptr;
ID3DX11EffectPass		* VectorVolumeData::pMetricPasses[VectorVolumeData::NUM_METRICS] = {};

ID3DX11EffectUnorderedAccessViewVariable * VectorVolumeData::pScalarMetricEV = nullptr;
ID3DX11EffectShaderResourceVariable * VectorVolumeData::pVectorVolume0EV = nullptr;
ID3DX11EffectShaderResourceVariable * VectorVolumeData::pVectorVolume1EV = nullptr;

ID3DX11EffectVectorVariable * VectorVolumeData::pMetricMinMaxEV = nullptr;
ID3DX11EffectVectorVariable * VectorVolumeData::pVolumeResEV = nullptr;
ID3DX11EffectScalarVariable * VectorVolumeData::pTimestepTEV = nullptr;
ID3DX11EffectScalarVariable * VectorVolumeData::pBoundaryMetricFixedEV = nullptr;
ID3DX11EffectScalarVariable * VectorVolumeData::pBoundaryMetricValueEV = nullptr;

HRESULT VectorVolumeData::Initialize(ID3D11Device * pd3dDevice_, TwBar* pParametersBar_)
{
	HRESULT hr;

	std::wstring effectPath = GetExePath() + L"VectorVolumeData.fxo";
	if(FAILED(hr = D3DX11CreateEffectFromFile(effectPath.c_str(), 0, pd3dDevice, &pEffect)))
	{
		std::wcout << L"Failed creating effect with error code " << int(hr) << std::endl;
		return hr;
	}

	SAFE_GET_TECHNIQUE(pEffect, "Metrics", pTechnique);
	for(int i=0; i < NUM_METRICS; i++) {
		SAFE_GET_PASS(pTechnique, metricPassNames[i], pMetricPasses[i]);
	}
	SAFE_GET_UAV(pEffect, "g_metricTexture", pScalarMetricEV);
	SAFE_GET_RESOURCE(pEffect, "g_vectorTexture0", pVectorVolume0EV);
	SAFE_GET_RESOURCE(pEffect, "g_vectorTexture1", pVectorVolume1EV);

	SAFE_GET_VECTOR(pEffect, "g_metricMinMax", pMetricMinMaxEV);
	SAFE_GET_VECTOR(pEffect, "g_volumeRes", pVolumeResEV);
	SAFE_GET_SCALAR(pEffect, "g_timestepT", pTimestepTEV);
	SAFE_GET_SCALAR(pEffect, "g_boundaryMetricFixed", pBoundaryMetricFixedEV);
	SAFE_GET_SCALAR(pEffect, "g_boundaryMetricValue", pBoundaryMetricValueEV);

	SetupTwBar(pParametersBar_);

	return S_OK;
}

HRESULT VectorVolumeData::Release(void)
{

	SAFE_RELEASE(pTechnique);
	for(int i=0; i < NUM_METRICS; i++) {
		SAFE_RELEASE(pMetricPasses[i]);
	}

	SAFE_RELEASE(pEffect);
	return S_OK;
}

void VectorVolumeData::SetupTwBar(TwBar * pParametersBar) {

}

void TW_CALL VectorVolumeData::OnComputeMinMaxCB(void* clientData)
{
	VectorVolumeData * me = reinterpret_cast<VectorVolumeData*>(clientData);
	XMFLOAT2 minMax = me->GetScalarMetricVolume()->GetMinMax();

	XMFLOAT3 actualMetricMinMax;
	if(!me->m_reverseMetricRange)
		actualMetricMinMax = me->m_metricMinMax;
	else
		//reversing actually just means switch max and min
		actualMetricMinMax = XMFLOAT3(me->m_metricMinMax.y, me->m_metricMinMax.x, 0);
	actualMetricMinMax.z = (actualMetricMinMax.y - actualMetricMinMax.x);

	float newMin = minMax.x * actualMetricMinMax.z + actualMetricMinMax.x;
	float newMax = minMax.y * actualMetricMinMax.z + actualMetricMinMax.x;


	std::cout << "Min and max are: [" << minMax.x << "," << minMax.y << "]" << std::endl;
	std::cout << "Actual Metric Limits are: [" << newMin << "," << newMax << "]" << std::endl;

	if(!_finitef(newMin) || !_finitef(newMax))
		return;

	if(me->m_reverseMetricRange) {
		me->m_metricMinMax.x = newMax;
		me->m_metricMinMax.y = newMin;
	}
	else {
		me->m_metricMinMax.x = newMin;
		me->m_metricMinMax.y = newMax;
	}
}

void TW_CALL VectorVolumeData::SetMetricCB(const void *value, void *clientData)
{ 
	VectorVolumeData * me = static_cast<VectorVolumeData*>(clientData);
	me->m_metricType = *(const MetricType *)value;
	OnComputeMinMaxCB(clientData);
}

void TW_CALL VectorVolumeData::GetMetricCB(void *value, void *clientData)
{ 
    *(unsigned int *)value = static_cast<VectorVolumeData*>(clientData)->m_metricType;
}

VectorVolumeData::VectorVolumeData(std::string objectFileName, DataFormat format, XMFLOAT3 sliceThickness, XMINT3 resolution, float timestep, XMINT3 timestepIndices) :
	VolumeData(objectFileName, format, sliceThickness, resolution, timestep, timestepIndices),
	m_scalarMetricData(nullptr),
	m_pScalarMetricUAV(nullptr),
	m_pScalarMetricSRV(nullptr),
	m_pScalarMetricTexture(nullptr),
	m_metricMinMax(0, 0, 0),
	m_metricType(MT_VELOCITY_MAGNITUDE), 
	m_lastMetricType(MT_VELOCITY_MAGNITUDE),
	m_reverseMetricRange(false),
	m_boundaryMetricFixed(false),
	m_boundaryMetricValue(0),
	m_lastMetricUpdateTime(-1)
{
	LoadDataFiles(objectFileName);

	TwType metricType; 
	metricType = TwDefineEnumFromString("MetricType", "Velocity Magnitude,Divergence,Vorticity Magnitude,Q_S,Q_Omega,Enstrophy Production,V^2,Q Parameter,Lambda2,Fourth Vector Component");
	
	TwAddVarCB(pParametersBar, "Metric", metricType, SetMetricCB, GetMetricCB, this, "group='Vector Volume'");
	TwAddVarRW(pParametersBar, "Metric Min", TW_TYPE_FLOAT, &m_metricMinMax.x, "group='Vector Volume'");
	TwAddVarRW(pParametersBar, "Metric Max", TW_TYPE_FLOAT, &m_metricMinMax.y, "group='Vector Volume'");
	TwAddButton(pParametersBar, "[Get Min and Max]", OnComputeMinMaxCB, this, "group='Vector Volume'");
	TwAddVarRW(pParametersBar, "Reverse Range", TW_TYPE_BOOLCPP, &m_reverseMetricRange, "group='Vector Volume'");
	TwAddVarRW(pParametersBar, "Metric Fixed at boundary", TW_TYPE_BOOLCPP, &m_boundaryMetricFixed, "group='Vector Volume'");
	TwAddVarRW(pParametersBar, "Metric Value at boundary (if fixed)", TW_TYPE_FLOAT, &m_boundaryMetricValue, "group='Vector Volume'");
	TwAddVarRO(pParametersBar, "Volume Data", volumeDataType, this, "group='Vector Volume'");
}


VectorVolumeData::~VectorVolumeData(void)
{
	SAFE_RELEASE(m_pVolumeData0SRV);
	SAFE_RELEASE(m_pVolumeData0);
	SAFE_RELEASE(m_pVolumeData1SRV);
	SAFE_RELEASE(m_pVolumeData1);

	SAFE_RELEASE(m_pScalarMetricUAV);
	SAFE_RELEASE(m_pScalarMetricSRV);
	SAFE_RELEASE(m_pScalarMetricTexture);

	if(m_scalarMetricData) delete m_scalarMetricData;
	std::for_each(m_data.begin(), m_data.end(), [](void* p) {if(p) delete[] p;});
	std::for_each(m_histogram.begin(), m_histogram.end(), [](float* p) {if(p) delete[] p;});

	TwRemoveVar(pParametersBar, "Vector Volume");
}

void VectorVolumeData::SaveConfig(SettingsStorage &store)
{
	store.StoreInt("vectorvolume.metric.type", m_metricType);
	store.StoreFloat("vectorvolume.metric.min", m_metricMinMax.x);
	store.StoreFloat("vectorvolume.metric.max", m_metricMinMax.y);
	store.StoreBool("vectorvolume.metric.reverse", m_reverseMetricRange);
	store.StoreBool("vectorvolume.metric.boundaryFixed", m_boundaryMetricFixed);
	store.StoreFloat("vectorvolume.metric.boundaryValue", m_boundaryMetricValue);
}

void VectorVolumeData::LoadConfig(SettingsStorage &store)
{
	int metric = m_metricType;
	store.GetInt("vectorvolume.metric.type", metric);
	m_metricType = (MetricType)metric;
	store.GetFloat("vectorvolume.metric.min", m_metricMinMax.x);
	store.GetFloat("vectorvolume.metric.max", m_metricMinMax.y);
	store.GetBool("vectorvolume.metric.reverse", m_reverseMetricRange);
	store.GetBool("vectorvolume.metric.boundaryFixed", m_boundaryMetricFixed);
	store.GetFloat("vectorvolume.metric.boundaryvalue", m_boundaryMetricValue);
}

void VectorVolumeData::SetTime(float currentTime) 
{
	bool changed = m_currentTime != currentTime;
	reinterpret_cast<VolumeData*>(this)->SetTime(currentTime);

	if(m_scalarMetricData)
		m_scalarMetricData->SetTime(0);

	if(changed && HasObservers()) {
		//std::cout << "VectorVolumeData Notify Event." << std::endl;
		NotifyAll();
	}

	if(m_scalarMetricData) {
		//update if any parameter has changed
		if(m_scalarMetricData->HasObservers() && (
				m_lastMetricUpdateTime != m_currentTime ||
				m_lastMetricType != m_metricType ||
				m_lastMetricMinMax.x != m_metricMinMax.x ||
				m_lastMetricMinMax.y != m_metricMinMax.y) 
			)
		{
			UpdateScalarMetric();
		}
	}
}

HRESULT VectorVolumeData::CreateGPUBuffers(void)
{
	assert(!m_externalData);
	HRESULT hr;

	DXGI_FORMAT dxgiFormat;
	switch(m_format) {
	case DF_HALF3:
	case DF_HALF4:
		dxgiFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		break;
	case DF_FLOAT3:
	case DF_FLOAT4:
		dxgiFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	default:
		assert(false);
	}

	//create the scalar texture
	D3D11_TEXTURE3D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = m_resolution.x;
	desc.Height = m_resolution.y;
	desc.Depth = m_resolution.z;
	desc.MipLevels = 1;
	desc.Format = dxgiFormat;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = m_data[0];
	initialData.SysMemPitch = m_resolution.x * (m_elementSize + m_elementPadding);
	initialData.SysMemSlicePitch = m_resolution.x * m_resolution.y * (m_elementSize + m_elementPadding);

	V_RETURN(pd3dDevice->CreateTexture3D(&desc, &initialData, &m_pVolumeData0));

	D3D11_SHADER_RESOURCE_VIEW_DESC pDesc;
	ZeroMemory(&pDesc, sizeof(pDesc));
	pDesc.Format = dxgiFormat;
	pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	pDesc.Texture3D.MipLevels = -1;
	pDesc.Texture3D.MostDetailedMip = 0;

	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pVolumeData0, &pDesc, &m_pVolumeData0SRV));

	if(m_data.size() > 1) {
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = m_resolution.x;
		desc.Height = m_resolution.y;
		desc.Depth = m_resolution.z;
		desc.MipLevels = 1;
		desc.Format = dxgiFormat;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		ZeroMemory(&initialData, sizeof(initialData));
		initialData.pSysMem = m_data[1];
		initialData.SysMemPitch = m_resolution.x * (m_elementSize + m_elementPadding);
		initialData.SysMemSlicePitch = m_resolution.x * m_resolution.y * (m_elementSize + m_elementPadding);

		V_RETURN(pd3dDevice->CreateTexture3D(&desc, &initialData, &m_pVolumeData1));

		ZeroMemory(&pDesc, sizeof(pDesc));
		pDesc.Format = dxgiFormat;
		pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		pDesc.Texture3D.MipLevels = -1;
		pDesc.Texture3D.MostDetailedMip = 0;

		V_RETURN(pd3dDevice->CreateShaderResourceView(m_pVolumeData1, &pDesc, &m_pVolumeData1SRV));

		m_currentDatasetSlot1 = 1;
	}

	if(m_scalarMetricData)
		CreateScalarMetricBuffers();

	return S_OK;
}

void VectorVolumeData::ReleaseGPUBuffers(void) {
	SAFE_RELEASE(m_pVolumeData0SRV);
	SAFE_RELEASE(m_pVolumeData0);
	SAFE_RELEASE(m_pVolumeData1SRV);
	SAFE_RELEASE(m_pVolumeData1);

	SAFE_RELEASE(m_pScalarMetricUAV);
	SAFE_RELEASE(m_pScalarMetricSRV);
	SAFE_RELEASE(m_pScalarMetricTexture);

	if(m_scalarMetricData) {
		delete m_scalarMetricData;
		m_scalarMetricData = nullptr;
	}

}

ScalarVolumeData * VectorVolumeData::GetScalarMetricVolume()
{
	if(m_scalarMetricData)
		return m_scalarMetricData;

	// if we don't have a min and max set, we set to default values and then recalculate afterwards using OnComputeMinMaxCB
	bool noMinMax = (m_metricMinMax.x == m_metricMinMax.y);
	if(noMinMax) {
		m_metricMinMax.x = 0;
		m_metricMinMax.y = 1;
	}

	CreateScalarMetricBuffers();
	m_scalarMetricData = new ScalarVolumeData(m_pScalarMetricSRV, VolumeData::DF_FLOAT, m_sliceThickness, m_resolution);
	
	TwAddVarRO(pParametersBar, "Metric Volume Data", volumeDataType, m_scalarMetricData, "group='Vector Volume'");

	UpdateScalarMetric();
	// we do this only once at creation because we don't recalculate histograms for generated scalar volumes
	m_scalarMetricData->UpdateHistogram(0, 0, 0.f);

	if(noMinMax) {
		OnComputeMinMaxCB(this); //calculate min and max if we just created the metric
	}
	return m_scalarMetricData;
}

void VectorVolumeData::UpdateScalarMetric(void) 
{
	assert(pd3dDevice);

	pScalarMetricEV->SetUnorderedAccessView(m_pScalarMetricUAV);
	pVectorVolume0EV->SetResource(m_pVolumeData0SRV);
	pVectorVolume1EV->SetResource(m_pVolumeData1SRV);

	pBoundaryMetricFixedEV->SetBool(m_boundaryMetricFixed);

	XMFLOAT3 actualMetricMinMax;
	if(!m_reverseMetricRange)
		actualMetricMinMax = m_metricMinMax;
	else
		//reversing actually just means switch max and min
		actualMetricMinMax = XMFLOAT3(m_metricMinMax.y, m_metricMinMax.x, 0);
	actualMetricMinMax.z = (actualMetricMinMax.y - actualMetricMinMax.x);
	pMetricMinMaxEV->SetFloatVector(&actualMetricMinMax.x);
	pBoundaryMetricValueEV->SetFloat((m_boundaryMetricValue - actualMetricMinMax.x)/actualMetricMinMax.z);
	XMUINT3 maxIndices = XMUINT3(m_resolution.x-1, m_resolution.y-1, m_resolution.z-1);
	pVolumeResEV->SetIntVector((int*)&maxIndices.x);
	pTimestepTEV->SetFloat(m_currentTimestepT);

	ID3D11DeviceContext * pContext;	
	pd3dDevice->GetImmediateContext(&pContext);
	pMetricPasses[m_metricType]->Apply(0, pContext);
	// the CS have (32, 2, 2) groups so we divide by this group size, rounding up so we get enough groups to cover the volume
	uint32_t	gx = static_cast<uint32_t>(ceilf(m_resolution.x / CSGroupSize.x)),
				gy = static_cast<uint32_t>(ceilf(m_resolution.y / CSGroupSize.y)), 
				gz = static_cast<uint32_t>(ceilf(m_resolution.z / CSGroupSize.z));
	pContext->Dispatch(gx, gy, gz);

	pScalarMetricEV->SetUnorderedAccessView(nullptr);
	pMetricPasses[m_metricType]->Apply(0, pContext);
	SAFE_RELEASE(pContext);

	m_lastMetricUpdateTime = m_currentTime;
	m_lastMetricType = m_metricType;
	m_lastMetricMinMax = m_metricMinMax;

	m_scalarMetricData->NotifyAll();
	
	std::cout << "Metric updated." << std::endl;
}


HRESULT VectorVolumeData::CreateScalarMetricBuffers(void)
{
	assert(pd3dDevice);

	HRESULT hr;
	//create the scalar texture
	D3D11_TEXTURE3D_DESC desc;
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

	V_RETURN(pd3dDevice->CreateTexture3D(&desc, nullptr, &m_pScalarMetricTexture));

	D3D11_SHADER_RESOURCE_VIEW_DESC pDesc;
	ZeroMemory(&pDesc, sizeof(pDesc));
	pDesc.Format = DXGI_FORMAT_R32_FLOAT;
	pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	pDesc.Texture3D.MipLevels = -1;
	pDesc.Texture3D.MostDetailedMip = 0;

	V_RETURN(pd3dDevice->CreateShaderResourceView(m_pScalarMetricTexture, &pDesc, &m_pScalarMetricSRV));

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	ZeroMemory(&uavDesc, sizeof(uavDesc));
	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	uavDesc.Texture3D.MipSlice = 0;
	uavDesc.Texture3D.FirstWSlice = 0;
	uavDesc.Texture3D.WSize = m_resolution.z;
	pd3dDevice->CreateUnorderedAccessView(m_pScalarMetricTexture, &uavDesc, &m_pScalarMetricUAV);

	return S_OK;
}