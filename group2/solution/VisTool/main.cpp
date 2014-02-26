// Internal includes
#include "util/util.h"
#include "util/Stereo.h"
#include "Globals.h"
#include "util/GUI2DHelper.h"
#include "TransparencyModule.h"

#include "Scene.h"
#include "SimpleMesh.h"
#include "RayCaster.h"
#include "SettingsStorage.h"
#include "GlyphVisualizer.h"
#include "ParticleTracer.h"
#include "SliceVisualizer.h"
#include "BoxManipulationManager.h"

#include "TransferFunctionEditor\TransferFunctionEditor.h"


#include <windowsx.h>
// Effect framework includes
#include <d3dx11effect.h>

// DXUT includes
#include <DXUT.h>
#include <DXUTcamera.h>
#include <ScreenGrab.h>

//DirectX includes
#include <DirectXMath.h>
using namespace DirectX;

#include <AntTweakBar.h>

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "util/dirent.h"

// DXUT camera
// NOTE: CModelViewerCamera does not only manage the standard view transformation/camera position 
//       (CModelViewerCamera::GetViewMatrix()), but also allows for model rotation
//       (CModelViewerCamera::GetWorldMatrix()). 
//       Look out for CModelViewerCamera::SetButtonMasks(...).
// assignment 1.2
CModelViewerCamera g_modelViewerCamera;
CFirstPersonCamera g_firstPersonCamera;
bool g_firstPersonCamActive = false;

Scene * g_currentScene = nullptr;
BoxManipulationManager * g_boxManipulationManager = nullptr;

//Tweakbar for global parameters
TwBar * g_pParametersBar = nullptr;
TwBar * g_pLightingShadingBar = nullptr;

TransferFunctionEditor * g_transferFunctionEditor = nullptr;

ID3D11Texture2D				* g_pDepthBufferCopy = nullptr;
ID3D11ShaderResourceView	* g_pDepthBufferCopySRV = nullptr;
ID3D11Texture2D				* g_pSecondaryRenderTarget = nullptr;
ID3D11ShaderResourceView	* g_pSecondaryRenderTargetSRV = nullptr;
ID3D11RenderTargetView		* g_pSecondaryRenderTargetRTV = nullptr;

// Initialize globals with default values
Globals g_globals = {
	/*bool		showBoundingBoxes;	*/ true,
	/*bool		showTransferFunctionEditor;*/ false,
	/*bool		showTweakbars;*/	true,
	/*XMFLOAT4	lightColor;			*/ XMFLOAT4(1, 1, 1, 1),
	/*XMFLOAT4	backgroundColor;	*/ XMFLOAT4(0, 0, 0, 1),
	/*float		mat_ambient;		*/ 0.1f,
	/*float		mat_diffuse;		*/ 0.6f,
	/*float		mat_specular;		*/ 0.4f,
	/*float		mat_specular_exp;	*/ 100,
	/*std::string configPath;		*/ "",
	/*std::string configFile;		*/ "",
	/*VolumeData * volumeData;		*/ nullptr,
	/*UINT		viewportSize[2];	*/ 0, 0
};

Stereo g_stereoHelper;

// forward declarations
void TW_CALL OnLoadConfigButtonClick(void* clientData);

// loads a config file and sets up the scene
void LoadConfig(std::string filename)
{
		SettingsStorage store;
		g_globals.configPath = GetPath(filename);
		g_globals.configFile = filename;
		if(!store.Load(std::string(filename))) {
			OnLoadConfigButtonClick(nullptr);
		}
		else {
		
			g_globals.LoadConfig(store);

			if(g_currentScene) delete g_currentScene;
			g_currentScene = new Scene(store);

			store.GetBool("camera.firstPersonActive", g_firstPersonCamActive);
			XMVECTOR eye, lookat;
			store.GetFloat4("camera.FirstPerson.eye", eye.m128_f32);
			store.GetFloat4("camera.FirstPerson.lookat", lookat.m128_f32);
			g_firstPersonCamera.SetViewParams(eye, lookat);
				XMMATRIX world;
			store.GetFloat4("camera.ModelView.eye", eye.m128_f32);
			store.GetFloat4("camera.ModelView.lookat", lookat.m128_f32);
			store.GetFloat4x4("camera.ModelView.worldMatrix", world.r[0].m128_f32);
			g_modelViewerCamera.SetViewParams(eye, lookat);
			g_modelViewerCamera.SetWorldMatrix(world);
		}
}

