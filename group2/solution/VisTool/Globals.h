#pragma once

#include <DirectXMath.h>
using namespace DirectX;

//forward declarations
class CBaseCamera;
class VolumeData;

#include "SettingsStorage.h"



typedef struct {
	// show the bounding boxes for objects?
	bool		showBoundingBoxes;	

	bool		showTransferFunctionEditor;

	bool		showTweakbars;

	// color of the light source
	XMFLOAT4	lightColor;		

	// background color
	XMFLOAT4	backgroundColor;

	// coefficients k_a, k_d and k_s for blinn-phong shading
	float		mat_ambient;	
	float		mat_diffuse;	
	float		mat_specular;	
	float		mat_specular_exp;

	std::string configPath;
	std::string configFile;

	VolumeData * volumeData;

	//size of the current viewport
	unsigned int		viewportSize[2];

	CBaseCamera		* currentlyActiveCamera;

	std::wstring startupPath;


	void SaveConfig(SettingsStorage &store) {
		store.StoreBool("globals.showBoundingBoxes", showBoundingBoxes);
		store.StoreBool("globals.showTransferFunctionEditor", showTransferFunctionEditor);
		store.StoreFloat4("globals.lightColor", &lightColor.x);
		store.StoreFloat4("globals.backgroundColor", &backgroundColor.x);
		store.StoreFloat("globals.material.ambient", mat_ambient);
		store.StoreFloat("globals.material.diffuse", mat_diffuse);
		store.StoreFloat("globals.material.specular", mat_specular);
		store.StoreFloat("globals.material.specular_exp", mat_specular_exp);
	}

	void LoadConfig(SettingsStorage &store) {
		store.GetBool("globals.showBoundingBoxes", showBoundingBoxes);
		store.GetBool("globals.showTransferFunctionEditor", showTransferFunctionEditor);
		store.GetFloat4("globals.lightColor", &lightColor.x);
		store.GetFloat4("globals.backgroundColor", &backgroundColor.x);
		store.GetFloat("globals.material.ambient", mat_ambient);
		store.GetFloat("globals.material.diffuse", mat_diffuse);
		store.GetFloat("globals.material.specular", mat_specular);
		store.GetFloat("globals.material.specular_exp", mat_specular_exp);
	}
} Globals;

extern Globals g_globals;