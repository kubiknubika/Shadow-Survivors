// Упрощённые объявления NDK — только для проверки типов.
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef int32_t GLint;
typedef uint32_t GLuint;
typedef int32_t GLsizei;
typedef float GLfloat;
typedef float GLclampf;
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_TRIANGLE_STRIP 0x0005
#define GL_FLOAT 0x1406
#define GL_UNSIGNED_BYTE 0x1401
#define GL_TEXTURE_2D 0x0DE1
#define GL_RGBA 0x1908
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
GLuint glCreateShader(GLenum type);
void glShaderSource(GLuint shader, GLsizei count, const char* const* string, const GLint* length);
void glCompileShader(GLuint shader);
void glGetShaderiv(GLuint shader, GLenum pname, GLint* params);
void glGetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* length, char* infoLog);
void glDeleteShader(GLuint shader);
GLuint glCreateProgram(void);
void glAttachShader(GLuint program, GLuint shader);
void glLinkProgram(GLuint program);
void glGetProgramiv(GLuint program, GLenum pname, GLint* params);
void glGetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* length, char* infoLog);
void glDeleteProgram(GLuint program);
void glUseProgram(GLuint program);
GLint glGetAttribLocation(GLuint program, const char* name);
GLint glGetUniformLocation(GLuint program, const char* name);
void glUniform1i(GLint location, GLint v0);
void glGenTextures(GLsizei n, GLuint* textures);
void glBindTexture(GLenum target, GLuint texture);
void glDeleteTextures(GLsizei n, const GLuint* textures);
void glTexParameteri(GLenum target, GLenum pname, GLint param);
void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
void glActiveTexture(GLenum texture);
void glViewport(GLint x, GLint y, GLsizei width, GLsizei height);
void glClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void glClear(GLbitfield mask);
void glEnableVertexAttribArray(GLuint index);
void glDisableVertexAttribArray(GLuint index);
void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
void glDrawArrays(GLenum mode, GLint first, GLsizei count);
