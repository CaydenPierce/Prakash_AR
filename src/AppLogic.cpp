// Copyright 2020 Varjo Technologies Oy. All rights reserved.

#include "AppLogic.hpp"

#include <cstdio>
#include <vector>
#include <string>

#include <Varjo.h>
#include <Varjo_events.h>
#include <Varjo_mr.h>
#include <Varjo_mr_experimental.h>
#include <Varjo_gl.h>

#include "D3D11Renderer.hpp"
#include "D3D11MultiLayerView.hpp"

#include "Shaders.hpp"
#include "TestScene.hpp"

#include "TestTextureGL.hpp"
#include "TestTextureD3D11.hpp"
#include "TestTextureD3D12.hpp"

// VarjoExamples namespace contains simple example wrappers for using Varjo API features.
// These are only meant to be used in SDK example applications. In your own application,
// use your own production quality integration layer.
using namespace VarjoExamples;

namespace
{
// Varjo application priority to force this as background behind other apps.
// The default value is 0, so we go way less than that.
constexpr int32_t c_appOrderBg = -1000;

}  // namespace

//---------------------------------------------------------------------------

AppLogic::~AppLogic()
{
    // Free post processor
    m_postProcess.reset();

#if (!USE_HEADLESS_MODE)

    // Free scene resources
    m_scene.reset();

#endif

    // Free view resources
    m_varjoView.reset();

#if (!USE_HEADLESS_MODE)

    // Free renderer resources
    m_renderer.reset();

#endif

    // Shutdown the varjo session. Can't check errors anymore after this.
    LOG_DEBUG("Shutting down Varjo session..");
    varjo_SessionShutDown(m_session);
    m_session = nullptr;
}

bool AppLogic::init(MultiGfxContext& context)
{
    // Initialize the varjo session
    LOG_DEBUG("Initializing Varjo session..");
    m_session = varjo_SessionInit();
    if (CHECK_VARJO_ERR(m_session) != varjo_NoError) {
        LOG_ERROR("Creating Varjo session failed.");
        return false;
    }

    // Create video post process instance
    m_postProcess = std::make_unique<PostProcess>(m_session);

    // NOTICE! In this example we always do VR scene rendering using the D3D11 graphics API.
    //
    // Still, we want to showcase video-see-through post processing API with D3D11, OpenGL and
    // D3D12 input textures. This is why we initialize all graphics contexts and support them
    // all in our example classes.
    //
    // In your own application, implement video post processing only for the same graphics
    // context you are using in your application for rendering.

    {
#if (!USE_HEADLESS_MODE)

        // Get graphics adapter used by Varjo session
        auto dxgiAdapter = D3D11MultiLayerView::getAdapter(m_session);

        // Init graphics
        context.init(dxgiAdapter.Get());

        // Create D3D11 renderer instance.
        auto d3d11Renderer = std::make_unique<D3D11Renderer>(dxgiAdapter.Get());

        // Set post process D3D11 support
        m_postProcess->initD3D11(d3d11Renderer->getD3DDevice());

        // Set post process D3D12 support
        m_postProcess->initD3D12(context.getD3D12CommandQueue());

        // Create varjo layer view
        m_varjoView = std::make_unique<D3D11MultiLayerView>(m_session, *d3d11Renderer);

        // Create scene instance
        m_scene = std::make_unique<TestScene>(*d3d11Renderer);

        // Store as generic renderer
        m_renderer = std::move(d3d11Renderer);

#else

        // Get graphics adapter used by Varjo session
        auto dxgiAdapter = D3D11LayerView::getAdapter(m_session);

        // Init graphics
        context.init(dxgiAdapter.Get());

        // Set post process D3D11 support
        m_postProcess->initD3D11(context.getD3D11Device());

        // Set post process D3D12 support
        m_postProcess->initD3D12(context.getD3D12CommandQueue());

        // Set application Z order so that this application is on background behind other apps. Only
        // used if application runs in headless mode not intended to render anything by itself.
        varjo_SessionSetPriority(m_session, c_appOrderBg);
        CHECK_VARJO_ERR(m_session);

        // Create varjo headless view
        m_varjoView = std::make_unique<HeadlessView>(m_session);
#endif
    }

    // Check if Mixed Reality features are available.
    varjo_Bool mixedRealityAvailable = varjo_False;
    varjo_SyncProperties(m_session);
    CHECK_VARJO_ERR(m_session);
    if (varjo_HasProperty(m_session, varjo_PropertyKey_MRAvailable)) {
        mixedRealityAvailable = varjo_GetPropertyBool(m_session, varjo_PropertyKey_MRAvailable);
    }

    // Handle mixed reality availability
    onMixedRealityAvailable(mixedRealityAvailable == varjo_True, false);

    return true;
}

