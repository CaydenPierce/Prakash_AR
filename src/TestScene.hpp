// Copyright 2019-2020 Varjo Technologies Oy. All rights reserved.

#pragma once

#include <memory>
#include <array>
#include <vector>

#include "Globals.hpp"
#include "Renderer.hpp"
#include "Scene.hpp"

//! Simple test scene consisting of grid of cubes and unit vectors in origin
class TestScene : public VarjoExamples::Scene
{
public:
    //! Constant for nominal exposure EV
    static const double c_nominalExposureEV;

    //! Constructor
    TestScene(VarjoExamples::Renderer& renderer);

protected:
    //! Update scene animation
    void onUpdate(double frameTime, double deltaTime, int64_t frameCounter, const UpdateParams& params) override;

    //! Render scene to given view
    void onRender(VarjoExamples::Renderer& renderer, VarjoExamples::Renderer::ColorDepthRenderTarget& target, int viewIndex, const glm::mat4x4& viewMat,
        const glm::mat4x4& projMat, void* userData) const override;

private:
    //! Simple class for storing object data
    struct Object {
        VarjoExamples::ObjectPose pose;              //!< Object pose
        glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};  //!< Object color + alhpa
        float vtxColorFactor = 1.0f;                 //!< Vertex color factor
    };

    std::vector<Object> m_cubes;                                    //!< Cube grid objects
    std::unique_ptr<VarjoExamples::Renderer::Mesh> m_cubeMesh;      //!< Mesh object instance
    std::unique_ptr<VarjoExamples::Renderer::Shader> m_cubeShader;  //!< Cube shader instance

    VarjoExamples::ExampleShaders::LightingData m_lighting;                //!< Scene lighting parameters
    float m_exposureGain = -1.0f;                                          //!< Exposure gain for VR content.
    VarjoExamples::ExampleShaders::WBNormalizationData m_wbNormalization;  //!< Whitebalance normalization data.
};
