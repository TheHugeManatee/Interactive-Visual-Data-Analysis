#ifndef __util_h__
#define __util_h__

#include <DirectXMath.h>
using namespace DirectX;

#include <string>
#include <iostream>
#include <windows.h>

#define SAFE_GET_PASS(Technique, name, var)   {assert(Technique!=nullptr);	var = Technique->GetPassByName( name );						assert(var->IsValid());}
#define SAFE_GET_TECHNIQUE(effect, name, var) {assert(effect!=nullptr);		var = effect->GetTechniqueByName( name );					assert(var->IsValid());}
#define SAFE_GET_SCALAR(effect, name, var)    {assert(effect!=nullptr);		var = effect->GetVariableByName( name )->AsScalar();		assert(var->IsValid());}
#define SAFE_GET_VECTOR(effect, name, var)    {assert(effect!=nullptr);		var = effect->GetVariableByName( name )->AsVector();		assert(var->IsValid());}
#define SAFE_GET_MATRIX(effect, name, var)    {assert(effect!=nullptr);		var = effect->GetVariableByName( name )->AsMatrix();		assert(var->IsValid());}
#define SAFE_GET_SAMPLER(effect, name, var)   {assert(effect!=nullptr);		var = effect->GetVariableByName( name )->AsSampler();		assert(var->IsValid());}
#define SAFE_GET_RESOURCE(effect, name, var)  {assert(effect!=nullptr);		var = effect->GetVariableByName( name )->AsShaderResource();assert(var->IsValid());}
#define SAFE_GET_UAV(effect, name, var) {assert(effect!=nullptr);		var = effect->GetVariableByName( name )->AsUnorderedAccessView();assert(var->IsValid());}


std::wstring GetExePath();
std::string GetPath(std::string path);
std::string GetFilename(std::string path, bool stripExt = false);

std::string ToAString(std::wstring s);
std::wstring ToWString(std::string s);

void UpdateWindowTitle(const std::wstring& appName);

bool GetFilenameDialog(const std::wstring& lpstrTitle, const WCHAR* lpstrFilter, std::wstring &filename, const bool save, HWND owner=NULL, DWORD* nFilterIndex=NULL);

void PrintMatrix(XMMATRIX m);

void printVector(std::string label, XMVECTOR &v);

#define XMFLOAT3_EQUAL(a,b) (((a).x == (b).x) && ((a).y == (b).y) && ((a).z == (b).z))
#define XMFLOAT4_EQUAL(a,b) (((a).x == (b).x) && ((a).y == (b).y) && ((a).z == (b).z) && ((a).w == (b).w))

class RenderTransformations;

class RenderTransformations {
public:
	XMMATRIX model;
	XMMATRIX world;
	XMMATRIX view;
	XMMATRIX proj;

	XMMATRIX modelWorld;
	XMMATRIX modelWorldView;
	XMMATRIX modelWorldViewInv;
	XMMATRIX modelWorldViewInvTransp;
	XMMATRIX modelWorldViewProj;
	XMMATRIX modelWorldViewProjInv;
	XMMATRIX worldView;
	XMMATRIX worldViewInv;
	XMMATRIX worldViewInvTransp;
	XMMATRIX worldViewProj;
	XMMATRIX viewProj;
	XMMATRIX viewInv;

	XMVECTOR camInWorld;

	XMFLOAT2 viewportSize;

	RenderTransformations(XMMATRIX _model, XMMATRIX _world, XMMATRIX _view, XMMATRIX _proj, XMFLOAT2 _viewportSize, XMVECTOR _camPos) :
		model(_model),
		world(_world),
		view(_view),
		proj(_proj) 
	{

		modelWorld = model*world;
		modelWorldView = modelWorld * view;
		modelWorldViewInv = XMMatrixInverse(nullptr, modelWorldView);
		modelWorldViewInvTransp = XMMatrixTranspose(modelWorldViewInv);
		modelWorldViewProj = modelWorldView * proj;
		modelWorldViewProjInv = XMMatrixInverse(nullptr, modelWorldViewProj);
		worldView = world*view;
		worldViewInv = XMMatrixInverse(nullptr, worldView);
		worldViewInvTransp = XMMatrixTranspose(worldViewInv);
		worldViewProj = worldView*proj;

		viewInv = XMMatrixInverse(nullptr, view);
		viewProj = view * proj;

		/*XMVECTOR origin = XMLoadFloat4(&XMFLOAT4(0, 0, 0, 1));
		XMVECTOR camInWold = XMVector4Transform(origin, viewInv);
		camInWold /= XMVectorGetW(camInWold);*/
		camInWorld = _camPos;
	};

	RenderTransformations PremultWorldMatrix(XMMATRIX pre) {
		return RenderTransformations(model, pre * world, view, proj, viewportSize, camInWorld);
	};

	RenderTransformations PremultModelMatrix(XMMATRIX pre) {
		return RenderTransformations(pre * model, world, view, proj, viewportSize, camInWorld);
	};
};

#endif
