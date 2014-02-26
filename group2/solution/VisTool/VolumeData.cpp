#include "VolumeData.h"

#include "util/util.h"

#include "AntTweakBar.h"

#include <iostream>
#include <fstream>

ID3D11Device	* VolumeData::pd3dDevice;
TwBar			* VolumeData::pParametersBar;
TwType			VolumeData::volumeDataType;

int VolumeData::GetElementSize(DataFormat f) {
	switch (f) {
		case DF_BYTE:		return sizeof(BYTE);
		case DF_FLOAT:		return sizeof(float);
		case DF_HALF3:		return 3 * sizeof(float)/2;
		case DF_FLOAT3:		return 3 * sizeof(float);
		case DF_HALF4:		return 4 * sizeof(float)/2;
		case DF_FLOAT4:		return 4 * sizeof(float);
	}
	return 0;
}

int VolumeData::GetElementPadding(DataFormat f) {
	switch (f) {
		case DF_BYTE:		return 0;
		case DF_FLOAT:		return 0;
		case DF_HALF3:		return sizeof(float)/2;
		case DF_FLOAT3:		return sizeof(float);
		case DF_HALF4:		return 0;
		case DF_FLOAT4:		return 0;
	}
	return 0;
}

VolumeData::DataFormat VolumeData::GetFormatFromStr(std::string str)
{
	if(str == "BYTE" || str == "UCHAR") 	return DF_BYTE;
	if(str == "float")		return DF_FLOAT;
	if(str == "half3")		return DF_HALF3;
	if(str == "float3")		return DF_FLOAT3;
	if(str == "half4")		return DF_HALF4;
	if(str == "float4")		return DF_FLOAT4;

	return DF_UNKNOWN;
}

HRESULT VolumeData::Initialize(ID3D11Device * pd3dDevice_, TwBar* pParametersBar_)
{
	pd3dDevice = pd3dDevice_;
	SetupTwBar(pParametersBar_);
	return S_OK;
}

HRESULT VolumeData::Release(void)
{
	return S_OK;
}

void VolumeData::SetupTwBar(TwBar * pParametersBar_) {

	//pParametersBar = pParametersBar_;
	pParametersBar = TwNewBar("Volume Data");
	TwDefine("'Volume Data' color='128 128 128' size='300 180' position='15 440'");
	
	static TwStructMember resolutionMembers[] = // array used to describe tweakable variables of the Light structure
    {
        { "x",    TW_TYPE_INT32, 0 * sizeof(int),    "" }, 
        { "y",    TW_TYPE_INT32, 1 * sizeof(int),    "" }, 
		{ "z",    TW_TYPE_INT32, 2 * sizeof(int),    "" },
    };
	TwType resType = TwDefineStruct("Resolution", resolutionMembers, 3, sizeof(int[3]), NULL, NULL);

	static TwStructMember sliceMembers[] = // array used to describe tweakable variables of the Light structure
    {
        { "x",    TW_TYPE_FLOAT, 0 * sizeof(float),    "" }, 
        { "y",    TW_TYPE_FLOAT, 1 * sizeof(float),    "" }, 
        { "z",    TW_TYPE_FLOAT, 2 * sizeof(float),    "" },
    };
	TwType sliceType = TwDefineStruct("Slice Thickness", sliceMembers, 3, sizeof(float[3]), NULL, NULL);

	static TwStructMember timeMembers[] = // array used to describe tweakable variables of the Light structure
    {
        { "Start",    TW_TYPE_INT32, 0 * sizeof(int),    "" }, 
        { "End",    TW_TYPE_INT32, 1 * sizeof(int),     "" }, 
        { "Step",    TW_TYPE_INT32, 2 * sizeof(int),    "" },
    };
	TwType timeType = TwDefineStruct("Time Data", timeMembers, 3, sizeof(int[3]), NULL, NULL);

	TwType fmtType = TwDefineEnumFromString("VolumeDataFormat", "UNKNOWN,BYTE,FLOAT,HALF3,FLOAT3,HALF4,FLOAT4");

	// Define a new struct type: light variables are embedded in this structure
    static TwStructMember vdMembers[] = // array used to describe tweakable variables of the Light structure
    {
		{ "Data Format",	fmtType, offsetof(VolumeData, m_format), ""},
		{ "Resolution",    resType, offsetof(VolumeData, m_resolution),    "" }, 
		{ "Slice Thickness",    sliceType, offsetof(VolumeData, m_sliceThickness),     "" }, 
		{ "Time Data",		timeType,   offsetof(VolumeData, m_timestepIndices),    "" },
		{ "External",		TW_TYPE_BOOLCPP,  offsetof(VolumeData, m_externalData), "" },
		{ "Timestep Tex0",	TW_TYPE_INT32,  offsetof(VolumeData, m_currentDatasetSlot0), "" },
		{ "Timestep Tex1",	TW_TYPE_INT32,  offsetof(VolumeData, m_currentDatasetSlot1), "" }
    };
    volumeDataType = TwDefineStruct("Volume Data", vdMembers, 7, sizeof(VolumeData), NULL, NULL);  // create a new TwType associated to the struct defined by the lightMembers array
}

