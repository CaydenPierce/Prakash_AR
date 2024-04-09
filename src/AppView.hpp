// Copyright 2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <imgui.h>
#include <GL/glew.h>

#include "Globals.hpp"
#include "UI.hpp"

#include "MultiGfxContext.hpp"
#include "AppLogic.hpp"

//! Application view class
class AppView
{
public:
    //! UI specific state
    struct UIState {
        int postProcessShaderSourceIndex = 1;  //!< Shader source combo box index
        int graphicsApiIndex = 0;              //!< Graphics API combo box index
        int textureTypeIndex = 0;              //!< Test texture type index
        bool anyItemActive = false;            //!< True if any UI item active. Ignore keys then.
        bool testPresets = false;              //!< Test presets in use flag
    };

public:
    //! Constructor
    AppView(AppLogic& logic);

    //! Destructor
    ~AppView();

    // Disable copy and assign
    AppView(const AppView& other) = delete;
    AppView(const AppView&& other) = delete;
    AppView& operator=(const AppView& other) = delete;
    AppView& operator=(const AppView&& other) = delete;

    //! Initialize application
    bool init();

    //! Application main loop
    void run();

private:
    //! UI frame callback
    bool onFrame(VarjoExamples::UI& ui);

    //! UI key press callback
    void onKeyPress(VarjoExamples::UI& ui, int keyCode);

    //! Updates UI based on logic state and writes changes back to it.
    void updateUI();

private:
    AppLogic& m_logic;                           //!< App logic instance
    std::unique_ptr<VarjoExamples::UI> m_ui;     //!< User interface wrapper
    std::unique_ptr<MultiGfxContext> m_context;  //!< Graphics contexts
    UIState m_uiState{};                         //!< UI specific states
};
