// Copyright 2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <GL/glew.h>

#include "Globals.hpp"
#include "Renderer.hpp"
#include "MultiLayerView.hpp"
#include "HeadlessView.hpp"
#include "Scene.hpp"

#include "AppState.hpp"
#include "PostProcess.hpp"
#include "MultiGfxContext.hpp"
#include "TestTexture.hpp"

//! Application logic class
class AppLogic
{
public:
    //! Constructor
    AppLogic() = default;

    //! Destruictor
    ~AppLogic();

    // Disable copy and assign
    AppLogic(const AppLogic& other) = delete;
    AppLogic(const AppLogic&& other) = delete;
    AppLogic& operator=(const AppLogic& other) = delete;
    AppLogic& operator=(const AppLogic&& other) = delete;

    //! Initialize application
    bool init(MultiGfxContext& context);

    //! Check for Varjo API events
    void checkEvents();

    //! Update application state
    void setState(const AppState& state, bool force);

    //! Returns application state
    const AppState& getState() const { return m_appState; }

    //! Update application
    void update();

private:
    //! Enable/disable VST rendering
    void setVSTRendering(bool enabled);

    //! Load post processing
    bool loadPostProcessing(
        VarjoExamples::PostProcess::ShaderSource shaderSource, VarjoExamples::PostProcess::GraphicsAPI graphicsAPI, TestTexture::Type textureType);

    //! Update post processing
    void updatePostProcessing();

private:
    //! Handle mixed reality availablity
    void onMixedRealityAvailable(bool available, bool forceSetState);

private:
    varjo_Session* m_session = nullptr;  //!< Varjo session

#if (!USE_HEADLESS_MODE)
    std::unique_ptr<VarjoExamples::Renderer> m_renderer;         //!< Renderer instance
    std::unique_ptr<VarjoExamples::MultiLayerView> m_varjoView;  //!< Varjo layer view instance
    std::unique_ptr<VarjoExamples::Scene> m_scene;               //!< Application scene instance
#else
    std::unique_ptr<VarjoExamples::HeadlessView> m_varjoView;  //!< Varjo headless view instance
#endif

    std::unique_ptr<VarjoExamples::PostProcess> m_postProcess;  //!< VST post processor
    std::unique_ptr<TestTexture> m_texture;                     //!< Test texture instance
    AppState m_appState;                                        //!< Application state
};
