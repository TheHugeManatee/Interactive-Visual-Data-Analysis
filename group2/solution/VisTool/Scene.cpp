#include "Scene.h"

#include "util/util.h"
#include "Globals.h"
#include "SimpleMesh.h"
#include "PLYMesh.h"
#include "Triangle.h"
#include "SettingsStorage.h"
#include "SliceVisualizer.h"
#include "BoxManipulationManager.h"

#include "../../../external/rply-1.1.3/rply.h"

#include <Shlwapi.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>
#include <algorithm>

#pragma warning (disable: 4351)

ID3D11Device * Scene::pd3dDevice = nullptr;
TwBar * Scene::pParametersBar = nullptr;

extern BoxManipulationManager * g_boxManipulationManager;

HRESULT Scene::Initialize(ID3D11Device * pd3dDevice_, TwBar * pParametersBar_)
{

	pd3dDevice = pd3dDevice_;
	pParametersBar = pParametersBar_;
	return S_OK;
}

HRESULT Scene::Release()
{
	return S_OK;
}

void TW_CALL Scene::SetTimeCB(const void *value, void *clientData)
{ 
	Scene * me = static_cast<Scene*>(clientData);
    me->m_playbackTime = *(const float *)value;

	// temporarily unpause, make a zero timestep, and pause again
	// to update the timestep buffers
	bool p = me->m_playbackPaused;
	me->m_playbackPaused = false;
	me->FrameMove(0, 0);
	me->m_playbackPaused = p;
}

void TW_CALL Scene::GetTimeCB(void *value, void *clientData)
{ 
    *(float *)value = static_cast<Scene*>(clientData)->m_playbackTime;
}

void TW_CALL Scene::SetRaycasterEnabledCB(const void *value, void *clientData)
{ 
	Scene * me = static_cast<Scene*>(clientData);
	bool enabled = *(const bool *)value;

	if(enabled && !me->m_rayCaster) {
		if(me->m_scalarVolumeData)
			me->m_rayCaster = new RayCaster(*me->m_scalarVolumeData);
		else
			me->m_rayCaster = new RayCaster(*me->m_vectorVolumeData->GetScalarMetricVolume());
	}
	else if(!enabled && me->m_rayCaster) {
		delete me->m_rayCaster;
		me->m_rayCaster = nullptr;
	}

}
void TW_CALL Scene::GetRaycasterEnabledCB(void *value, void *clientData)
{ 
    *(bool *)value = static_cast<Scene*>(clientData)->m_rayCaster != nullptr;
}

void TW_CALL Scene::SetGlyphVisualizerCB(const void *value, void *clientData)
{ 
	Scene * me = static_cast<Scene*>(clientData);
	bool enabled = *(const bool *)value;

	if(enabled && !me->m_glyphVisualizer) {
		assert(me->m_vectorVolumeData);

		me->m_glyphVisualizer = new GlyphVisualizer(*me->m_vectorVolumeData);
	}
	else if(!enabled && me->m_glyphVisualizer) {
		delete me->m_glyphVisualizer;
		me->m_glyphVisualizer = nullptr;
	}
}

void TW_CALL Scene::GetGlyphVisualizerCB(void *value, void *clientData)
{ 
	*(bool *)value = static_cast<Scene*>(clientData)->m_glyphVisualizer != nullptr;
}

void TW_CALL Scene::CreateParticleTracerCB(void *clientData)
{ 
	Scene * me = static_cast<Scene*>(clientData);

	// particle tracer instances are managed by the ParticleTracer static functions and methods internally
	// so its ok just to create a "dangling" pointer here and forget about it, it will be deleted
	// by ParticleTracer::DeleteInstances()
	new ParticleTracer(*me->m_vectorVolumeData);
}

void TW_CALL Scene::CreateSliceVisualizerCB(void *clientData)
{ 
	Scene * me = static_cast<Scene*>(clientData);

	// same strategy as with particle tracer instances
	new SliceVisualizer(me->m_scalarVolumeData, me->m_vectorVolumeData);
}

