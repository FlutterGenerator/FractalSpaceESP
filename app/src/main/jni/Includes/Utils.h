#ifndef UTILS
#define UTILS

#include <jni.h>
#include <unistd.h>
#include <cstdint>
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <cstdlib>
#include "Logger.h"

typedef unsigned long DWORD;
static uintptr_t libBase;

bool libLoaded = false;

// Exact load base of a library: ask the dynamic linker where one of its exported functions lives.
// (/proc/self/maps can contain several mappings of the same file; taking the first line that
// contains the name gave a wrong base, so every RVA was off.)
static DWORD baseFromDladdr(const char *library) {
    void *h = dlopen(library, RTLD_NOLOAD | RTLD_LAZY);
    if (!h) return 0;
    static const char *syms[] = {"il2cpp_init", "il2cpp_domain_get", "il2cpp_class_from_name",
                                 "il2cpp_string_new", "il2cpp_object_new"};
    for (size_t i = 0; i < sizeof(syms) / sizeof(syms[0]); i++) {
        void *p = dlsym(h, syms[i]);
        Dl_info info;
        if (p && dladdr(p, &info) && info.dli_fbase) return (DWORD) info.dli_fbase;
    }
    return 0;
}

DWORD findLibrary(const char *library) {
    static DWORD cached = 0;
    static std::string cachedName;
    if (cached && cachedName == library) return cached;
    DWORD exact = baseFromDladdr(library);
    if (exact) {
        cachedName = library;
        return cached = exact;
    }

    // fallback: /proc/self/maps - the mapping whose path ends with "/<library>" and file offset is 0
    char buffer[1024] = {0};
    FILE *fp = fopen(OBFUSCATE("/proc/self/maps"), OBFUSCATE("rt"));
    if (fp == NULL) {
        perror(OBFUSCATE("fopen"));
        return 0;
    }
    size_t libLen = strlen(library);
    DWORD firstMatch = 0, result = 0;
    while (fgets(buffer, sizeof(buffer), fp)) {
        unsigned long start = 0, end = 0, offset = 0;
        int pathPos = 0;
        char perms[8] = {0};
        if (sscanf(buffer, "%lx-%lx %7s %lx %*s %*s %n", &start, &end, perms, &offset, &pathPos) < 4)
            continue;
        if (pathPos <= 0) continue;
        std::string path(buffer + pathPos);
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r' || path.back() == ' '))
            path.pop_back();
        if (path.size() < libLen || path.compare(path.size() - libLen, libLen, library) != 0)
            continue;
        if (path.size() > libLen && path[path.size() - libLen - 1] != '/')
            continue;
        if (!firstMatch) firstMatch = (DWORD) start;
        if (offset == 0) {
            result = (DWORD) start;
            break;
        }
    }
    fclose(fp);
    return result ? result : firstMatch;
}

DWORD getAbsoluteAddress(const char *libraryName, DWORD relativeAddr) {
    libBase = findLibrary(libraryName);
    if (libBase == 0)
        return 0;
    return (reinterpret_cast<DWORD>(libBase + relativeAddr));
}


jboolean isGameLibLoaded(JNIEnv *env, jobject thiz) {
    return libLoaded;
}

bool isLibraryLoaded(const char *libraryName) {
    //libLoaded = true;
    char line[512] = {0};
    FILE *fp = fopen(OBFUSCATE("/proc/self/maps"), OBFUSCATE("rt"));
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            std::string a = line;
            if (strstr(line, libraryName)) {
                libLoaded = true;
                fclose(fp);
                return true;
            }
        }
        fclose(fp);
    }
    return false;
}

uintptr_t string2Offset(const char *c) {
    int base = 16;
    // See if this function catches all possibilities.
    // If it doesn't, the function would have to be amended
    // whenever you add a combination of architecture and
    // compiler that is not yet addressed.
    static_assert(sizeof(uintptr_t) == sizeof(unsigned long)
                  || sizeof(uintptr_t) == sizeof(unsigned long long),
                  "Please add string to handle conversion for this architecture.");

    // Now choose the correct function ...
    if (sizeof(uintptr_t) == sizeof(unsigned long)) {
        return strtoul(c, nullptr, base);
    }

    // All other options exhausted, sizeof(uintptr_t) == sizeof(unsigned long long))
    return strtoull(c, nullptr, base);
}

namespace ToastLength {
    inline const int LENGTH_LONG = 1;
    inline const int LENGTH_SHORT = 0;
}

#endif