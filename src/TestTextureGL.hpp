#pragma once

#include <glm/glm.hpp>

#include "MultiGfxContext.hpp"
#include "Globals.hpp"
#include "TestTexture.hpp"

//! Example of providing texture input from OpenGL
class TestTextureGL : public TestTexture
{
public:
    //! Constructor
    TestTextureGL(Type testType, varjo_TextureFormat varjoFormat, const glm::ivec2& size, GLenum baseFormat, GLenum internalFormat);

    //! Destructor
    ~TestTextureGL();

    //! Update texture data to given varjo texture
    void update(const varjo_Texture& varjoTexture, bool useGPU) override;

private:
    //! Generate noise texture on GPU
    void generateOnGPU(GLuint dstTexture);

    //! Generate noise texture on CPU
    void generateOnCPU(GLuint dstTexture);

private:
    GLenum m_baseFormat = 0;             //!< Base texture format
    GLenum m_internalFormat = 0;         //!< Internal texture format
    GLuint m_generateShaderId = 0;       //!< Noise compute shader ID
    GLuint m_generateProgramId = 0;      //!< Noise compute program ID
    bool m_gpuNoiseInitialized = false;  //!< GPU noise texture initialized flag
};
