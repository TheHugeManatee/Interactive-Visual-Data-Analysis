#ifndef __TRANSFERFUNCTIONLINE_H__
#define __TRANSFERFUNCTIONLINE_H__


#include <d3d11_1.h>
#include <DirectXMath.h>
using namespace DirectX;

#include <d3dx11effect.h>

class TransferFunctionLine
{
public:
    struct Vec2i {
        int x, y;
        Vec2i(int x, int y) : x(x), y(y) {}
    };

	struct sControlPoint 
	{
		sControlPoint(Vec2i v2iPt, sControlPoint* pPrev, sControlPoint* pNext) : v2iPt_(v2iPt), pPrev_(pPrev), pNext_(pNext) {};

		Vec2i	        v2iPt_;
		sControlPoint*	pPrev_;
		sControlPoint*	pNext_;
	};


	TransferFunctionLine(XMFLOAT4 v4fLineColor, UINT uiMaxX, UINT uiMaxY);
	~TransferFunctionLine();

	HRESULT							onCreateDevice( ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect );

	void							reset();

	sControlPoint*					getFirstPoint() { return pFirstPoint_; }

	int								getControlPointId(Vec2i v2iPos);		//closest point to pos (-1 otherwise)
	Vec2i					        getControlPoint(int iCpId);

	bool							liesOnTheLine(Vec2i v2iPos);

	void							addControlPoint(Vec2i v2iPt);
	void							moveControlPoint(int iCpId, Vec2i v2iPt);
	void							deleteControlPoint(int iCpId);

	void							draw( void );

	void							fillArray( float* pData, int iSize, int iComponent );

	float							getMinValue();

protected:
	HRESULT							createVertexBuffer();
	void							updateVertexBuffer();
	void							updateVertex(int index, sControlPoint *pCp);



	ID3D11Device*					pd3dDevice_;

	ID3DX11EffectTechnique*			pFxTechRenderTfEdLines_;
	ID3DX11EffectTechnique*			pFxTechRenderTfEdPoints_;

	ID3DX11EffectVectorVariable*	pFxv4fLineColor_;


	ID3D11Buffer*					pVb_;
	ID3D11InputLayout*				pVl_;


	sControlPoint*					pFirstPoint_;
	UINT							uiNumPoints_;


	Vec2i					        v2iMaxXY_;

    XMFLOAT2*					    pData_;

    XMFLOAT4 					    v4fLineColor_;
};


#endif /* __TRANSFERFUNCTIONLINE_H__ */
