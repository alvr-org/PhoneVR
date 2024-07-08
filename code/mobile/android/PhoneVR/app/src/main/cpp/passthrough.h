#ifndef PHONEVR_PASSTHROUGH_H
#define PHONEVR_PASSTHROUGH_H

#include "cardboard.h"

struct PassthroughInfo {
    bool enabled = false;

    GLuint cameraTexture = 0;
    GLuint passthroughTexture = 0;

    GLuint passthroughDepthRenderBuffer = 0;
    GLuint passthroughFramebuffer = 0;

    float passthroughVertices[8];
    float passthroughSize = 1.0;

    int *screenWidth = 0;
    int *screenHeight = 0;
};

void passthrough_createPlane(PassthroughInfo *info);
GLuint passthrough_init(PassthroughInfo *info);
void passthrough_cleanup(PassthroughInfo *info);
void passthrough_setup(PassthroughInfo *info);
void passthrough_render(PassthroughInfo *info, CardboardEyeTextureDescription viewDescs[]);

#endif   // PHONEVR_PASSTHROUGH_H
