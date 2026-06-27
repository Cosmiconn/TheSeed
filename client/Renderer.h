#pragma once
// =============================================================================
// client/Renderer.h — Renderer Interface (STUB for V13.1)
// =============================================================================
#include <cstdint>

// Forward declarations
using GLuint = unsigned int;

void RendererInit(GLuint& grassTex, GLuint& rockTex);
void RendererShutdown(GLuint& grassTex, GLuint& rockTex);
