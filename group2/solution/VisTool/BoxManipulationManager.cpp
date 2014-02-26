#include "BoxManipulationManager.h"

#include "SimpleMesh.h"

ID3DX11Effect * BoxManipulationManager::pEffect;
ID3DX11EffectTechnique * BoxManipulationManager::pTechnique;
char * BoxManipulationManager::passNames[NUM_PASSES];
ID3DX11EffectPass * BoxManipulationManager::pPasses[NUM_PASSES];
ID3D11Device * BoxManipulationManager::pd3dDevice;

ID3DX11EffectMatrixVariable	* BoxManipulationManager::pWorldViewProjEV;
ID3DX11EffectScalarVariable	* BoxManipulationManager::pTimestepTEV;
ID3DX11EffectVectorVariable	* BoxManipulationManager::pCamPosEV;

HRESULT BoxManipulationManager::Initialize(ID3D11Device * pd3dDevice, TwBar* pParametersBar)
{
	return S_OK;
}

HRESULT BoxManipulationManager::Release(void)
{
	return S_OK;
}

BoxManipulationManager::BoxManipulationManager(void) :
	m_selectedBox(nullptr)
{
}


BoxManipulationManager::~BoxManipulationManager(void)
{
}


HRESULT BoxManipulationManager::Render(ID3D11DeviceContext* pd3dImmediateContext, RenderTransformations camMtcs)
{

	for(auto it = m_boxes.begin(), end=m_boxes.end(); it!= end; ++it) {
		if(!(*it)->visible) continue;
		XMMATRIX centering = XMMatrixTranslation((*it)->center.x - 0.5f*(*it)->size.x, (*it)->center.y - 0.5f*(*it)->size.y, (*it)->center.z - 0.5f*(*it)->size.z);
		SimpleMesh::DrawBoundingBox(pd3dImmediateContext, centering * (*it)->texToNDCSpace, (*it)->size);
		
		//we reset flags here because the rendering is guaranteed to be between FrameMove events
		(*it)->axisSurfaceChanged = false;
		(*it)->changed = false;
	}
	return S_OK;
}


void BoxManipulationManager::FrameMove(double dTime, float fElapsedTime)
{
}

void BoxManipulationManager::AddBox(ManipulationBox * box)
{
	m_boxes.push_back(box);
}

void BoxManipulationManager::RemoveBox(ManipulationBox * box)
{
	m_boxes.remove(box);
}

// normal is defined as cross(u, v)
// returns the ray parameter t for the intersection point
float BoxManipulationManager::intersectRayQuad(XMVECTOR p, XMVECTOR u, XMVECTOR v, XMVECTOR o, XMVECTOR d)
{
	XMVECTOR n = XMVector3Cross(v, u);
	XMVECTOR plane = XMPlaneFromPointNormal(p, n);

	XMVECTOR intersection = XMPlaneIntersectLine(plane, o, o+d);
	//printVector("Intersection", intersection);
	float tu = XMVectorGetX(XMVector3Dot(intersection - p, u));
	float tv = XMVectorGetX(XMVector3Dot(intersection - p, v));
	
	float dot = XMVectorGetX(XMVector3Dot(d, intersection - o));
	//std::cout << "tu : " << tu << " tv: " << tv << " Dot: " << dot << std::endl;
	//check if the intersection is inside the quad
	if(tu >= 0 && tv >= 0 && tu <= 1 && tv <= 1)
		return dot;
	else
		return std::numeric_limits<float>::infinity();	
}

