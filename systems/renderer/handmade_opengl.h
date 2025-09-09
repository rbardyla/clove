#ifndef HANDMADE_OPENGL_H
#define HANDMADE_OPENGL_H

/*
    Handmade OpenGL Function Loader
    Loads OpenGL 3.3 Core Profile functions at runtime
*/

#include "../../src/handmade.h"

// OpenGL types
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;
typedef char GLchar;
typedef ptrdiff_t GLintptr;
typedef ptrdiff_t GLsizeiptr;

// OpenGL constants
#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_NONE                           0

// Error codes
#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_OUT_OF_MEMORY                  0x0505
#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506

// Types
#define GL_BYTE                           0x1400
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_SHORT                          0x1402
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_DOUBLE                         0x140A

// Clear bits
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_COLOR_BUFFER_BIT               0x00004000

// Primitives
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_LOOP                      0x0002
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_TRIANGLE_FAN                   0x0006

// Blending
#define GL_BLEND                          0x0BE2
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307

// Depth
#define GL_DEPTH_TEST                     0x0B71
#define GL_DEPTH_WRITEMASK                0x0B72
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207

// Face culling
#define GL_CULL_FACE                      0x0B44
#define GL_FRONT                          0x0404
#define GL_BACK                           0x0405
#define GL_FRONT_AND_BACK                 0x0408
#define GL_CCW                            0x0901
#define GL_CW                             0x0900

// Polygon mode
#define GL_POINT                          0x1B00
#define GL_LINE                           0x1B01
#define GL_FILL                           0x1B02

// Textures
#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE_CUBE_MAP               0x8513
#define GL_TEXTURE_BINDING_2D             0x8069
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_TEXTURE_WRAP_R                 0x8072
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_LINEAR_MIPMAP_NEAREST          0x2701
#define GL_NEAREST_MIPMAP_LINEAR          0x2702
#define GL_LINEAR_MIPMAP_LINEAR           0x2703
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_CLAMP_TO_BORDER                0x812D
#define GL_REPEAT                         0x2901
#define GL_MIRRORED_REPEAT                0x8370

// Texture formats
#define GL_RED                            0x1903
#define GL_RG                             0x8227
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_DEPTH_COMPONENT                0x1902
#define GL_DEPTH_STENCIL                  0x84F9
#define GL_R8                             0x8229
#define GL_RG8                            0x822B
#define GL_RGB8                           0x8051
#define GL_RGBA8                          0x8058
#define GL_R16F                           0x822D
#define GL_RG16F                          0x822F
#define GL_RGB16F                         0x881B
#define GL_RGBA16F                        0x881A
#define GL_R32F                           0x822E
#define GL_RG32F                          0x8230
#define GL_RGB32F                         0x8815
#define GL_RGBA32F                        0x8814
#define GL_DEPTH24_STENCIL8               0x88F0
#define GL_DEPTH_COMPONENT32F             0x8CAC

// Texture units
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE3                       0x84C3
#define GL_TEXTURE4                       0x84C4
#define GL_TEXTURE5                       0x84C5
#define GL_TEXTURE6                       0x84C6
#define GL_TEXTURE7                       0x84C7

// Shaders
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_GEOMETRY_SHADER                0x8DD9
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84

// Vertex arrays
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_UNIFORM_BUFFER                 0x8A11
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_STREAM_DRAW                    0x88E0

// Framebuffers
#define GL_FRAMEBUFFER                    0x8D40
#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_COLOR_ATTACHMENT1              0x8CE1
#define GL_COLOR_ATTACHMENT2              0x8CE2
#define GL_COLOR_ATTACHMENT3              0x8CE3
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_STENCIL_ATTACHMENT             0x8D20
#define GL_DEPTH_STENCIL_ATTACHMENT       0x821A
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5

