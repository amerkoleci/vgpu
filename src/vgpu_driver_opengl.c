// Copyright © Amer Koleci and Contributors.
// Licensed under the MIT License (MIT). See LICENSE in the repository root for more information.

#if defined(VGPU_OPENGL_DRIVER)

#include "vgpu_driver.h"
#include <stdbool.h>

#if defined(__EMSCRIPTEN__)
#   include <emscripten.h>
#   include <GLES3/gl3.h>
#   include <GLES2/gl2ext.h>
#   include <GL/gl.h>
#   include <GL/glext.h>
#else

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#define APIENTRY
#endif

// OpenGL Value Types
typedef ptrdiff_t        GLintptr;
typedef ptrdiff_t        GLsizeiptr;
typedef unsigned int     GLenum;
typedef unsigned char    GLboolean;
typedef unsigned int     GLbitfield;
typedef void             GLvoid;
typedef signed char      GLbyte;      /* 1-byte signed */
typedef short            GLshort;     /* 2-byte signed */
typedef int              GLint;       /* 4-byte signed */
typedef unsigned char    GLubyte;     /* 1-byte unsigned */
typedef unsigned short   GLushort;    /* 2-byte unsigned */
typedef unsigned int     GLuint;      /* 4-byte unsigned */
typedef int              GLsizei;     /* 4-byte signed */
typedef float            GLfloat;     /* single precision float */
typedef float            GLclampf;    /* single precision float in [0,1] */
typedef double           GLdouble;    /* double precision float */
typedef double           GLclampd;    /* double precision float in [0,1] */
typedef char             GLchar;

// OpenGL Constants
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_DONT_CARE 0x1100
#define GL_ZERO 0x0000
#define GL_ONE 0x0001
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_UNSIGNED_SHORT_4_4_4_4_REV 0x8365
#define GL_UNSIGNED_SHORT_5_5_5_1_REV 0x8366
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_EXTENSIONS 0x1F03
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_DEPTH_BUFFER_BIT 0x0100
#define GL_STENCIL_BUFFER_BIT 0x0400
#define GL_SCISSOR_TEST 0x0C11
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02
#define GL_CW 0x0900
#define GL_CCW 0x0901
#define GL_NONE 0
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_CULL_FACE 0x0B44
#define GL_POLYGON_OFFSET_FILL 0x8037
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_BLEND 0x0BE2
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_DST_ALPHA 0x0304
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#define GL_DST_COLOR 0x0306
#define GL_ONE_MINUS_DST_COLOR 0x0307
#define GL_SRC_ALPHA_SATURATE 0x0308
#define GL_CONSTANT_COLOR 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002
#define GL_CONSTANT_ALPHA 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA 0x8004
#define GL_SRC1_ALPHA 0x8589
#define GL_SRC1_COLOR 0x88F9
#define GL_ONE_MINUS_SRC1_COLOR 0x88FA
#define GL_ONE_MINUS_SRC1_ALPHA 0x88FB
#define GL_MIN 0x8007
#define GL_MAX 0x8008
#define GL_FUNC_ADD 0x8006
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_NEVER 0x0200
#define GL_LESS 0x0201
#define GL_EQUAL 0x0202
#define GL_LEQUAL 0x0203
#define GL_GREATER 0x0204
#define GL_NOTEQUAL 0x0205
#define GL_GEQUAL 0x0206
#define GL_ALWAYS 0x0207
#define GL_INVERT 0x150A
#define GL_KEEP 0x1E00
#define GL_REPLACE 0x1E01
#define GL_INCR 0x1E02
#define GL_DECR 0x1E03
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_MIRRORED_REPEAT 0x8370
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_RED 0x1903
#define GL_RED_INTEGER 0x8D94
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_LUMINANCE 0x1909
#define GL_RGB8 0x8051
#define GL_RGBA8 0x8058
#define GL_RGBA4 0x8056
#define GL_RGB5_A1 0x8057
#define GL_RGB10_A2_EXT 0x8059
#define GL_RGBA16 0x805B
#define GL_BGRA 0x80E1
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_RG 0x8227
#define GL_RG8 0x822B
#define GL_RG16 0x822C
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RG32F 0x8230
#define GL_RGBA32F 0x8814
#define GL_RGBA16F 0x881A
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_R8 0x8229
#define GL_R16 0x822A
#define GL_RG8 0x822B
#define GL_RG16 0x822C
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RG32F 0x8230
#define GL_R8I 0x8231
#define GL_R8UI 0x8232
#define GL_R16I 0x8233
#define GL_R16UI 0x8234
#define GL_R32I 0x8235
#define GL_R32UI 0x8236
#define GL_RG8I 0x8237
#define GL_RG8UI 0x8238
#define GL_RG16I 0x8239
#define GL_RG16UI 0x823A
#define GL_RG32I 0x823B
#define GL_RG32UI 0x823C
#define GL_R8_SNORM 0x8F94
#define GL_RG8_SNORM 0x8F95
#define GL_RGB8_SNORM 0x8F96
#define GL_RGBA8_SNORM 0x8F9
#define GL_COMPRESSED_TEXTURE_FORMATS 0x86A3
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_STENCIL 0x84F9
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_LOD_BIAS 0x8501
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_TEXTURE0 0x84C0
#define GL_MAX_TEXTURE_IMAGE_UNITS 0x8872
#define GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS 0x8B4C
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_RENDERBUFFER 0x8D41
#define GL_MAX_DRAW_BUFFERS 0x8824
#define GL_POINTS 0x0000
#define GL_LINES 0x0001
#define GL_LINE_STRIP 0x0003
#define GL_TRIANGLES 0x0004
#define GL_TRIANGLE_STRIP 0x0005
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_SAMPLES_PASSED 0x8914
#define GL_MULTISAMPLE 0x809D
#define GL_MAX_SAMPLES 0x8D57
#define GL_SAMPLE_MASK 0x8E51
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_ACTIVE_UNIFORMS 0x8B86
#define GL_ACTIVE_ATTRIBUTES 0x8B89
#define GL_FLOAT_VEC2 0x8B50
#define GL_FLOAT_VEC3 0x8B51
#define GL_FLOAT_VEC4 0x8B52
#define GL_SAMPLER_2D 0x8B5E
#define GL_FLOAT_MAT3x2 0x8B67
#define GL_FLOAT_MAT4 0x8B5C
#define GL_FLOAT_MAT2x3 0x8B65
#define GL_FLOAT_MAT2x4 0x8B66
#define GL_FLOAT_MAT3x2 0x8B67
#define GL_FLOAT_MAT3x4 0x8B68
#define GL_FLOAT_MAT4x2 0x8B69
#define GL_FLOAT_MAT4x3 0x8B6A
#define GL_SRGB 0x8C40
#define GL_SRGB8 0x8C41
#define GL_SRGB_ALPHA 0x8C42
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_COMPRESSED_SRGB 0x8C48
#define GL_COMPRESSED_SRGB_ALPHA 0x8C49
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_NUM_EXTENSIONS 0x821D
#define GL_COLOR 0x1800
#define GL_DEPTH 0x1801
#define GL_STENCIL 0x1802
#define GL_STENCIL_INDEX 0x1901
#define GL_COPY_READ_BUFFER 0x8F36
#define GL_COPY_WRITE_BUFFER 0x8F37
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_UNIFORM_BUFFER_BINDING 0x8A28
#define GL_UNIFORM_BUFFER_START 0x8A29
#define GL_UNIFORM_BUFFER_SIZE 0x8A2A