Scene::Scene(std::string sceneDatFile) :
	m_sceneDatFile(sceneDatFile),
	m_objectFileName(""),
	m_meshFileName(""),
	m_objectFileFormat(VolumeData::DF_UNKNOWN),
	m_boundingBoxSize(),
	m_sliceThickness(),
	m_resolution(),

	m_mesh(nullptr),
	m_scalarVolumeData(nullptr),
	m_vectorVolumeData(nullptr),
	m_rayCaster(nullptr),
	m_glyphVisualizer(nullptr),

	m_playbackTime(0),
	m_playbackPaused(false),
	m_playbackRepeat(true),
	m_playbackSpeed(1.f),
	m_timestep(1.f)
{
	LoadSceneDatFile(sceneDatFile);

	SetupTwBar(pParametersBar);
}

Scene::Scene(SettingsStorage &store) :
	m_sceneDatFile(""),
	m_objectFileName(""),
	m_meshFileName(""),
	m_objectFileFormat(VolumeData::DF_UNKNOWN),
	m_boundingBoxSize(),
	m_sliceThickness(),
	m_resolution(),

	m_mesh(nullptr),
	m_scalarVolumeData(nullptr),
	m_vectorVolumeData(nullptr),
	m_rayCaster(nullptr),
	m_glyphVisualizer(nullptr),

	m_playbackTime(0),
	m_playbackPaused(false),
	m_playbackRepeat(true),
	m_playbackSpeed(1.f),
	m_timestep(1.f)
{
	store.GetString("scene.filename", m_sceneDatFile);
	LoadSceneDatFile(m_sceneDatFile);
	LoadConfig(store);

	SetupTwBar(pParametersBar);
}

Scene::~Scene(void)
{
	if(m_mesh)			delete m_mesh;
	if(m_rayCaster)		delete m_rayCaster;
	if(m_glyphVisualizer) delete m_glyphVisualizer;

	ParticleTracer::DeleteInstances();
	SliceVisualizer::DeleteInstances();

	if(m_scalarVolumeData)	m_scalarVolumeData->ReleaseGPUBuffers();
	if(m_vectorVolumeData) m_vectorVolumeData->ReleaseGPUBuffers();

	TwRemoveVar(pParametersBar, "Ray Caster Enabled");
	TwRemoveVar(RayCaster::pParametersBar, "Ray Caster Enabled");
	TwRemoveVar(pParametersBar, "Create Slice Visualization");
	TwRemoveVar(SliceVisualizer::pParametersBar, "Create Slice Visualization");
	if(m_vectorVolumeData) {
		TwRemoveVar(pParametersBar, "Glyph Rendering Enabled");
		TwRemoveVar(GlyphVisualizer::pParametersBar, "Glyph Rendering Enabled");
		TwRemoveVar(pParametersBar, "Create Particle Probe");
		TwRemoveVar(ParticleTracer::pParametersBar, "Create Particle Probe");
	}
	TwDeleteBar(m_playbackBar);
}

void Scene::SetupTwBar(TwBar * pParametersBar)
{
	m_playbackBar = TwNewBar("Playback");
	TwDefine("Playback color='30 150 30' size='300 100' position='15 335' refresh=0.1");

	TwAddVarRW(m_playbackBar, "Paused", TW_TYPE_BOOLCPP, &m_playbackPaused, "key=P");
	TwAddVarRW(m_playbackBar, "Repeat", TW_TYPE_BOOLCPP, &m_playbackRepeat, "");
	TwAddVarRW(m_playbackBar, "Speed (Timestep)", TW_TYPE_FLOAT, &m_playbackSpeed, "min=0 step=0.05");
	TwAddVarCB(m_playbackBar, "Current Time", TW_TYPE_FLOAT, SetTimeCB, GetTimeCB, this, "min=0");
}