void SaveConfig(std::string filename)
{
		g_globals.configPath = GetPath(filename);
		g_globals.configFile = filename;

		SettingsStorage store;
		store.StoreBool("camera.firstPersonActive", g_firstPersonCamActive);
		store.StoreFloat4("camera.FirstPerson.eye", g_firstPersonCamera.GetEyePt().m128_f32);
		store.StoreFloat4("camera.FirstPerson.lookat", g_firstPersonCamera.GetLookAtPt().m128_f32);
		store.StoreFloat4("camera.ModelView.eye", g_modelViewerCamera.GetEyePt().m128_f32);
		store.StoreFloat4("camera.ModelView.lookat", g_modelViewerCamera.GetLookAtPt().m128_f32);
		XMMATRIX camWorld = g_modelViewerCamera.GetViewMatrix();
		store.StoreFloat4x4("camera.ModelView.worldMatrix", camWorld.r[0].m128_f32);

		g_globals.SaveConfig(store);
		if(g_currentScene)
			g_currentScene->SaveConfig(store);
		store.Save(std::string(filename));
}

int FindFiles(std::string  strDir, std::vector<std::string> &files, std::string strPattern=".cfg", bool recurse=true) {
	struct dirent *ent;
	DIR *dir;
	dir = opendir (strDir.c_str());
	int retVal = 0;
	if (dir != NULL) {
		while ((ent = readdir (dir)) != NULL) {
			if (strcmp(ent->d_name, ".") !=0 &&  strcmp(ent->d_name, "..") !=0){
				std::string strFullName = strDir +"\\"+std::string(ent->d_name);
				std::string strType = "N/A";
				
				bool isDir = (ent->d_type == DT_DIR);
				strType = (isDir)?"DIR":"FILE";                 
				if ((!isDir)){
					//printf ("%s <%s>\n", strFullName.c_str(),strType.c_str());//ent->d_name);
					if(strFullName.substr(strFullName.length() - 4, std::string::npos) == strPattern)
						files.push_back(strFullName);
					retVal++;
				}   
				if (isDir && recurse){
					retVal += FindFiles(strFullName, files, strPattern, recurse);
				}
			}
		}
		closedir (dir);
		return retVal;
	} else {
		/* could not open directory */
		perror ("DIR NOT FOUND!");
		return -1;
	}
}

std::vector<std::string> cfgFiles;
TwBar * g_pConfigFileBrowser;

void TW_CALL OnSelectConfigFileCB(void * clientData)
{
	std::string * cfgFile = (std::string*)clientData;

	SetCurrentDirectory(g_globals.startupPath.c_str());

	LoadConfig(*cfgFile);
}

void CreateConfigBrowser()
{
	g_pConfigFileBrowser = TwNewBar("Presets");
	TwDefine("'Presets' size='300 300' position='15 625'");

	SetCurrentDirectory(g_globals.startupPath.c_str());

	int n = FindFiles("../../", cfgFiles, ".cfg");
	std::cout << "Found " << n << " files:" << std::endl;
	for(int i=0; i < cfgFiles.size(); i++) {
		std::string name = GetFilename(cfgFiles[i]);
		std::string path = GetPath(cfgFiles[i]);
		std::string lastFolder = GetFilename(path.substr(0, path.length() - 1));
		TwAddButton(g_pConfigFileBrowser, (lastFolder + "/" + name).c_str(), OnSelectConfigFileCB, &cfgFiles[i], "");
	}
}



// ============================================================
// DXUT Callbacks
// ============================================================


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
	return true;
}


//--------------------------------------------------------------------------------------
// Called right before creating a device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
	pDeviceSettings->d3d11.AutoDepthStencilFormat = DXGI_FORMAT_D32_FLOAT;
	return true;
}