#define GL_DEBUG_SOURCE_API 0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM 0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER 0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY 0x8249
#define GL_DEBUG_SOURCE_APPLICATION 0x824A
#define GL_DEBUG_SOURCE_OTHER 0x824B
#define GL_DEBUG_TYPE_ERROR 0x824C
#define GL_DEBUG_TYPE_PUSH_GROUP 0x8269
#define GL_DEBUG_TYPE_POP_GROUP 0x826A
#define GL_DEBUG_TYPE_MARKER 0x8268
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR 0x824E
#define GL_DEBUG_TYPE_PORTABILITY 0x824F
#define GL_DEBUG_TYPE_PERFORMANCE 0x8250
#define GL_DEBUG_TYPE_OTHER 0x8251
#define GL_DEBUG_SEVERITY_HIGH 0x9146
#define GL_DEBUG_SEVERITY_MEDIUM 0x9147
#define GL_DEBUG_SEVERITY_LOW 0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GL_DEBUG_OUTPUT 0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS 0x8242
#define GL_BUFFER 0x82E0
#define GL_SHADER 0x82E1
#define GL_PROGRAM 0x82E2
#define GL_VERTEX_ARRAY 0x8074
#define GL_QUERY 0x82E3
#define GL_PROGRAM_PIPELINE 0x82E4
#define GL_SAMPLER 0x82E6
#define GL_MAX_LABEL_LENGTH 0x82E8

