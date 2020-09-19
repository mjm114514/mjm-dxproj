#pragma once
#include "CubeMap.h"

class DiffuseCubeMap : public CubeMap {
public:
	DiffuseCubeMap(ID3D12Device* device, ID3D12Resource* lightMap, UINT width, UINT height, DXGI_FORMAT format);
	DiffuseCubeMap(const DiffuseCubeMap& rhs) = delete;
	DiffuseCubeMap& operator=(const DiffuseCubeMap& rhs) = delete;
	virtual ~DiffuseCubeMap() = default;

	virtual void OnResize(UINT newWidth, UINT newHeight)override;
	virtual void BakeCubeMap(ID3D12GraphicsCommandList * cmdList)override;

protected:
	virtual void BuildResource()override;
	virtual void BuildDescriptorHeaps()override;
	virtual void BuildPso()override;
	virtual void BuildRootSignature()override;
	virtual void BuildDescriptors()override;

protected:
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv[6];
	CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
};