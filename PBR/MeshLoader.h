#pragma once
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
#include <vector>

#include "FrameResource.h"
#include "../Common/d3dUtil.h"

using namespace std;



struct Mesh {
	vector<Vertex> vertices;
	vector<uint32_t> indices;
};

class Model {
public:
	Model(string path) {
		loadModel(path);
	}
	vector<Mesh> meshes;

	UINT totalIndexCount;
	UINT totalVertexCount;
private:

	void loadModel(string path);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* node, const aiScene* scene);
};