// OpenGL Functions
#define GL_FUNCTIONS \
	GL_FUNC(glGetString, const GLubyte*, GLenum name) \
    GL_FUNC(glGetStringi, const GLubyte*, GLenum name, GLuint index) \
	GL_FUNC(glFlush, void, void) \
	GL_FUNC(glEnable, void, GLenum mode) \
	GL_FUNC(glDisable, void, GLenum mode) \
	GL_FUNC(glClearBufferiv, void, GLenum buffer, GLint drawbuffer, const GLint *value) \
    GL_FUNC(glClearBufferuiv, void, GLenum buffer, GLint drawbuffer, const GLuint *value) \
    GL_FUNC(glClearBufferfv, void, GLenum buffer, GLint drawbuffer, const GLfloat *value) \
    GL_FUNC(glClearBufferfi, void, GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil) \
	GL_FUNC(glDepthMask, void, GLboolean enabled) \
	GL_FUNC(glDepthFunc, void, GLenum func) \
	GL_FUNC(glViewport, void, GLint x, GLint y, GLint width, GLint height) \
	GL_FUNC(glScissor, void, GLint x, GLint y, GLint width, GLint height) \
    GL_FUNC(glDepthRangef, void, GLfloat n, GLfloat f) \
	GL_FUNC(glCullFace, void, GLenum mode) \
	GL_FUNC(glBlendEquation, void, GLenum eq) \
	GL_FUNC(glBlendEquationSeparate, void, GLenum modeRGB, GLenum modeAlpha) \
	GL_FUNC(glBlendFunc, void, GLenum sfactor, GLenum dfactor) \
	GL_FUNC(glBlendFuncSeparate, void, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha) \
	GL_FUNC(glBlendColor, void, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) \
	GL_FUNC(glColorMask, void, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha) \
	GL_FUNC(glGetIntegerv, void, GLenum name, GLint* data) \
	GL_FUNC(glGenTextures, void, GLint n, void* textures) \
	GL_FUNC(glGenRenderbuffers, void, GLint n, void* textures) \
	GL_FUNC(glGenFramebuffers, void, GLint n, void* textures) \
	GL_FUNC(glActiveTexture, void, GLuint id) \
	GL_FUNC(glBindTexture, void, GLenum target, GLuint id) \
	GL_FUNC(glTexStorage2D, void, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) \
    GL_FUNC(glTexStorage3D, void, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) \
	GL_FUNC(glBindRenderbuffer, void, GLenum target, GLuint id) \
	GL_FUNC(glBindFramebuffer, void, GLenum target, GLuint id) \
	GL_FUNC(glFramebufferRenderbuffer, void, GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) \
	GL_FUNC(glFramebufferTexture2D, void, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) \
	GL_FUNC(glTexParameteri, void, GLenum target, GLenum name, GLint param) \
	GL_FUNC(glRenderbufferStorage, void, GLenum target, GLenum internalformat, GLint width, GLint height) \
	GL_FUNC(glGetTexImage, void, GLenum target, GLint level, GLenum format, GLenum type, void* data) \
	GL_FUNC(glDrawElements, void, GLenum mode, GLint count, GLenum type, void* indices) \
	GL_FUNC(glDrawElementsInstanced, void, GLenum mode, GLint count, GLenum type, void* indices, GLint amount) \
	GL_FUNC(glDeleteTextures, void, GLint n, GLuint* textures) \
	GL_FUNC(glDeleteRenderbuffers, void, GLint n, GLuint* renderbuffers) \
	GL_FUNC(glDeleteFramebuffers, void, GLint n, GLuint* textures) \
	GL_FUNC(glGenVertexArrays, void, GLint n, GLuint* arrays) \
	GL_FUNC(glBindVertexArray, void, GLuint id) \
	GL_FUNC(glGenBuffers, void, GLint n, GLuint* arrays) \
	GL_FUNC(glBindBuffer, void, GLenum target, GLuint buffer) \
	GL_FUNC(glBufferData, void, GLenum target, GLsizeiptr size, const void* data, GLenum usage) \
	GL_FUNC(glBufferSubData, void, GLenum target, GLintptr offset, GLsizeiptr size, const void* data) \
	GL_FUNC(glDeleteBuffers, void, GLint n, GLuint* buffers) \
	GL_FUNC(glDeleteVertexArrays, void, GLint n, GLuint* arrays) \
	GL_FUNC(glEnableVertexAttribArray, void, GLuint location) \
	GL_FUNC(glDisableVertexAttribArray, void, GLuint location) \
	GL_FUNC(glVertexAttribPointer, void, GLuint index, GLint size, GLenum type, GLboolean normalized, GLint stride, const void* pointer) \
	GL_FUNC(glVertexAttribDivisor, void, GLuint index, GLuint divisor) \
	GL_FUNC(glCreateShader, GLuint, GLenum type) \
	GL_FUNC(glAttachShader, void, GLuint program, GLuint shader) \
	GL_FUNC(glDetachShader, void, GLuint program, GLuint shader) \
	GL_FUNC(glDeleteShader, void, GLuint shader) \
	GL_FUNC(glShaderSource, void, GLuint shader, GLsizei count, const GLchar** string, const GLint* length) \
	GL_FUNC(glCompileShader, void, GLuint shader) \
	GL_FUNC(glGetShaderiv, void, GLuint shader, GLenum pname, GLint* result) \
	GL_FUNC(glGetShaderInfoLog, void, GLuint shader, GLint maxLength, GLsizei* length, GLchar* infoLog) \
	GL_FUNC(glCreateProgram, GLuint, ) \
	GL_FUNC(glDeleteProgram, void, GLuint program) \
	GL_FUNC(glLinkProgram, void, GLuint program) \
	GL_FUNC(glGetProgramiv, void, GLuint program, GLenum pname, GLint* result) \
	GL_FUNC(glGetProgramInfoLog, void, GLuint program, GLint maxLength, GLsizei* length, GLchar* infoLog) \
	GL_FUNC(glGetActiveUniform, void, GLuint program, GLuint index, GLint bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) \
	GL_FUNC(glGetActiveAttrib, void, GLuint program, GLuint index, GLint bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) \
	GL_FUNC(glUseProgram, void, GLuint program) \
	GL_FUNC(glGetUniformLocation, GLint, GLuint program, const GLchar* name) \
	GL_FUNC(glGetAttribLocation, GLint, GLuint program, const GLchar* name) \
	GL_FUNC(glUniform1f, void, GLint location, GLfloat v0) \
	GL_FUNC(glUniform2f, void, GLint location, GLfloat v0, GLfloat v1) \
	GL_FUNC(glUniform3f, void, GLint location, GLfloat v0, GLfloat v1, GLfloat v2) \
	GL_FUNC(glUniform4f, void, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) \
	GL_FUNC(glUniform1fv, void, GLint location, GLint count, const GLfloat* value) \
	GL_FUNC(glUniform2fv, void, GLint location, GLint count, const GLfloat* value) \
	GL_FUNC(glUniform3fv, void, GLint location, GLint count, const GLfloat* value) \
	GL_FUNC(glUniform4fv, void, GLint location, GLint count, const GLfloat* value) \
	GL_FUNC(glUniform1i, void, GLint location, GLint v0) \
	GL_FUNC(glUniform2i, void, GLint location, GLint v0, GLint v1) \
	GL_FUNC(glUniform3i, void, GLint location, GLint v0, GLint v1, GLint v2) \
	GL_FUNC(glUniform4i, void, GLint location, GLint v0, GLint v1, GLint v2, GLint v3) \
	GL_FUNC(glUniform1iv, void, GLint location, GLint count, const GLint* value) \
	GL_FUNC(glUniform2iv, void, GLint location, GLint count, const GLint* value) \
	GL_FUNC(glUniform3iv, void, GLint location, GLint count, const GLint* value) \
	GL_FUNC(glUniform4iv, void, GLint location, GLint count, const GLint* value) \
	GL_FUNC(glUniform1ui, void, GLint location, GLuint v0) \
	GL_FUNC(glUniform2ui, void, GLint location, GLuint v0, GLuint v1) \
	GL_FUNC(glUniform3ui, void, GLint location, GLuint v0, GLuint v1, GLuint v2) \
	GL_FUNC(glUniform4ui, void, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3) \
	GL_FUNC(glUniform1uiv, void, GLint location, GLint count, const GLint* value) \
	GL_FUNC(glUniform2uiv, void, GLint location, GLint count, const GLint* value) \
	GL_FUNC(glUniform3uiv, void, GLint location, GLint count, const GLint* value) \
	GL_FUNC(glUniform4uiv, void, GLint location, GLint count, const GLint* value) \
	GL_FUNC(glUniformMatrix2fv, void, GLint location, GLint count, GLboolean transpose, const GLfloat* value) \
	GL_FUNC(glUniformMatrix3fv, void, GLint location, GLint count, GLboolean transpose, const GLfloat* value) \
	GL_FUNC(glUniformMatrix4fv, void, GLint location, GLint count, GLboolean transpose, const GLfloat* value) \
	GL_FUNC(glUniformMatrix2x3fv, void, GLint location, GLint count, GLboolean transpose, const GLfloat* value) \
	GL_FUNC(glUniformMatrix3x2fv, void, GLint location, GLint count, GLboolean transpose, const GLfloat* value) \
	GL_FUNC(glUniformMatrix2x4fv, void, GLint location, GLint count, GLboolean transpose, const GLfloat* value) \
	GL_FUNC(glUniformMatrix4x2fv, void, GLint location, GLint count, GLboolean transpose, const GLfloat* value) \
	GL_FUNC(glUniformMatrix3x4fv, void, GLint location, GLint count, GLboolean transpose, const GLfloat* value) \
	GL_FUNC(glUniformMatrix4x3fv, void, GLint location, GLint count, GLboolean transpose, const GLfloat* value) \
	GL_FUNC(glPixelStorei, void, GLenum pname, GLint param) \
    GL_FUNC(glDebugMessageCallback, void, GLDEBUGPROC callback, const void* userParam) \
    GL_FUNC(glDebugMessageControl, void, GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled) \
    GL_FUNC(glObjectLabel, void, GLenum identifier, GLuint name, GLsizei length, const GLchar *label) 

