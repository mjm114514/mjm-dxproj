#include "PreFilteredCubeMap.h"

PreFilteredCubeMap::PreFilteredCubeMap(ID3D12Device* device, ID3D12Resource* lightMap, UINT width, UINT height, DXGI_FORMAT format)
	:CubeMap(device, lightMap, width, height, format)
{
}

void PreFilteredCubeMap::BuildResource() {
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(texDesc));
	texDesc.Height = mHeight;
	texDesc.Width = mWidth;
	texDesc.Format = mFormat;
	texDesc.DepthOrArraySize = 6;
	texDesc.MipLevels = 0;
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mCubeMap)
	));

	mMipLevels = mCubeMap->GetDesc().MipLevels;
}

void PreFilteredCubeMap::BuildDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = 6 * mMipLevels;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
		&srvHeapDesc,
		IID_PPV_ARGS(&mSrvHeap)
	));
	mhCpuSrv = CD3DX12_CPU_DESCRIPTOR_HANDLE(mSrvHeap->GetCPUDescriptorHandleForHeapStart());
}

void PreFilteredCubeMap::BuildDescriptors() {
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mLightMap->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = mLightMap->GetDesc().MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	md3dDevice->CreateShaderResourceView(mLightMap, &srvDesc, mhCpuSrv);
	mhGpuSrv = CD3DX12_GPU_DESCRIPTOR_HANDLE(mSrvHeap->GetGPUDescriptorHandleForHeapStart());

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());

	for (int mip = 0; mip < mMipLevels; mip++) {
		for (int i = 0; i < 6; i++) {
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Format = mFormat;
			rtvDesc.Texture2DArray.MipSlice = mip;
			rtvDesc.Texture2DArray.PlaneSlice = 0;

			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.FirstArraySlice = i;

			md3dDevice->CreateRenderTargetView(mCubeMap.Get(), &rtvDesc, handle);
			handle.Offset(mCbvSrvUavDescriptorSize);
		}
	}
}

void PreFilteredCubeMap::BuildRootSignature() {
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	slotRootParameter[0].InitAsConstantBufferView(0); // CbPerFace
	slotRootParameter[1].InitAsConstants(1, 1);       // roughness (per cube)
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable); // LightMap

	CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		0,
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP
	);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 1> samplers = {
		linearWrap
	};

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
		_countof(slotRootParameter),
		slotRootParameter,
		samplers.size(),
		samplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	);

	// create a root sig with a single slot which points to a descriptor range consisting of a single constant buffer 
	Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())
	));
}

void PreFilteredCubeMap::BuildPso() {
	mVertexShader = d3dUtil::CompileShader(L"Shaders/preFilter.hlsl", nullptr, "VS", "vs_5_1");
	mPixelShader = d3dUtil::CompileShader(L"Shaders/preFilter.hlsl", nullptr, "PS", "ps_5_1");

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc;
	ZeroMemory(&psoDesc, sizeof(psoDesc));
	psoDesc.InputLayout = { inputLayout.data(), ( UINT )inputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS = {
		reinterpret_cast< BYTE* >(mVertexShader->GetBufferPointer()),
		mVertexShader->GetBufferSize()
	};
	psoDesc.PS = {
		reinterpret_cast< BYTE* >(mPixelShader->GetBufferPointer()),
		mPixelShader->GetBufferSize()
	};
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.DepthStencilState.DepthEnable = false;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(
		&psoDesc,
		IID_PPV_ARGS(&mPSO)
	));
}

void PreFilteredCubeMap::OnResize(UINT newWidth, UINT newHeight) {
	if (newWidth != mWidth || newHeight != mHeight) {
		mWidth = newWidth;
		mHeight = newHeight;

		Initialize();
	}
}

void PreFilteredCubeMap::BakeCubeMap(ID3D12GraphicsCommandList* cmdList) {
	ThrowIfFailed(md3dDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(cmdListAlloc.GetAddressOf())
	));

	ThrowIfFailed(cmdListAlloc->Reset());
	ThrowIfFailed(cmdList->Reset(cmdListAlloc.Get(), mPSO.Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvHeap.Get() };
	cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	cmdList->SetGraphicsRootSignature(mRootSignature.Get());

	// Set the Light Map
	cmdList->SetGraphicsRootDescriptorTable(2, mhGpuSrv);

	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mCubeMap.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	UINT faceCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(faceCB));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(mRtvHeap->GetCPUDescriptorHandleForHeapStart());

	float clearValue[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	for (int mip = 0; mip < mMipLevels; mip++) {

		float roughness = ( float )mip / (mMipLevels - 1);
		cmdList->SetGraphicsRoot32BitConstant(1, roughness, 0);

		D3D12_VIEWPORT viewport = mViewport;
		D3D12_RECT scissorRect = mScissorRect;

		cmdList->RSSetViewports(1, &viewport);
		cmdList->RSSetScissorRects(1, &scissorRect);

		for (int i = 0; i < 6; i++) {

			D3D12_GPU_VIRTUAL_ADDRESS faceCBAddress = faceCB->Resource()->GetGPUVirtualAddress() + i * faceCBByteSize;
			cmdList->SetGraphicsRootConstantBufferView(0, faceCBAddress);

			cmdList->OMSetRenderTargets(1, &rtv, true, nullptr);

			rtv.Offset(mCbvSrvUavDescriptorSize);

			cmdList->IASetVertexBuffers(0, 0, nullptr);
			cmdList->IASetIndexBuffer(nullptr);
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->DrawInstanced(6, 1, 0, 0);
		}
		viewport.Height /= 2;
		viewport.Width /= 2;

		scissorRect.right /= 2;
		scissorRect.bottom /= 2;
	}

	cmdList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mCubeMap.Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_GENERIC_READ
		)
	);
}
