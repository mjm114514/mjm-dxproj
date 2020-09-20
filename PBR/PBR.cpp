#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "../Common/Camera.h"
#include "FrameResource.h"
#include "PBRUtil.h"
#include "DiffuseCubeMap.h"
#include "PreFilteredCubeMap.h"
#include "LUTMap.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

const int IBLMapSize = 2048;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
    RenderItem(const RenderItem& rhs) = delete;
 
    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	MaterialObj* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

class PBR : public D3DApp
{
public:
    PBR(HINSTANCE hInstance);
    PBR(const PBR& rhs) = delete;
    PBR& operator=(const PBR& rhs) = delete;
    ~PBR();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	virtual void CreateRtvAndDsvDescriptorHeaps()override;

    void OnKeyboardInput(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialBuffer(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void LoadTextures();
    void BuildRootSignature();
	void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<MaterialObj>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<TextureData>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::unique_ptr<TextureData> mCubeTexture;

	std::unique_ptr<DiffuseCubeMap> mDiffuseLight;
	std::unique_ptr<PreFilteredCubeMap> mPrefilteredMap;
	std::unique_ptr<LUTMap> mLUTMap;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
 
	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

    PassConstants mMainPassCB;

	Camera mCamera;

    POINT mLastMousePos;
};

int WINAPI main(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        PBR theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

PBR::PBR(HINSTANCE hInstance) : D3DApp(hInstance)
{
}

PBR::~PBR()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool PBR::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mCamera.SetPosition(0.0f, 0.0f, -3.0f);

	LoadTextures();
    BuildRootSignature();
	BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
	BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();


    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	mDiffuseLight->BakeTexture(mCommandList.Get());
    ThrowIfFailed(mCommandList->Close());
	cmdsLists[0] = mCommandList.Get();
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	mPrefilteredMap->BakeTexture(mCommandList.Get());
    ThrowIfFailed(mCommandList->Close());
	cmdsLists[0] = mCommandList.Get();
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	mLUTMap->BakeTexture(mCommandList.Get());
    ThrowIfFailed(mCommandList->Close());
	cmdsLists[0] = mCommandList.Get();
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void PBR::OnResize()
{
    D3DApp::OnResize();

	mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void PBR::CreateRtvAndDsvDescriptorHeaps() {

	// +6 for IBL CubeMapping
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 6;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &rtvHeapDesc, IID_PPV_ARGS(mRtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(
        &dsvHeapDesc, IID_PPV_ARGS(mDsvHeap.GetAddressOf())));
}

void PBR::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, NULL, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialBuffer(gt);
	UpdateMainPassCB(gt);
}

void PBR::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Gray, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	// Bind all the materials used in this scene.  For structured buffers, we can bypass the heap and 
	// set as a root descriptor.
	auto matBuffer = mCurrFrameResource->MaterialBuffer->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(2, matBuffer->GetGPUVirtualAddress());

	// Bind all the textures used in this scene.  Observe
    // that we only have to specify the first descriptor in the table.  
    // The root signature knows how many descriptors are expected in the table.
	mCommandList->SetGraphicsRootDescriptorTable(3, mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	// Bind sky texture
	CD3DX12_GPU_DESCRIPTOR_HANDLE skyHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	skyHandle.Offset(mPrefilteredMap->srvHeapIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(4, skyHandle);

	// Bind irradiance texture
	CD3DX12_GPU_DESCRIPTOR_HANDLE irradianceHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	irradianceHandle.Offset(mDiffuseLight->srvHeapIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(5, irradianceHandle);

	// Bind prefilteredMap texture
	CD3DX12_GPU_DESCRIPTOR_HANDLE prefilteredHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	prefilteredHandle.Offset(mPrefilteredMap->srvHeapIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(6, prefilteredHandle);

	// Bind LUT map texture
	CD3DX12_GPU_DESCRIPTOR_HANDLE lutHandle(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	lutHandle.Offset(mLUTMap->srvHeapIndex, mCbvSrvDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(7, lutHandle);

    DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[( int )RenderLayer::Sky]);

	// Draw a texture on the quad to check the baking result
//	mCommandList->SetPipelineState(mPSOs["debug"].Get());
//	mCommandList->IASetVertexBuffers(0, 0, nullptr);
//	mCommandList->IASetIndexBuffer(nullptr);
//	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
//	mCommandList->DrawInstanced(6, 1, 0, 0);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void PBR::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void PBR::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void PBR::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void PBR::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if(GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f*dt);

	if(GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f*dt);

	if(GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f*dt);

	if(GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f*dt);

	mCamera.UpdateViewMatrix();
}
 
void PBR::AnimateMaterials(const GameTimer& gt)
{
	
}

void PBR::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			objConstants.MaterialIndex = e->Mat->MatCBIndex;

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void PBR::UpdateMaterialBuffer(const GameTimer& gt)
{
	auto currMaterialBuffer = mCurrFrameResource->MaterialBuffer.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		MaterialObj* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			MaterialData matData;
			matData.AlbedoMapIndex = mat->AlbedoTex->srvHeapIndex;
			matData.MetallicMapIndex = mat->MetallicTex->srvHeapIndex;
			matData.RoughnessMapIndex = mat->RoughnessTex->srvHeapIndex;
			matData.AOMapIndex = mat->AOTex->srvHeapIndex;
			matData.NormalMapIndex = mat->NormalTex->srvHeapIndex;

			matData.albedo = mat->albedo;
			matData.metallic = mat->Metallic;
			matData.roughness = mat->Roughness;
			matData.ao = mat->AO;

			currMaterialBuffer->CopyData(mat->MatCBIndex, matData);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void PBR::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	mMainPassCB.Lights[0].LightPosAndDir = XMFLOAT3(-10, 10, 10);
	mMainPassCB.Lights[1].LightPosAndDir = XMFLOAT3(10, 10, 10);
	mMainPassCB.Lights[2].LightPosAndDir = XMFLOAT3(-10, -10, 10);
	mMainPassCB.Lights[3].LightPosAndDir = XMFLOAT3(10, -10, 10);

	mMainPassCB.Lights[0].LightColor = XMFLOAT3(300, 300, 300);
	mMainPassCB.Lights[1].LightColor = XMFLOAT3(300, 300, 300);
	mMainPassCB.Lights[2].LightColor = XMFLOAT3(300, 300, 300);
	mMainPassCB.Lights[3].LightColor = XMFLOAT3(300, 300, 300);
	
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void PBR::LoadTextures()
{
	std::vector<std::string> texNames = 
	{
		"default",
		"defaultNormal",
		"rusted_iron_albedo",
		"rusted_iron_ao",
		"rusted_iron_metallic",
		"rusted_iron_roughness",
		"rusted_iron_normal",
		"plastic_albedo",
		"plastic_ao",
		"plastic_metallic",
		"plastic_roughness",
		"plastic_normal",
	};

	std::vector<bool> isSrgb = {
		false,
		false,
		// rusted iron
		true,
		false,
		false,
		false,
		false,
		// gold
		true,
		false,
		false,
		false,
		false,
	};
	
	std::vector<std::wstring> texFilenames = 
	{
		L"../Textures/white1x1.dds",
		L"../Textures/default_nmap.dds",
		L"../textures-nondds/pbr/rusted_iron/albedo.png",
		L"../textures-nondds/pbr/rusted_iron/ao.png",
		L"../textures-nondds/pbr/rusted_iron/metallic.png",
		L"../textures-nondds/pbr/rusted_iron/roughness.png",
		L"../textures-nondds/pbr/rusted_iron/normal.png",
		L"../textures-nondds/pbr/gold/albedo.png",
		L"../textures-nondds/pbr/gold/ao.png",
		L"../textures-nondds/pbr/gold/metallic.png",
		L"../textures-nondds/pbr/gold/roughness.png",
		L"../textures-nondds/pbr/gold/normal.png",
	};

	std::vector<bool> isDDS = {
		true,
		true,
		// rusted iron
		false,
		false,
		false,
		false,
		false,
		// gold
		false,
		false,
		false,
		false,
		false,
	};

	ResourceUploadBatch resUpload(md3dDevice.Get());
	resUpload.Begin();
	
	for(int i = 0; i < (int)texNames.size(); ++i)
	{
		auto texMap = std::make_unique<TextureData>();
		texMap->Name = texNames[i];
		texMap->FileName = texFilenames[i];
		texMap->isDDS = isDDS[i];
		texMap->srvHeapIndex = i;

		if (texMap->isDDS) {
			ThrowIfFailed(CreateDDSTextureFromFile(
				md3dDevice.Get(),
				resUpload,
				texMap->FileName.c_str(),
				texMap->Resource.ReleaseAndGetAddressOf(),
				true
			));
		}
		else {
			ThrowIfFailed(CreateWICTextureFromFileEx(
				md3dDevice.Get(),
				resUpload,
				texMap->FileName.c_str(),
				0,
				D3D12_RESOURCE_FLAG_NONE,
				WIC_LOADER_MIP_AUTOGEN | (isSrgb[i] ? WIC_LOADER_FORCE_SRGB : WIC_LOADER_DEFAULT),
				texMap->Resource.ReleaseAndGetAddressOf()
			));
		}

		mTextures[texMap->Name] = std::move(texMap);
	}
	UINT srvIndex = texNames.size();
	mCubeTexture = std::make_unique<TextureData>();
	mCubeTexture->FileName = L"../Textures/Cubemap_LancellottiChapel.dds";
	mCubeTexture->isDDS = true;
	mCubeTexture->srvHeapIndex = srvIndex++;
	ThrowIfFailed(CreateDDSTextureFromFileEx(
		md3dDevice.Get(),
		resUpload,
		mCubeTexture->FileName.c_str(),
		0,
		D3D12_RESOURCE_FLAG_NONE,
		DDS_LOADER_FORCE_SRGB | DDS_LOADER_MIP_AUTOGEN,
		mCubeTexture->Resource.ReleaseAndGetAddressOf()
	));
	auto uploadResourceFinished = resUpload.End(mCommandQueue.Get());
	uploadResourceFinished.wait();

	mDiffuseLight = std::make_unique<DiffuseCubeMap>(md3dDevice.Get(), mCubeTexture->Resource.Get(), IBLMapSize, IBLMapSize);
	mDiffuseLight->srvHeapIndex = srvIndex++;
	mDiffuseLight->Initialize();

	mPrefilteredMap = std::make_unique<PreFilteredCubeMap>(md3dDevice.Get(), mCubeTexture->Resource.Get(), IBLMapSize, IBLMapSize);
	mPrefilteredMap->srvHeapIndex = srvIndex++;
	mPrefilteredMap->Initialize();

	mLUTMap = std::make_unique<LUTMap>(md3dDevice.Get(), IBLMapSize, IBLMapSize);
	mLUTMap->srvHeapIndex = srvIndex++;
	mLUTMap->Initialize();

}

void PBR::BuildRootSignature()
{

	CD3DX12_DESCRIPTOR_RANGE texTable1;
	texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 20, 4);

	CD3DX12_DESCRIPTOR_RANGE skyTable;
	skyTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE irradianceTable;
	irradianceTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

	CD3DX12_DESCRIPTOR_RANGE prefilteredTable;
	prefilteredTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);

	CD3DX12_DESCRIPTOR_RANGE lutTable;
	lutTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[8];

	// Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsConstantBufferView(0); // cbPerObject
    slotRootParameter[1].InitAsConstantBufferView(1); // cbPass
    slotRootParameter[2].InitAsShaderResourceView(0, 1); // gMaterialData
	slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL); // TextureMaps
	slotRootParameter[4].InitAsDescriptorTable(1, &skyTable, D3D12_SHADER_VISIBILITY_PIXEL); // CubeMap
	slotRootParameter[5].InitAsDescriptorTable(1, &irradianceTable, D3D12_SHADER_VISIBILITY_PIXEL); // irradianceMap
	slotRootParameter[6].InitAsDescriptorTable(1, &prefilteredTable, D3D12_SHADER_VISIBILITY_PIXEL); // prefilteredMap
	slotRootParameter[7].InitAsDescriptorTable(1, &lutTable, D3D12_SHADER_VISIBILITY_PIXEL);      // LUTMap

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(_countof(slotRootParameter), slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
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
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void PBR::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 25;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for(auto &t: mTextures)
	{
		auto tex = t.second.get();
		srvDesc.Format = tex->Resource->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->Resource->GetDesc().MipLevels;

		CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			tex->srvHeapIndex,
			mCbvSrvUavDescriptorSize
		);

		md3dDevice->CreateShaderResourceView(tex->Resource.Get(), &srvDesc, hDescriptor);
	}

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = mCubeTexture->Resource->GetDesc().MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = mCubeTexture->Resource->GetDesc().Format;
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(
			mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
			mCubeTexture->srvHeapIndex,
			mCbvSrvUavDescriptorSize
	);
	md3dDevice->CreateShaderResourceView(mCubeTexture->Resource.Get(), &srvDesc, hDescriptor);

	ID3D12Resource* DiffuseLightResource = mDiffuseLight->Resource();
	srvDesc.TextureCube.MipLevels = DiffuseLightResource->GetDesc().MipLevels;
	srvDesc.Format = DiffuseLightResource->GetDesc().Format;
	hDescriptor.Offset(mCbvSrvDescriptorSize);
	md3dDevice->CreateShaderResourceView(DiffuseLightResource, &srvDesc, hDescriptor);

	ID3D12Resource* PrefilteredReource = mPrefilteredMap->Resource();
	srvDesc.TextureCube.MipLevels = PrefilteredReource->GetDesc().MipLevels;
	srvDesc.Format = PrefilteredReource->GetDesc().Format;
	hDescriptor.Offset(mCbvSrvDescriptorSize);
	md3dDevice->CreateShaderResourceView(PrefilteredReource, &srvDesc, hDescriptor);

	ID3D12Resource* LUTMapResource = mLUTMap->Resource();
	srvDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = LUTMapResource->GetDesc().MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	hDescriptor.Offset(mCbvSrvDescriptorSize);
	md3dDevice->CreateShaderResourceView(LUTMapResource, &srvDesc, hDescriptor);
}

void PBR::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\PBR.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\PBR.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["skyVS"] = d3dUtil::CompileShader(L"Shaders\\sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = d3dUtil::CompileShader(L"Shaders\\sky.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["debugVS"] = d3dUtil::CompileShader(L"Shaders\\watchTexture.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["debugPS"] = d3dUtil::CompileShader(L"Shaders\\watchTexture.hlsl", nullptr, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void PBR::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 30, 30);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for(size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
		vertices[k].TangentU = box.Vertices[i].TangentU;
	}

	for(size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
		vertices[k].TangentU = grid.Vertices[i].TangentU;
	}

	for(size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
		vertices[k].TangentU = sphere.Vertices[i].TangentU;
	}

	for(size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
		vertices[k].TangentU = cylinder.Vertices[i].TangentU;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void PBR::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;
	skyPsoDesc.VS = {
		reinterpret_cast< BYTE* >(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPsoDesc.PS = {
		reinterpret_cast< BYTE* >(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPso = opaquePsoDesc;
	debugPso.VS = {
		reinterpret_cast< BYTE* >(mShaders["debugVS"]->GetBufferPointer()),
		mShaders["debugVS"]->GetBufferSize()
	};
	debugPso.PS = {
		reinterpret_cast< BYTE* >(mShaders["debugPS"]->GetBufferPointer()),
		mShaders["debugPS"]->GetBufferSize()
	};
	debugPso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	debugPso.DepthStencilState.DepthEnable = false;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPso, IID_PPV_ARGS(&mPSOs["debug"])));
}

void PBR::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
    }
}

void PBR::BuildMaterials()
{
	int index = 0;
	for (int row = 0; row < 10; row++) {
		for (int col = 0; col < 10; col++) {
			auto Mat = std::make_unique<MaterialObj>();
			Mat->Name = "Mat_" + std::to_string(row) + "_" + std::to_string(col);
			Mat->MatCBIndex = index++;
			Mat->albedo = XMFLOAT3(Colors::Red);
			Mat->AO = 1.0f;
			Mat->Metallic = (float)col / 10;
			Mat->Roughness = (1 - 0.05) * ((float)row / 10) + 0.05;

			Mat->AlbedoTex = mTextures["default"].get();
			Mat->MetallicTex = mTextures["default"].get();
			Mat->AOTex = mTextures["default"].get();
			Mat->RoughnessTex = mTextures["default"].get();
			Mat->NormalTex = mTextures["defaultNormal"].get();
			mMaterials[Mat->Name] = std::move(Mat);
		}
	}

	auto rusted_iron = std::make_unique<MaterialObj>();
	rusted_iron->Name = "rusted_iron";
	rusted_iron->MatCBIndex = index++;
	rusted_iron->albedo = XMFLOAT3(1, 1, 1);
	rusted_iron->AO = 1.0f;
	rusted_iron->Metallic = 1.0f;
	rusted_iron->Roughness = 1.0f;
	rusted_iron->AlbedoTex = mTextures["rusted_iron_albedo"].get();
	rusted_iron->MetallicTex = mTextures["rusted_iron_metallic"].get();
	rusted_iron->AOTex = mTextures["rusted_iron_ao"].get();
	rusted_iron->RoughnessTex = mTextures["rusted_iron_roughness"].get();
	rusted_iron->NormalTex = mTextures["rusted_iron_normal"].get();
	mMaterials[rusted_iron->Name] = std::move(rusted_iron);

	auto plastic = std::make_unique<MaterialObj>();
	plastic->Name = "plastic";
	plastic->MatCBIndex = index++;
	plastic->albedo = XMFLOAT3(1, 1, 1);
	plastic->AO = 1.0f;
	plastic->Metallic = 1.0f;
	plastic->Roughness = 1.0f;
	plastic->AlbedoTex = mTextures["plastic_albedo"].get();
	plastic->MetallicTex = mTextures["plastic_metallic"].get();
	plastic->AOTex = mTextures["plastic_ao"].get();
	plastic->RoughnessTex = mTextures["plastic_roughness"].get();
	plastic->NormalTex = mTextures["plastic_normal"].get();
	mMaterials[plastic->Name] = std::move(plastic);
}

void PBR::BuildRenderItems()
{
	int index = 0;
	XMMATRIX scaling = XMMatrixScaling(2, 2, 2);
	for (int row = 0; row < 10; row++) {
		for (int col = 0; col < 10; col++) {
			auto ball = std::make_unique<RenderItem>();
			XMMATRIX translate = XMMatrixTranslation((col - 5) * 3, (row - 5) * 3, 0);
			XMStoreFloat4x4(&ball->World, scaling * translate);
			ball->TexTransform = MathHelper::Identity4x4();
			ball->ObjCBIndex = index++;
			ball->Mat = mMaterials["Mat_" + std::to_string(row) + "_" + std::to_string(col)].get();
			ball->Geo = mGeometries["shapeGeo"].get();
			ball->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			ball->IndexCount = ball->Geo->DrawArgs["sphere"].IndexCount;
			ball->StartIndexLocation = ball->Geo->DrawArgs["sphere"].StartIndexLocation;
			ball->BaseVertexLocation = ball->Geo->DrawArgs["sphere"].BaseVertexLocation;

			mRitemLayer[(int)RenderLayer::Opaque].push_back(ball.get());
			mAllRitems.push_back(std::move(ball));
		}
	}

	auto ball = std::make_unique<RenderItem>();
	XMMATRIX translate = XMMatrixTranslation(0, 0, 5);
	XMStoreFloat4x4(&ball->World, scaling * translate);
	ball->TexTransform = MathHelper::Identity4x4();
	ball->ObjCBIndex = index++;
	ball->Mat = mMaterials["rusted_iron"].get();
	ball->Geo = mGeometries["shapeGeo"].get();
	ball->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ball->IndexCount = ball->Geo->DrawArgs["sphere"].IndexCount;
	ball->StartIndexLocation = ball->Geo->DrawArgs["sphere"].StartIndexLocation;
	ball->BaseVertexLocation = ball->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(ball.get());
	mAllRitems.push_back(std::move(ball));

	ball = std::make_unique<RenderItem>();
	translate = XMMatrixTranslation(0, 5, 5);
	XMStoreFloat4x4(&ball->World, scaling * translate);
	ball->TexTransform = MathHelper::Identity4x4();
	ball->ObjCBIndex = index++;
	ball->Mat = mMaterials["plastic"].get();
	ball->Geo = mGeometries["shapeGeo"].get();
	ball->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	ball->IndexCount = ball->Geo->DrawArgs["sphere"].IndexCount;
	ball->StartIndexLocation = ball->Geo->DrawArgs["sphere"].StartIndexLocation;
	ball->BaseVertexLocation = ball->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(ball.get());
	mAllRitems.push_back(std::move(ball));

	auto sky = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&sky->World, XMMatrixScaling(5000, 5000, 5000));
	sky->TexTransform = MathHelper::Identity4x4();
	sky->ObjCBIndex = index++;
	sky->Mat = mMaterials["plastic"].get();
	sky->Geo = mGeometries["shapeGeo"].get();
	sky->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sky->IndexCount = sky->Geo->DrawArgs["sphere"].IndexCount;
	sky->StartIndexLocation = sky->Geo->DrawArgs["sphere"].StartIndexLocation;
	sky->BaseVertexLocation = sky->Geo->DrawArgs["sphere"].BaseVertexLocation;

	mRitemLayer[( int )RenderLayer::Sky].push_back(sky.get());
	mAllRitems.push_back(std::move(sky));
}

void PBR::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;

		cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> PBR::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}
