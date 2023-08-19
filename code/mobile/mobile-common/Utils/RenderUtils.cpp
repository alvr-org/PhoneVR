#include "RenderUtils.h"
#include "PVRGlobals.h"
#include "Utils/StrUtils.h"

using namespace std;

void glCheckError(string glFuncName);

namespace {
    char errorLog[256];

    // fbo and screen vertex shaders must have different axis convention, why??
    string VS_PT =
        R"glsl( #version 300 es
            const vec2 verts[4] = vec2[4](vec2(-1, -1), vec2(-1, 1), vec2(1, 1), vec2(1, -1));
            const vec2 coords[4] = vec2[4](vec2(0, 0), vec2(0, 1), vec2(1, 1), vec2(1, 0));
            out vec2 coord;
            void main() {
                gl_Position = vec4(verts[gl_VertexID], 0, 1);
                coord = coords[gl_VertexID];
            }
    )glsl";

    string VS_REPR =
        R"glsl( #version 300 es
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
        GLuint shader;
        try {
            shader = glCreateShader(type);
            if (shader == GL_INVALID_ENUM || shader == GL_NO_ERROR) {
                PVR_DB_I("RenderUtils::loadShader:: Error Creating Shader Object, Returned: " +
                         string(((shader) ? "GL_INVALID_ENUM" : "GL_NO_ERROR")));
                return shader;
            } else {
                const char *str = text.c_str();
                glShaderSource(shader, 1, &str, nullptr);
                glCheckError("loadShader::glShaderSource");
                glCompileShader(shader);
                glCheckError("loadShader::glCompileShader");
                GLint compiled;
                glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
                glCheckError("loadShader::glGetShaderiv");
                if (!compiled) {
                    glGetShaderInfoLog(shader, sizeof(errorLog), nullptr, errorLog);
                    glCheckError("loadShader::glGetShaderInfoLog");
                    PVR_DB_I("RenderUtils::loadShader:: Error has Occurred while compiling Shader "
                             "Object: ErrorLog: " +
                             string(errorLog) + "\nProg: '" + text + "'");
                    exit(1);
                }
            }
        } catch (exception e) {
            PVR_DB_I("RenderUtils::loadShader:: Caught Exception: " + string(e.what()));
        }
        return shader;
    }

    GLuint genProg(string vs, string fs) {
        GLuint prog;
        try {
            prog = glCreateProgram();
            if (prog == 0) {
                PVR_DB_I("RenderUtils::genProg:: Error Creating Program Object, Returned: " +
                         to_string(prog));
                return prog;
            }
            glAttachShader(prog, loadShader(GL_VERTEX_SHADER, vs));
            glCheckError("genProg::glAttachShader");
            glAttachShader(prog, loadShader(GL_FRAGMENT_SHADER, fs));
            glCheckError("genProg::glAttachShader");

            GLint linked;
            glLinkProgram(prog);
            glCheckError("genProg::glLinkProgram");
            glGetProgramiv(prog, GL_LINK_STATUS, &linked);
            glCheckError("genProg::glGetProgramiv");
            if (!linked) {
                glGetProgramInfoLog(prog, sizeof(errorLog), nullptr, errorLog);
                glCheckError("genProg::glGetProgramInfoLog");
                PVR_DB_I("RenderUtils::genProg:: Error has Occurred while Linking to Program "
                         "Object: ErrorLog: " +
                         string(errorLog));
                exit(1);
            }
        } catch (exception e) {
            PVR_DB_I("RenderUtils::genProg:: Caught Exception: " + string(e.what()));
        }
        return prog;
    }
}   // namespace

