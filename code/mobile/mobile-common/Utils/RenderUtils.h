#pragma once

#include <string>
#include <cstdlib>
#include <vector>
#include <functional>

#include "Geometry"

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#endif
#ifdef __APPLE__
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES2/glext.h>
#endif

const char *const FS_PT = "void main() { color = texture(tex0, coord); }";

GLuint genTexture(bool isOES, int width = 0, int height = 0, GLenum fmt = GL_RGBA8, GLint mag = GL_LINEAR, int maxLod = 0);
void setupRTS(int w, int h);

class Renderer {
    GLuint prog, frmBuf, outTex;
    //<TextureID, TextureUniformRes, True:"ExternalOES": False:"2D">
    std::vector<std::tuple<GLuint, GLint, GLenum>> inTexs;
    int width, height, size, maxLod;
    GLint matUnif;
    bool rtt;

    //                             TexResource  True:"ExternalOES": False:"2D"
    void initProg(std::vector<std::pair<GLuint, bool>> inTexs, std::string vs, std::string fragBody);

public:
    //inTexs: {tex uniform, is oes}
    //RTS
    Renderer(std::vector<std::pair<GLuint, bool>> inTexs, std::string fragBody = FS_PT, float xStart = .0, float xEnd = 1.0);

    //RTT
    // fmt: format for creating render buffer texture, 0 if providing render target
    // rdrBuf: provide an already created render buffer texture (to allow ping-pong pipeline)
    Renderer(int outWidth, int outHeight, std::vector<std::pair<GLuint, bool>> inTexs, std::string fragBody,
             GLenum fmt, GLint mag, GLuint rdrBuf = 0, int maxLod = 0);
    GLuint target() { return outTex; }

    void render(Eigen::Matrix4f mvp = Eigen::Matrix4f::Identity());

    // needed to inject uniforms values during rendering
    GLuint getProg() { return prog; };

};

class PBO {
    void *cpuBuf;
    int w, h, sz;
    GLenum fmt, type;
    GLuint pbo;

public:
    PBO(void *cpuBuf, int width, int height, int byteSize, GLenum fmt = GL_RGBA, GLenum glType = GL_UNSIGNED_SHORT);

    //I should always memcpy all the data to the cpu before processing
    void download();
};