#include "CubeMap.h"

CubeMap::CubeMap(ID3D12Device* device, ID3D12Resource* LightMap, UINT width, UINT height, DXGI_FORMAT format) {
	md3dDevice = device;
	mWidth = width;
	mHeight = height;
	mFormat = format;

	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };

	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mLightMap = LightMap;

}

void CubeMap::Initialize() {
	BuildResource();
	BuildDescriptorHeaps();
	BuildDescriptors();
	BuildFaceConstant();
	BuildRootSignature();
	BuildPso();
}

ID3D12Resource* CubeMap::Resource() {
	return mCubeMap.Get();
}

void CubeMap::BuildFaceConstant() {
	float x = 0, y = 0, z = 0;

	DirectX::XMFLOAT3 targets[6] =
	{
		DirectX::XMFLOAT3(x + 1.0f, y, z), // +X
		DirectX::XMFLOAT3(x - 1.0f, y, z), // -X
		DirectX::XMFLOAT3(x, y + 1.0f, z), // +Y
		DirectX::XMFLOAT3(x, y - 1.0f, z), // -Y
		DirectX::XMFLOAT3(x, y, z + 1.0f), // +Z
		DirectX::XMFLOAT3(x, y, z - 1.0f)  // -Z
	};

	// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
	// are looking down +Y or -Y, so we need a different "up" vector.
	DirectX::XMFLOAT3 ups[6] =
	{
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),  // +X
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),  // -X
		DirectX::XMFLOAT3(0.0f, 0.0f, -1.0f), // +Y
		DirectX::XMFLOAT3(0.0f, 0.0f, +1.0f), // -Y
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f),	 // +Z
		DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f)	 // -Z
	};


	faceCB = std::make_unique<UploadBuffer<FaceConstants>>(md3dDevice, 6, true);

	for (int i = 0; i < 6; i++) {
		FaceConstants fc;
		fc.LookAt = targets[i];
		fc.Up = ups[i];
		DirectX::XMVECTOR lookAt = DirectX::XMLoadFloat3(&fc.LookAt);
		DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&fc.Up);
		DirectX::XMStoreFloat3(&fc.Right, DirectX::XMVector3Cross(up, lookAt));
		faceCB->CopyData(i, fc);
	}
}
