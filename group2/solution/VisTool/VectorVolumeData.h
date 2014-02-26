#pragma once


#include "VolumeData.h"
#include "ScalarVolumeData.h"
#include "SettingsStorage.h"

class VectorVolumeData :
	public VolumeData
{
public:
	// types
	enum MetricType {
		MT_VELOCITY_MAGNITUDE,
		MT_DIVERGENCE,
		MT_VORTICITY_MAGNITUDE,
		MT_Q_S,
		MT_Q_OMEGA,
		MT_ENSTROPHY_PRODUCTION,
		MT_V_SQUARED,
		MT_Q_PARAMETER,
		MT_LAMBDA_2,
		MT_FOURTH_COMPONENT,
		NUM_METRICS	// not a real metric
	};
	
	//statics 
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);


	// ctor, dtor
	VectorVolumeData(std::string objectFileName, DataFormat format, XMFLOAT3 sliceThickness, XMINT3 resolution, float timestep = 1.f, XMINT3 timestepIndices = XMINT3(0, 0, 1));
	~VectorVolumeData(void);

	// methods
	HRESULT CreateGPUBuffers(void) override;
	void ReleaseGPUBuffers(void) override;
	void SetTime(float currentTime);
	void SaveConfig(SettingsStorage &store);
	void LoadConfig(SettingsStorage &store);

	// accessors
	ScalarVolumeData * GetScalarMetricVolume();
	void SetMetric(MetricType m) {		m_metricType = m;	};

private:
	// static functions
	static void SetupTwBar(TwBar * pParametersBar);
	static void TW_CALL OnComputeMinMaxCB(void* clientData);
	static void TW_CALL GetMetricCB(void* value, void* clientData);
	static void TW_CALL SetMetricCB(const void* value, void* clientData);

	// static variables
	static const XMFLOAT3 CSGroupSize;				//size of the compute shader groups
	static char * metricPassNames[NUM_METRICS];
	static ID3DX11Effect			* pEffect;
	static ID3DX11EffectTechnique	* pTechnique;
	static ID3DX11EffectPass		* pMetricPasses[NUM_METRICS];
	static ID3DX11EffectUnorderedAccessViewVariable * pScalarMetricEV;
	static ID3DX11EffectShaderResourceVariable * pVectorVolume0EV;
	static ID3DX11EffectShaderResourceVariable * pVectorVolume1EV;
	static ID3DX11EffectVectorVariable * pMetricMinMaxEV;
	static ID3DX11EffectVectorVariable * pVolumeResEV;
	static ID3DX11EffectScalarVariable * pTimestepTEV;
	static ID3DX11EffectScalarVariable * pBoundaryMetricFixedEV;
	static ID3DX11EffectScalarVariable * pBoundaryMetricValueEV;

	// methods
	void UpdateScalarMetric();	
	HRESULT CreateScalarMetricBuffers();

	// members
	MetricType m_metricType, m_lastMetricType;
	ScalarVolumeData * m_scalarMetricData;
	XMFLOAT3 m_metricMinMax, m_lastMetricMinMax;
	bool m_reverseMetricRange;
	bool m_boundaryMetricFixed;
	float m_boundaryMetricValue;
	float m_lastMetricUpdateTime;

	// dx resources
	ID3D11Texture3D * m_pScalarMetricTexture;
	ID3D11ShaderResourceView * m_pScalarMetricSRV;
	ID3D11UnorderedAccessView * m_pScalarMetricUAV;

};

