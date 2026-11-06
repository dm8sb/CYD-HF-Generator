#pragma once

// Shared pieces of the fuzz harnesses (W-S25). C++11, no libFuzzer headers: the same harness
// builds three ways (test/fuzz/README.md):
//   libFuzzer (clang -fsanitize=fuzzer,address,undefined): one target per binary.
//   FUZZ_STANDALONE: one target per binary, driven by standalone_main.cpp (MinGW, gcc).
//   FUZZ_REGISTRY: several harnesses #included into one program (test/test_fuzz_random), each
//   registered by name.
// A harness keeps everything in its own namespace and ends with FUZZ_TARGET(name, function).

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*FuzzFn)(const uint8_t* data, size_t size);

// The input being run (set by the drivers), printed when a check fails.
struct FuzzCurrent {
    const uint8_t* data;
    size_t size;
    const char* target;
};
inline FuzzCurrent& fuzzCurrent() {
    static FuzzCurrent c {nullptr, 0, ""};
    return c;
}

// A broken invariant: report and abort (libFuzzer then saves the input as crash-<sha1>).
inline void fuzzFail(const char* file, int line, const char* what) {
    fprintf(stderr, "\nFUZZ CHECK FAILED: %s\n  at %s:%d\n", what, file, line);
    const FuzzCurrent& c = fuzzCurrent();
    if (c.data) {
        fprintf(stderr, "  target %s, input (%u bytes):", c.target, (unsigned)c.size);
        for (size_t i = 0; i < c.size && i < 512; ++i) fprintf(stderr, "%s%02x", i % 32 ? "" : "\n  ", c.data[i]);
        fprintf(stderr, "\n");
    }
    fflush(stderr);
    abort();
}

#define FUZZ_CHECK(cond)                                         \
    do {                                                         \
        if (!(cond)) fuzzFail(__FILE__, __LINE__, #cond);        \
    } while (0)

#if defined(FUZZ_REGISTRY)

struct FuzzTarget {
    const char* name;
    FuzzFn fn;
    FuzzTarget* next;
};
inline FuzzTarget*& fuzzTargets() {
    static FuzzTarget* head = nullptr;
    return head;
}
struct FuzzRegistrar {
    FuzzTarget target;
    FuzzRegistrar(const char* name, FuzzFn fn) {
        target.name = name;
        target.fn = fn;
        target.next = fuzzTargets();
        fuzzTargets() = &target;
    }
};
#define FUZZ_TARGET(name, fn) static FuzzRegistrar fuzzRegistrar_##name(#name, fn);

#else

#define FUZZ_TARGET(name, fn)                                                          \
    extern "C" const char* fuzzTargetName() { return #name; }                         \
    extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {         \
        return fn(data, size);                                                         \
    }

#endif

// --- Independent checks used by several harnesses -------------------------------------------

// Strict UTF-8 (RFC 3629): no overlong forms, no surrogates, nothing above U+10FFFF. With
// noControls also no C0, DEL or C1 code points. Written separately from Sanitize on purpose.
inline bool fuzzValidUtf8(const char* s, size_t n, bool noControls) {
    const unsigned char* p = (const unsigned char*)s;
    size_t i = 0;
    while (i < n) {
        const unsigned c = p[i];
        unsigned cp;
        size_t len;
        if (c <= 0x7F) {
            cp = c;
            len = 1;
        } else if (c >= 0xC2 && c <= 0xDF) {
            cp = c & 0x1F;
            len = 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            cp = c & 0x0F;
            len = 3;
        } else if (c >= 0xF0 && c <= 0xF4) {
            cp = c & 0x07;
            len = 4;
        } else {
            return false;
        }
        if (i + len > n) return false;
        for (size_t k = 1; k < len; ++k) {
            if ((p[i + k] & 0xC0) != 0x80) return false;
            cp = (cp << 6) | (p[i + k] & 0x3F);
        }
        if ((len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF))) return false;
        if (cp >= 0xD800 && cp <= 0xDFFF) return false;
        if (noControls && (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0))) return false;
        i += len;
    }
    return true;
}

// Length of the UTF-8 character starting with lead byte c (1 for ASCII and anything else).
inline size_t fuzzUtf8Len(unsigned char c) {
    if (c >= 0xF0) return 4;
    if (c >= 0xE0) return 3;
    if (c >= 0xC0) return 2;
    return 1;
}
