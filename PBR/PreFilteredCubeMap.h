#pragma once
#include "RenderTexture.h"

class PreFilteredCubeMap : public RenderTexture {

public:
	PreFilteredCubeMap(ID3D12Device* device, ID3D12Resource* lightMap, UINT width, UINT height);
	PreFilteredCubeMap(const PreFilteredCubeMap& rhs) = delete;
	PreFilteredCubeMap& operator=(const PreFilteredCubeMap& rhs) = delete;

	virtual ~PreFilteredCubeMap() = default;

	virtual void OnResize(UINT newWidth, UINT newHeight)override;
	virtual void BakeTexture(ID3D12GraphicsCommandList * cmdList)override;

protected:
	virtual void BuildResource()override;
	virtual void BuildDescriptorHeaps()override;
	virtual void BuildPso()override;
	virtual void BuildRootSignature()override;
	virtual void BuildDescriptors()override;

protected:
	ID3D12Resource* mLightMap = nullptr;
	UINT mMipLevels;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap = nullptr;
};
