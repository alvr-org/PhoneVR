#pragma once
#if __linux__

#define _stricmp strcasecmp
#define _wfopen wfopen
#define __LINUX_PATH_SIZE__ 1024
// https://bugzilla.redhat.com/show_bug.cgi?id=467172 says maybe this isnt correct
// but they have not considered I am lazy
#undef __THROW
#define __THROW



#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <cstring>

// possibly replace with std::filesystem::path ??
FILE* wfopen(const wchar_t* fileName, const wchar_t* mode)
{
    char fileNameBuffer[__LINUX_PATH_SIZE__];
    char modeBuffer[__LINUX_PATH_SIZE__];
    wcstombs(fileNameBuffer, fileName, __LINUX_PATH_SIZE__);
    wcstombs(modeBuffer, mode, __LINUX_PATH_SIZE__);
    return fopen(fileNameBuffer, modeBuffer);
}

#endif