// Debug Function Delegate
typedef void (APIENTRY* GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);

// GL function pointers
#define GL_FUNC(name, ret, ...) typedef ret (*name ## Func) (__VA_ARGS__); name ## Func name;
GL_FUNCTIONS
#undef GL_FUNC
#endif

typedef struct GL_Buffer
{
    GLuint handle;
} GL_Buffer;

typedef struct GL_Texture
{
    uint32_t width;
    uint32_t height;
    GLenum target;
    GLuint handle;
} GL_Texture;

typedef struct GL_Sampler
{
    GLuint handle;
} GL_Sampler;

typedef struct GL_Shader
{
    GLuint handle;
} GL_Shader;

typedef struct GL_Pipeline
{
    GLuint handle;
    GLenum primitiveTopology;
} GL_Pipeline;

typedef struct GL_SwapChain
{
    uint32_t width;
    uint32_t height;
    vgpu_pixel_format format;
    void* window;
    GLuint framebuffer;
    GL_Texture* texture;
} GL_SwapChain;

typedef struct GL_CommandBuffer
{
    bool hasLabel;
    bool insideRenderPass;
} GL_CommandBuffer;

typedef struct GL_Renderer
{
    uint32_t frameIndex;
    uint64_t frameCount;

    VGPUCommandBuffer_T* mainCommandBuffer;
} GL_Renderer;

typedef struct gl_pixel_format_info
{
    GLenum internal;
    GLenum external;
    GLenum data_type;
} gl_pixel_format_info;

static const gl_pixel_format_info c_gl_format_info[] = {
    { GL_NONE,      GL_NONE,            GL_NONE },
    { GL_R8,        GL_RED,             GL_UNSIGNED_BYTE },   // R8Unorm
    { GL_R8_SNORM,  GL_RED,             GL_BYTE },   // R8Snorm
    { GL_R8UI,      GL_RED_INTEGER,     GL_UNSIGNED_BYTE },    // R8Uint
    { GL_R8I,       GL_RED_INTEGER,     GL_BYTE },    // R8Sint
};

const gl_pixel_format_info* _gl_get_format_info(vgpu_pixel_format format)
{
    //VGPU_ASSERT(c_gl_format_info[(uint32_t)format].format == format);
    return &c_gl_format_info[format];
}

static void gl_destroyDevice(VGPUDevice* device)
{
    // Wait idle
    glFlush();

    GL_Renderer* renderer = (GL_Renderer*)device->driverData;
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)renderer->mainCommandBuffer->driverData;
    VGPU_FREE(commandBuffer);
    VGPU_FREE(renderer->mainCommandBuffer);
    VGPU_FREE(renderer);
    VGPU_FREE(device);
}

