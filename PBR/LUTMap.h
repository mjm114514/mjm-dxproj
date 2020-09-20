#pragma once
#include "RenderTexture.h"

class LUTMap : public RenderTexture{
public:
	LUTMap(ID3D12Device* device, UINT width, UINT height);
	LUTMap(const LUTMap& rhs) = delete;
	LUTMap& operator=(const LUTMap& rhs) = delete;
	virtual ~LUTMap() = default;

	virtual void OnResize(UINT newWidth, UINT newHeight)override;
	virtual void BakeTexture(ID3D12GraphicsCommandList* cmdList)override;

protected:
	virtual void BuildResource()override;
	virtual void BuildDescriptorHeaps()override;
	virtual void BuildPso()override;
	virtual void BuildRootSignature()override;
	virtual void BuildDescriptors()override;
};