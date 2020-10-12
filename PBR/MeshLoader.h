#pragma once
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
#include <vector>

#include "FrameResource.h"
#include "../Common/d3dUtil.h"

struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
};

class Model {
public:
	Model(std::string path) {
		loadModel(path);
	}
	std::vector<Mesh> meshes;

	UINT totalVertexCount = 0;
	UINT totalIndexCount = 0;

private:

	void loadModel(std::string path);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiMesh* node, const aiScene* scene);
};
