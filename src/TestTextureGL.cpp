
#include "TestTextureGL.hpp"

#include <cstdio>
#include <vector>
#include <unordered_map>
#include <Varjo_gl.h>

#define CHECK_GL_ERR()                                                            \
    {                                                                             \
        auto err = glGetError();                                                  \
        if (err != GL_NO_ERROR) {                                                 \
            CRITICAL("GL Error @ %s():%d: code=%d", __FUNCTION__, __LINE__, err); \
        }                                                                         \
    }

namespace
{
// Example compute shader for generating noise texture on GPU
static const char* c_noiseShaderSource = R"(
#version 430

layout(local_size_x = 1, local_size_y = 1) in;
layout(rgba32f, binding = 0) uniform image2D tex;
//layout(r32f, binding = 0) uniform image2D tex;
//layout(r8ui, binding = 0) uniform image2D tex;

uniform vec4 noiseSeed;

void main() {

    // get index in global work group i.e xy position
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);

    // Generate noise
    const float PHI = 1.61803398874989484820459;
    vec2 xy = vec2(uv) + 1.0;
    vec4 rgba = fract(tan(noiseSeed*distance(PHI * xy, xy)) * xy.x);

    // Output to a specific pixel in the image
    imageStore(tex, uv, rgba);
}
)";

// Example compute shader for generating gradient texture on GPU
static const char* c_gradientShaderSource = R"(
#version 430

layout(local_size_x = 1, local_size_y = 1) in;
layout(rgba32f, binding = 0) uniform image2D tex;

void main() {

    // get index in global work group i.e x,y position
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);

    const vec2 ps = uv;
    const vec2 sz = imageSize(tex);

    vec4 rgba = vec4(0.0);
    rgba.r = ps.x / sz.x;
    rgba.g = ps.y / sz.y;
    rgba.b = ((sz.x - ps.x) + (sz.y - ps.y)) / (sz.x + sz.y);
    rgba.a = (ps.x + (sz.y - ps.y)) / (sz.x + sz.y);

    // output to a specific pixel in the image
    imageStore(tex, uv, rgba);
}
)";

bool loadShader(const char* source, GLuint& shaderId, GLuint& programId)
{
    shaderId = programId = 0;

    constexpr int maxLen = 1024;
    int len;
    std::vector<GLchar> errorLog(1024);

    shaderId = glCreateShader(GL_COMPUTE_SHADER);
    CHECK_GL_ERR();
    glShaderSource(shaderId, 1, &source, NULL);
    CHECK_GL_ERR();
    glCompileShader(shaderId);
    CHECK_GL_ERR();
    glGetShaderInfoLog(shaderId, maxLen, &len, errorLog.data());
    CHECK_GL_ERR();
    if (len > 0) {
        CRITICAL("Compiling GL compute shader failed: %s", std::string(errorLog.data()).c_str());
    }

    programId = glCreateProgram();
    CHECK_GL_ERR();
    glAttachShader(programId, shaderId);
    CHECK_GL_ERR();
    glLinkProgram(programId);
    CHECK_GL_ERR();
    glGetProgramInfoLog(programId, maxLen, &len, errorLog.data());
    CHECK_GL_ERR();
    if (len > 0) {
        CRITICAL("Linking GL compute shader failed: %s", std::string(errorLog.data()).c_str());
    }

    return true;
}

}  // namespace

TestTextureGL::TestTextureGL(Type testType, varjo_TextureFormat varjoFormat, const glm::ivec2& size, GLenum baseFormat, GLenum internalFormat)
    : TestTexture(testType, varjoFormat, size)
    , m_baseFormat(baseFormat)
    , m_internalFormat(internalFormat)
{
    // Init CPU generate
    try {
        m_cpuSupported = true;
    } catch (const std::runtime_error&) {
        LOG_ERROR("Initializing GL CPU texture support failed.");
        m_cpuSupported = false;
    }

    // Init GPU generate
    try {
        const char* shaderSource = nullptr;
        if (m_testType == Type::Gradient) {
            shaderSource = c_gradientShaderSource;
        } else if (m_testType == Type::Noise) {
            shaderSource = c_noiseShaderSource;
        } else {
            CRITICAL("Unsupported type: %d", m_testType);
        }

        // Load generate compute shader from source
        if (!loadShader(shaderSource, m_generateShaderId, m_generateProgramId)) {
            CRITICAL("Loading GL noise compute shader failed.");
        }

        m_gpuSupported = true;
    } catch (const std::runtime_error&) {
        LOG_ERROR("Initializing GL GPU texture support failed.");
        m_gpuSupported = false;
    }
}

