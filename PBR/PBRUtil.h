#include "../Common/d3dUtil.h"

struct TextureData {
	std::string Name;
	std::wstring FileName;
	bool isDDS = false;
	UINT srvHeapIndex = -1;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
};

struct MaterialObj {
	std::string Name;
	UINT MatCBIndex;

	TextureData* DiffuseTexture;
	TextureData* NormalTexture;

	int NumFramesDirty = 3;

	DirectX::XMFLOAT3 Specular = { 0.01f, 0.01f, 0.01f };
	float Shininess = 0.25f;

	DirectX::XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

enum class RenderLayer : int
{
	Opaque = 0,
	Sky,
	Count
};

