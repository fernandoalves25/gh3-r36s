/* The GLES2 entry points the 3D stage uses, declared here and resolved at run time with SDL_GL_GetProcAddress, so the
 * same source builds against the host's desktop GL (SDL's opengl or opengles2 renderer) and the device's libmali
 * without either GL header: the game only links SDL2. Only what r3d.c calls is here. */
#ifndef R3D_GL_H
#define R3D_GL_H
#include <stdint.h>
#include <stddef.h>
typedef unsigned int GLenum; typedef unsigned char GLboolean; typedef unsigned int GLbitfield; typedef int GLint; typedef int GLsizei;
typedef unsigned int GLuint; typedef float GLfloat; typedef float GLclampf; typedef char GLchar; typedef void GLvoid; typedef ptrdiff_t GLsizeiptr; typedef ptrdiff_t GLintptr;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0
#define GL_TRIANGLES 0x0004
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_ZERO 0
#define GL_ONE 1
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_FUNC_ADD 0x8006
#define GL_BLEND_EQUATION_RGB 0x8009
#define GL_BLEND_EQUATION_ALPHA 0x883D
#define GL_BLEND_DST_RGB 0x80C8
#define GL_BLEND_SRC_RGB 0x80C9
#define GL_BLEND_DST_ALPHA 0x80CA
#define GL_BLEND_SRC_ALPHA 0x80CB
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#define GL_STATIC_DRAW 0x88E4
#define GL_CULL_FACE 0x0B44
#define GL_DEPTH_TEST 0x0B71
#define GL_DEPTH_WRITEMASK 0x0B72
#define GL_DEPTH_FUNC 0x0B74
#define GL_BLEND 0x0BE2
#define GL_SCISSOR_BOX 0x0C10
#define GL_SCISSOR_TEST 0x0C11
#define GL_COLOR_WRITEMASK 0x0C23
#define GL_VIEWPORT 0x0BA2
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_BINDING_2D 0x8069
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_FRAMEBUFFER 0x8D40
#define GL_LEQUAL 0x0203
#define GL_LESS 0x0201
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT 0x1403
#define GL_FLOAT 0x1406
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED 0x8622
#define GL_MAX_VERTEX_ATTRIBS 0x8869
#define GL_EXTENSIONS 0x1F03
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_SRC_COLOR 0x0300
#define GL_ONE_MINUS_SRC_COLOR 0x0301

#define R3D_GL_FUNCS(X) \
    X(void, glActiveTexture, (GLenum)) X(void, glAttachShader, (GLuint, GLuint)) X(void, glBindBuffer, (GLenum, GLuint)) X(void, glBindTexture, (GLenum, GLuint)) \
    X(void, glBlendEquationSeparate, (GLenum, GLenum)) X(void, glBlendFuncSeparate, (GLenum, GLenum, GLenum, GLenum)) X(void, glBlendFunc, (GLenum, GLenum)) \
    X(void, glBufferData, (GLenum, GLsizeiptr, const void *, GLenum)) X(void, glClear, (GLbitfield)) X(void, glColorMask, (GLboolean, GLboolean, GLboolean, GLboolean)) \
    X(void, glCompileShader, (GLuint)) X(void, glCompressedTexImage2D, (GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const void *)) X(GLuint, glCreateProgram, (void)) X(GLuint, glCreateShader, (GLenum)) \
    X(void, glCullFace, (GLenum)) X(void, glDeleteBuffers, (GLsizei, const GLuint *)) X(void, glDeleteProgram, (GLuint)) X(void, glDeleteShader, (GLuint)) X(void, glDeleteTextures, (GLsizei, const GLuint *)) \
    X(void, glDepthFunc, (GLenum)) X(void, glDepthMask, (GLboolean)) X(void, glDisable, (GLenum)) X(void, glDisableVertexAttribArray, (GLuint)) X(void, glDrawElements, (GLenum, GLsizei, GLenum, const void *)) \
    X(void, glEnable, (GLenum)) X(void, glEnableVertexAttribArray, (GLuint)) X(void, glFinish, (void)) X(void, glGenBuffers, (GLsizei, GLuint *)) X(void, glGenTextures, (GLsizei, GLuint *)) X(void, glGenerateMipmap, (GLenum)) \
    X(GLint, glGetAttribLocation, (GLuint, const GLchar *)) X(void, glGetBooleanv, (GLenum, GLboolean *)) X(GLenum, glGetError, (void)) X(void, glGetIntegerv, (GLenum, GLint *)) \
    X(void, glGetProgramInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) X(void, glGetProgramiv, (GLuint, GLenum, GLint *)) X(void, glGetShaderInfoLog, (GLuint, GLsizei, GLsizei *, GLchar *)) X(void, glGetShaderiv, (GLuint, GLenum, GLint *)) \
    X(const unsigned char *, glGetString, (GLenum)) X(GLint, glGetUniformLocation, (GLuint, const GLchar *)) X(void, glGetVertexAttribiv, (GLuint, GLenum, GLint *)) X(GLboolean, glIsEnabled, (GLenum)) \
    X(void, glLinkProgram, (GLuint)) X(void, glPixelStorei, (GLenum, GLint)) X(void, glReadPixels, (GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void *)) X(void, glScissor, (GLint, GLint, GLsizei, GLsizei)) \
    X(void, glShaderSource, (GLuint, GLsizei, const GLchar *const *, const GLint *)) X(void, glTexImage2D, (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void *)) X(void, glTexParameteri, (GLenum, GLenum, GLint)) \
    X(void, glUniform1f, (GLint, GLfloat)) X(void, glUniform1i, (GLint, GLint)) X(void, glUniform2f, (GLint, GLfloat, GLfloat)) X(void, glUniform3f, (GLint, GLfloat, GLfloat, GLfloat)) X(void, glUniform4f, (GLint, GLfloat, GLfloat, GLfloat, GLfloat)) \
    X(void, glUniformMatrix4fv, (GLint, GLsizei, GLboolean, const GLfloat *)) X(void, glUseProgram, (GLuint)) X(void, glVertexAttribPointer, (GLuint, GLint, GLenum, GLboolean, GLsizei, const void *)) X(void, glViewport, (GLint, GLint, GLsizei, GLsizei))

#define R3D_GL_DECL(ret, name, args) extern ret (*r3d_##name) args;
R3D_GL_FUNCS(R3D_GL_DECL)
#undef R3D_GL_DECL
int r3d_gl_load(void);   /* resolves every pointer above (through SDL); 0 when all were found */
#endif
