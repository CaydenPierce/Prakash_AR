
#include "TestTexture.hpp"

#include <random>
#include <limits>
#include <unordered_map>

namespace
{
const std::unordered_map<varjo_TextureFormat, int> c_formatChannels = {
    {varjo_TextureFormat_R8G8B8A8_SRGB, 4},
    {varjo_TextureFormat_R8G8B8A8_UNORM, 4},
    {varjo_MaskTextureFormat_A8_UNORM, 1},
    {varjo_TextureFormat_R32_FLOAT, 1},
    {varjo_TextureFormat_R32_UINT, 1},
};

std::default_random_engine g_generator;

template <typename T>
inline T getGradientValue(int64_t x, int64_t y, int64_t w, int64_t h, int c, T maxValue)
{
    T value = 0;
    switch (c) {
        case 0: value = static_cast<T>(static_cast<double>(x * maxValue) / w); break;
        case 1: value = static_cast<T>(static_cast<double>(y * maxValue) / h); break;
        case 2: value = static_cast<T>(static_cast<double>(((w - x) + (h - y)) * maxValue) / (w + h)); break;
        case 3: value = static_cast<T>(static_cast<double>((x + (h - y)) * maxValue) / (w + h)); break;
    }
    return value;
}

template <typename T>
void generateNoise(const glm::ivec2& size, size_t rowPitch, int numChannels, T* target, T maxValue)
{
    std::uniform_real_distribution<double> distribution(0, maxValue);

    size_t yoffs = 0;
    for (int y = 0; y < size.y; y++) {
        size_t offs = yoffs;
        for (int x = 0; x < size.x; x++) {
            for (int c = 0; c < numChannels; c++) {
                target[offs + c] = static_cast<T>(distribution(g_generator));

                // Debug: Black grid
                if (((x & 15) == 8) || ((y & 15) == 8)) {
                    target[offs + c] = 0;
                }
            }
            offs += numChannels;
        }
        yoffs += (rowPitch / sizeof(T));
    }
}

template <typename T>
void generateGradient(const glm::ivec2& size, size_t rowPitch, int numChannels, T* target, T maxValue)
{
    const size_t bytes = size.x * size.y * numChannels;

    size_t yoffs = 0;
    for (int y = 0; y < size.y; y++) {
        size_t offs = yoffs;
        for (int x = 0; x < size.x; x++) {
            for (int c = 0; c < numChannels; c++) {
                target[offs + c] = getGradientValue<T>(x, y, size.x, size.y, c, maxValue);

                // Debug: Black grid
                if (((x & 15) == 8) || ((y & 15) == 8)) {
                    target[offs + c] = 0;
                }
            }
            offs += numChannels;
        }
        yoffs += (rowPitch / sizeof(T));
    }
}


}  // namespace

TestTexture::TestTexture(Type testType, varjo_TextureFormat textureFormat, const glm::ivec2& size)
    : m_testType(testType)
    , m_varjoFormat(textureFormat)
    , m_size(size)
    , m_numChannels(c_formatChannels.at(textureFormat))
{
}

void TestTexture::generate(uint8_t* target, size_t rowPitch)
{
    if (m_testType == Type::Gradient) {
        // Generate gradient texture
        generateGradient<uint8_t>(m_size, rowPitch, m_numChannels, target, std::numeric_limits<uint8_t>::max());
    } else if (m_testType == Type::Noise) {
        // Generate noise texture
        generateNoise<uint8_t>(m_size, rowPitch, m_numChannels, target, std::numeric_limits<uint8_t>::max());
    }
}

void TestTexture::generate(float* target, size_t rowPitch)
{
    if (m_testType == Type::Gradient) {
        // Generate gradient texture
        generateGradient<float>(m_size, rowPitch, m_numChannels, target, 1.0f);
    } else if (m_testType == Type::Noise) {
        // Generate noise texture
        generateNoise<float>(m_size, rowPitch, m_numChannels, target, 1.0f);
    }
}

void TestTexture::generate(uint32_t* target, size_t rowPitch)
{
    if (m_testType == Type::Gradient) {
        // Generate gradient texture
        generateGradient<uint32_t>(m_size, rowPitch, m_numChannels, target, std::numeric_limits<uint32_t>::max());
    } else if (m_testType == Type::Noise) {
        // Generate noise texture
        generateNoise<uint32_t>(m_size, rowPitch, m_numChannels, target, std::numeric_limits<uint32_t>::max());
    }
}