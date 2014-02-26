// Scene class contains all information on the current scene
// loads the information from the 

#pragma once

#include "SimpleMesh.h"
#include "ScalarVolumeData.h"
#include "VectorVolumeData.h"
#include "RayCaster.h"
#include "GlyphVisualizer.h"
#include "ParticleTracer.h"
#include "BoxManipulationManager.h"

#include <DXUT.h>
#include <DXUTcamera.h>

#include "AntTweakBar.h"

#include <string>


class SettingsStorage;

class Scene
{
public:
	// statics
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);

	// ctors, dtor
	Scene(std::string sceneDatFile);
	Scene(SettingsStorage &store);
	~Scene(void);

	// methods
	void LoadSceneDatFile(std::string sceneDatFile);

	HRESULT Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations camMtcs);
	HRESULT RenderTransparency(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations camMtcs);
	void FrameMove(double dTime, float fElapsedTime);
	void OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
					int nMouseWheelDelta, float xPos, float yPos);

	std::string GetSceneDatFile();

	void SaveConfig(SettingsStorage &store);
	void LoadConfig(SettingsStorage &store);

	// accessors
	XMFLOAT4 GetSceneScenter();
	void SetPaused(bool paused) {m_playbackPaused = paused;};
	bool GetPaused(void) {return m_playbackPaused;};

private:
	// static functions
	static void TW_CALL SetTimeCB(const void *value, void *clientData);
	static void TW_CALL GetTimeCB(void *value, void *clientData);
	static void TW_CALL SetRaycasterEnabledCB(const void *value, void *clientData);
	static void TW_CALL GetRaycasterEnabledCB(void *value, void *clientData);
	static void TW_CALL SetGlyphVisualizerCB(const void *value, void *clientData);
	static void TW_CALL GetGlyphVisualizerCB(void *value, void *clientData);
	static void TW_CALL CreateParticleTracerCB(void *clientData);
	static void TW_CALL CreateSliceVisualizerCB(void *clientData);

	// static variables
	static ID3D11Device * pd3dDevice;
	static TwBar * pParametersBar;

	//methods
	void loadPly(std::string scenePlyFile);	
	void SetupTwBar(TwBar * pParametersBar);

	// members

	// [scene objects]
	SimpleMesh	* m_mesh;
	ScalarVolumeData * m_scalarVolumeData;
	VectorVolumeData * m_vectorVolumeData;
	RayCaster	* m_rayCaster;
	GlyphVisualizer * m_glyphVisualizer;

	// [playback]
	TwBar * m_playbackBar;
	float	m_playbackTime;
	bool	m_playbackRepeat;
	float	m_playbackSpeed;
	bool	m_playbackPaused;
	float	m_timestep;

	// [transformation]
	XMMATRIX m_globalTransform;

	// [filenames]
	std::string m_meshFileName;
	std::string m_objectFileName;
	std::string m_sceneDatFile;

	// [volume data properties]
	VolumeData::DataFormat m_objectFileFormat;
	int m_objectIndices[3];
	float m_sliceThickness[3];
	float m_boundingBoxSize[3];
	int m_resolution[3];
};

