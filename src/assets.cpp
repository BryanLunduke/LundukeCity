// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#include "assets.hpp"

#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include <vector>

namespace {

bool is_asset_root(const std::string &path)
{
    const std::string marker = path + "/images/micropolisEngine/tiles.png";
    std::ifstream in(marker, std::ios::binary);
    return in.good();
}

std::string exe_dir()
{
    char buf[4096];
    const ssize_t n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) {
        return {};
    }
    buf[n] = '\0';
    std::string path(buf);
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return {};
    }
    return path.substr(0, slash);
}

} // namespace

std::string asset_root()
{
    static const std::string cached = [] {
        std::vector<std::string> candidates;
        if (const char *env = std::getenv("LUNDUKE_CITY_ASSETS")) {
            if (env[0] != '\0') {
                candidates.emplace_back(env);
            }
        }
#ifdef LUNDUKE_CITY_ASSET_DIR
        candidates.emplace_back(LUNDUKE_CITY_ASSET_DIR);
#endif
        const std::string exe = exe_dir();
        if (!exe.empty()) {
            candidates.push_back(exe + "/../share/lunduke-city/micropolis-assets");
            candidates.push_back(exe + "/../../third_party/micropolis-assets");
        }
        candidates.emplace_back("third_party/micropolis-assets");
        for (const auto &candidate : candidates) {
            if (is_asset_root(candidate)) {
                return candidate;
            }
        }
        return std::string();
    }();
    return cached;
}

std::string asset_file(const std::string &relative)
{
    const std::string root = asset_root();
    if (root.empty()) {
        return {};
    }
    const std::string path = root + "/" + relative;
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        return {};
    }
    return path;
}
