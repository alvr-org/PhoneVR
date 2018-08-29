#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern char watchMode;

void startWatch(const char *const name);
void stopWatch(const char *const name);
void watchTick(const char *const name);

#ifdef __cplusplus
}
#endif
