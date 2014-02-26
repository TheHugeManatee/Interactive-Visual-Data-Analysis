#pragma once
#include "SimpleMesh.h"

#include <string>
#include <vector>
#include <cstdint>

class PLYMesh :
	public SimpleMesh
{
public:
	PLYMesh(std::string plyFileName);
	~PLYMesh(void);

private:

	struct SimpleVertex
	{
		XMFLOAT3 pos;
		XMFLOAT3 nor;
	};

	void loadPly(std::string scenePlyFile);
	std::vector<SimpleVertex>* vertexList;
	std::vector<uint32_t>* indicesList;
};

