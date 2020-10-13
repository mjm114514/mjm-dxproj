#include "../Common/d3dUtil.h"

class TextureData {
public:
	std::string Name;
	std::wstring FileName;
	bool isDDS = false;
	UINT srvHeapIndex = -1;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource;
};

struct MaterialObj {
	std::string Name;
	UINT MatCBIndex;

	TextureData* AlbedoTex;
	TextureData* MetallicTex;
	TextureData* RoughnessTex;
	TextureData* AOTex;
	TextureData* NormalTex;

	DirectX::XMFLOAT3 albedo;
	float Metallic;
	float Roughness;
	float AO;

	int NumFramesDirty = 3;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Gun,
	Sky,
	Count
};