void Scene::LoadSceneDatFile(std::string sceneDatFile)
{
	//we want to reset the instance counter for meshes before loading a new scene
	SimpleMesh::instanceCount = 0;

	std::ifstream datFile;
	m_resolution[0] = m_resolution[1] = m_resolution[2] = 1;
	m_objectIndices[0] = m_objectIndices[1] = 0;
	m_objectIndices[2] = 1;
	
	std::cout << "Opening File " << sceneDatFile << std::endl;
	char cwd[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH, cwd);
	std::cout << "Relative To " << cwd << std::endl;
	datFile.open(sceneDatFile);
	while(!datFile.is_open()) {
		size_t length;
		WCHAR szFile[256];
		OPENFILENAME ofn;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = szFile;
		// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
		// use the contents of szFile to initialize itself.
		ofn.lpstrFile[0] = '\0';
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter = L"All\0*.*\0Text\0*.TXT\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
		BOOL successful = GetOpenFileName(&ofn);
		
		if(successful) {
			char filename[256];
			wcstombs_s(&length, filename, szFile, sizeof(filename));
			datFile.open(filename);
		}
	}
	//save the absolute path to the dat file
	char absolutePath[1024];
	if(_fullpath(absolutePath, m_sceneDatFile.c_str(), 1024))
		m_sceneDatFile = absolutePath;

	//extract file path of .dat file
	std::string filepath = GetPath(sceneDatFile);

	float volumeTimestep = 1;

	//parse the dat file
	while(!datFile.eof()) {
		std::string identifier;
		char comment[512];

		datFile >> identifier;

		//ignore comments
		if(identifier.size() && identifier[0] != '#') {

			if(!identifier.compare("MeshFileName:"))
			{
				//append .ply file name to file path of .dat file
				datFile >> m_meshFileName;
				m_meshFileName.insert(0, filepath);
				std::cout << "Mesh File Name: " << m_meshFileName << std::endl;
			}
			else if(!identifier.compare("ObjectFileName:"))
			{
				//append .ply file name to file path of .dat file
				datFile >> m_objectFileName;
				m_objectFileName.insert(0, filepath);
				std::cout << "Object File Name: " << m_objectFileName << std::endl;
			}
			else if(!identifier.compare("ObjectIndices:"))
				datFile >> m_objectIndices[0] >> m_objectIndices[1] >> m_objectIndices[2];
			else if(!identifier.compare("SliceThickness:"))
				datFile >> m_sliceThickness[0] >> m_sliceThickness[1] >> m_sliceThickness[2];
			else if(!identifier.compare("Resolution:"))
				datFile >> m_resolution[0] >> m_resolution[1] >> m_resolution[2];
			else if(!identifier.compare("Format:")) {
				std::string str;
				datFile >> str;
				m_objectFileFormat = VolumeData::GetFormatFromStr(str);
			}
			else if(!identifier.compare("Timestep:"))
				datFile >> volumeTimestep;
			else
				std::cout << "Unknown Identifier: \"" << identifier << "\"" << std::endl;
		}
		//throw away the rest of the line
		datFile.getline(comment, sizeof(comment));
	}

	//calculate object size
	m_boundingBoxSize[0] = m_resolution[0] * m_sliceThickness[0];
	m_boundingBoxSize[1] = m_resolution[1] * m_sliceThickness[1];
	m_boundingBoxSize[2] = m_resolution[2] * m_sliceThickness[2];

	//load .ply file
	if(!m_meshFileName.empty()) {
		m_mesh = new PLYMesh(m_meshFileName);
		//m_mesh = new Triangle;
		m_mesh->SetColor(XMFLOAT4(0, 1, 0, 1));
		m_mesh->SetBoundingBox(XMFLOAT3(m_boundingBoxSize));
	}
	
	TwAddVarCB(pParametersBar, "Ray Caster Enabled", TW_TYPE_BOOLCPP, SetRaycasterEnabledCB, GetRaycasterEnabledCB, this, "");
	TwAddVarCB(RayCaster::pParametersBar, "Ray Caster Enabled", TW_TYPE_BOOLCPP, SetRaycasterEnabledCB, GetRaycasterEnabledCB, this, "");
	TwAddButton(pParametersBar, "Create Slice Visualization", CreateSliceVisualizerCB, this, "");
	TwAddButton(SliceVisualizer::pParametersBar, "Create Slice Visualization", CreateSliceVisualizerCB, this, "");

	//load volume data
	if(!m_objectFileName.empty())
	{
		//
		switch(m_objectFileFormat) {
		case VolumeData::DF_BYTE:
			if(g_globals.volumeData && m_objectFileName == g_globals.volumeData->GetObjectFileName()) {
				m_scalarVolumeData = dynamic_cast<ScalarVolumeData*>(g_globals.volumeData);
			}
			else {
				m_scalarVolumeData = new ScalarVolumeData(m_objectFileName, m_objectFileFormat,
					XMFLOAT3(m_sliceThickness[0], m_sliceThickness[1], m_sliceThickness[2]),
					XMINT3(m_resolution[0], m_resolution[1], m_resolution[2]), 
					volumeTimestep,
					XMINT3(m_objectIndices[0], m_objectIndices[1], m_objectIndices[2]));

				if(g_globals.volumeData)	delete g_globals.volumeData;
				g_globals.volumeData = m_scalarVolumeData;
			}
			//create buffer for time-dependent data
			m_scalarVolumeData->CreateGPUBuffers();

			//m_rayCaster = new RayCaster(*m_scalarVolumeData);
			//new SliceVisualizer(m_scalarVolumeData, nullptr);
			break;
		case VolumeData::DF_FLOAT3:
		case VolumeData::DF_HALF3:
		case VolumeData::DF_FLOAT4:
		case VolumeData::DF_HALF4:
			TwAddVarCB(pParametersBar, "Glyph Rendering Enabled", TW_TYPE_BOOLCPP, SetGlyphVisualizerCB, GetGlyphVisualizerCB, this, "");
			TwAddVarCB(GlyphVisualizer::pParametersBar, "Glyph Rendering Enabled", TW_TYPE_BOOLCPP, SetGlyphVisualizerCB, GetGlyphVisualizerCB, this, "");
			TwAddButton(pParametersBar, "Create Particle Probe", CreateParticleTracerCB, this, "");
			TwAddButton(ParticleTracer::pParametersBar, "Create Particle Probe", CreateParticleTracerCB, this, "");
			//check if we have the volume data cached already
			if(g_globals.volumeData && m_objectFileName == g_globals.volumeData->GetObjectFileName()) {
				m_vectorVolumeData = dynamic_cast<VectorVolumeData*>(g_globals.volumeData);
			}
			else {
				m_vectorVolumeData = new VectorVolumeData(m_objectFileName, m_objectFileFormat,
					XMFLOAT3(m_sliceThickness[0], m_sliceThickness[1], m_sliceThickness[2]),
					XMINT3(m_resolution[0], m_resolution[1], m_resolution[2]), 
					volumeTimestep,
					XMINT3(m_objectIndices[0], m_objectIndices[1], m_objectIndices[2]));
				if(g_globals.volumeData)
					delete g_globals.volumeData;
				g_globals.volumeData = m_vectorVolumeData;
			}
			m_vectorVolumeData->CreateGPUBuffers();
			
			//m_rayCaster = new RayCaster(*m_vectorVolumeData->GetScalarMetricVolume());
			//m_glyphVisualizer = new GlyphVisualizer(*m_vectorVolumeData);
			//new ParticleTracer(*m_vectorVolumeData);
			//new SliceVisualizer(nullptr, m_vectorVolumeData);
			break;
		default:
			std::cerr << "Unknown Data Format! Cannot Visualize..." << std::endl;
			break;
		}
	}

	// we globally scale the scene to fit into the standard view space and center it on the center position of the bounding box
	float s = 3.f/(m_boundingBoxSize[0] + m_boundingBoxSize[1] + m_boundingBoxSize[2]);
	m_globalTransform = XMMatrixScaling(s, s, s) * XMMatrixTranslation(-0.5f*m_boundingBoxSize[0]*s, -0.5f*m_boundingBoxSize[1]*s, -0.5f*m_boundingBoxSize[2]*s);
}

