#include "Stereo.h"

#include <fstream>
#include "AntTweakBar.h"

const char* Stereo::ms_cParamsfile = "stereoparams.txt";

Stereo::Stereo(bool enabled)
    : m_bStereoEnabled(enabled),
      m_pTwBar(nullptr),
      m_currentView(Default)
{
    m_params.fEyeDist     = 0.065f; 
    m_params.fFocalPlane  = 2.0f;   
    m_params.fNearPlane   = 0.1f;   
    m_params.fScreenWidth = 1.45f;  

    // If file exists, load params from file, 
    LoadParams();
}

void Stereo::CreateStereoTweakBar(bool iconified)
{
    m_pTwBar = TwNewBar("Stereo");

    if (iconified) TwDefine(" Stereo iconified=true "); // start iconified (= minimized)

    TwAddVarRW(m_pTwBar, "Enabled"     , TW_TYPE_BOOLCPP, &m_bStereoEnabled         , "key=s");
    TwAddVarRW(m_pTwBar, "Eye Dist"    , TW_TYPE_FLOAT  , &m_params.fEyeDist    , "min=0.001 max=0.5 step=0.001");
    TwAddVarRW(m_pTwBar, "Focal Plane" , TW_TYPE_FLOAT  , &m_params.fFocalPlane , "min=0.1 max=100 step=0.01");
    TwAddVarRW(m_pTwBar, "Near Plane"  , TW_TYPE_FLOAT  , &m_params.fNearPlane  , "min=0.1 max=100 step=0.01");
    TwAddVarRW(m_pTwBar, "Screen Width", TW_TYPE_FLOAT  , &m_params.fScreenWidth, "min=0.1 max=100 step=0.01");

    TwAddSeparator(m_pTwBar, "StereoSep", "");

    TwAddButton(m_pTwBar, "Load Params", [](void* p)->void{((Stereo*)p)->LoadParams();return;}, this, "");
    TwAddButton(m_pTwBar, "Save Params", [](void* p)->void{((Stereo*)p)->SaveParams();return;}, this, "");
}

void Stereo::UpdateViews(const XMMATRIX& view, const XMMATRIX& proj, 
                         unsigned int backbufferWidth, unsigned int backbufferHeight)
{
    float d = m_params.fEyeDist / 2.0f;

    // Compute view matrices
    m_viewMatrices.clear();
    {

        m_viewMatrices.push_back(view);
        m_viewMatrices.push_back(view * XMMatrixTranslation( d, 0, 0));
        m_viewMatrices.push_back(view * XMMatrixTranslation(-d, 0, 0));
    }

    // Compute projection matrices
    m_projMatrices.clear();
    {
        float aspect = (float)backbufferWidth / (float)backbufferHeight;
        float dx = m_params.fScreenWidth;
        float dy = m_params.fScreenWidth / aspect;
    
        // s rescales distances at focal plane to near plane
        float s = m_params.fNearPlane / m_params.fFocalPlane; 

        XMMATRIX mProjL = XMMatrixPerspectiveOffCenterLH( -s*dx/2 + s*d, s*dx/2 + s*d, 
                                                          -s*dy/2      , s*dy/2      ,        
                                                           m_params.fNearPlane, 
                                                           m_params.fNearPlane*1000.0f);

        XMMATRIX mProjR = XMMatrixPerspectiveOffCenterLH( -s*dx/2 - s*d, s*dx/2 - s*d, 
                                                          -s*dy/2      , s*dy/2      ,        
                                                           m_params.fNearPlane, 
                                                           m_params.fNearPlane*1000.0f);

        m_projMatrices.push_back(proj);
        m_projMatrices.push_back(mProjL);
        m_projMatrices.push_back(mProjR);
    }

    // Compute viewports
    m_viewports.clear();
    {
        D3D11_VIEWPORT full;
        full.TopLeftX = full.TopLeftY = 0.0f;
        full.MinDepth = 0.0f;
        full.MaxDepth = 1.0f;
        full.Width  = (float) backbufferWidth;
        full.Height = (float) backbufferHeight;

        m_viewports.push_back(full);

        D3D11_VIEWPORT half = full;
        half.Width  = (float) backbufferWidth / 2.0f;
        
        m_viewports.push_back(half);
        
        half.TopLeftX = (float) backbufferWidth / 2.0f;
        
        m_viewports.push_back(half);
    }
}


void Stereo::LoadParams()
{
    std::ifstream file(ms_cParamsfile, std::ios::in);

    if (file.fail()) return;

    std::string buf;
    file >> buf >> m_params.fEyeDist;
    file >> buf >> m_params.fFocalPlane;
    file >> buf >> m_params.fNearPlane;
    file >> buf >> m_params.fScreenWidth;

    file.close();
}

void Stereo::SaveParams()
{
    std::ofstream file(ms_cParamsfile, std::ios::out);

    if (file.fail()) return;

    file << "EyeDist     " << m_params.fEyeDist     << std::endl;
    file << "FocalPlane  " << m_params.fFocalPlane  << std::endl;
    file << "NearPlane   " << m_params.fNearPlane   << std::endl;
    file << "ScreenWidth " << m_params.fScreenWidth << std::endl;

    file.close();
}



