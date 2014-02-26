#include "PLYMesh.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <exception>
#include <vector>
#include <cstdint>

#include "../../../external/rply-1.1.3/rply.h"


static int vertex_cb(p_ply_argument argument);
static int face_cb(p_ply_argument argument);

PLYMesh::PLYMesh(std::string plyFileName)
{
	loadPly(plyFileName);

	//TODO:	fill m_vertices and m_indices

	//initialize the GPU vertex and index buffers
	SetupGPUBuffers();
}


PLYMesh::~PLYMesh(void)
{

}

//Method for loading ply files via RPly
void PLYMesh::loadPly(std::string scenePlyFile)
{

	long nvertices, ntriangles;
	p_ply ply = ply_open((char*)scenePlyFile.c_str(), NULL, 0, NULL);
	if (!ply)
	{
		std::cerr << "Could not open ply file!" << std::endl;
		throw std::exception();
	}
	if (!ply_read_header(ply))
	{
		std::cerr << "Could not read ply header!" << std::endl;
		throw std::exception();
	}

	//set callbacks for vertices
	nvertices = ply_set_read_cb(ply, "vertex", "x", vertex_cb, &m_vertices, 0);
    ply_set_read_cb(ply, "vertex", "y", vertex_cb, &m_vertices, 1);
    ply_set_read_cb(ply, "vertex", "z", vertex_cb, &m_vertices, 2);
	ply_set_read_cb(ply, "vertex", "nx", vertex_cb, &m_vertices, 3);
	ply_set_read_cb(ply, "vertex", "ny", vertex_cb, &m_vertices, 4);
	ply_set_read_cb(ply, "vertex", "nz", vertex_cb, &m_vertices, 5);


	//set callback for faces
	ntriangles = ply_set_read_cb(ply, "face", "vertex_indices", face_cb, &m_indices, 0);
    
	printf("%ld\n%ld\n", nvertices, ntriangles);
	

	//finally do the reading
    if (!ply_read(ply))
	{
		std::cerr << "Error reading ply file!" << std::endl;
		throw std::exception();
	}

    ply_close(ply);
}

//RPly callback function for vertices
static int vertex_cb(p_ply_argument argument) {
    long flag;
	std::vector<SimpleVertex>* list = NULL;
	SimpleVertex v = {XMFLOAT3(0, 0, 0), XMFLOAT3(0, 0, 0)};
	void** ptr = NULL;

    ply_get_argument_user_data(argument, (void**)&list, &flag);
	//list = reinterpret_cast<std::vector<SimpleVertex>*>(*ptr);

	switch(flag)
	{
	case 0:
		v.pos.x = (float)ply_get_argument_value(argument);
		list->push_back(v);
		break;
	case 1:
		list->back().pos.y = (float)ply_get_argument_value(argument);
		break;
	case 2:
		list->back().pos.z = (float)ply_get_argument_value(argument);
		break;
	case 3:
		list->back().nor.x = (float)ply_get_argument_value(argument);
		break;
	case 4:
		list->back().nor.y = (float)ply_get_argument_value(argument);
		break;
	case 5:
		list->back().nor.z = (float)ply_get_argument_value(argument);
		break;
	default:
		break;
	}
	//std::cout << "2" << std::endl;
    return 1;
}

//RPly callback function for faces
static int face_cb(p_ply_argument argument) {
    long length, value_index;
	std::vector<uint32_t>* list = NULL;
	void ** ptr = NULL;

	ply_get_argument_user_data(argument, (void**)&list, &length);
	//list = reinterpret_cast<std::vector<uint32_t>*>(&ptr);
    
	//TODO
	ply_get_argument_property(argument, NULL, &length, &value_index);

	switch (value_index) {
        case 0:
        case 1: 
            //printf("%g ", ply_get_argument_value(argument));
			list->push_back((unsigned int)ply_get_argument_value(argument));
            break;
        case 2:
            //printf("%g\n", ply_get_argument_value(argument));
			list->push_back((unsigned int)ply_get_argument_value(argument));
            break;
        default: 
            break;
    }
	
    return 1;
}