TestTextureGL::~TestTextureGL()
{
    // Free resources
    glDeleteProgram(m_generateProgramId);
    CHECK_GL_ERR();

    glDeleteShader(m_generateShaderId);
    CHECK_GL_ERR();
}

void TestTextureGL::generateOnGPU(GLuint dstTexture)
{
    // Bind compute shader
    glUseProgram(m_generateProgramId);
    CHECK_GL_ERR();

    auto bindImageFormat = m_internalFormat;

    // NOTICE: Refer to OpenGL Image Load Store specification on texture formats and
    // how they should be bound for compute shaders. It is possible that not all formats
    // provided by Varjo API can be bound for compute shader output.
    // https://www.khronos.org/opengl/wiki/Image_Load_Store

    const std::unordered_map<varjo_TextureFormat, GLenum> bindFormats = {
        {varjo_TextureFormat_R8G8B8A8_UNORM, GL_RGBA8},  // OK!
        {varjo_TextureFormat_R8G8B8A8_SRGB, GL_RGBA8},   // Writing sRGB is not supported!
        {varjo_TextureFormat_R32_UINT, GL_R32UI},        // OK!
        {varjo_TextureFormat_R32_FLOAT, GL_R32F},        // Does not work. Why?
        {varjo_MaskTextureFormat_A8_UNORM, GL_R8},       // Does not work. Why?
    };

    // Get format from conversion table above
    bindImageFormat = bindFormats.at(m_varjoFormat);

    // Bind texture
    glBindImageTexture(0, dstTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, bindImageFormat);
    CHECK_GL_ERR();

    // Set uniforms for noise seed
    if (m_testType == Type::Noise) {
        glm::vec4 seed(rand(), rand(), rand(), rand());
        glUniform4fv(0, 1, &seed.x);
        CHECK_GL_ERR();
    }

    // Dispatch compute shader
    glDispatchCompute((GLuint)m_size.x, (GLuint)m_size.y, 1);
    CHECK_GL_ERR();

    // Make sure everything done
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    CHECK_GL_ERR();
}

void TestTextureGL::generateOnCPU(GLuint dstTexture)
{
    // Active texture unit
    glActiveTexture(GL_TEXTURE0);
    CHECK_GL_ERR();

    // Bind texture
    glBindTexture(GL_TEXTURE_2D, dstTexture);
    CHECK_GL_ERR();

    // Generate CPU noise buffer and update data
    if (m_varjoFormat == varjo_TextureFormat_R32_FLOAT) {
        const GLenum dataType = GL_FLOAT;
        std::vector<float> dataBuffer(m_size.x * m_size.y * m_numChannels);
        generate(dataBuffer.data(), m_size.x * m_numChannels * sizeof(float));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_size.x, m_size.y, m_baseFormat, dataType, dataBuffer.data());
        CHECK_GL_ERR();
    } else if (m_varjoFormat == varjo_TextureFormat_R32_UINT) {
        const GLenum dataType = GL_UNSIGNED_INT;
        // HACK: Our shader is reading floats from texture, so we write the buffer here in float format,
        // even though the texture is uint32. If you use data as uints, you should write it as uint32_t here.
        std::vector<float> dataBuffer(m_size.x * m_size.y * m_numChannels);
        // std::vector<uint32_t> dataBuffer(m_size.x * m_size.y * m_numChannels);
        generate(dataBuffer.data(), m_size.x * m_numChannels * sizeof(float));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_size.x, m_size.y, m_baseFormat, dataType, dataBuffer.data());
        CHECK_GL_ERR();
    } else {
        const GLenum dataType = GL_UNSIGNED_BYTE;
        std::vector<uint8_t> dataBuffer(m_size.x * m_size.y * m_numChannels);
        generate(dataBuffer.data(), m_size.x * m_numChannels * sizeof(uint8_t));
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_size.x, m_size.y, m_baseFormat, dataType, dataBuffer.data());
        CHECK_GL_ERR();
    }
}

void TestTextureGL::update(const varjo_Texture& varjoTexture, bool useGPU)
{
    // Get GL texture id
    GLuint glTexture = varjo_ToGLTexture(varjoTexture);

    if (glTexture) {
        if (useGPU && m_gpuSupported) {
            try {
                // Generate texture on GPU
                generateOnGPU(glTexture);
            } catch (const std::runtime_error&) {
                LOG_ERROR("Generating texture on GPU failed.");
                m_gpuSupported = false;
            }
        } else if (!useGPU && m_cpuSupported) {
            try {
                // Generate texture on CPU
                generateOnCPU(glTexture);
            } catch (const std::runtime_error&) {
                LOG_ERROR("Generating texture on CPU failed.");
                m_cpuSupported = false;
            }
        } else {
            // Skip generate
        }
    } else {
        LOG_ERROR("Getting GL texture failed.");
    }
}