void AppLogic::setVSTRendering(bool enabled)
{
    varjo_MRSetVideoRender(m_session, enabled ? varjo_True : varjo_False);
    if (CHECK_VARJO_ERR(m_session) == varjo_NoError) {
        LOG_INFO("VST rendering: %s", enabled ? "ON" : "OFF");
    }
    m_appState.general.vstEnabled = enabled;
}

void AppLogic::setState(const AppState& state, bool force)
{
    // Store previous state and set new one
    const auto prevState = m_appState;
    m_appState = state;

    // Check for mixed reality availability
    if (!m_appState.general.mrAvailable) {
        // Toggle post process off
        if (force || m_postProcess->isEnabled() || m_postProcess->getShaderSource() != PostProcess::ShaderSource::None) {
            m_postProcess->reset();
            LOG_INFO("Video post process: %s", "OFF");
        }

        // Toggle video rendering off
        if (m_appState.general.vstEnabled) {
            setVSTRendering(false);
        }

        return;
    }

    // Toggle VST post processing
    if (force || state.postProcess.enabled != prevState.postProcess.enabled) {
        LOG_INFO("Video post process: %s", state.postProcess.enabled ? "ON" : "OFF");
        m_postProcess->setEnabled(state.postProcess.enabled);
        m_appState.postProcess.enabled = state.postProcess.enabled;
    }

    // Post processing shader
    if (force || state.postProcess.shaderSource != prevState.postProcess.shaderSource ||  //
        state.postProcess.graphicsAPI != prevState.postProcess.graphicsAPI ||             //
        state.postProcess.textureType != prevState.postProcess.textureType) {
        if (!loadPostProcessing(state.postProcess.shaderSource, state.postProcess.graphicsAPI, state.postProcess.textureType)) {
            LOG_ERROR("Loading post processor failed.");
            m_postProcess->reset();
        }
        m_appState.postProcess.shaderSource = m_postProcess->getShaderSource();
        m_appState.postProcess.graphicsAPI = state.postProcess.graphicsAPI;
        m_appState.postProcess.textureType = state.postProcess.textureType;
    }

    // Render video-see-through
    if (force || state.general.vstEnabled != prevState.general.vstEnabled) {
        setVSTRendering(state.general.vstEnabled);
    }

    // Render VR scene
#if (!USE_HEADLESS_MODE)
    if (force || state.general.vrEnabled != prevState.general.vrEnabled) {
        LOG_INFO("Render VR scene: %s", state.general.vrEnabled ? "ON" : "OFF");
        m_appState.general.vrEnabled = state.general.vrEnabled;
    }
#endif
}