static uint64_t gl_frame(VGPURenderer* driverData)
{
    HRESULT hr = S_OK;
    GL_Renderer* renderer = (GL_Renderer*)driverData;

    renderer->frameCount++;
    renderer->frameIndex = renderer->frameCount % VGPU_MAX_INFLIGHT_FRAMES;

    // Return current frame
    return renderer->frameCount - 1;
}

static void gl_waitIdle(VGPURenderer* driverData)
{
    VGPU_UNUSED(driverData);
    glFlush();
}

static vgpu_backend gl_getBackendType(void)
{
    return VGPU_BACKEND_OPENGL;
}

static VGPUBool32 gl_queryFeature(VGPURenderer* driverData, VGPUFeature feature, void* pInfo, uint32_t infoSize)
{
    (void)pInfo;
    (void)infoSize;

    GL_Renderer* renderer = (GL_Renderer*)driverData;
    switch (feature)
    {
        case VGPUFeature_TextureCompressionBC:
        case VGPUFeature_ShaderFloat16:
        case VGPUFeature_PipelineStatisticsQuery:
        case VGPUFeature_TimestampQuery:
        case VGPUFeature_DepthClamping:
        case VGPUFeature_Depth24UNormStencil8:
        case VGPUFeature_Depth32FloatStencil8:
        case VGPUFeature_IndependentBlend:
        case VGPUFeature_TextureCubeArray:
        case VGPUFeature_Tessellation:
        case VGPUFeature_DescriptorIndexing:
        case VGPUFeature_ConditionalRendering:
        case VGPUFeature_DrawIndirectFirstInstance:
            return true;

        case VGPUFeature_TextureCompressionETC2:
        case VGPUFeature_TextureCompressionASTC:
            return false;

        case VGPUFeature_ShaderOutputViewportIndex:
            return false;

            // https://docs.microsoft.com/en-us/windows/win32/direct3d11/tiled-resources-texture-sampling-features
        case VGPUFeature_SamplerMinMax:
            return false;

        case VGPUFeature_MeshShader:
            return false;

        case VGPUFeature_RayTracing:
            return false;


        default:
            return false;
    }
}

static void gl_getAdapterProperties(VGPURenderer* driverData, VGPUAdapterProperties* properties)
{
    GL_Renderer* renderer = (GL_Renderer*)driverData;
    memset(properties, 0, sizeof(VGPUAdapterProperties));

    //properties->vendorID = renderer->vendorID;
    //properties->deviceID = renderer->deviceID;
    //properties->name = renderer->adapterName.c_str();
    //properties->driverDescription = renderer->driverDescription.c_str();
    //properties->adapterType = renderer->adapterType;
}

static void gl_getLimits(VGPURenderer* driverData, VGPULimits* limits)
{
    GL_Renderer* renderer = (GL_Renderer*)driverData;
    memset(limits, 0, sizeof(VGPULimits));
}

/* Buffer */
static vgpu_buffer* gl_createBuffer(VGPURenderer* driverData, const vgpu_buffer_desc* desc, const void* pInitialData)
{
    GL_Renderer* renderer = (GL_Renderer*)driverData;

    GL_Buffer* buffer = VGPU_ALLOC(GL_Buffer);
    if (desc->handle)
    {
        buffer->handle = (GLuint)(intptr_t)desc->handle;

        return (vgpu_buffer*)buffer;
    }

    glGenBuffers(1, &buffer->handle);
    glBindBuffer(GL_COPY_READ_BUFFER, buffer->handle);

    GLenum usage = GL_STATIC_DRAW;
    if (desc->access == VGPU_CPU_ACCESS_WRITE) {
        usage = GL_DYNAMIC_DRAW;
    }

    glBufferData(GL_COPY_READ_BUFFER, (GLsizeiptr)desc->size, pInitialData, usage);

#if !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    if (desc->label) {
        glObjectLabel(GL_BUFFER, buffer->handle, (GLsizei)strlen(desc->label), desc->label);
    }
#endif

    return (vgpu_buffer*)buffer;
}

static void gl_destroyBuffer(VGPURenderer* driverData, vgpu_buffer* resource)
{
    VGPU_UNUSED(driverData);
    GL_Buffer* glBuffer = (GL_Buffer*)resource;
    if (glBuffer->handle)
    {
        glDeleteBuffers(1, &glBuffer->handle);
    }

    VGPU_FREE(glBuffer);
}

static VGPUDeviceAddress gl_buffer_get_device_address(vgpu_buffer* resource)
{
    VGPU_UNUSED(resource);
    return 0;
}

static void gl_buffer_set_label(VGPURenderer* driverData, vgpu_buffer* resource, const char* label)
{
    VGPU_UNUSED(driverData);
    VGPU_UNUSED(resource);
    VGPU_UNUSED(label);

#if !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    GL_Buffer* buffer = (GL_Buffer*)resource;
    glObjectLabel(GL_BUFFER, buffer->handle, (GLsizei)strlen(label), label);
#endif
}

