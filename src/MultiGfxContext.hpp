
#pragma once

#include <GL/glew.h>
#include <GL/wglew.h>
#include <GL/GL.h>
#include <d3d11.h>
#include <d3d12.h>

#include "Globals.hpp"
#include "GfxContext.hpp"

// NOTICE! For simplicity, we instantiate all contexts into this class. In real
// world use post processing should use the same graphics api context as rendering.

//! Wrapper class for multi graphics contexts used in this example.
class MultiGfxContext : public VarjoExamples::GfxContext
{
public:
    //! Constructor
    MultiGfxContext(HWND hwnd);

    //! Destructor
    ~MultiGfxContext() = default;

    //! Initialize gfx sessions
    void init(IDXGIAdapter* adapter) override;

    //! Returns D3D12 command queue
    ComPtr<ID3D12CommandQueue> getD3D12CommandQueue() const { return m_d3d12Queue; }

private:
    //! Initialize OpenGL
    void initGL();

    //! Initialize D3D12
    void initD3D12(IDXGIAdapter* adapter);

private:
    HGLRC m_hglrc{NULL};                      //!< GL context handle
    ComPtr<ID3D12Device> m_d3d12Device;       //!< D3D12 device
    ComPtr<ID3D12CommandQueue> m_d3d12Queue;  //!< D3D12 command queue
};