bool AppLogic::loadPostProcessing(PostProcess::ShaderSource shaderSource, PostProcess::GraphicsAPI graphicsAPI, TestTexture::Type textureType)
{
    if (!m_appState.general.mrAvailable) {
        LOG_ERROR("MR not available.");
        return false;
    }

    // Reset noise texture
    m_texture.reset();

    // Load shader
    if (!m_postProcess->loadShader(graphicsAPI, shaderSource, c_postProcessShaderSources.at(shaderSource), c_postProcessShaderParams)) {
        LOG_ERROR("Loading shader failed.");
        m_postProcess->reset();
        return false;
    }

    // Generate a new noise tex and update it to Varjo API.
    if (m_postProcess->getShaderSource() != PostProcess::ShaderSource::None) {
        LOG_DEBUG("Create new post process texture..");

        // Check texture format support
        const auto& texParams = c_postProcessShaderParams.textures[0];
        if (!m_postProcess->checkTextureFormat(graphicsAPI, texParams.format)) {
            LOG_ERROR("Input texture format not supported: %d", static_cast<int>(texParams.format));
            m_postProcess->reset();
            return false;
        }

        // Lock varjo texture for copying
        const int64_t textureIndex = 0;
        varjo_Texture varjoTexture;
        if (!m_postProcess->lockTextureBuffer(textureIndex, varjoTexture)) {
            LOG_ERROR("Getting varjo texture failed.");
            m_postProcess->reset();
            return false;
        }

        try {
            glm::ivec2 texSize(texParams.width, texParams.height);

            switch (m_appState.postProcess.graphicsAPI) {
                case PostProcess::GraphicsAPI::D3D11: {
                    auto d3d11Device = m_postProcess->getD3D11Device();
                    auto dxgiFormat = static_cast<DXGI_FORMAT>(m_postProcess->toDXGIFormat(texParams.format));
                    m_texture = std::make_unique<TestTextureD3D11>(textureType, texParams.format, texSize, d3d11Device, dxgiFormat);
                } break;
                case PostProcess::GraphicsAPI::OpenGL: {
                    auto glFormat = m_postProcess->toGLFormat(texParams.format);
                    m_texture = std::make_unique<TestTextureGL>(textureType, texParams.format, texSize, glFormat.baseFormat, glFormat.internalFormat);
                } break;
                case PostProcess::GraphicsAPI::D3D12: {
                    auto d3d12CommandQueue = m_postProcess->getD3D12CommandQueue();
                    auto dxgiFormat = static_cast<DXGI_FORMAT>(m_postProcess->toDXGIFormat(texParams.format));
                    m_texture = std::make_unique<TestTextureD3D12>(textureType, texParams.format, texSize, d3d12CommandQueue, dxgiFormat);
                } break;
                default: {
                    LOG_ERROR("Unsupported graphics API: %d", m_appState.postProcess.graphicsAPI);
                    return false;
                }
            }

            // Update locked texture on once CPU and GPU just to catch possible errors
            m_texture->update(varjoTexture, false);
            m_texture->update(varjoTexture, true);

        } catch (const std::runtime_error&) {
            LOG_ERROR("Creating test texture failed.");
            m_postProcess->reset();
            return false;
        }

        // Unlock shader input texture
        m_postProcess->unlockTextureBuffer(textureIndex);

        // Release textures
        std::vector<int32_t> updatedIndices;
        updatedIndices.push_back(textureIndex);
        m_postProcess->applyInputBuffers(nullptr, 0, updatedIndices);
    }

    if (m_postProcess->getShaderSource() != PostProcess::ShaderSource::None && m_appState.postProcess.enabled) {
        m_postProcess->setEnabled(true);
    }

    return true;
}

void AppLogic::updatePostProcessing()
{
    // Early exit if not enabled
    if (!m_appState.general.mrAvailable || !m_postProcess->isActive()) {
        return;
    }

    const auto& state = m_appState.postProcess;

    // Set shader constant parameter values
    PostProcessConstantBuffer cBuffer{};
    const double a = state.animAmpl;
    const double o = state.animOffs;
    const double t = state.animTime;

    // Color grade params
    cBuffer.colorFactor = state.colorEnabled ? state.colorFactor * static_cast<float>(o + a * (0.25 * (sin(t * 1.071657) + sin(t * 1.32674)) - 0.5)) : 0.0f;
    memcpy(&cBuffer.colorExp, &state.colorExp, sizeof(cBuffer.colorExp));
    cBuffer.colorExp = glm::vec4(1.0f) / glm::max(glm::vec4(0.01f), (cBuffer.colorExp / state.colorExpScale));
    memcpy(&cBuffer.colorValue, &state.colorValue, sizeof(cBuffer.colorValue));
    cBuffer.colorValue = glm::max(glm::vec4(0.0f), cBuffer.colorValue * state.colorScale);
    cBuffer.colorPreserveSaturated = state.colorPreserveSaturated;

    // Noise texture params
    cBuffer.noiseAmount = state.textureEnabled ? state.textureAmount * static_cast<float>(o + a * (0.25 * (sin(t * 1.158693) + sin(t * 1.51397)) - 0.5)) : 0.0f;
    cBuffer.noiseScale = state.textureScale;

    // Blur filter params
    cBuffer.blurScale = state.blurEnabled ? state.blurScale * static_cast<float>(o + a * (0.25 * (sin(t * 1.013575) + sin(t * 1.26575)) - 0.5)) : 0.0f;
    cBuffer.blurKernelSize = state.blurKernelSize;

    // List of shader input texture indices updated
    std::vector<int32_t> updatedTextures;

    // Update texture if noise enabled
    if (m_texture && m_appState.postProcess.textureEnabled) {
        // Lock varjo texture for copying
        constexpr int64_t textureIndex = 0;
        varjo_Texture varjoTexture;
        if (m_postProcess->lockTextureBuffer(textureIndex, varjoTexture)) {
            try {
                // Update texture
                m_texture->update(varjoTexture, m_appState.postProcess.textureGeneratedOnGPU);
            } catch (const std::runtime_error&) {
                LOG_ERROR("Updating texture failed.");
            }

            // Flag texture that has been updated
            updatedTextures.push_back(textureIndex);

            // Unlock texture
            m_postProcess->unlockTextureBuffer(textureIndex);
        }
    }

    // Update constant buffer
    m_postProcess->applyInputBuffers(reinterpret_cast<char*>(&cBuffer), sizeof(cBuffer), updatedTextures);
}