std::pair<int, float> BoxManipulationManager::checkMouseBoxIntersection(ManipulationBox  *box, float xPos, float yPos)
{
	//mouse is in normalized screen coords, i.e. between 0 and 1 in both directions
	//std::cout << "Mouse pos: " << xPos << " | " << yPos << std::endl;
	XMVECTOR mouseRayOrg = XMLoadFloat4(&XMFLOAT4(2*xPos-1, 2*yPos-1, -1, 1));
	XMVECTOR mouseRayDest = XMLoadFloat4(&XMFLOAT4(2*xPos-1, 2*yPos-1, 1, 1));

	XMMATRIX m_NDCToTexSpace = XMMatrixInverse(nullptr, box->texToNDCSpace);

	//// transform into tex coordinates
	mouseRayOrg = XMVector3TransformCoord(mouseRayOrg, m_NDCToTexSpace);
	mouseRayDest = XMVector3TransformCoord(mouseRayDest, m_NDCToTexSpace);
	//calculate ray direction
	XMVECTOR mouseRayDir = mouseRayDest - mouseRayOrg;

	float dx = box->size.x;
	float dy = box->size.y;
	float dz = box->size.z;

	XMVECTOR ex = XMLoadFloat4(&XMFLOAT4(1/dx, 0, 0, 0));
	XMVECTOR ey = XMLoadFloat4(&XMFLOAT4(0, 1/dy, 0, 0));
	XMVECTOR ez = XMLoadFloat4(&XMFLOAT4(0, 0, 1/dz, 0));
	XMVECTOR o = XMLoadFloat3(&box->center) - 0.5*XMLoadFloat3(&box->size); //origin of the cube in tex space

	float intersectionPoints[3];
	// front/back
	float frontIntersect = intersectRayQuad(o, ex, ey, mouseRayOrg, mouseRayDir);
	float backIntersect = intersectRayQuad(o + dz*dz*ez, ex, ey, mouseRayOrg, mouseRayDir);
	intersectionPoints[0] = std::min(frontIntersect, backIntersect);
	//std::cout << "Front: " << frontIntersect << std::endl;
	//std::cout << "Back:  " << backIntersect << std::endl;
	
	// left/right
	float leftIntersect = intersectRayQuad(o, ey, ez, mouseRayOrg, mouseRayDir);
	float rightIntersect = intersectRayQuad(o + dx*dx*ex, ey, ez, mouseRayOrg, mouseRayDir);
	intersectionPoints[1] = std::min(leftIntersect, rightIntersect);
	//std::cout << "Left:  " << leftIntersect << std::endl;
	//std::cout << "Right: " << rightIntersect << std::endl;

	// down/up
	float downIntersect = intersectRayQuad(o, ez, ex, mouseRayOrg, mouseRayDir);
	float upIntersect = intersectRayQuad(o + dy*dy*ey, ez, ex, mouseRayOrg, mouseRayDir);
	intersectionPoints[2] = std::min(downIntersect, upIntersect);
	//std::cout << "Down:  " << downIntersect << std::endl;
	//std::cout << "Up:    " << upIntersect << std::endl;


	//std::cout << "Front/Back: " << intersectionPoints[0] << std::endl;
	//std::cout << "Left/Right: " << intersectionPoints[1] << std::endl;
	//std::cout << "Up/Down:    " << intersectionPoints[2] << std::endl;

	if(!_finitef(intersectionPoints[0]) && !_finitef(intersectionPoints[1]) && !_finitef(intersectionPoints[2]))
		return std::pair<int, float>(-1, std::numeric_limits<float>::infinity());

	if(intersectionPoints[0] < intersectionPoints[1]) {
		if(intersectionPoints[2] < intersectionPoints[0])
			return (downIntersect < upIntersect) ? std::pair<int, float>(4, downIntersect) : std::pair<int, float>(5, upIntersect);
		else
			return (frontIntersect < backIntersect) ? std::pair<int, float>(0, frontIntersect) : std::pair<int, float>(1, backIntersect);
	}
	else {
		if(intersectionPoints[2] < intersectionPoints[1])
			return (downIntersect < upIntersect) ? std::pair<int, float>(4, downIntersect) : std::pair<int, float>(5, upIntersect);
		else
			return (leftIntersect < rightIntersect) ?  std::pair<int, float>(2, leftIntersect) :  std::pair<int, float>(3, rightIntersect);
	}
}