// OpenGL function pointers
#define GL_FUNCTIONS \
    X(void, glClearColor, (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)) \
    X(void, glClear, (GLbitfield mask)) \
    X(void, glViewport, (GLint x, GLint y, GLsizei width, GLsizei height)) \
    X(void, glEnable, (GLenum cap)) \
    X(void, glDisable, (GLenum cap)) \
    X(void, glBlendFunc, (GLenum sfactor, GLenum dfactor)) \
    X(void, glDepthFunc, (GLenum func)) \
    X(void, glDepthMask, (GLboolean flag)) \
    X(void, glCullFace, (GLenum mode)) \
    X(void, glFrontFace, (GLenum mode)) \
    X(void, glPolygonMode, (GLenum face, GLenum mode)) \
    X(void, glScissor, (GLint x, GLint y, GLsizei width, GLsizei height)) \
    X(GLenum, glGetError, (void)) \
    X(void, glGetIntegerv, (GLenum pname, GLint *params)) \
    X(void, glGetFloatv, (GLenum pname, GLfloat *params)) \
    X(const GLubyte*, glGetString, (GLenum name)) \
    X(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count)) \
    X(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)) \
    X(void, glDrawArraysInstanced, (GLenum mode, GLint first, GLsizei count, GLsizei instancecount)) \
    X(void, glDrawElementsInstanced, (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLsizei instancecount)) \
    X(void, glGenBuffers, (GLsizei n, GLuint *buffers)) \
    X(void, glDeleteBuffers, (GLsizei n, const GLuint *buffers)) \
    X(void, glBindBuffer, (GLenum target, GLuint buffer)) \
    X(void, glBufferData, (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)) \
    X(void, glBufferSubData, (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data)) \
    X(void, glGenVertexArrays, (GLsizei n, GLuint *arrays)) \
    X(void, glDeleteVertexArrays, (GLsizei n, const GLuint *arrays)) \
    X(void, glBindVertexArray, (GLuint array)) \
    X(void, glEnableVertexAttribArray, (GLuint index)) \
    X(void, glDisableVertexAttribArray, (GLuint index)) \
    X(void, glVertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer)) \
    X(void, glVertexAttribDivisor, (GLuint index, GLuint divisor)) \
    X(GLuint, glCreateShader, (GLenum type)) \
    X(void, glDeleteShader, (GLuint shader)) \
    X(void, glShaderSource, (GLuint shader, GLsizei count, const GLchar **string, const GLint *length)) \
    X(void, glCompileShader, (GLuint shader)) \
    X(void, glGetShaderiv, (GLuint shader, GLenum pname, GLint *params)) \
    X(void, glGetShaderInfoLog, (GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog)) \
    X(GLuint, glCreateProgram, (void)) \
    X(void, glDeleteProgram, (GLuint program)) \
    X(void, glAttachShader, (GLuint program, GLuint shader)) \
    X(void, glDetachShader, (GLuint program, GLuint shader)) \
    X(void, glLinkProgram, (GLuint program)) \
    X(void, glUseProgram, (GLuint program)) \
    X(void, glGetProgramiv, (GLuint program, GLenum pname, GLint *params)) \
    X(void, glGetProgramInfoLog, (GLuint program, GLsizei maxLength, GLsizei *length, GLchar *infoLog)) \
    X(GLint, glGetUniformLocation, (GLuint program, const GLchar *name)) \
    X(void, glUniform1i, (GLint location, GLint v0)) \
    X(void, glUniform1f, (GLint location, GLfloat v0)) \
    X(void, glUniform2f, (GLint location, GLfloat v0, GLfloat v1)) \
    X(void, glUniform3f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2)) \
    X(void, glUniform4f, (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3)) \
    X(void, glUniform2fv, (GLint location, GLsizei count, const GLfloat *value)) \
    X(void, glUniform3fv, (GLint location, GLsizei count, const GLfloat *value)) \
    X(void, glUniform4fv, (GLint location, GLsizei count, const GLfloat *value)) \
    X(void, glUniformMatrix3fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)) \
    X(void, glUniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value)) \
    X(void, glGenTextures, (GLsizei n, GLuint *textures)) \
    X(void, glDeleteTextures, (GLsizei n, const GLuint *textures)) \
    X(void, glBindTexture, (GLenum target, GLuint texture)) \
    X(void, glTexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)) \
    X(void, glTexSubImage2D, (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)) \
    X(void, glTexParameteri, (GLenum target, GLenum pname, GLint param)) \
    X(void, glTexParameterf, (GLenum target, GLenum pname, GLfloat param)) \
    X(void, glGenerateMipmap, (GLenum target)) \
    X(void, glActiveTexture, (GLenum texture)) \
    X(void, glGenFramebuffers, (GLsizei n, GLuint *framebuffers)) \
    X(void, glDeleteFramebuffers, (GLsizei n, const GLuint *framebuffers)) \
    X(void, glBindFramebuffer, (GLenum target, GLuint framebuffer)) \
    X(void, glFramebufferTexture2D, (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)) \
    X(GLenum, glCheckFramebufferStatus, (GLenum target)) \
    X(void, glDrawBuffers, (GLsizei n, const GLenum *bufs)) \
    X(void, glBindBufferBase, (GLenum target, GLuint index, GLuint buffer)) \
    X(void, glBindBufferRange, (GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size)) \
    X(GLuint, glGetUniformBlockIndex, (GLuint program, const GLchar *uniformBlockName)) \
    X(void, glUniformBlockBinding, (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding))

// Declare function pointers
#define X(ret, name, args) typedef ret (*PFN##name)args; extern PFN##name name;
GL_FUNCTIONS
#undef X

// Load all OpenGL functions
b32 gl_load_functions(void *(*get_proc_address)(char *));

#endif // HANDMADE_OPENGL_H