void Scene::SaveConfig(SettingsStorage &store)
{
	CHAR relativeDatFile[MAX_PATH];
	
	//std::cout << "Original: " << m_sceneDatFile << std::endl;
	//std::cout << "Config Path: " << g_globals.configPath << std::endl;

	BOOL successful = PathRelativePathToA(relativeDatFile, g_globals.configPath.c_str(), FILE_ATTRIBUTE_DIRECTORY, m_sceneDatFile.c_str(), FILE_ATTRIBUTE_NORMAL);
	if(successful) {
		//std::cout << "Relative Path of the Dat file:" << std::string(relativeDatFile) << std::endl;
		store.StoreString("scene.filename", relativeDatFile);
	}
	else {
		//std::cout << "Storing absolute path: " << m_sceneDatFile << std::endl;
		store.StoreString("scene.filename", m_sceneDatFile);
	}
	store.StoreFloat("scene.playback.currentTime", m_playbackTime);
	store.StoreFloat("scene.playback.speed", m_playbackSpeed);
	store.StoreBool("scene.playback.repeat", m_playbackRepeat);
	store.StoreBool("scene.playback.paused", m_playbackPaused);
	store.StoreBool("scene.raycaster.enabled", m_rayCaster != nullptr);
	store.StoreBool("scene.glyphs.enabled", m_glyphVisualizer != nullptr);


	if(m_mesh)
		m_mesh->SaveConfig(store);

	if(m_rayCaster)
		m_rayCaster->SaveConfig(store);
	if(m_vectorVolumeData)
		m_vectorVolumeData->SaveConfig(store);
	if(m_scalarVolumeData)
		m_scalarVolumeData->SaveConfig(store);

	ParticleTracer::SaveConfig(store);
	SliceVisualizer::SaveConfig(store);

	if(m_glyphVisualizer)
		m_glyphVisualizer->SaveConfig(store);

	//save transfer function
	std::string tfFilename = GetPath(g_globals.configFile) + GetFilename(g_globals.configFile, true) + ".tf";
	std::ofstream file(tfFilename, std::ios_base::binary);
	g_transferFunctionEditor->saveTransferFunction(&file);
	file.close();
}