void BoxManipulationManager::OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
					int nMouseWheelDelta, float xPos, float yPos)
{
	if(!bLeftButtonDown && !bMiddleButtonDown) {
		m_mouseLeftBeenDown = m_mouseMiddleBeenDown = false;
		if(m_selectedBox) {
			m_selectedBox->changed = false;
			m_selectedBox->axisSurfaceChanged = false;
		}
		m_selectedBox = nullptr;
		return;
	}


	if((!m_mouseLeftBeenDown && bLeftButtonDown) || (!m_mouseMiddleBeenDown && bMiddleButtonDown)) {
		ManipulationBox *selectedBox = nullptr;
		float leastDistance = std::numeric_limits<float>::infinity();
		int selectedPlane = 0;
		for(auto it = m_boxes.begin(), end=m_boxes.end(); it!= end; ++it) {
			//note moveable or scalable and we want to do that?
			if( (bMiddleButtonDown && !(*it)->scalable) || (bLeftButtonDown && !(*it)->moveable))
				continue;

			std::pair<int, float> sel = checkMouseBoxIntersection(*it, xPos, yPos);
			if(sel.first >= 0 && sel.second < leastDistance) {
				selectedPlane = sel.first;
				leastDistance = sel.second;
				selectedBox = *it;
			}
		}

		if(selectedBox) {
			m_selectedBox = selectedBox;

			//std::cout << "Selected plane: " << selectedPlane << std::endl;
			m_mouseLeftBeenDown = bLeftButtonDown;
			m_mouseMiddleBeenDown = bMiddleButtonDown;

			m_centerAtMouseDown = selectedBox->center;
			m_sizeAtMouseDown = selectedBox->size;
			XMMATRIX NDCToTexSpace = XMMatrixInverse(nullptr, selectedBox->texToNDCSpace);
			XMVECTOR xmCamRightInTex = XMVector3Transform(XMLoadFloat3(&XMFLOAT3(0, 0, 0)), NDCToTexSpace) -  XMVector3Transform(XMLoadFloat3(&XMFLOAT3(1, 0, 0)), NDCToTexSpace);
			XMVECTOR xmCamUpInTex = XMVector3Transform(XMLoadFloat3(&XMFLOAT3(0, 0, 0)), NDCToTexSpace) -  XMVector3Transform(XMLoadFloat3(&XMFLOAT3(0, 1, 0)), NDCToTexSpace);

			switch(selectedPlane) {
			case 0: // front
				m_movementDirectionTEX = XMFLOAT3(0, 0, 1); 
				m_movementDirInvert = true;
				break;
			case 1: // back
				m_movementDirectionTEX = XMFLOAT3(0, 0, 1);
				m_movementDirInvert = false;
				break;
			case 2: // left
				m_movementDirectionTEX = XMFLOAT3(1, 0, 0);
				m_movementDirInvert = true;
				break;
			case 3: // right
				m_movementDirectionTEX = XMFLOAT3(1, 0, 0);
				m_movementDirInvert = false;
				break;
			case 4: // down
				m_movementDirectionTEX = XMFLOAT3(0, 1, 0);
				m_movementDirInvert = true;
				break;
			case 5: // up
				m_movementDirectionTEX = XMFLOAT3(0, 1, 0);
				m_movementDirInvert = false;
				break;
			default:
				//wait, what??
				break;
			}

			XMVECTOR xmTexInNDC = XMVector3Transform(XMLoadFloat3(&XMFLOAT3(0, 0, 0)), selectedBox->texToNDCSpace) - XMVector3Transform(XMLoadFloat3(&m_movementDirectionTEX), m_selectedBox->texToNDCSpace);
			XMStoreFloat3(&m_movementDirectionNDC, xmTexInNDC);
			float l = std::max(sqrt(m_movementDirectionNDC.x*m_movementDirectionNDC.x + m_movementDirectionNDC.y*m_movementDirectionNDC.y), 0.001f);
			m_movementDirectionNDC.x /= l;
			m_movementDirectionNDC.y /= l;
			//std::cout << "Movement in NDC is " << m_movementDirectionNDC.x << " " << m_movementDirectionNDC.y << " " << m_movementDirectionNDC.z << std::endl;

			m_xPosAtMouseDown = xPos;
			m_yPosAtMouseDown = yPos;

			//std::cout << "Selected Plane: " << selectedPlane << std::endl;
			//std::cout << "XDir " << m_movementDirectionX.x << " " << m_movementDirectionX.y << " " << m_movementDirectionX.z << std::endl;
			//std::cout << "YDir " << m_movementDirectionY.x << " " << m_movementDirectionY.y << " " << m_movementDirectionY.z << std::endl;
		}
	}

	//movement behavior
	if(m_selectedBox && m_mouseLeftBeenDown && bLeftButtonDown) {
		float dx = m_xPosAtMouseDown - xPos;
		float dy =  m_yPosAtMouseDown - yPos;

		//std::cout << "Movement in TEX is " << m_movementDirectionTEX.x << " " << m_movementDirectionTEX.y << " " << m_movementDirectionTEX.z << std::endl;
		//std::cout << "Movement in NDC is " << m_movementDirectionNDC.x << " " << m_movementDirectionNDC.y << " " << m_movementDirectionNDC.z << std::endl;
		//std::cout << "Delta is dx " << dx << ", dy " << dy << std::endl;

		XMVECTOR mDir = XMLoadFloat3(&m_movementDirectionTEX);
		float d = m_movementDirectionNDC.x * dx + m_movementDirectionNDC.y * dy;

		XMVECTOR newCenter = XMLoadFloat3(&m_centerAtMouseDown) + d * mDir;
		XMStoreFloat3(&m_selectedBox->center, newCenter);

		m_selectedBox->SetChanged();
	}

	//scaling behavior
	else if(m_selectedBox && m_mouseMiddleBeenDown && bMiddleButtonDown) {
		float dx = m_xPosAtMouseDown - xPos;
		float dy =  m_yPosAtMouseDown - yPos;

		//std::cout << "Movement in TEX is " << m_movementDirectionTEX.x << " " << m_movementDirectionTEX.y << " " << m_movementDirectionTEX.z << std::endl;
		//std::cout << "Movement in NDC is " << m_movementDirectionNDC.x << " " << m_movementDirectionNDC.y << " " << m_movementDirectionNDC.z << std::endl;
		//std::cout << "Delta is dx " << dx << ", dy " << dy << std::endl;

		XMVECTOR mDir = XMLoadFloat3(&m_movementDirectionTEX);
		float d = m_movementDirectionNDC.x * dx + m_movementDirectionNDC.y * dy;

		XMVECTOR newSize;
		if(m_movementDirInvert)
			newSize = XMLoadFloat3(&m_sizeAtMouseDown) - 2 * d * mDir;
		else
			newSize = XMLoadFloat3(&m_sizeAtMouseDown) + 2 * d * mDir;
		XMStoreFloat3(&m_selectedBox->size, newSize);

		m_selectedBox->SetChanged();
	}

	m_mouseLeftBeenDown = bLeftButtonDown;
	m_mouseMiddleBeenDown = bMiddleButtonDown;
}