GLuint genTexture(bool oes, int w, int h, GLenum fmt, GLint mag, int max) {
    GLuint tex;
    try {
        glGenTextures(1, &tex);
        glCheckError("genTexture::glGenTextures");
        GLenum tgt = (oes ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D);
        glBindTexture(tgt, tex);
        glCheckError("genTexture::glBindTexture");
        if (!oes) {
            glTexStorage2D(tgt, max + 1, fmt, w, h);
            glCheckError("genTexture::glTexStorage2D");
        }
        glTexParameteri(tgt, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(tgt, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(tgt, GL_TEXTURE_MAG_FILTER, mag);
        glTexParameteri(
            tgt, GL_TEXTURE_MIN_FILTER, max == 0 ? GL_LINEAR : GL_LINEAR_MIPMAP_NEAREST);
        glCheckError("genTexture::glTexParameteri");
    } catch (exception e) {
        PVR_DB_I("RenderUtils::genTexture:: Caught Exception: " + string(e.what()));
    }
    return tex;
}

void setupRTS(int w, int h) {
    try {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glCheckError("setupRTS::glBindFramebuffer");
        glViewport(0, 0, w, h);
        glCheckError("setupRTS::glViewport");
    } catch (exception e) {
        PVR_DB_I("RenderUtils::setupRTS:: Caught Exception: " + string(e.what()));
    }
}

void Renderer::initProg(vector<pair<GLuint, bool>> texs, std::string vs, string frag) {
    string fs =
        R"glsl( #version 300 es
            #extension GL_OES_EGL_image_external_essl3 : require
            precision highp float;
            in vec2 coord;
            out vec4 color;
    )glsl";
    try {
        for (int i = 0; i < texs.size(); i++)
            fs += (string) "uniform sampler" + (get<1>(texs[i]) ? "ExternalOES" : "2D") + " tex" +
                  to_string(i) + ";";
        fs += frag;

        prog = genProg(vs, fs);
        for (int i = 0; i < texs.size(); i++) {
            auto jdjaskl = glGetUniformLocation(prog, ("tex" + to_string(i)).c_str());
            glCheckError("Renderer::initProg::glGetUniformLocation");
            tuple<GLuint, GLint, GLenum> t(get<0>(texs[i]),
                                           jdjaskl,
                                           get<1>(texs[i]) ? GL_TEXTURE_EXTERNAL_OES
                                                           : GL_TEXTURE_2D);
            inTexs.push_back(t);   // todo: try emplace_back
        }
    } catch (exception e) {
        PVR_DB_I("RenderUtils::Renderer::initProg:: Caught Exception: " + string(e.what()));
    }
}

std::string ReplaceAll(std::string str, const std::string &from, const std::string &to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();   // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

void Renderer::PVRPrintGLDesc() {
    const GLubyte *sVendor, *sRenderer, *sVersion, *sExts;

    if ((sVendor = glGetString(GL_VENDOR)) == nullptr)
        glCheckError("Renderer::PVRPrintGLDesc::glGetString(GL_VENDOR)");
    if ((sRenderer = glGetString(GL_RENDERER)) == nullptr)
        glCheckError("Renderer::PVRPrintGLDesc::glGetString(GL_RENDERER)");
    if ((sVersion = glGetString(GL_VERSION)) == nullptr)
        glCheckError("Renderer::PVRPrintGLDesc::glGetString(GL_VERSION)");
    if ((sExts = glGetString(GL_EXTENSIONS)) == nullptr)
        glCheckError("Renderer::PVRPrintGLDesc::glGetString(GL_EXTENSIONS)");

    string strExts = string((const char *) sExts);
    strExts = ReplaceAll(strExts, " ", ", ");

    PVR_DB_I("Renderer::PVRPrintGLDesc: glVendor : " + string((const char *) sVendor) +
             ", glRenderer : " + string((const char *) sRenderer) +
             ", glVersion : " + string((const char *) sVersion));
    PVR_DB_I("Renderer::PVRPrintGLDesc: glExts : " + strExts);
}

// This is the actuall Constructor
Renderer::Renderer(vector<pair<GLuint, bool>> texs, string frag, float xStart, float xEnd)
    : frmBuf(0), rtt(false), maxLod(0) {
    try {
        PVRPrintGLDesc();

        char vs[512];
        sprintf(vs, VS_REPR.c_str(), xStart, xEnd);
        initProg(texs, vs, frag);
        matUnif = glGetUniformLocation(prog, "mvp");
        glCheckError("Renderer::Renderer::glGetUniformLocation");
    } catch (exception e) {
        PVR_DB_I("RenderUtils::Renderer::Renderer:: Caught Exception: " + string(e.what()));
    }
}

Renderer::Renderer(int width,
                   int height,
                   vector<pair<GLuint, bool>> texs,
                   string frag,
                   GLenum fmt,
                   GLint mag,
                   GLuint rdrBuf,
                   int max)
    : width(width), height(height), size(width * height * 4), maxLod(max), rtt(true) {
    try {
        PVRPrintGLDesc();

        initProg(texs, VS_PT, frag);
        outTex = fmt == 0 ? rdrBuf : genTexture(false, width, height, fmt, mag, max);
        glGenFramebuffers(1, &frmBuf);
        glCheckError("Renderer::Renderer::glGenFramebuffers");
        glBindFramebuffer(GL_FRAMEBUFFER, frmBuf);
        glCheckError("Renderer::Renderer::glBindFramebuffer");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex, 0);
        glCheckError("Renderer::Renderer::glFramebufferTexture2D");
    } catch (exception e) {
        PVR_DB_I("RenderUtils::Renderer::Renderer:: Caught Exception: " + string(e.what()));
    }
}

void Renderer::render(Eigen::Matrix4f mat) {
    try {
        glUseProgram(prog);
        glCheckError("Renderer::render::glUseProgram");
        if (rtt) {
            glBindFramebuffer(GL_FRAMEBUFFER, frmBuf);
            glCheckError("Renderer::render::glBindFramebuffer");
            glViewport(0, 0, width, height);
            glCheckError("Renderer::render::glViewport");
        } else {
            glUniformMatrix4fv(matUnif, 1, GL_FALSE, mat.data());
            glCheckError("Renderer::render::glUniformMatrix4fv");
            // glViewport(0, 0, 1024, 768);
        }

        for (int i = 0; i < inTexs.size(); i++) {
            auto t = inTexs[i];
            glUniform1i(get<1>(t), i);
            glCheckError("Renderer::render::glUniform1i");
            glActiveTexture(GLenum(GL_TEXTURE0 + i));
            glCheckError("Renderer::render::glActiveTexture");
            glBindTexture(get<2>(t), get<0>(t));
            glCheckError("Renderer::render::glBindTexture");
        }
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glCheckError("Renderer::render::glDrawArrays");
        if (maxLod > 0) {
            glBindTexture(GL_TEXTURE_2D, outTex);
            glCheckError("Renderer::render::glBindTexture");
            glGenerateMipmap(GL_TEXTURE_2D);
            glCheckError("Renderer::render::glGenerateMipmap");
        }
    } catch (exception e) {
        PVR_DB_I("RenderUtils::Renderer::Renderer:: Caught Exception: " + string(e.what()));
    }
}

PBO::PBO(void *cpuBuf, int w, int h, int sz, GLenum fmt, GLenum type)
    : cpuBuf(cpuBuf), w(w), h(h), sz(sz), fmt(fmt), type(type) {
    try {
        glGenBuffers(1, &pbo);
        glCheckError("PBO::PBO::glGenBuffers");
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
        glCheckError("PBO::PBO::glBindBuffer");
        glBufferData(GL_PIXEL_PACK_BUFFER, sz, nullptr, GL_DYNAMIC_READ);
        glCheckError("PBO::PBO::glBufferData");
    } catch (exception e) {
        PVR_DB_I("RenderUtils::PBO::PBO:: Caught Exception: " + string(e.what()));
    }
}

void PBO::download() {
    try {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glCheckError("PBO::download::glReadBuffer");
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
        glCheckError("PBO::download::glBindBuffer");
        glReadPixels(0, 0, w, h, fmt, type, nullptr);
        glCheckError("PBO::download::glReadPixels");
        void *gBuf = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, sz, GL_MAP_READ_BIT);
        glCheckError("PBO::download::glMapBufferRange");
        memcpy(cpuBuf, gBuf, sz);
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        glCheckError("PBO::download::glUnmapBuffer");
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glCheckError("PBO::download::glBindBuffer");
    } catch (exception e) {
        PVR_DB_I("RenderUtils::PBO::download:: Caught Exception: " + string(e.what()));
    }
}

void glCheckError(string glFuncName) {
    try {
        static GLenum error = 0;
        while ((error = glGetError()) != GL_NO_ERROR) {
            PVR_DB_I("RenderUtils::glCheckError Caught Error:" + to_string(error) +
                     string(" at Function:") + glFuncName);
        }
    } catch (exception &e) {
        PVR_DB_I("RenderUtils::glCheckError:: Caught Exception: " + string(e.what()));
    }
}