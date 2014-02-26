#pragma once

#include "util/util.h"

#include "SettingsStorage.h"
#include "VectorVolumeData.h"

#include <DirectXMath.h>
#include <d3dx11effect.h>
#include <DXUT.h>
#include <DXUTcamera.h>
using namespace DirectX;

#include "AntTweakBar.h"

#include <list>

class BoxManipulationManager
{
public:
	//types
	enum PassType {
		PASS_RENDER,
		NUM_PASSES
	};
	typedef struct ManipulationBox {
		XMFLOAT3 center;
		XMFLOAT3 size;
		XMMATRIX texToNDCSpace;
		int longestAxis;
		int largestFaceAxis;
		bool visible;
		bool scalable;
		bool moveable;
		bool changed;
		bool axisSurfaceChanged;

		ManipulationBox(XMFLOAT3 center_, XMFLOAT3 size_, bool visible_, bool scalable_, bool moveable_) :
			center(center_),			size(size_),			visible(visible_),
			scalable(scalable_),			moveable(moveable_),			changed(false), axisSurfaceChanged(false),
			texToNDCSpace(XMMatrixIdentity())		
		{
				SetChanged();
				axisSurfaceChanged = false;
		};
		void SetChanged() {
			int la = 0;
			float laLength = size.x;
			int lfa = 0;
			float lfArea = size.y * size.z;

			if(laLength < size.y) {			la = 1;			laLength = size.y;		}
			if(laLength < size.z) {			la = 2;			laLength = size.z;		}
			if(lfArea < size.x * size.z) {			lfa = 1;			laLength = size.x*size.z;		}
			if(lfArea < size.x * size.y) {		lfa = 2;			laLength = size.x*size.y;		}
			
			if(longestAxis != la || largestFaceAxis != lfa)
				axisSurfaceChanged = true;

			longestAxis = la;
			largestFaceAxis = lfa;

			changed = true;
		};
	} ManipulationBox;

	//statics
	static HRESULT Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar);
	static HRESULT Release(void);

	//constructors & destructor
	BoxManipulationManager();
	virtual ~BoxManipulationManager(void);

	//methods
	virtual HRESULT Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations camMtcs);
	void OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
					int nMouseWheelDelta, float xPos, float yPos);
	void FrameMove(double dTime, float fElapsedTime);

	void AddBox(ManipulationBox * box);
	void RemoveBox(ManipulationBox * box);

protected:
	//statics
	static ID3DX11Effect * pEffect;
	static ID3DX11EffectTechnique * pTechnique;
	static char * passNames[NUM_PASSES];
	static ID3DX11EffectPass * pPasses[NUM_PASSES];
	static ID3D11Device * pd3dDevice;

	static ID3DX11EffectMatrixVariable	* pWorldViewProjEV;
	static ID3DX11EffectScalarVariable	* pTimestepTEV;
	static ID3DX11EffectVectorVariable	* pCamPosEV;

	float intersectRayQuad(XMVECTOR p, XMVECTOR u, XMVECTOR v, XMVECTOR o, XMVECTOR d);
	std::pair<int, float>  checkMouseBoxIntersection(ManipulationBox *box, float xPos, float yPos);

	std::list<ManipulationBox * > m_boxes;
	ManipulationBox * m_selectedBox;

	bool m_mouseLeftBeenDown;
	bool m_mouseMiddleBeenDown;
	float m_xPosAtMouseDown,m_yPosAtMouseDown;
	XMFLOAT3 m_centerAtMouseDown;
	XMFLOAT3 m_sizeAtMouseDown;
	XMFLOAT3 m_movementDirectionNDC, m_movementDirectionTEX;
	bool m_movementDirInvert;
};

