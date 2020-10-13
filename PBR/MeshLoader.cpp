#include "MeshLoader.h"

void Model::loadModel(std::string path) {
    Assimp::Importer import;
    const aiScene *scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);	
	
    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
    {
		std::string errorStr(import.GetErrorString());
        throw std::exception(errorStr.c_str());
    }

    processNode(scene->mRootNode, scene);
}

void Model::processNode(aiNode* node, const aiScene* scene) {
    for(unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]]; 
        meshes.push_back(processMesh(mesh, scene));			
    }
    // then do the same for each of its children
    for(unsigned int i = 0; i < node->mNumChildren; i++)
    {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
    std::vector<Vertex> vertices(mesh->mNumVertices);
    std::vector<std::uint32_t> indices;
    std::vector<Texture> textures;

    for(unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;
        // process vertex positions, normals and texture coordinates
        vertex.Pos.x = mesh->mVertices[i].x;
        vertex.Pos.y = mesh->mVertices[i].y;
        vertex.Pos.z = mesh->mVertices[i].z;

        vertex.Normal.x = mesh->mNormals[i].x;
        vertex.Normal.y = mesh->mNormals[i].y;
        vertex.Normal.z = mesh->mNormals[i].z;

        if (mesh->mTextureCoords[0]) {
            vertex.TexC.x = mesh->mTextureCoords[0][i].x;
            vertex.TexC.y = mesh->mTextureCoords[0][i].y;
        }

		vertices[i] = vertex;
    }

	std::vector<uint8_t> shareCount(vertices.size(), 0);
    // process indices
	for (unsigned int i = 0; i < mesh->mNumFaces; i++){
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
		Vertex v[3];
		v[0] = vertices[face.mIndices[0]];
		v[1] = vertices[face.mIndices[1]];
		v[2] = vertices[face.mIndices[2]];

		DirectX::XMFLOAT3 e1(
			v[1].Pos.x - v[0].Pos.x,
			v[1].Pos.y - v[0].Pos.y,
			v[1].Pos.z - v[0].Pos.z
		);
		DirectX::XMFLOAT3 e2(
			v[2].Pos.x - v[0].Pos.x,
			v[2].Pos.y - v[0].Pos.y,
			v[2].Pos.z - v[0].Pos.z
		);

		float u0 = v[0].TexC.x;
		float u1 = v[1].TexC.x;
		float u2 = v[2].TexC.x;

		float v0 = v[0].TexC.y;
		float v1 = v[1].TexC.y;
		float v2 = v[2].TexC.y;

		float denom = (u1 - u0) * (v2 - v0) - (u2 - u0) * (v1 - v0);

		float para1 = v2 - v0;
		float para2 = -(v1 - v0);

		DirectX::XMFLOAT3 tangent;
		tangent.x = (e1.x * para1 + e2.x * para2) / denom;
		tangent.y = (e1.y * para1 + e2.y * para2) / denom;
		tangent.z = (e1.z * para1 + e2.z * para2) / denom;

		vertices[face.mIndices[0]].TangentU.x += tangent.x;
		vertices[face.mIndices[0]].TangentU.y += tangent.y;
		vertices[face.mIndices[0]].TangentU.z += tangent.z;

		vertices[face.mIndices[1]].TangentU.x += tangent.x;
		vertices[face.mIndices[1]].TangentU.y += tangent.y;
		vertices[face.mIndices[1]].TangentU.z += tangent.z;

		vertices[face.mIndices[2]].TangentU.x += tangent.x;
		vertices[face.mIndices[2]].TangentU.y += tangent.y;
		vertices[face.mIndices[2]].TangentU.z += tangent.z;

		shareCount[face.mIndices[0]]++;
		shareCount[face.mIndices[1]]++;
		shareCount[face.mIndices[2]]++;
	}

	for (int i = 0; i < shareCount.size(); i++) {
		vertices[i].TangentU.x /= shareCount[i];
		vertices[i].TangentU.y /= shareCount[i];
		vertices[i].TangentU.z /= shareCount[i];
	}
    // process material

    totalVertexCount += vertices.size();
    totalIndexCount += indices.size();

    return {vertices, indices};
}