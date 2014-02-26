#ifndef __STEREO_H__
#define __STEREO_H__

#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>
using namespace DirectX;

struct CTwBar;
typedef struct CTwBar TwBar;

// Simple helper class for side-by-side stereoscopic rendering.
// With AntTweakBar support.
class Stereo
{
public:
    enum View
    {
        Default = 0,
        Left    = 1,
        Right   = 2,
    };

    Stereo(bool enabled = false);

    void CreateStereoTweakBar(bool iconified = true);
    
    void UpdateViews(const XMMATRIX& view, const XMMATRIX& proj, 
                      unsigned int backbufferWidth, unsigned int backbufferHeight);

    std::vector<View> GetRequiredViews() { 
        std::vector<View> res;
        if (m_bStereoEnabled) { res.push_back(Left); res.push_back(Right); }
        else                  { res.push_back(Default);                    }
        return res;
    };

    void SetCurrentView(View v) { m_currentView = v; }
    View GetCurrentView() { return m_currentView; }
   
    const D3D11_VIEWPORT* GetCurrentViewport  () { return &m_viewports  [m_currentView]; }
    const XMMATRIX&       GetCurrentViewMatrix() { return m_viewMatrices[m_currentView]; }
    const XMMATRIX&       GetCurrentProjMatrix() { return m_projMatrices[m_currentView]; }
    
    const D3D11_VIEWPORT* GetViewport  (View v) { return &m_viewports  [v]; }
    const XMMATRIX&       GetViewMatrix(View v) { return m_viewMatrices[v]; }
    const XMMATRIX&       GetProjMatrix(View v) { return m_projMatrices[v]; }

    bool  GetEnabled    () { return m_bStereoEnabled     ; }
    float GetEyeDist    () { return m_params.fEyeDist    ; }
    float GetFocalPlane () { return m_params.fFocalPlane ; }
    float GetNearPlane  () { return m_params.fNearPlane  ; }
    float GetScreenWidth() { return m_params.fScreenWidth; }

    void SetEnabled    (bool enabled) { m_bStereoEnabled      = enabled; }
    void SetEyeDist    (float  value) { m_params.fEyeDist     = value; }
    void SetFocalPlane (float  value) { m_params.fFocalPlane  = value; }
    void SetNearPlane  (float  value) { m_params.fNearPlane   = value; }
    void SetScreenWidth(float  value) { m_params.fScreenWidth = value; }

private:
    void LoadParams(); // load m_params from file
    void SaveParams(); // save m_params to file

    bool  m_bStereoEnabled;

    TwBar* m_pTwBar;

    View                        m_currentView;
    std::vector<D3D11_VIEWPORT> m_viewports;
    std::vector<XMMATRIX>       m_viewMatrices;
    std::vector<XMMATRIX>       m_projMatrices;

    // All parameters have to be given in view-space units.
    // For easy configuration specify everything in meters.
    struct Parameters
    {
        float fEyeDist;     // average human pupillary distance is about 65mm
        float fFocalPlane;  // distance to screen
        float fNearPlane;   // far plane is taken as fNearPlane*1000
        float fScreenWidth; // width of screen
    } m_params;

    const static char* ms_cParamsfile;
};

#endif
