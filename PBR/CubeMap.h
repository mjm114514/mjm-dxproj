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
};

class CubeMap{
public:
	CubeMap(ID3D12Device* device, ID3D12Resource* LightMap, UINT width, UINT height, DXGI_FORMAT format);
	CubeMap(const CubeMap& rhs) = delete;
	CubeMap& operator=(const CubeMap& rhs) = delete;
	~CubeMap() = default;

	ID3D12Resource* Resource();
	CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv(int faceIndex);
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv();

	D3D12_VIEWPORT Viewport()const;
	D3D12_RECT ScissorRect()const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv[6]
	);

	void OnResize(UINT newWidth, UINT newHeight);

private:
	void BuildResource();
	void BuildDescriptorHeaps();
	void BuildPso();
	void BuildRootSignature();
	void BuildDescriptors();
	void BuildFaceConstant();
	void DrawToCubeMap(ID3D12GraphicsCommandList* cmdList);

private:
	ID3D12Device* md3dDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
	
	UINT mWidth;
	UINT mHeight;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	UINT mRtvDescriptorSize;
	UINT mDsvDescriptorSize;
	UINT CbvSrvUavDescriptorSize;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv[6];

	Microsoft::WRL::ComPtr<ID3D12Resource> mCubeMap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mRtvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mDsvHeap = nullptr;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob> mVertexShader;
	Microsoft::WRL::ComPtr<ID3DBlob> mPixelShader;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> cmdListAlloc;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ID3D12Resource* mLightMap = nullptr;

	std::unique_ptr<UploadBuffer<FaceConstants>> faceCB = nullptr;
};