void Scene::LoadConfig(SettingsStorage &store)
{
	store.GetFloat("scene.playback.currentTime", m_playbackTime);
	store.GetFloat("scene.playback.speed", m_playbackSpeed);
	store.GetBool("scene.playback.repeat", m_playbackRepeat);
	store.GetBool("scene.playback.paused", m_playbackPaused);

	if(m_mesh)
		m_mesh->LoadConfig(store);

	if(m_vectorVolumeData)
		m_vectorVolumeData->LoadConfig(store);
	if(m_scalarVolumeData)
		m_scalarVolumeData->LoadConfig(store);

	bool rayCasterEnabled = false, glyphVisualizerEnabled = false;
	store.GetBool("scene.raycaster.enabled", rayCasterEnabled);
	store.GetBool("scene.glyphs.enabled", glyphVisualizerEnabled);
	SetRaycasterEnabledCB(&rayCasterEnabled, this);
	SetGlyphVisualizerCB(&glyphVisualizerEnabled, this);

	if(m_vectorVolumeData)
		ParticleTracer::LoadConfig(store, *m_vectorVolumeData);

	SliceVisualizer::LoadConfig(store, m_scalarVolumeData, m_vectorVolumeData);

	if(m_rayCaster)
		m_rayCaster->LoadConfig(store);
	if(m_glyphVisualizer)
		m_glyphVisualizer->LoadConfig(store);
	
	//restore transfer function
	std::string tfFilename = GetPath(g_globals.configFile) + GetFilename(g_globals.configFile, true) + ".tf";
	std::ifstream file(tfFilename, std::ios_base::binary);
	if(file.is_open()) {
		g_transferFunctionEditor->loadTransferFunction(&file);
		file.close();
	}
}

