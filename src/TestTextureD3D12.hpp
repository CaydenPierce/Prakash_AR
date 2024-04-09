#pragma once

#include <glm/glm.hpp>
#include <array>

#include "Globals.hpp"
#include "TestTexture.hpp"
#include "MultiGfxContext.hpp"

//! Example of providing texture input from OpenGL
class TestTextureD3D12 : public TestTexture
{
public:
    //! Constructor
    TestTextureD3D12(Type testType, varjo_TextureFormat varjoFormat, const glm::ivec2& size, ComPtr<ID3D12CommandQueue> commandQueue, DXGI_FORMAT dxgiFormat);

    //! Destructor
    ~TestTextureD3D12();

    //! Update texture data to given varjo texture
    void update(const varjo_Texture& varjoTexture, bool useGPU) override;

private:
    //! TODO: Generate noise texture on GPU
    // void generateOnGPU(ID3D12Resource* dstTexture);

    //! Generate noise texture on CPU
    void generateOnCPU(ID3D12Resource* dstTexture);

    //! Wait for previous frame to finish
    void waitForPreviousFrame();

private:
    ComPtr<ID3D12Device> m_device;                                      //!< D3D12 device
    ComPtr<ID3D12CommandQueue> m_commandQueue;                          //!< D3D12 command queue
    std::array<ComPtr<ID3D12CommandAllocator>, 2> m_commandAllocators;  //!< D3D12 command allocator
    ComPtr<ID3D12GraphicsCommandList> m_commandList;                    //!< D3D12 command list
    ComPtr<ID3D12Fence> m_fence;                                        //!< D3D12 frame fence
    HANDLE m_fenceEvent = 0;                                            //!< Fence event handle
    uint64_t m_fenceValue = 0;                                          //!< Fence value
    ComPtr<ID3D12Resource> m_uploadBuffer;                              //!< D3D12 upload buffer
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_uploadFootprint;               //!< D3D12 upload mapping footprint
    uint8_t* m_pDataBegin = nullptr;                                    //!< Starting position of upload buffer
    uint8_t* m_pDataCur = nullptr;                                      //!< Current position of upload buffer
    uint8_t* m_pDataEnd = nullptr;                                      //!< Ending position of upload buffer
};