/* Texture */
static VGPUTexture gl_createTexture(VGPURenderer* driverData, const VGPUTextureDesc* desc, const void* pInitialData)
{
    VGPU_UNUSED(driverData);

    GL_Texture* texture = VGPU_ALLOC_CLEAR(GL_Texture);
    texture->width = desc->size.width;
    texture->height = desc->size.height;
    texture->target = GL_TEXTURE_2D;

    glGenTextures(1, &texture->handle);
    glBindTexture(texture->target, texture->handle);
    glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(texture->target, 1, GL_SRGB8_ALPHA8, texture->width, texture->height);

    return (VGPUTexture)texture;
}

static void gl_destroyTexture(VGPURenderer* driverData, VGPUTexture texture)
{
    VGPU_UNUSED(driverData);
    GL_Texture* glTexture = (GL_Texture*)texture;
    if (glTexture->handle)
    {
        glDeleteTextures(1, &glTexture->handle);
    }

    VGPU_FREE(glTexture);
}

/* Sampler */
static VGPUSampler* gl_createSampler(VGPURenderer* driverData, const VGPUSamplerDesc* desc)
{
    VGPU_UNUSED(driverData);

    GL_Sampler* sampler = VGPU_ALLOC(GL_Sampler);

    return (VGPUSampler*)sampler;
}

static void gl_destroySampler(VGPURenderer* driverData, VGPUSampler* resource)
{
    VGPU_UNUSED(driverData);

    GL_Sampler* sampler = (GL_Sampler*)resource;
    VGPU_FREE(sampler);
}

/* ShaderModule */
static VGPUShaderModule gl_createShaderModule(VGPURenderer* driverData, const void* pCode, size_t codeSize)
{
    VGPU_UNUSED(driverData);

    GL_Shader* shader = VGPU_ALLOC(GL_Shader);
    return (VGPUShaderModule)shader;
}

static void gl_destroyShaderModule(VGPURenderer* driverData, VGPUShaderModule resource)
{
    VGPU_UNUSED(driverData);

    GL_Shader* shader = (GL_Shader*)resource;
    VGPU_FREE(shader);
}

/* Pipeline */
static VGPUPipeline* gl_createRenderPipeline(VGPURenderer* driverData, const VGPURenderPipelineDesc* desc)
{
    VGPU_UNUSED(driverData);

    GL_Pipeline* pipeline = VGPU_ALLOC_CLEAR(GL_Pipeline);
    pipeline->handle = 0;
    pipeline->primitiveTopology = GL_TRIANGLES;
    return (VGPUPipeline*)pipeline;
}

static VGPUPipeline* gl_createComputePipeline(VGPURenderer* driverData, const VGPUComputePipelineDesc* desc)
{
    VGPU_UNUSED(driverData);

    GL_Pipeline* pipeline = VGPU_ALLOC(GL_Pipeline);
    pipeline->handle = 0;
    return (VGPUPipeline*)pipeline;
}

static VGPUPipeline* gl_createRayTracingPipeline(VGPURenderer* driverData, const VGPURayTracingPipelineDesc* desc)
{
    VGPU_UNUSED(driverData);

    GL_Pipeline* pipeline = VGPU_ALLOC(GL_Pipeline);
    return (VGPUPipeline*)pipeline;
}

static void gl_destroyPipeline(VGPURenderer* driverData, VGPUPipeline* resource)
{
    VGPU_UNUSED(driverData);

    GL_Pipeline* pipeline = (GL_Pipeline*)resource;
    VGPU_FREE(pipeline);
}

/* SwapChain */
static void gl_updateSwapChain(VGPURenderer* driver_data, GL_SwapChain* swapchain)
{
    if (swapchain->framebuffer)
    {
        glDeleteFramebuffers(1, &swapchain->framebuffer);
    }

    if (swapchain->texture)
    {
        gl_destroyTexture(driver_data, (VGPUTexture)swapchain->texture);
    }

    swapchain->texture = VGPU_ALLOC_CLEAR(GL_Texture);
    swapchain->texture->width = swapchain->width;
    swapchain->texture->height = swapchain->height;
    swapchain->texture->target = GL_TEXTURE_2D;

    glGenTextures(1, &swapchain->texture->handle);
    glBindTexture(swapchain->texture->target, swapchain->texture->handle);
    glTexParameteri(swapchain->texture->target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(swapchain->texture->target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    //glTexParameteri(swapchain->texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    //glTexParameteri(swapchain->texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(swapchain->texture->target, 1, GL_SRGB8_ALPHA8, swapchain->width, swapchain->height);

    // Create FBO now
    glGenFramebuffers(1, &swapchain->framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, swapchain->framebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, swapchain->texture->target, swapchain->texture->handle, 0);
    glBindTexture(swapchain->texture->target, 0);
}

static VGPUSwapChain* gl_createSwapChain(VGPURenderer* driver_data, void* window, const VGPUSwapChainDesc* desc)
{
    GL_SwapChain* swapChain = VGPU_ALLOC_CLEAR(GL_SwapChain);
    swapChain->width = desc->width;
    swapChain->height = desc->height;
    swapChain->window = window;
    swapChain->format = desc->format;
    gl_updateSwapChain(driver_data, swapChain);

    return (VGPUSwapChain*)swapChain;
}

static void gl_destroySwapChain(VGPURenderer* driverData, VGPUSwapChain* swapChain)
{
    GL_SwapChain* glSwapChain = (GL_SwapChain*)swapChain;

    gl_destroyTexture(driverData, (VGPUTexture)glSwapChain->texture);

    VGPU_FREE(glSwapChain);
}

static vgpu_pixel_format gl_getSwapChainFormat(VGPURenderer* driverData, VGPUSwapChain* swapChain)
{
    VGPU_UNUSED(driverData);

    GL_SwapChain* d3dSwapChain = (GL_SwapChain*)swapChain;
    return d3dSwapChain->format;
}

static void gl_pushDebugGroup(VGPUCommandBufferImpl* driverData, const char* groupLabel)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
}

static void gl_popDebugGroup(VGPUCommandBufferImpl* driverData)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
}