VolumeData::VolumeData(std::string objectFileName, DataFormat format, XMFLOAT3 sliceThickness, XMINT3 resolution, float timestep, XMINT3 timestepIndices) :
	m_objectFileName(objectFileName),
	m_format(format),
	m_sliceThickness(sliceThickness),
	m_resolution(resolution),
	m_pVolumeData0(nullptr),
	m_pVolumeData1(nullptr),
	m_pVolumeData0SRV(nullptr),
	m_pVolumeData1SRV(nullptr),
	m_timestepIndices(timestepIndices),
	m_currentDatasetSlot0(0),
	m_currentDatasetSlot1(0),
	m_externalData(false),
	m_pHistoTexture(nullptr),
	m_pHistogramSRV(nullptr),
	m_timestep(timestep),
	m_currentTime(0),
	m_currentTimestepT(0),
	m_timeSequenceLength(timestep * (timestepIndices.y - timestepIndices.x)/(float)timestepIndices.z)
{
	m_elementSize = GetElementSize(format);
	m_elementPadding = GetElementPadding(format);
}


VolumeData::~VolumeData(void)
{
}

/*
	currentTime is the simulation time passed in seconds (i.e. real time passed divided by playback speed)
*/
void VolumeData::SetTime(float currentTime) 
{

//	std::cout << "SetTime()" << std::endl;
	assert(currentTime >= 0);
	//assert(currentTime <= m_data.size());

	m_currentTime = currentTime;

	// don't do anything if we have only one timestep
	if(!m_timeSequenceLength)
		return;

	assert(!m_externalData);	// we don't want to mess with SRVs if another class manages the data

	//determine the required dataset indices for the two resources
	int requiredTimestep0;

	requiredTimestep0 = (int) (currentTime / m_timestep); // round down to next integer timestep
	requiredTimestep0 = std::min((int)m_data.size() - 1, requiredTimestep0); // assure limits

	// if we have the last dataset, map the last and last-but-one 
	if(requiredTimestep0 == m_data.size() - 1) {
		requiredTimestep0--;
	}

	m_currentTimestepT = currentTime/m_timestep - floorf(currentTime/m_timestep);
	UpdateHistogram(requiredTimestep0, requiredTimestep0 + 1, m_currentTimestepT);
//	std::cout << "T: " << currentTime << "; timestepT: " << m_currentTimestepT << std::endl;

	// update the resources accordingly
	// both slots correct
	if(requiredTimestep0 == m_currentDatasetSlot0 && requiredTimestep0 + 1 == m_currentDatasetSlot1)
		return;
	
	//std::cout << "Currently loaded: " << m_currentDatasetSlot0 << " and " << m_currentDatasetSlot1 << std::endl;
	//std::cout << "Required: " << requiredTimestep0 << " and " << requiredTimestep0 + 1 << std::endl;

	// case that second slot has data that should be in first slot
	if(requiredTimestep0 == m_currentDatasetSlot1) {
		// load the new data into the first buffer
		LoadTimestep(requiredTimestep0 + 1);

		// switch the srvs around
		auto t = m_pVolumeData0SRV;
		m_pVolumeData0SRV = m_pVolumeData1SRV;
		m_pVolumeData1SRV = t;
	}
	else {
		// fill both buffers with new data*/
		LoadTimestep(requiredTimestep0, requiredTimestep0 + 1);
	}
	m_currentDatasetSlot0 = requiredTimestep0;
	m_currentDatasetSlot1 = requiredTimestep0 + 1;
}

