
#include "TestTextureD3D11.hpp"

#include <Varjo_d3d11.h>

#include "Shaders.hpp"
#include "D3D11Renderer.hpp"

// VarjoExamples namespace contains simple example wrappers for using Varjo API features.
// These are only meant to be used in SDK example applications. In your own application,
// use your own production quality integration layer.
using namespace VarjoExamples;

namespace
{
// Example compute shader for generating noise texture on GPU
static const char* c_noiseShaderSource = R"(
RWTexture2D<unorm float4> tex : register(u0);

cbuffer ConstantBuffer : register(b0) {
    float4 noiseSeed;
};

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const int2 uv = dispatchThreadID.xy;

    // Generate noise
    const float PHI = 1.61803398874989484820459;
    const float2 xy = float2(uv) + 1.0;
    float4 rgba = frac(tan(noiseSeed * distance(PHI * xy, xy)) * xy.x);
    tex[uv] = rgba;
}
)";

// Example compute shader for generating noise texture on GPU
static const char* c_gradientShaderSource = R"(
RWTexture2D<float4> tex : register(u0);

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    const int2 uv = dispatchThreadID.xy;

    uint2 texSize;
    tex.GetDimensions(texSize.x, texSize.y);

    const float2 ps = uv;
    float2 sz = texSize;

    float4 rgba = 0.0;
    rgba.r = ps.x / sz.x;
    rgba.g = ps.y / sz.y;
    rgba.b = ((sz.x - ps.x) + (sz.y - ps.y)) / (sz.x + sz.y);
    rgba.a = (ps.x + (sz.y - ps.y)) / (sz.x + sz.y);

    tex[uv] = rgba;
}
)";

}  // namespace

