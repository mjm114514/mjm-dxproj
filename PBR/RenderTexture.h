#pragma once
#include "../Common/d3dUtil.h"
#include "../Common/UploadBuffer.h"

enum class CubeMapFace : int
{
	PositiveX = 0,
	NegativeX = 1,
	PositiveY = 2,
	NegativeY = 3,
	PositiveZ = 4,
	NegativeZ = 5
};

struct FaceConstants {
	DirectX::XMFLOAT3 LookAt;
	float padding0;
	DirectX::XMFLOAT3 Up;
	float padding1;
	DirectX::XMFLOAT3 Right;
	float padding2;
};

class RenderTexture{
public:
	RenderTexture(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format);
	RenderTexture(const RenderTexture& rhs) = delete;
	RenderTexture& operator=(const RenderTexture& rhs) = delete;
	virtual ~RenderTexture() = default;

	ID3D12Resource* Resource();

	virtual void OnResize(UINT newWidth, UINT newHeight) = 0;
	virtual void BakeTexture(ID3D12GraphicsCommandList* cmdList) = 0;

	UINT srvHeapIndex = 0;

	void Initialize();

protected:
	virtual void BuildResource() = 0;
	virtual void BuildDescriptorHeaps() = 0;
	virtual void BuildPso() = 0;
	virtual void BuildRootSignature() = 0;
	virtual void BuildDescriptors() = 0;
	virtual void BuildFaceConstant();

protected:
	ID3D12Device* md3dDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
	
	UINT mWidth;
	UINT mHeight;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	UINT mRtvDescriptorSize;
	UINT mDsvDescriptorSize;
	UINT mCbvSrvUavDescriptorSize;

	Microsoft::WRL::ComPtr<ID3D12Resource> mTextureMap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob> mVertexShader;
	Microsoft::WRL::ComPtr<ID3DBlob> mPixelShader;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;


	std::unique_ptr<UploadBuffer<FaceConstants>> faceCB = nullptr;
};
