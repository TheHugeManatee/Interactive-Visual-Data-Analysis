#pragma once
#include "simplemesh.h"
class Triangle :
	public SimpleMesh
{
public:

	Triangle(void)
	{

		SimpleVertex v[] = {
			XMFLOAT3(0, 0, 0),	XMFLOAT3(0, 0, -1),
			XMFLOAT3(0, 1, 0),	XMFLOAT3(0, 0, -1),
			XMFLOAT3(1, 0, 0),	XMFLOAT3(0, 0, -1)
		};

		m_vertices.push_back(v[0]);
		m_vertices.push_back(v[1]);
		m_vertices.push_back(v[2]);

		m_indices.push_back(0);
		m_indices.push_back(1);
		m_indices.push_back(2);

		//red triangle
		m_color = XMFLOAT4(1, 0, 0, 1);

		m_bboxSize = XMFLOAT3(1, 1, 0.3f);

		SetupGPUBuffers();
	}

	~Triangle(void)
	{
	}
};

