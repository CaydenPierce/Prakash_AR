#pragma once

#include <d3d11_1.h>

#include "Globals.hpp"
#include "TestTexture.hpp"

class TestTextureD3D11 : public TestTexture
{
public:
    //! Constructor
    TestTextureD3D11(Type testType, varjo_TextureFormat varjoFormat, const glm::ivec2& size, ComPtr<ID3D11Device>& d3dDevice, DXGI_FORMAT format);

    //! Destructor
    ~TestTextureD3D11();

    //! Update texture data to given varjo texture
    void update(const varjo_Texture& varjoTexture, bool useGPU) override;

private:
    //! Generate noise texture on GPU
    void generateOnGPU(ID3D11Texture2D* dstTexture);

    //! Generate noise texture on CPU
    void generateOnCPU(ID3D11Texture2D* dstTexture);

private:
    ComPtr<ID3D11Device> m_d3dDevice;               //!< D3D11 device
    ComPtr<ID3D11DeviceContext> m_d3dContext;       //!< D3D11 context
    ComPtr<ID3D11Texture2D> m_textureCPU;           //!< generated texture (CPU mode)
    ComPtr<ID3D11ComputeShader> m_generateShader;   //!< Generating compute shader (GPU mode)
    DXGI_FORMAT m_uavFormat = DXGI_FORMAT_UNKNOWN;  //!< UAV format (GPU mode)
};