void TW_CALL OnOpenFileButtonClick(void* clientData)
{
	WCHAR szFile[256];
	std::cout << "Opened File..." << std::endl;
	std::string currentFile = g_currentScene?g_currentScene->GetSceneDatFile():"";
	size_t length;
	mbstowcs_s(&length, szFile, currentFile.c_str(), sizeof(szFile));
	
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = szFile;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
	// use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = L"Data Description File (.dat)\0*.dat\0All\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = L"Open Data File";
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	BOOL successful = GetOpenFileName(&ofn);

	if(successful) {
		std::cout << "Selected file " << szFile << std::endl;
		char filename[256];
		wcstombs_s(&length, filename, szFile, sizeof(filename));
		if(g_currentScene) delete g_currentScene;
		g_currentScene = new Scene(std::string(filename));
	}
}

void TW_CALL OnSaveConfigButtonClick(void* clientData)
{
	WCHAR szFile[256];
	std::cout << "Opened File..." << std::endl;
	std::string currentFile = g_currentScene?g_currentScene->GetSceneDatFile():"";
	size_t length;
	mbstowcs_s(&length, szFile, currentFile.c_str(), sizeof(szFile));

	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = szFile;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
	// use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = L"Config File (.cfg)\0*.CFG\0All\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = L"Select Config Save File";
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	BOOL successful = GetSaveFileName(&ofn);

	if(successful) {
		std::cout << "Selected file " << szFile << std::endl;
		char filename[256];
		wcstombs_s(&length, filename, szFile, sizeof(filename));

		SaveConfig(filename);

		//also, save a screenshot
		std::string ssName = GetPath(filename) + GetFilename(filename, true) + ".png";
		
		ID3D11Resource* pTex2D = nullptr;
		DXUTGetD3D11RenderTargetView()->GetResource(&pTex2D);
		SaveWICTextureToFile(DXUTGetD3D11DeviceContext(), pTex2D, GUID_ContainerFormatPng, ToWString(ssName).c_str());
		SAFE_RELEASE(pTex2D);
	}
}