void AppLogic::update()
{
    // Check for new mixed reality events
    checkEvents();

    // Sync frame
    m_varjoView->syncFrame();

    // Update frame time
    m_appState.general.frameTime += m_varjoView->getDeltaTime();
    m_appState.general.frameCount = m_varjoView->getFrameNumber();

    // Update animation time
    if (m_appState.postProcess.animate) {
        m_appState.postProcess.animTime += m_appState.postProcess.animFreq * m_varjoView->getDeltaTime();
    }

    // Update video post processing if active
    if (m_appState.general.mrAvailable && m_postProcess->isActive()) {
        updatePostProcessing();
    }

#if (!USE_HEADLESS_MODE)

    // Update scene
    m_scene->update(m_varjoView->getFrameTime(), m_varjoView->getDeltaTime(), m_varjoView->getFrameNumber(), Scene::UpdateParams());

    // Begin frame
    m_varjoView->beginFrame();

    // Render layer
    if (m_appState.general.vrEnabled) {
        // Get layer for rendering
        constexpr int layerIndex = 0;
        auto& layer = m_varjoView->getLayer(layerIndex);

        // Setup render params
        MultiLayerView::Layer::SubmitParams submitParams{};
        submitParams.submitColor = m_appState.general.vrEnabled;
        submitParams.submitDepth = true;
        submitParams.depthTestEnabled = false;
        submitParams.depthTestRangeEnabled = false;
        submitParams.depthTestRangeLimits = {0.0f, 0.0f};
        submitParams.chromaKeyEnabled = false;
        submitParams.alphaBlend = true;

        // Begin layer rendering
        layer.begin(submitParams);

        // Clear frame
        layer.clear();

        // Render frame
        layer.renderScene(*m_scene);

        // End layer rendering
        layer.end();
    }

    // End and submit frame
    m_varjoView->endFrame();

#endif
}

void AppLogic::onMixedRealityAvailable(bool available, bool forceSetState)
{
    m_appState.general.mrAvailable = available;

    if (available) {
        // Update stuff here if needed
    } else {
        LOG_ERROR("Mixed Reality features not available.");
        // Update stuff here if needed
    }

    // Force set state when MR becomes active
    if (forceSetState) {
        setState(m_appState, true);
    }
}


void AppLogic::checkEvents()
{
    varjo_Bool ret = varjo_False;

    do {
        varjo_Event evt{};
        ret = varjo_PollEvent(m_session, &evt);
        CHECK_VARJO_ERR(m_session);

        if (ret == varjo_True) {
            switch (evt.header.type) {
                case varjo_EventType_MRDeviceStatus: {
                    switch (evt.data.mrDeviceStatus.status) {
                        // Occurs when Mixed Reality features are enabled
                        case varjo_MRDeviceStatus_Connected: {
                            LOG_INFO("EVENT: Mixed reality device status: %s", "Connected");
                            constexpr bool forceSetState = true;
                            onMixedRealityAvailable(true, forceSetState);
                        } break;
                        // Occurs when Mixed Reality features are disabled
                        case varjo_MRDeviceStatus_Disconnected: {
                            LOG_INFO("EVENT: Mixed reality device status: %s", "Disconnected");
                            constexpr bool forceSetState = false;
                            onMixedRealityAvailable(false, forceSetState);
                        } break;
                        default: {
                            // Ignore unknown status.
                        } break;
                    }
                } break;

                default: {
                    // Ignore unknown event.
                } break;
            }
        }
    } while (ret);
}