XMFLOAT3	VolumeData::GetBoundingBox()
{
	return XMFLOAT3(m_resolution.x * m_sliceThickness.x,
					m_resolution.y * m_sliceThickness.y,
					m_resolution.z * m_sliceThickness.z);
}

/**
	Loads timestep0 and timestep1 data into the respective GPU buffers
	Performs lazy update, i.e. does not upload more data than necessary
*/
void VolumeData::LoadTimestep(int timestep0, int timestep1) {
	ID3D11DeviceContext * pContext;
	pd3dDevice->GetImmediateContext(&pContext);
	
	ID3D11Resource *srv0Resource, *srv1Resource;
	assert(timestep0 < m_data.size());
	m_pVolumeData0SRV->GetResource(&srv0Resource);
	pContext->UpdateSubresource(srv0Resource, 0, nullptr, m_data[timestep0], m_resolution.x * (m_elementSize + m_elementPadding), m_resolution.x * m_resolution.y * (m_elementSize + m_elementPadding));
	SAFE_RELEASE(srv0Resource);
	
	if(timestep1 >= 0) {
		assert(timestep1 < m_data.size());
		m_pVolumeData1SRV->GetResource(&srv1Resource);
		pContext->UpdateSubresource(srv1Resource, 0, nullptr, m_data[timestep1], m_resolution.x * (m_elementSize + m_elementPadding), m_resolution.x * m_resolution.y * (m_elementSize + m_elementPadding));
		SAFE_RELEASE(srv1Resource);
	}
	SAFE_RELEASE(pContext);
}


/**
	Loads the data from HDD to RAM
	objectFileName can be a either a fixed path or contain printf format specifiers. In
	that case, the format is filled by the appropriate indices indicated by m_timestepIndices
*/
void VolumeData::LoadDataFiles(std::string objectFileName) 
{
	std::cout << "Loading Data  from \"" << objectFileName << "\"to RAM..." << std::endl;
	int numFiles = (m_timestepIndices.y - m_timestepIndices.x)/m_timestepIndices.z + 1;
	char * unpadded;
	m_data.resize(numFiles);
	m_histogram.resize(numFiles);
	unsigned int voxelCount = m_resolution.x * m_resolution.y * m_resolution.z;

	if(m_elementPadding) {
		unpadded = new char[voxelCount * m_elementSize];
	}

	for(int i = 0; i < numFiles; i++)
	{
		char objectFileName_a[2048];
		sprintf_s(objectFileName_a, 2048, objectFileName.c_str(), i * m_timestepIndices.z + m_timestepIndices.x);
		std::ifstream in(objectFileName_a, std::ifstream::in | std::ifstream::binary | std::ifstream::ate);


		auto size = in.tellg();
		assert(voxelCount*m_elementSize == size);

		m_data[i] = new char[voxelCount * (m_elementSize + m_elementPadding)];
		in.seekg (0, std::ios::beg);
		std::cout << "Reading " << size << " bytes from \"" << GetFilename(objectFileName_a) << "\"..." << std::flush;
		
		// insert padding if necessary
		if(m_elementPadding) {
			in.read(unpadded, size);
			ZeroMemory(m_data[i], voxelCount * (m_elementSize + m_elementPadding));
			for(unsigned int j=0; j < voxelCount; j++) {
				memcpy(&static_cast<char*>(m_data[i])[j*(m_elementSize + m_elementPadding)], &unpadded[j*m_elementSize], m_elementSize);
			}
		}
		else
			in.read ((char*)m_data[i], size);

		std::cout << "\tDONE." << std::endl;
		in.close();

		if(m_format == DF_BYTE) {
			m_histogram[i] = new float[255 * 4];
			float histoCount[255];

			memset(histoCount, 0, sizeof(histoCount));

			for(int j=0; j < size; j++) {
				histoCount[static_cast<unsigned char*>(m_data[i])[j]] += 1.f;
			}

			//calculate the histogram
			for(int j=0; j < 255; j++) {
				m_histogram[i][4*j] = histoCount[j] / size;
				m_histogram[i][4*j + 1] = histoCount[j] / size;
				m_histogram[i][4*j + 2] = histoCount[j] / size;
				m_histogram[i][4*j + 3] = histoCount[j] / size;
			}
		}
	}

	if(m_elementPadding)
		delete[] unpadded;
}