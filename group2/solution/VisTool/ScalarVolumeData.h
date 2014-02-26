#pragma once

#include "VolumeData.h"

#include "SettingsStorage.h"

class ScalarVolumeData : public VolumeData
{
public:
	// statics
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);

	// ctors, dtor
	ScalarVolumeData(std::string objectFileName, DataFormat format, XMFLOAT3 sliceThickness, XMINT3 resolution, float timestep = 1.f, XMINT3 timestepIndices = XMINT3(0, 0, 1));
	ScalarVolumeData(ID3D11ShaderResourceView * pVolumeDataSRV, DataFormat format, XMFLOAT3 sliceThickness, XMINT3 resolution);
	~ScalarVolumeData(void);

	// methods
	HRESULT CreateGPUBuffers(void) override;
	HRESULT CreateNormalDataGPUBuffers(void);
	HRESULT CreateHistoGPUBuffers(void);
	HRESULT CreateMinMaxGPUBuffers(void);
	void ReleaseGPUBuffers(void) override;
	void SetTime(float currentTime);
	void SetNormalsRequired(bool required);
	void SaveConfig(SettingsStorage &store);
	void LoadConfig(SettingsStorage &store);
	virtual void UpdateHistogram(int timestep0, int timestep1, float timestepT) override;
	void CalculateVolumeNormals(void);
	XMFLOAT2 GetMinMax(void);

	// accessors
	ID3D11ShaderResourceView * GetNormalTextureSRV();
	ID3D11ShaderResourceView * GetInterpolatedTextureSRV();

protected:
	// static variables	
	static const XMFLOAT3 CSGroupSize;				//size of the compute shader groups
	static ID3DX11Effect			* pEffect;
	static ID3DX11EffectTechnique	* pTechnique;
	static ID3DX11EffectPass		* pInterpolationPass;
	static ID3DX11EffectPass		* pNormalsPass;
	static ID3DX11EffectPass		* pMinMaxPass;
	static ID3DX11EffectUnorderedAccessViewVariable * pScalarTextureInterpolatedEV;
	static ID3DX11EffectUnorderedAccessViewVariable * pNormalTextureEV;
	static ID3DX11EffectShaderResourceVariable * pScalarTextureT0EV;
	static ID3DX11EffectShaderResourceVariable * pScalarTextureT1EV;
	static ID3DX11EffectScalarVariable * pTimestepTEV;

	static ID3DX11EffectUnorderedAccessViewVariable * pMinMaxTextureEV;
	static ID3DX11EffectVectorVariable * pVolumeResEV;


	// methods
	void InterpolateTimesteps(void);
	XMVECTOR GetGradient(int volumeIdx, XMINT3 pos);
	unsigned char SampleVolume(int volumeIdx, XMINT3 pos);
	
	// members
	bool m_normalsRequired;

	// dx resources
	ID3D11Texture3D * m_pNormalTexture;
	ID3D11Texture3D * m_pInterpolatedTexture;
	ID3D11ShaderResourceView * m_pInterpolatedTextureSRV;
	ID3D11UnorderedAccessView * m_pInterpolatedTextureUAV;
	ID3D11ShaderResourceView * m_pNormalTextureSRV;
	ID3D11UnorderedAccessView * m_pNormalTextureUAV;

	ID3D11Texture2D * m_pMinMaxTexture;
	ID3D11Texture2D * m_pMinMaxTextureDownload;
	ID3D11UnorderedAccessView * m_pMinMaxTextureUAV;
};

