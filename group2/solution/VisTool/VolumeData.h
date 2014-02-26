#pragma once

#include "util/notification.h"

#include "AntTweakBar.h"

#include <DirectXMath.h>
#include <d3dx11effect.h>
#include <DXUT.h>
using namespace DirectX;

#include <string>


class VolumeData : public Observable
{
public:
	// types
	enum DataFormat {
		DF_UNKNOWN,
		DF_BYTE,
		DF_FLOAT,
		DF_HALF3,
		DF_FLOAT3,
		DF_HALF4,
		DF_FLOAT4
	};

	// statics 
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);
	static DataFormat GetFormatFromStr(std::string str);
	static int GetElementSize(DataFormat f);
	static int GetElementPadding(DataFormat f);

	// ctor, dtor
	VolumeData(std::string objectFileName, DataFormat format, XMFLOAT3 sliceThickness, XMINT3 resolution, float timestep, XMINT3 timestepIndices);
	virtual ~VolumeData(void);

	// methods
	virtual HRESULT CreateGPUBuffers(void) { return S_OK; };
	virtual void ReleaseGPUBuffers(void) { };
	virtual void UpdateHistogram(int timestep0, int timestep1, float timestepT) {};

	//accessors
	std::string GetObjectFileName() {			return m_objectFileName; };
	XMFLOAT3	GetBoundingBox();
	void SetTime(float currentTime);
	const float		& GetTime() {				return m_currentTime; };
	const float		& GetCurrentTimestepT() {	return m_currentTimestepT; };

	const XMINT3	& GetResolution() 	{		return m_resolution;	};
	const XMFLOAT3	& GetSliceThickness() {		return m_sliceThickness;};
	const float		& GetTimestep()		{		return m_timestep;	};
	const float		& GetTimeSequenceLength() {	return m_timeSequenceLength;	};

	ID3D11ShaderResourceView * GetTexture0SRV() {		return m_pVolumeData0SRV;	};
	ID3D11ShaderResourceView * GetTexture1SRV() {		return m_pVolumeData1SRV;	};
	ID3D11ShaderResourceView * GetHistogramSRV() {		return m_pHistogramSRV;		};

protected:
	// static functions
	static void SetupTwBar(TwBar * pParametersBar);

	// static variables
	static ID3D11Device	* pd3dDevice;
	static TwBar		* pParametersBar;
	static TwType		volumeDataType;

	// methods
	virtual void LoadTimestep(int timestep0, int timestep1 = -1);
	void LoadDataFiles(std::string objectFileName);

	// members
	std::string m_objectFileName;
	float m_currentTime;
	float m_currentTimestepT;
	std::vector<void *> m_data;
	bool m_externalData;	// this signals that another class manages the data
							// in that case, m_data contains nothing and the other class
							// manages the GPU textures and updates the SRVs etc.
	const float m_timeSequenceLength;	//the realtime length of the dataset
	
	std::vector<float*>		    m_histogram;

	DataFormat	m_format;
	int			m_elementSize;
	int			m_elementPadding;
	XMFLOAT3	m_sliceThickness;
	XMINT3		m_resolution;
	float		m_timestep;			// time in seconds between two data steps
	XMINT3		m_timestepIndices;	//Specifies the index for the individual timesteps: x = min, y = max, z = step
									//i.e. (0, 6, 2) produces timesteps 0, 2, 4, 6
									//has nothing to do with the realtime length of the dataset
	unsigned int m_currentDatasetSlot0;	//the timestep currently loaded to the GPU buffer of m_pVolumeData0SRV
	unsigned int m_currentDatasetSlot1;	// - " - of m_pVolumeData1SRV
	
	// dx resources
	ID3D11Texture3D * m_pVolumeData0;
	ID3D11Texture3D * m_pVolumeData1;
	ID3D11ShaderResourceView * m_pVolumeData0SRV;	//SRVs pointing to the currently loaded timesteps 0 and 1
	ID3D11ShaderResourceView * m_pVolumeData1SRV;	//warning: they not in general refer to m_pTexture0 and m_pTexture1

	ID3D11Texture1D * m_pHistoTexture;
	ID3D11ShaderResourceView * m_pHistogramSRV;
};