TestTextureD3D11::TestTextureD3D11(Type testType, varjo_TextureFormat varjoFormat, const glm::ivec2& size, ComPtr<ID3D11Device>& d3dDevice, DXGI_FORMAT format)
    : TestTexture(testType, varjoFormat, size)
{
    HRESULT hr = 0;

    // Store D3D11 device
    m_d3dDevice = d3dDevice;

    // Get immediate context
    m_d3dDevice->GetImmediateContext(&m_d3dContext);

    // Create CPU mode resources.
    try {
        DXGI_FORMAT texFormat = format;

        CD3D11_TEXTURE2D_DESC desc(texFormat, static_cast<UINT>(size.x), static_cast<UINT>(size.y), 1, 1);
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        if (FAILED(hr = d3dDevice->CreateTexture2D(&desc, nullptr, &m_textureCPU))) {
            CRITICAL("Creating staging texture failed (%d): %s", hr, std::system_category().message(hr).c_str());
        }

        m_cpuSupported = true;
    } catch (const std::exception&) {
        LOG_ERROR("Initializing D3D11 CPU texture support failed.");
        m_cpuSupported = false;
    }

    // Create GPU mode resources.
    try {
        m_uavFormat = format;

        // We need to convert SRGB formats to be able to use them as UAV
        if (format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) {
            m_uavFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        const char* shaderSource = nullptr;

        if (m_testType == Type::Gradient) {
            shaderSource = c_gradientShaderSource;
        } else if (m_testType == Type::Noise) {
            shaderSource = c_noiseShaderSource;
        } else {
            CRITICAL("Unsupported type: %d", m_testType);
        }

        // Compile compute shader for generating texture
        ComPtr<ID3DBlob> shaderBlob = D3D11Renderer::compileShader("postprocess_cs", shaderSource, "cs_5_0");
        if (!shaderBlob) {
            CRITICAL("Compiling compute shader failed.");
        }

        // Create compute shader for generating texture
        m_generateShader = D3D11Renderer::createComputeShader(d3dDevice.Get(), shaderBlob.Get());
        if (!m_generateShader) {
            CRITICAL("Loading compute shader failed.");
        }

        m_gpuSupported = true;
    } catch (const std::exception&) {
        LOG_ERROR("Initializing D3D11 GPU texture support failed.");
        m_cpuSupported = false;
    }
}

TestTextureD3D11::~TestTextureD3D11()
{
    // Free resources
    m_textureCPU.Reset();
    m_generateShader.Reset();
}

void TestTextureD3D11::generateOnGPU(ID3D11Texture2D* dstTexture)
{
    HRESULT hr = 0;

    // Set generate compute shader
    m_d3dContext->CSSetShader(m_generateShader.Get(), nullptr, 0);

    // Generate UAV for texture.
    // We are now re-generating these on each update, but we could also stash these for each swapchain texture.
    ComPtr<ID3D11UnorderedAccessView> textureUAV;
    CD3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc(D3D11_UAV_DIMENSION_TEXTURE2D, m_uavFormat);
    if (FAILED(hr = m_d3dDevice->CreateUnorderedAccessView(dstTexture, &uavDesc, &textureUAV))) {
        m_gpuSupported = false;
        CRITICAL("Creating UAV failed (%d): %s", hr, std::system_category().message(hr).c_str());
    }

    // Set UAV
    std::vector<ID3D11UnorderedAccessView*> uavs;
    uavs.push_back(textureUAV.Get());
    m_d3dContext->CSSetUnorderedAccessViews(0, static_cast<UINT>(uavs.size()), uavs.data(), nullptr);

    // Constant buffer
    // We are now re-generating these on each update, but we could also stash this
    ComPtr<ID3D11Buffer> cb;
    if (m_testType == Type::Noise) {
        glm::vec4 noiseSeeds(rand(), rand(), rand(), rand());

        D3D11_BUFFER_DESC bufferDesc;
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(noiseSeeds);
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bufferDesc.CPUAccessFlags = 0;
        bufferDesc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA dataDesc;
        dataDesc.pSysMem = &noiseSeeds;
        dataDesc.SysMemPitch = sizeof(noiseSeeds);

        if (FAILED(hr = m_d3dDevice->CreateBuffer(&bufferDesc, &dataDesc, &cb))) {
            m_gpuSupported = false;
            CRITICAL("Creating constant buffer failed. err=%d", hr);
        }

        ID3D11Buffer* cbs[] = {cb.Get()};
        m_d3dContext->CSSetConstantBuffers(0, 1, cbs);
    }

    // Dispatch compute shader
    m_d3dContext->Dispatch(16, 16, 1);
}

void TestTextureD3D11::generateOnCPU(ID3D11Texture2D* dstTexture)
{
    HRESULT hr = 0;

    // Map staging texture and generate some noise on CPU
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(hr = m_d3dContext->Map(m_textureCPU.Get(), 0, D3D11_MAP_WRITE, 0, &mapped))) {
        m_cpuSupported = false;
        CRITICAL("Mapping texture for writing failed. err=%d", hr);
    }

    // Generate noise
    if (m_varjoFormat == varjo_TextureFormat_R32_FLOAT) {
        generate(static_cast<float*>(mapped.pData), mapped.RowPitch);
    } else if (m_varjoFormat == varjo_TextureFormat_R32_UINT) {
        // HACK : Our shader is reading floats from texture, so we write the buffer here in float format,
        // even though the texture is uint32. If you use data as uints, you should write it as uint32_t here.
        generate(static_cast<float*>(mapped.pData), mapped.RowPitch);
        // generate(static_cast<uint32_t*>(mapped.pData));
    } else {
        generate(static_cast<uint8_t*>(mapped.pData), mapped.RowPitch);
    }

    // Unmap staging texture
    m_d3dContext->Unmap(m_textureCPU.Get(), 0);

    // Copy texture from CPU staging texture to varjo GPU texture.
    m_d3dContext->CopyResource(dstTexture, m_textureCPU.Get());
}

void TestTextureD3D11::update(const varjo_Texture& varjoTexture, bool useGPU)
{
    ID3D11Texture2D* d3dTexture = nullptr;
    d3dTexture = varjo_ToD3D11Texture(varjoTexture);

    if (d3dTexture) {
        if (useGPU && m_gpuSupported) {
            try {
                // Generate texture on GPU
                generateOnGPU(d3dTexture);
            } catch (const std::runtime_error&) {
                LOG_ERROR("Generating texture on GPU failed.");
                m_gpuSupported = false;
            }
        } else if (!useGPU && m_cpuSupported) {
            try {
                // Generate texture on CPU
                generateOnCPU(d3dTexture);
            } catch (const std::runtime_error&) {
                LOG_ERROR("Generating texture on CPU failed.");
                m_cpuSupported = false;
            }
        } else {
            // Skip generate
        }
    } else {
        LOG_ERROR("Getting D3D11 texture failed.");
    }
}