static void gl_insertDebugMarker(VGPUCommandBufferImpl* driverData, const char* markerLabel)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
}

static void gl_setPipeline(VGPUCommandBufferImpl* driverData, VGPUPipeline* pipeline)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    GL_Pipeline* glPipeline = (GL_Pipeline*)pipeline;
}

static void gl_dispatch(VGPUCommandBufferImpl* driverData, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    //commandBuffer->commandList->Dispatch(groupCountX, groupCountY, groupCountZ);
}

static void gl_dispatchIndirect(VGPUCommandBufferImpl* driverData, vgpu_buffer* buffer, uint64_t offset)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
}

static VGPUTexture gl_acquireSwapchainTexture(VGPUCommandBufferImpl* driverData, VGPUSwapChain* swapChain, uint32_t* pWidth, uint32_t* pHeight)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    GL_SwapChain* glSwapChain = (GL_SwapChain*)swapChain;

    GL_Texture* swapChainTexture = glSwapChain->texture;

    if (pWidth) {
        *pWidth = swapChainTexture->width;
    }

    if (pHeight) {
        *pHeight = swapChainTexture->height;
    }

    //commandBuffer->swapChains.push_back(d3d12SwapChain);
    return (VGPUTexture)swapChainTexture;
}

static void gl_beginRenderPass(VGPUCommandBufferImpl* driverData, const VGPURenderPassDesc* desc)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClearBufferfv(GL_COLOR, 0, &desc->colorAttachments[0].clearColor.r);

    commandBuffer->insideRenderPass = true;
}

static void gl_endRenderPass(VGPUCommandBufferImpl* driverData)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    commandBuffer->insideRenderPass = false;
}

static void gl_setViewport(VGPUCommandBufferImpl* driverData, const VGPUViewport* viewport)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    glViewport((GLint)viewport->x, (GLint)viewport->y, (GLint)viewport->width, (GLint)viewport->height);
    glDepthRangef(viewport->minDepth, viewport->maxDepth);
}

static void gl_setScissorRect(VGPUCommandBufferImpl* driverData, const VGPURect* rect)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    glScissor(rect->x, rect->y, rect->width, rect->height);
}

static void gl_setVertexBuffer(VGPUCommandBufferImpl* driverData, uint32_t index, vgpu_buffer* buffer, uint64_t offset)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    GL_Buffer* glBuffer = (GL_Buffer*)buffer;
}

static void gl_setIndexBuffer(VGPUCommandBufferImpl* driverData, vgpu_buffer* buffer, uint64_t offset, vgpu_index_type type)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    GL_Buffer* glBuffer = (GL_Buffer*)buffer;
}

static void gl_prepareDraw(GL_CommandBuffer* commandBuffer)
{
    VGPU_ASSERT(commandBuffer->insideRenderPass);
}

static void gl_draw(VGPUCommandBufferImpl* driverData, uint32_t vertexStart, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstInstance)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    gl_prepareDraw(commandBuffer);
    //commandBuffer->commandList->DrawInstanced(vertexCount, instanceCount, vertexStart, firstInstance);
}

static void gl_drawIndexed(VGPUCommandBufferImpl* driverData, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    GL_CommandBuffer* commandBuffer = (GL_CommandBuffer*)driverData;
    gl_prepareDraw(commandBuffer);
}

static VGPUCommandBuffer gl_beginCommandBuffer(VGPURenderer* driverData, VGPUCommandQueue queueType, const char* label)
{
    GL_Renderer* renderer = (GL_Renderer*)driverData;
    return renderer->mainCommandBuffer;

#if TODO
    D3D12CommandBuffer* impl = nullptr;

    renderer->cmdBuffersLocker.lock();
    uint32_t cmd_current = renderer->cmdBuffersCount++;
    if (cmd_current >= renderer->commandBuffers.size())
    {
        impl = new D3D12CommandBuffer();
        impl->renderer = renderer;
        impl->type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        for (uint32_t i = 0; i < VGPU_MAX_INFLIGHT_FRAMES; ++i)
        {
            hr = renderer->device->CreateCommandAllocator(impl->type, IID_PPV_ARGS(&impl->commandAllocators[i]));
            VGPU_ASSERT(SUCCEEDED(hr));
        }

        hr = renderer->device->CreateCommandList1(0, impl->type, D3D12_COMMAND_LIST_FLAG_NONE,
            IID_PPV_ARGS(&impl->commandList)
        );
        VGPU_ASSERT(SUCCEEDED(hr));

        VGPUCommandBuffer_T* commandBuffer = VGPU_ALLOC_CLEAR(VGPUCommandBuffer_T);
        ASSIGN_COMMAND_BUFFER(d3d12);
        commandBuffer->driverData = (VGPUCommandBufferImpl*)impl;

        renderer->commandBuffers.push_back(commandBuffer);
    }
    else
    {
        impl = (D3D12CommandBuffer*)renderer->commandBuffers.back()->driverData;
    }

    renderer->cmdBuffersLocker.unlock();

    // Start the command list in a default state.
    hr = impl->commandAllocators[renderer->frameIndex]->Reset();
    VGPU_ASSERT(SUCCEEDED(hr));
    hr = impl->commandList->Reset(impl->commandAllocators[renderer->frameIndex], nullptr);
    VGPU_ASSERT(SUCCEEDED(hr));

    if (queueType == VGPUCommandQueue_Graphics ||
        queueType == VGPUCommandQueue_Compute)
    {
        //ID3D12DescriptorHeap* heaps[2] = {
        //    renderer->resourceHeap.handle,
        //    renderer->samplerHeap.handle
        //};
        //impl->commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);

        impl->commandList->SetComputeRootSignature(renderer->globalRootSignature);

        if (queueType == VGPUCommandQueue_Graphics)
        {
            impl->commandList->SetGraphicsRootSignature(renderer->globalRootSignature);
        }
    }

    impl->insideRenderPass = false;

    static constexpr float defaultBlendFactor[4] = { 0, 0, 0, 0 };
    impl->commandList->OMSetBlendFactor(defaultBlendFactor);
    impl->commandList->OMSetStencilRef(0);
    impl->numBarriersToFlush = 0;

    impl->hasLabel = false;
    if (label)
    {
        d3d12_pushDebugGroup((VGPUCommandBufferImpl*)impl, label);
        impl->hasLabel = true;
    }

    return renderer->commandBuffers.back();
#endif // TODO

}