HRESULT Scene::Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations camMtcs)
{
	RenderTransformations sceneMtcs = camMtcs.PremultWorldMatrix(m_globalTransform);

	HRESULT hr;
	XMFLOAT3 bbox = (m_scalarVolumeData?(VolumeData*)m_scalarVolumeData:(VolumeData*)m_vectorVolumeData)->GetBoundingBox();
	V_RETURN(SimpleMesh::DrawBoundingBox(pd3dImmediateContext, sceneMtcs.modelWorldViewProj, bbox));

	if(m_mesh)
		V_RETURN(m_mesh->Render(pd3dImmediateContext, sceneMtcs));

	V_RETURN(ParticleTracer::Render(pd3dImmediateContext, sceneMtcs));

	V_RETURN(SliceVisualizer::Render(pd3dImmediateContext, sceneMtcs));

	if(m_glyphVisualizer)
		V_RETURN(m_glyphVisualizer->Render(pd3dImmediateContext, sceneMtcs));

	if(m_rayCaster)
		V_RETURN(m_rayCaster->Render(pd3dImmediateContext, sceneMtcs));

	return S_OK;
}

HRESULT Scene::RenderTransparency(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations camMtcs)
{
	RenderTransformations sceneMtcs = camMtcs.PremultWorldMatrix(m_globalTransform);

	HRESULT hr;

	if(m_rayCaster)
		V_RETURN(m_rayCaster->RenderTransparency(pd3dImmediateContext, sceneMtcs));
	
	V_RETURN(SliceVisualizer::RenderTransparency(pd3dImmediateContext, sceneMtcs));

	V_RETURN(ParticleTracer::RenderTransparency(pd3dImmediateContext, sceneMtcs));

	return S_OK;
}

void Scene::FrameMove(double dTime, float fElapsedTime)
{
	// we just skip longer pauses and time travel
	if(fElapsedTime < 0 || fElapsedTime > 4)
		return;

	float timeSequenceLength = (m_vectorVolumeData?(VolumeData*)m_vectorVolumeData:m_scalarVolumeData)->GetTimeSequenceLength();

	//we always update the vector dataset because we potentially need to recalculate the metric due to changing parameters
	// here's room for performance improvement (only recalculate if really necessary => parameters changed)
	if(m_vectorVolumeData)
		m_vectorVolumeData->SetTime(m_playbackTime);

	// advances all visualizers
	ParticleTracer::FrameMove(dTime, fElapsedTime, fElapsedTime * m_playbackSpeed);
	SliceVisualizer::FrameMove(dTime, fElapsedTime, fElapsedTime * m_playbackSpeed);
	if(m_rayCaster)	m_rayCaster->FrameMove(dTime, fElapsedTime, fElapsedTime * m_playbackSpeed);

	// return if we don't have multiple time steps or if we are paused
	if(m_objectIndices[1] < 1 || m_playbackPaused) {
		return;
	}

	m_playbackTime += fElapsedTime * m_playbackSpeed;


	//rewind or limit to end time
	if(m_playbackRepeat) {
		if(m_playbackTime >= timeSequenceLength)
			m_playbackTime = 0.f;
	}
	else {
		m_playbackTime = std::min(m_playbackTime, timeSequenceLength);
	}

	if(m_scalarVolumeData)
		m_scalarVolumeData->SetTime(m_playbackTime);

	//calculate the timestep time (in [0, 1]) between the two timesteps
	float timestepTime = m_playbackTime - floorf(m_playbackTime);
	//before playback?
	if(m_playbackTime < 0)
		timestepTime = 0;
	//end of playback?
	if(m_playbackTime >= timeSequenceLength)
		timestepTime = 1;
}

void Scene::OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
					int nMouseWheelDelta, float xPos, float yPos)
{

}

std::string Scene::GetSceneDatFile()
{
	return m_sceneDatFile;
}