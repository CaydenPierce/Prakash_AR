
set(_app_name "VideoPostProcess")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(_build_output_dir ${CMAKE_BINARY_DIR}/bin)
foreach(OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${_build_output_dir})
endforeach(OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES)

# Application sources
set(_src_dir ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(_sources_app
    ${_src_dir}/main.cpp
    ${_src_dir}/AppLogic.hpp
    ${_src_dir}/AppLogic.cpp
    ${_src_dir}/AppState.hpp
    ${_src_dir}/AppView.hpp
    ${_src_dir}/AppView.cpp
    ${_src_dir}/MultiGfxContext.hpp
    ${_src_dir}/MultiGfxContext.cpp
    ${_src_dir}/TestTexture.hpp
    ${_src_dir}/TestTexture.cpp
    ${_src_dir}/TestTextureD3D11.hpp
    ${_src_dir}/TestTextureD3D11.cpp
    ${_src_dir}/TestTextureD3D12.hpp
    ${_src_dir}/TestTextureD3D12.cpp
    ${_src_dir}/TestTextureGL.hpp
    ${_src_dir}/TestTextureGL.cpp
    ${_src_dir}/TestScene.hpp
    ${_src_dir}/TestScene.cpp
    ${_src_dir}/Shaders.hpp
)

# Application shader sources
set(_src_shaders_dir ${CMAKE_CURRENT_SOURCE_DIR}/res)
set(_sources_shaders
    ${_src_shaders_dir}/vstPostProcess.hlsl
)
set_property(SOURCE ${_sources_shaders} PROPERTY VS_SHADER_TYPE Compute)
set_property(SOURCE ${_sources_shaders} PROPERTY VS_SHADER_MODEL 5.0)
set_property(SOURCE ${_sources_shaders} PROPERTY VS_SHADER_OBJECT_FILE_NAME "${_build_output_dir}/%(Filename).cso")

# Public common sources
set(_src_common_dir ${CMAKE_CURRENT_SOURCE_DIR}/../Common)
set(_sources_common
    ${_src_common_dir}/D3D11MultiLayerView.hpp
    ${_src_common_dir}/D3D11MultiLayerView.cpp
    ${_src_common_dir}/D3D11Renderer.hpp
    ${_src_common_dir}/D3D11Renderer.cpp
    ${_src_common_dir}/D3D11Shaders.hpp
    ${_src_common_dir}/D3D11Shaders.cpp
    ${_src_common_dir}/ExampleShaders.hpp
    ${_src_common_dir}/GfxContext.hpp
    ${_src_common_dir}/GfxContext.cpp
    ${_src_common_dir}/Globals.hpp
    ${_src_common_dir}/Globals.cpp
    ${_src_common_dir}/HeadlessView.hpp
    ${_src_common_dir}/HeadlessView.cpp
    ${_src_common_dir}/MultiLayerView.hpp
    ${_src_common_dir}/MultiLayerView.cpp
    ${_src_common_dir}/Renderer.hpp
    ${_src_common_dir}/Renderer.cpp
    ${_src_common_dir}/Scene.hpp
    ${_src_common_dir}/Scene.cpp
    ${_src_common_dir}/SyncView.hpp
    ${_src_common_dir}/SyncView.cpp
    ${_src_common_dir}/UI.hpp
    ${_src_common_dir}/UI.cpp
)

# Experimental common sources
set(_src_experimental_common_dir ${CMAKE_CURRENT_SOURCE_DIR}/../CommonExperimental)
set(_sources_experimental_common
    ${_src_experimental_common_dir}/PostProcess.hpp
    ${_src_experimental_common_dir}/PostProcess.cpp
)

# Visual studio source groups
#source_group("Application" FILES ${_sources_app})
source_group("Shaders" FILES ${_sources_shaders})
source_group("Common" FILES ${_sources_common})
source_group("CommonExperimental" FILES ${_sources_experimental_common})

# Application exe target
set(_target ${_app_name})
add_executable(${_target}
    ${_sources_app}
    ${_sources_shaders}
    ${_sources_common}
    ${_sources_experimental_common}
)

# Include directories
target_include_directories(${_target}
    PRIVATE ${_src_common_dir}
    PRIVATE ${_src_experimental_common_dir}
)

# VS debugger properties
set_property(TARGET ${_target} PROPERTY FOLDER "Examples")
set_target_properties(${_target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(${_target} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "--console")

# Console / Windows application
if (_build_console_app)
    set_target_properties(${_target} PROPERTIES LINK_FLAGS /SUBSYSTEM:CONSOLE)
else(_build_console_app)
    set_target_properties(${_target} PROPERTIES LINK_FLAGS /SUBSYSTEM:WINDOWS)
endif(_build_console_app)

# Preprocerssor definitions
target_compile_definitions(${_target} PUBLIC -D_UNICODE -DUNICODE -DNOMINMAX)

# Linked libraries
target_link_libraries(${_target}
    PRIVATE ImGui::ImGui
    PRIVATE ImGui::ImGuiBackends
    PRIVATE FreeType::FreeType
    PRIVATE Glew::Glew
    PRIVATE D3DX12::D3DX12
    PRIVATE GLM::GLM
    PRIVATE CxxOpts::CxxOpts
    PRIVATE d3d11
    PRIVATE d3d12
    PRIVATE opengl32
    PRIVATE dxgi
    PRIVATE d3dcompiler
    PRIVATE windowscodecs
    PRIVATE VarjoLib
)

# Copy resource files
add_custom_command(
    TARGET ${_target}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E copy ${_src_shaders_dir}/vstPostProcess.hlsl ${CMAKE_BINARY_DIR}/bin/
)