static void gl_submit(VGPURenderer* driverData, VGPUCommandBuffer* commandBuffers, uint32_t count)
{
    GL_Renderer* renderer = (GL_Renderer*)driverData;
}

static void gl_load_functions(const vgpu_config* info)
{
#if !defined(__EMSCRIPTEN__)
#define GL_FUNC(name, ...) name = (name ## Func)(info->gl.gl_get_proc_address(#name));
    GL_FUNCTIONS
#undef GL_FUNC
#endif
}

#if !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
static void APIENTRY gl_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    // these are basically never useful
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION &&
        type == GL_DEBUG_TYPE_OTHER)
        return;

    const char* typeName = "";
    const char* severityName = "";

    switch (type)
    {
        case GL_DEBUG_TYPE_ERROR: typeName = "ERROR"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: typeName = "DEPRECATED BEHAVIOR"; break;
        case GL_DEBUG_TYPE_MARKER: typeName = "MARKER"; break;
        case GL_DEBUG_TYPE_OTHER: typeName = "OTHER"; break;
        case GL_DEBUG_TYPE_PERFORMANCE: typeName = "PEROFRMANCE"; break;
        case GL_DEBUG_TYPE_POP_GROUP: typeName = "POP GROUP"; break;
        case GL_DEBUG_TYPE_PORTABILITY: typeName = "PORTABILITY"; break;
        case GL_DEBUG_TYPE_PUSH_GROUP: typeName = "PUSH GROUP"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: typeName = "UNDEFINED BEHAVIOR"; break;
    }

    switch (severity)
    {
        case GL_DEBUG_SEVERITY_HIGH: severityName = "HIGH"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: severityName = "MEDIUM"; break;
        case GL_DEBUG_SEVERITY_LOW: severityName = "LOW"; break;
        case GL_DEBUG_SEVERITY_NOTIFICATION: severityName = "NOTIFICATION"; break;
    }

    if (type == GL_DEBUG_TYPE_ERROR)
    {
        vgpu_log_error("GL (%s:%s) %s", typeName, severityName, message);
    }
    else if (severity != GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        vgpu_log_warn("GL (%s:%s) %s", typeName, severityName, message);
    }
    else
    {
        vgpu_log_info("GL (%s) %s", typeName, message);
    }
}
#endif

static VGPUBool32 gl_isSupported(void)
{
    return true;
}

static VGPUDevice* gl_createDevice(const vgpu_config* info)
{
    VGPU_ASSERT(info);
    VGPU_ASSERT(info->gl.gl_get_proc_address);

    GL_Renderer* renderer = VGPU_ALLOC(GL_Renderer);

    // Bind opengl functions
    gl_load_functions(info);

    if (info->validationMode != VGPUValidationMode_Disabled)
    {
#if !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
        // bind debug message callback
        if (glDebugMessageCallback)
        {
            glEnable(GL_DEBUG_OUTPUT);
            glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback(gl_message_callback, NULL);
            //glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, 0, NULL, GL_TRUE);
            glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_MEDIUM, 0, NULL, GL_TRUE);
            if (info->validationMode != VGPUValidationMode_Verbose)
            {
                glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, NULL, GL_FALSE);
                glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
            }
            else
            {
                glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_TRUE);
            }
        }
#endif
    }

    GL_CommandBuffer* glCommandBuffer = VGPU_ALLOC(GL_CommandBuffer);

    VGPUCommandBuffer_T* commandBuffer = VGPU_ALLOC_CLEAR(VGPUCommandBuffer_T);
    ASSIGN_COMMAND_BUFFER(gl);
    commandBuffer->driverData = (VGPUCommandBufferImpl*)glCommandBuffer;
    renderer->mainCommandBuffer = commandBuffer;

    VGPUDevice* device = VGPU_ALLOC_CLEAR(VGPUDevice);
    ASSIGN_DRIVER(gl);
    device->driverData = (VGPURenderer*)renderer;
    return device;
}

VGFXDriver OpenGL_Driver = {
    VGPU_BACKEND_OPENGL,
    gl_isSupported,
    gl_createDevice
};

#endif /* VGPU_OPENGL_DRIVER */