void TW_CALL OnLoadConfigButtonClick(void* clientData)
{
	WCHAR szFile[256] = L"";
	size_t length = 0;
	if(g_currentScene) {
		std::string currentFile = g_currentScene->GetSceneDatFile();

		mbstowcs_s(&length, szFile, currentFile.c_str(), sizeof(szFile));
	}

	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = szFile;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
	// use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = L"Config File (.cfg)\0*.CFG\0All\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = L"Select Config Save File";
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	BOOL successful = GetOpenFileName(&ofn);

	if(successful) {

		char filename[256];
		wcstombs_s(&length, filename, szFile, sizeof(filename));

		LoadConfig(filename);
	}
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependent on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
	std::wcout << L"Device: " << DXUTGetDeviceStats() << std::endl;

	HRESULT hr;

	TwInit(TW_DIRECT3D11, pd3dDevice);

	CreateConfigBrowser();

	g_pParametersBar = TwNewBar("General Parameters");	
	TwDefine("'General Parameters' size='300 180' position='15 15'");
	TwAddButton(g_pParametersBar, "Save Config File", OnSaveConfigButtonClick, nullptr, "");
	TwAddButton(g_pParametersBar, "Load Config File", OnLoadConfigButtonClick, nullptr, "");
	TwAddButton(g_pParametersBar, "Open Data File", OnOpenFileButtonClick, nullptr, "");
	//TwAddVarRW(g_pParametersBar, "Use First Person Camera", TW_TYPE_BOOLCPP, &g_firstPersonCamActive, "");
	TwAddVarRW(g_pParametersBar, "Bounding Box", TW_TYPE_BOOLCPP, &g_globals.showBoundingBoxes, "key=b");
	TwAddVarRW(g_pParametersBar, "Show TF Editor", TW_TYPE_BOOLCPP, &g_globals.showTransferFunctionEditor, "key=t");
	
	g_transferFunctionEditor = new TransferFunctionEditor(500, 250);

	V_RETURN(TransparencyModule::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(GUI2DHelper::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(BoxManipulationManager::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(Scene::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(VolumeData::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(ScalarVolumeData::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(VectorVolumeData::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(SimpleMesh::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(RayCaster::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(GlyphVisualizer::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(ParticleTracer::Initialize(pd3dDevice, g_pParametersBar));
	V_RETURN(SliceVisualizer::Initialize(pd3dDevice, g_pParametersBar));

	g_boxManipulationManager = new BoxManipulationManager();

	g_transferFunctionEditor->onCreateDevice(pd3dDevice);

	if(!g_globals.configFile.empty()) {
		LoadConfig(g_globals.configFile);
	}
	//else {
	//	OnLoadConfigButtonClick(nullptr);		
	//}


	// initialize Scene
	//g_currentScene = new Scene("../../../data/vector/Drain [128 64 64] [16]/DrainZ.dat");
	//g_currentScene = new Scene("../../../data/vector/VortexStreet [61 21 21] [201]/vortexStreet.dat");
	//g_currentScene = new Scene("../../../bigdata/vector/StuttgartCylinder [256 128 128] [23]/L2LES_40000.dat");
	//g_currentScene = new Scene("../../../data/vector/SquareCylinder [192 64 48]/SquareCylinder_t1368.dat");
	//g_currentScene = new Scene("../../../data/scalar/C60 Molecule [128 128 128]/C60Large_zTest.dat");
	//g_currentScene = new Scene("../../../data/scalar/Richtmyer-Meshkov [128 128 128] [16]/ppmlow.dat");
	//g_currentScene = new Scene("../../../data/scalar/VisHuman Head [256 256 256] (CT)/vmhead256cubed.dat");
	//g_currentScene = new Scene("../../../data/mesh/apple.dat");
	//g_currentScene = new Scene("../../../data/scalar/VisHuman Foot [125 255 183] (CT)/Foot.dat");
	
	g_pLightingShadingBar = TwNewBar("Lighting / Shading");
	TwDefine("'Lighting / Shading' color='200 200 10' size='300 130' position='15 200'");

	TwAddVarRW(g_pLightingShadingBar, "Light Color", TW_TYPE_COLOR4F, &g_globals.lightColor, "");
	TwAddVarRW(g_pLightingShadingBar, "Background Color", TW_TYPE_COLOR4F, &g_globals.backgroundColor, "");
	TwAddVarRW(g_pLightingShadingBar, "Ambient Coefficient", TW_TYPE_FLOAT, &g_globals.mat_ambient, "min=0 max=1 step=0.05");
	TwAddVarRW(g_pLightingShadingBar, "Diffuse Coefficient", TW_TYPE_FLOAT, &g_globals.mat_diffuse, "min=0 max=1 step=0.05");
	TwAddVarRW(g_pLightingShadingBar, "Specular Coefficient", TW_TYPE_FLOAT, &g_globals.mat_specular, "min=0 max=1 step=0.05");
	TwAddVarRW(g_pLightingShadingBar, "Specular Exponent", TW_TYPE_FLOAT, &g_globals.mat_specular_exp, "min=1 max=1000 step=10");


	//OnLoadConfigButtonClick(nullptr);

	g_stereoHelper.CreateStereoTweakBar();

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
	HRESULT hr;

	// Update camera parameters
	int width = pBackBufferSurfaceDesc->Width;
	int height = pBackBufferSurfaceDesc->Height;

	g_globals.viewportSize[0] = width;
	g_globals.viewportSize[1] = height;
	
	// assignment 3.1
	// resize AntTweakBar
	TwWindowSize(width, height);

	// assignment 1.2
	std::cout << "Resized swap chain\n";
	g_modelViewerCamera.SetWindow(width, height);
	g_modelViewerCamera.SetProjParams(XM_PI / 4.0f, float(width) / float(height), 0.1f, 100.0f);
	g_firstPersonCamera.SetProjParams(XM_PI / 4.0f, float(width) / float(height), 0.1f, 100.0f);
	
	g_transferFunctionEditor->onResizeSwapChain(width, height);

	TransparencyModule::OnResizeSwapChain(pd3dDevice, width, height);
	GUI2DHelper::OnResizeSwapChain(pd3dDevice, width, height);

	// assignment 5.2 - create texture to receive depth buffer copy
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = width;
	desc.Height = height;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateTexture2D(&desc, nullptr, &g_pDepthBufferCopy));
	
	D3D11_SHADER_RESOURCE_VIEW_DESC pDesc;
	ZeroMemory(&pDesc, sizeof(pDesc));
	pDesc.Format = DXGI_FORMAT_R32_FLOAT;
	pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	pDesc.Texture2D.MipLevels = -1;
	pDesc.Texture2D.MostDetailedMip = 0;
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_pDepthBufferCopy, &pDesc, &g_pDepthBufferCopySRV));
	
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = width;
	desc.Height = height;
	desc.ArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateTexture2D(&desc, nullptr, &g_pSecondaryRenderTarget));
	
	ZeroMemory(&pDesc, sizeof(pDesc));
	pDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	pDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	pDesc.Texture2D.MipLevels = -1;
	pDesc.Texture2D.MostDetailedMip = 0;

	V_RETURN(pd3dDevice->CreateShaderResourceView(g_pSecondaryRenderTarget, &pDesc, &g_pSecondaryRenderTargetSRV));

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	ZeroMemory(&rtvDesc, sizeof(rtvDesc));
	rtvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	V_RETURN(pd3dDevice->CreateRenderTargetView(g_pSecondaryRenderTarget, &rtvDesc, &g_pSecondaryRenderTargetRTV));
	return S_OK;
}


//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
	// clear render target and depth stencil
	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
	pd3dImmediateContext->ClearRenderTargetView( pRTV, &g_globals.backgroundColor.x );
	pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0f, 0 );

	for (Stereo::View v : g_stereoHelper.GetRequiredViews())
    {
        g_stereoHelper.SetCurrentView(v);

		pd3dImmediateContext->RSSetViewports(1, g_stereoHelper.GetCurrentViewport());
		
		auto viewport = g_stereoHelper.GetCurrentViewport();
		
		RenderTransformations camMtcs(XMMatrixIdentity(), XMMatrixIdentity(), g_stereoHelper.GetViewMatrix(v), g_stereoHelper.GetCurrentProjMatrix(), 
			XMFLOAT2(viewport->Width, viewport->Height), g_globals.currentlyActiveCamera->GetEyePt());

		if(g_currentScene)
			g_currentScene->Render(pd3dImmediateContext, camMtcs);

		g_boxManipulationManager->Render(pd3dImmediateContext, camMtcs);

		if(g_currentScene)
			g_currentScene->RenderTransparency(pd3dImmediateContext, camMtcs);
	}

	// get mono viewport and render transparency
	pd3dImmediateContext->RSSetViewports(1, g_stereoHelper.GetViewport(Stereo::Default));
	TransparencyModule::DrawAccumulatedTransparency(pd3dImmediateContext);

	if(g_globals.showTweakbars) {
		GUI2DHelper::Begin2DSection(pd3dImmediateContext);
		TwDraw();

		g_transferFunctionEditor->onFrameRender(fTime, fElapsedTime);
		GUI2DHelper::End2DSection(pd3dImmediateContext);
	}
	GUI2DHelper::SetSideBySide3DEnabled(g_stereoHelper.GetEnabled());
	GUI2DHelper::Draw2DGUI(pd3dImmediateContext);
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
	std::cout << "Releasing SwapChain" << std::endl;
	g_transferFunctionEditor->onReleasingSwapChain();

	SAFE_RELEASE(g_pDepthBufferCopySRV);
	SAFE_RELEASE(g_pDepthBufferCopy);
	SAFE_RELEASE(g_pSecondaryRenderTarget);
	SAFE_RELEASE(g_pSecondaryRenderTargetSRV);
	SAFE_RELEASE(g_pSecondaryRenderTargetRTV);

	TransparencyModule::OnReleasingSwapChain();
	GUI2DHelper::OnReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{


	if(g_currentScene) delete g_currentScene;
	g_currentScene = nullptr;

	if(g_globals.volumeData) {
		g_globals.volumeData->ReleaseGPUBuffers();
	}
	

	delete g_boxManipulationManager;
	g_boxManipulationManager = nullptr;

	BoxManipulationManager::Release();
	Scene::Release();
	SimpleMesh::Release();
	RayCaster::Release();
	VolumeData::Release();
	ScalarVolumeData::Release();
	VectorVolumeData::Release();
	GlyphVisualizer::Release();
	ParticleTracer::Release();
	SliceVisualizer::Release();
	TransparencyModule::Release();
	GUI2DHelper::Release();

	g_transferFunctionEditor->onDestroyDevice();
	delete g_transferFunctionEditor;
	g_transferFunctionEditor = nullptr;

	TwTerminate();
	
	std::cout << "Device Destroyed" << std::endl;
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
	std::cout << "Device Removed" << std::endl;
	return true;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
	if(bKeyDown)
	{
		switch(nChar)
		{
		case VK_SPACE:
			if(g_currentScene)
				g_currentScene->SetPaused(!g_currentScene->GetPaused());
		break;
		case 'A':
			g_globals.showTweakbars = !g_globals.showTweakbars;
			break;
		// ALT+RETURN: toggle fullscreen
		case VK_F5 :
		{
			SetCurrentDirectory(g_globals.startupPath.c_str());
			if(g_currentScene)
				SaveConfig("fullscreenConfig.cfg");
			DXUTToggleFullScreen();
			break;
		}
		// F8: Take screenshot
		case VK_F8:
		{
			// Save current render target as png
			static int nr = 0;
			std::wstringstream ss;
			ss << L"Screenshot" << std::setfill(L'0') << std::setw(4) << nr++ << L".png";

			ID3D11Resource* pTex2D = nullptr;
			DXUTGetD3D11RenderTargetView()->GetResource(&pTex2D);
			SaveWICTextureToFile(DXUTGetD3D11DeviceContext(), pTex2D, GUID_ContainerFormatPng, ss.str().c_str());
			SAFE_RELEASE(pTex2D);

			std::wcout << L"Screenshot written to " << ss.str() << std::endl;
			break;
		}
		case VK_F7: 
		{
			g_firstPersonCamActive = !g_firstPersonCamActive;
			std::wcout << L"Toggled Camera" << std::endl;
			break;
		}
		case 'J':
			TransparencyModule::transparencyRenderType = (TransparencyModule::transparencyRenderType+1) % 3;
		break;
		default : return;
		}
	}
}


//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
                       bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta,
                       int xPos, int yPos, void* pUserContext )
{
	float x, y;
	/*if(g_stereoHelper.GetEnabled()) {
		x = 2*xPos/((float)g_globals.viewportSize[0]);
		if(x > 1.f)
			x -= 1.f;
		y = 1.f - yPos/((float)g_globals.viewportSize[1]);
	}
	else */
	{
		x = xPos/((float)g_globals.viewportSize[0]);
		y = 1.f - yPos/((float)g_globals.viewportSize[1]);
	}

	GUI2DHelper::SetMousePosition(XMFLOAT2(float(xPos), float(yPos)));

	if(g_currentScene)
		g_currentScene->OnMouse(bLeftButtonDown, bRightButtonDown, bMiddleButtonDown, nMouseWheelDelta, x, y);

	//TODO: move this to msgProc to improve usability
	if(g_boxManipulationManager)
		g_boxManipulationManager->OnMouse(bLeftButtonDown, bRightButtonDown, bMiddleButtonDown, nMouseWheelDelta, x, y);

}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
	if(g_stereoHelper.GetEnabled() && (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP || uMsg == WM_MOUSEMOVE)) {
		unsigned int xPos = 2*GET_X_LPARAM(lParam); 
		if(xPos > g_globals.viewportSize[0])
			xPos -= g_globals.viewportSize[0];

		SHORT x = xPos;
		//set the lower bits of lparam
		//lParam = lParam & ((LPARAM)-1 ^ 0xFFFF) | x & 0xFFFF;
	}

	if(g_globals.showTweakbars) {
		if( TwEventWin(hWnd, uMsg, wParam, lParam) ) // send event message to AntTweakBar
			return 0; // event has been handled by AntTweakBar

		if(g_transferFunctionEditor && g_transferFunctionEditor->msgProc(hWnd, uMsg, wParam, lParam))
			return 0;
	}
	// Send message to camera
	if(g_firstPersonCamActive) {
		if(g_firstPersonCamera.HandleMessages(hWnd,uMsg,wParam,lParam))
		{
			*pbNoFurtherProcessing = true;
			return 0;
		}
	}
	else {
		if(g_modelViewerCamera.HandleMessages(hWnd,uMsg,wParam,lParam))
		{
			*pbNoFurtherProcessing = true;
			return 0;
		}
	}

	return 0;
}



//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double dTime, float fElapsedTime, void* pUserContext )
{
	UpdateWindowTitle(L"VisTool");
	
	// Move Camera
	if(g_firstPersonCamActive)
		g_firstPersonCamera.FrameMove(fElapsedTime);
	else
		g_modelViewerCamera.FrameMove(fElapsedTime);


	g_stereoHelper.UpdateViews(g_modelViewerCamera.GetViewMatrix(), g_modelViewerCamera.GetProjMatrix(),
		DXUTGetDXGIBackBufferSurfaceDesc()->Width, DXUTGetDXGIBackBufferSurfaceDesc()->Height);

	if(g_currentScene)
		g_currentScene->FrameMove(dTime, fElapsedTime);
	g_boxManipulationManager->FrameMove(dTime, fElapsedTime);

	//if(g_transferFunctionEditor->HasObservers() != g_transferFunctionEditor->isVisible())
	//	g_transferFunctionEditor->setVisible(g_transferFunctionEditor->HasObservers());
	g_transferFunctionEditor->setVisible(g_globals.showTransferFunctionEditor);
}

//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	WCHAR cwd[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, cwd);
	g_globals.startupPath = std::wstring(cwd);

#if defined(DEBUG) | defined(_DEBUG)
	// Enable run-time memory check for debug builds.
	// (on program exit, memory leaks are printed to Visual Studio's Output console)
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

#ifdef _DEBUG
	std::wcout << L"---- DEBUG BUILD ----\n\n";
#endif

	ShowCursor(false);

	// Set general DXUT callbacks
	DXUTSetCallbackMsgProc( MsgProc );
	DXUTSetCallbackMouse( OnMouse, true );
	DXUTSetCallbackKeyboard( OnKeyboard );

	DXUTSetCallbackFrameMove( OnFrameMove );

	DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
	DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

	// Set the D3D11 DXUT callbacks
	DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
	DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
	DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
	DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
	DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
	DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

	// Init camera
	XMFLOAT3 eye(0.0f, 0.0f, -2.0f);
	XMFLOAT3 lookAt(0.0f, 0.0f, 0.0f);
	// assignment 1.2
	g_modelViewerCamera.SetViewParams(XMLoadFloat3(&eye), XMLoadFloat3(&lookAt));
	g_modelViewerCamera.SetButtonMasks(0, MOUSE_WHEEL, MOUSE_RIGHT_BUTTON);
	g_modelViewerCamera.FrameMove(0);
	g_globals.currentlyActiveCamera = &g_modelViewerCamera;
	g_firstPersonCamera.SetViewParams(XMLoadFloat3(&eye), XMLoadFloat3(&lookAt));
	g_firstPersonCamera.SetScalers(0.005f, 8.f);
	g_firstPersonCamera.SetEnablePositionMovement(true);
	g_firstPersonCamera.FrameMove(0);	

	g_firstPersonCamera.SetRotateButtons(true, false, false, true);

	// Init DXUT and create device
	DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
	//DXUTSetIsInGammaCorrectMode( false ); // true by default (SRGB backbuffer), disable to force a RGB backbuffer
	DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
	DXUTCreateWindow( L"VisTool" );
	DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1280, 960 );

	DXUTMainLoop(); // Enter into the DXUT render loop

	DXUTShutdown(); // Shuts down DXUT (includes calls to OnD3D11ReleasingSwapChain() and OnD3D11DestroyDevice())

	if(g_globals.volumeData)
		delete g_globals.volumeData;

	return DXUTGetExitCode();
}

