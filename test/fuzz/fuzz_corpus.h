// SPDX-FileCopyrightText: 2026 DM8SB
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// Reads seed corpora (files or directories, sorted by name) for the standalone driver and
// test_fuzz_random. POSIX dirent, also in MinGW.

#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <algorithm>
#include <string>
#include <vector>

inline bool fuzzReadFile(const std::string& path, std::string& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    fclose(f);
    return true;
}

inline bool fuzzIsDir(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IFDIR);
}

// Adds path (a file) or the files below it (a directory) to files.
inline void fuzzCollect(const std::string& path, std::vector<std::string>& files) {
    if (!fuzzIsDir(path)) {
        files.push_back(path);
        return;
    }
    DIR* d = opendir(path.c_str());
    if (!d) return;
    std::vector<std::string> names;
    while (dirent* e = readdir(d)) {
        if (e->d_name[0] == '.') continue;
        names.push_back(e->d_name);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    for (size_t i = 0; i < names.size(); ++i) fuzzCollect(path + "/" + names[i], files);
}
