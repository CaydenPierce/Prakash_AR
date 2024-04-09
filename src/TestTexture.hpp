
#pragma once

#include <glm/glm.hpp>

#include "Globals.hpp"

//! Base class for noise texture implementations
class TestTexture
{
public:
    enum class Type { None = 0, Noise, Gradient };

    //! Destructor
    virtual ~TestTexture() = default;

    //! Update texture data to given varjo texture
    virtual void update(const varjo_Texture& varjoTexture, bool useGPU) = 0;

protected:
    //! Protected constructor
    TestTexture(Type testType, varjo_TextureFormat textureFormat, const glm::ivec2& size);

    //! Generate texture to given buffer, row pitch in bytes. Caller must ensure the target has enough space.
    void generate(uint8_t* target, size_t rowPitch);

    //! Generate texture to given buffer, row pitch in bytes.. Caller must ensure the target has enough space.
    void generate(float* target, size_t rowPitch);

    //! Generate texture to given buffer, row pitch in bytes.. Caller must ensure the target has enough space.
    void generate(uint32_t* target, size_t rowPitch);

protected:
    Type m_testType;                    //!< Test texture type
    varjo_TextureFormat m_varjoFormat;  //!< Varjo texture format
    glm::ivec2 m_size;                  //!< Texture size
    int m_numChannels = 0;              //!< Number of channels
    bool m_gpuSupported = false;        //! GPU generate supported
    bool m_cpuSupported = false;        //! CPU generate supported
};
