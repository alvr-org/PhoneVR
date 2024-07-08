#ifndef PHONEVR_UTILS_H
#define PHONEVR_UTILS_H

#include "alvr_client_core.h"
#include <GLES3/gl3.h>
#include <android/log.h>
#include <string>

#define error(...) log(ALVR_LOG_LEVEL_ERROR, __FILE_NAME__, __LINE__, __func__, __VA_ARGS__)
#define info(...)  log(ALVR_LOG_LEVEL_INFO, __FILE_NAME__, __LINE__, __func__, __VA_ARGS__)
#define debug(...) log(ALVR_LOG_LEVEL_DEBUG, __FILE_NAME__, __LINE__, __func__, __VA_ARGS__)

const char LOG_TAG[] = "ALVR_PVR_NATIVE";

static void log(AlvrLogLevel level,
                const char *FILE_NAME,
                unsigned int LINE_NO,
                const char *FUNC,
                const char *format,
                ...) {
    va_list args;
    va_start(args, format);

    char buf[1024];
    std::string format_str;
    format_str = std::string(FILE_NAME) + ":" + std::to_string(LINE_NO) + ": " + std::string(FUNC) +
                 "():" + format;

    int count = vsnprintf(buf, sizeof(buf), format_str.c_str(), args);
    if (count > (int) sizeof(buf))
        count = (int) sizeof(buf);
    if (count > 0 && buf[count - 1] == '\n')
        buf[count - 1] = '\0';

    alvr_log(level, buf);

    switch (level) {
    case ALVR_LOG_LEVEL_DEBUG:
        __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "%s", buf);
        break;
    case ALVR_LOG_LEVEL_INFO:
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "%s", buf);
        break;
    case ALVR_LOG_LEVEL_ERROR:
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s", buf);
        break;
    case ALVR_LOG_LEVEL_WARN:
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "%s", buf);
        break;
    }

    va_end(args);
}

static const char *GlErrorString(GLenum error) {
    switch (error) {
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
        return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return "unknown";
    }
}

[[maybe_unused]] static void GLCheckErrors(const char *file, int line) {
    GLenum error = glGetError();
    if (error == GL_NO_ERROR) {
        return;
    }
    while (error != GL_NO_ERROR) {
        error("GL error on %s:%d : %s", file, line, GlErrorString(error));
        error = glGetError();
    }
    abort();
}

#define GL(func)                                                                                   \
    func;                                                                                          \
    GLCheckErrors(__FILE__, __LINE__)

#endif   // PHONEVR_UTILS_H
