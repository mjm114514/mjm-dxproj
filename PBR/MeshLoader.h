#pragma once
#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
#include <vector>

#include "FrameResource.h"

using namespace std;



struct Mesh {
public:
	vector<Vertex> vertices;
	vector<uint32_t> indices;
};

class Model {
public:
	Model(char* path) {
		loadModel(path);
	}
private:
	vector<Mesh> meshes;

	void loadModel(string path);
	void processNode(aiNode* node, const aiScene* scene);
	Mesh processMesh(aiNode* node, const aiScene* scene);
};
