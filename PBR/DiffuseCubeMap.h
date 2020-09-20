#pragma once
#include "RenderTexture.h"

class DiffuseCubeMap : public RenderTexture {
public:
	DiffuseCubeMap(ID3D12Device* device, ID3D12Resource* lightMap, UINT width, UINT height);
	DiffuseCubeMap(const DiffuseCubeMap& rhs) = delete;
	DiffuseCubeMap& operator=(const DiffuseCubeMap& rhs) = delete;
	virtual ~DiffuseCubeMap() = default;

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
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSrvHeap = nullptr;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv[6];
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
};