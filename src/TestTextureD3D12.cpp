
#include "TestTextureD3D12.hpp"

#include <cstdio>
#include <array>
#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>

#include <Varjo_d3d12.h>

namespace
{
uint64_t align(uint64_t uLocation, uint64_t uAlign)
{
    if ((0 == uAlign) || (uAlign & (uAlign - 1))) {
        CRITICAL("Non-pow2 alignment");
    }
    return ((uLocation + (uAlign - 1)) & ~(uAlign - 1));
}

HRESULT suballocateFromBuffer(uint8_t*& pDataCur, uint8_t*& pDataEnd, uint64_t uSize, uint64_t uAlign)
{
    pDataCur = reinterpret_cast<uint8_t*>(align(reinterpret_cast<uint64_t>(pDataCur), uAlign));
    return (pDataCur + uSize > pDataEnd) ? E_INVALIDARG : S_OK;
}
}  // namespace

TestTextureD3D12::TestTextureD3D12(
    Type testType, varjo_TextureFormat varjoFormat, const glm::ivec2& size, ComPtr<ID3D12CommandQueue> commandQueue, DXGI_FORMAT dxgiFormat)
    : TestTexture(testType, varjoFormat, size)
{
    HRESULT hr = 0;

    // Store D3D12 command queue instance
    m_commandQueue = commandQueue;

    // Get D3D12 device
    hr = m_commandQueue->GetDevice(__uuidof(ID3D12Device), (IID_PPV_ARGS(&m_device)));
    CHECK_HRESULT(hr);

    // Create command allocator instance
    for (size_t i = 0; i < m_commandAllocators.size(); i++) {
        hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i]));
        CHECK_HRESULT(hr);
        hr = m_commandAllocators[i]->SetName(L"TestTexture_CommandAlloc");
        CHECK_HRESULT(hr);
    }

    // Create command list instance
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    CHECK_HRESULT(hr);
    hr = m_commandList->SetName(L"TestTexture_CommandList");
    CHECK_HRESULT(hr);

    // Close command list
    hr = m_commandList->Close();
    CHECK_HRESULT(hr);

    // Create frame fence
    hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    CHECK_HRESULT(hr);
    hr = m_fence->SetName(L"TestTexture_Fence");
    CHECK_HRESULT(hr);
    m_fenceValue = 0;

    // Create an event handle to use for frame synchronization.
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr) {
        CHECK_HRESULT(HRESULT_FROM_WIN32(GetLastError()));
    }

    constexpr int numChannels = 4;

    // Create upload buffer
    auto bufSize = m_size.y * align(m_size.x * numChannels * sizeof(float), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(bufSize);
    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD, 1, 1);
    hr = m_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_uploadBuffer));
    CHECK_HRESULT(hr);
    hr = m_uploadBuffer->SetName(L"TestTexture_UploadBuffer");
    CHECK_HRESULT(hr);

    // Permanently map upload buffer
    {
        void* pData = nullptr;
        CD3DX12_RANGE readRange(0, 0);  // No CPU reads will be done from the resource.
        hr = m_uploadBuffer->Map(0, &readRange, &pData);
        CHECK_HRESULT(hr);

        m_pDataCur = m_pDataBegin = reinterpret_cast<uint8_t*>(pData);
        m_pDataEnd = m_pDataBegin + bufSize;

        D3D12_SUBRESOURCE_FOOTPRINT subresFootprint = {};
        subresFootprint.Format = dxgiFormat;
        subresFootprint.Width = m_size.x;
        subresFootprint.Height = m_size.y;
        subresFootprint.Depth = 1;

        if (m_varjoFormat == varjo_TextureFormat_R32_FLOAT) {
            subresFootprint.RowPitch = static_cast<UINT>(align(m_size.x * numChannels * sizeof(float), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
        } else if (m_varjoFormat == varjo_TextureFormat_R32_UINT) {
            subresFootprint.RowPitch = static_cast<UINT>(align(m_size.x * numChannels * sizeof(uint32_t), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
        } else {
            subresFootprint.RowPitch = static_cast<UINT>(align(m_size.x * numChannels * sizeof(uint8_t), D3D12_TEXTURE_DATA_PITCH_ALIGNMENT));
        }

        hr = suballocateFromBuffer(m_pDataCur, m_pDataEnd, subresFootprint.Height * subresFootprint.RowPitch, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        CHECK_HRESULT(hr);

        m_uploadFootprint = {0};
        m_uploadFootprint.Offset = static_cast<UINT64>(m_pDataCur - m_pDataBegin);
        m_uploadFootprint.Footprint = subresFootprint;

        m_cpuSupported = true;
    }
}

TestTextureD3D12::~TestTextureD3D12()
{
    // Flush frames
    for (size_t i = 0; i < m_commandAllocators.size(); i++) {
        waitForPreviousFrame();
    }

    // Reset command allocators, list and queue
    m_commandAllocators = {};
    m_commandList.Reset();
    m_commandQueue.Reset();

    // Reset upload buffer
    m_uploadBuffer->Unmap(0, NULL);
    m_uploadBuffer.Reset();

    // Reset device
    m_device.Reset();

    // Close fence event handle
    CloseHandle(m_fenceEvent);
}

void TestTextureD3D12::waitForPreviousFrame()
{
    HRESULT hr;

    // Wait until the previous frame is finished.
    auto completedValue = m_fence->GetCompletedValue();
    if (completedValue < m_fenceValue) {
        // LOG_INFO("Wait for fence: %d", m_fenceValue);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Signal fence
    hr = m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    CHECK_HRESULT(hr);
    hr = m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
    CHECK_HRESULT(hr);
    m_fenceValue++;
}

void TestTextureD3D12::generateOnCPU(ID3D12Resource* dstTexture)
{
    HRESULT hr;

    // Wait for previous frame
    waitForPreviousFrame();

    uint8_t* mappedData = m_pDataBegin + m_uploadFootprint.Offset;

    if (m_varjoFormat == varjo_TextureFormat_R32_FLOAT) {
        generate(reinterpret_cast<float*>(mappedData), m_uploadFootprint.Footprint.RowPitch);
    } else if (m_varjoFormat == varjo_TextureFormat_R32_UINT) {
        // HACK : Our shader is reading floats from texture, so we write the buffer here in float format,
        // even though the texture is uint32. If you use data as uints, you should write it as uint32_t here.
        generate(reinterpret_cast<float*>(mappedData), m_uploadFootprint.Footprint.RowPitch);
    } else {
        generate(reinterpret_cast<uint8_t*>(mappedData), m_uploadFootprint.Footprint.RowPitch);
    }

    // Reset command allocator
    hr = m_commandAllocators[m_fenceValue % m_commandAllocators.size()]->Reset();
    CHECK_HRESULT(hr);

    // Reset command list
    hr = m_commandList->Reset(m_commandAllocators[m_fenceValue % m_commandAllocators.size()].Get(), nullptr);
    CHECK_HRESULT(hr);

    // Copy upload buffer to texture
    auto dst = CD3DX12_TEXTURE_COPY_LOCATION(dstTexture, 0);
    auto src = CD3DX12_TEXTURE_COPY_LOCATION(m_uploadBuffer.Get(), m_uploadFootprint);
    m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

    // Close command list
    hr = m_commandList->Close();
    CHECK_HRESULT(hr);

    // Execute commands
    ID3D12CommandList* commandLists[] = {m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists(1, commandLists);
}

void TestTextureD3D12::update(const varjo_Texture& varjoTexture, bool useGPU)
{
    // Get D3D12 texture
    ID3D12Resource* d3d12Texture = varjo_ToD3D12Texture(varjoTexture);

    if (d3d12Texture) {
        if (useGPU && m_gpuSupported) {
            try {
                assert(false);
                // TODO: Generate texture on GPU
                // At the moment we don't have example implementation of GPU rendering on D3D12.
                // However, you should be able to follow D3D11 example and implement one.
                // generateOnGPU(d3d12Texture);
            } catch (const std::runtime_error&) {
                LOG_ERROR("Generating texture on GPU failed.");
                m_gpuSupported = false;
            }
        } else if (!useGPU && m_cpuSupported) {
            try {
                // Generate texture on CPU
                generateOnCPU(d3d12Texture);
            } catch (const std::runtime_error&) {
                LOG_ERROR("Generating texture on CPU failed.");
                m_cpuSupported = false;
            }
        } else {
            // Skip generate
        }
    } else {
        LOG_ERROR("Getting D3D12 texture failed.");
    }
}
