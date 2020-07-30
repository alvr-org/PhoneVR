#include "Utils/StrUtils.h"
#include "RenderUtils.h"

using namespace std;

namespace {
    char errorLog[256];

    //fbo and screen vertex shaders must have different axis convention, why??
    string VS_PT = R"glsl(
        #version 300 es
        const vec2 verts[4] = vec2[4](vec2(-1, -1), vec2(-1, 1), vec2(1, 1), vec2(1, -1));
        const vec2 coords[4] = vec2[4](vec2(0, 0), vec2(0, 1), vec2(1, 1), vec2(1, 0));
        out vec2 coord;
        void main() {
            gl_Position = vec4(verts[gl_VertexID], 0, 1);
            coord = coords[gl_VertexID];
        }
    )glsl";

    const char *VS_REPR = R"glsl(
        #version 300 es
        uniform mat4 mvp;
        #define XSTART %f
        #define XEND %f
        const vec2 verts[4] = vec2[4](vec2(-1, 1), vec2(-1, -1), vec2(1, -1), vec2(1, 1));
        const vec2 coords[4] = vec2[4](vec2(XSTART, 0), vec2(XSTART, 1), vec2(XEND, 1), vec2(XEND, 0));
        out vec2 coord;
        void main() {
            gl_Position = mvp * vec4(verts[gl_VertexID], 0, 1);
            coord = coords[gl_VertexID];
        }
    )glsl";

    GLuint loadShader(GLenum type, string text) {
        GLuint shader = glCreateShader(type);
        const char *str = text.c_str();
        glShaderSource(shader, 1, &str, nullptr);
        glCompileShader(shader);
        GLint compiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            glGetShaderInfoLog(shader, sizeof(errorLog), nullptr, errorLog);
            exit(1);
        }
        return shader;
    }

    GLuint genProg(string vs, string fs) {
        GLuint prog = glCreateProgram();
        glAttachShader(prog, loadShader(GL_VERTEX_SHADER, vs));
        glAttachShader(prog, loadShader(GL_FRAGMENT_SHADER, fs));

        GLint linked;
        glLinkProgram(prog);
        glGetProgramiv(prog, GL_LINK_STATUS, &linked);
        if (!linked) {
            glGetProgramInfoLog(prog, sizeof(errorLog), nullptr, errorLog);
            exit(1);
        }
        return prog;
    }
}

GLuint genTexture(bool oes, int w, int h, GLenum fmt, GLint mag, int max){
    GLuint tex;
    glGenTextures(1, &tex);
    GLenum tgt = (oes ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D);
    glBindTexture(tgt, tex);
    if (!oes)
        glTexStorage2D(tgt, max + 1, fmt, w, h);
    glTexParameteri(tgt, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(tgt, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(tgt, GL_TEXTURE_MAG_FILTER, mag);
    glTexParameteri(tgt, GL_TEXTURE_MIN_FILTER, max == 0 ? GL_LINEAR : GL_LINEAR_MIPMAP_NEAREST);
    return tex;
}

void setupRTS(int w, int h) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, w, h);
}

void Renderer::initProg(vector<pair<GLuint, bool>> texs, std::string vs, string frag) {
    string fs = R"glsl(
        #version 300 es
        #extension GL_OES_EGL_image_external_essl3 : require
        precision highp float;
        in vec2 coord;
        out vec4 color;
    )glsl";
    for (int i = 0; i < texs.size(); i++)
        fs += (string)"uniform sampler" + (get<1>(texs[i])? "ExternalOES": "2D") + " tex" + to_string(i) + ";";
    fs += frag;

    prog = genProg(vs, fs);
    for (int i = 0; i <texs.size(); i++) {
        auto jdjaskl = glGetUniformLocation(prog, ("tex" + to_string(i)).c_str());
        tuple<GLuint, GLint, GLenum> t(
                get<0>(texs[i]), jdjaskl,
                get<1>(texs[i]) ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D);
        inTexs.push_back(t);// todo: try emplace_back
    }
}

// This is the actuall Constructor
Renderer::Renderer(vector<pair<GLuint, bool>> texs, string frag, float xStart, float xEnd)
        : rtt(false), frmBuf(0), maxLod(0){
    char vs[512];
    sprintf(vs, VS_REPR, xStart, xEnd);
    initProg(texs, vs, frag);
    matUnif = glGetUniformLocation(prog, "mvp");
}

Renderer::Renderer(int width, int height, vector<pair<GLuint, bool>> texs, string frag, GLenum fmt,
                   GLint mag, GLuint rdrBuf, int max) : width(width), height(height),
                                                        size(width * height * 4), rtt(true), maxLod(max) {
    initProg(texs, VS_PT, frag);
    outTex = fmt == 0 ? rdrBuf : genTexture(false, width, height, fmt, mag, max);
    glGenFramebuffers(1, &frmBuf);
    glBindFramebuffer(GL_FRAMEBUFFER, frmBuf);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);


}

void Renderer::render(Eigen::Matrix4f mat) {
    glUseProgram(prog);
    if(rtt) {
        glBindFramebuffer(GL_FRAMEBUFFER, frmBuf);
        glViewport(0, 0, width, height);
    }
    else {
        glUniformMatrix4fv(matUnif, 1, GL_FALSE, mat.data());
        //glViewport(0, 0, 1024, 768);
    }

    for (int i = 0; i < inTexs.size(); i++) {
        auto t = inTexs[i];
        glUniform1i(get<1>(t), i);
        glActiveTexture(GLenum(GL_TEXTURE0 + i));
        glBindTexture(get<2>(t), get<0>(t));
    }
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    if (maxLod > 0) {
        glBindTexture(GL_TEXTURE_2D, outTex);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
}

PBO::PBO(void *cpuBuf, int w, int h, int sz, GLenum fmt, GLenum type)
        : cpuBuf(cpuBuf), w(w), h(h), sz(sz), fmt(fmt), type(type) {
    glGenBuffers(1, &pbo);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, sz, nullptr, GL_DYNAMIC_READ);
}

void PBO::download() {
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glReadPixels(0, 0, w, h, fmt, type, nullptr);
    void *gBuf = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, sz, GL_MAP_READ_BIT);
    memcpy(cpuBuf, gBuf, sz);
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}