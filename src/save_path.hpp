// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

#pragma once

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

// True when the path already ends in .cty, ignoring case.
inline bool path_has_cty_suffix(const std::string &path)
{
    if (path.size() < 4) {
        return false;
    }
    const unsigned char dot = static_cast<unsigned char>(path[path.size() - 4]);
    const unsigned char a = static_cast<unsigned char>(path[path.size() - 3]);
    const unsigned char b = static_cast<unsigned char>(path[path.size() - 2]);
    const unsigned char c = static_cast<unsigned char>(path[path.size() - 1]);
    return dot == '.' && std::tolower(a) == 'c' && std::tolower(b) == 't' && std::tolower(c) == 'y';
}

// The path that will actually be opened. Append .cty before the chooser
// confirms a name that does not already have the suffix.
inline std::string with_cty_suffix(const std::string &path)
{
    if (path_has_cty_suffix(path)) {
        return path;
    }
    return path + ".cty";
}

// File name offered by Save City. A city that already has a file keeps that
// file's name. Otherwise the name is the city's name, with characters that
// are illegal in a file name replaced, and a .cty suffix.
inline std::string proposed_city_filename(const std::string &city_name, const std::string &current_path)
{
    if (!current_path.empty()) {
        const auto slash = current_path.find_last_of('/');
        std::string base = slash == std::string::npos ? current_path : current_path.substr(slash + 1);
        if (!base.empty() && base != "." && base != "..") {
            if (!path_has_cty_suffix(base)) {
                base += ".cty";
            }
            return base;
        }
    }

    std::string cleaned;
    cleaned.reserve(city_name.size());
    bool pending_space = false;
    for (unsigned char ch : city_name) {
        if (ch < 0x20 || ch == 0x7f) {
            continue;
        }
        const bool illegal = ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' ||
                             ch == '<' || ch == '>' || ch == '|';
        if (illegal) {
            if (!cleaned.empty() && cleaned.back() != '_') {
                cleaned.push_back('_');
            }
            pending_space = false;
            continue;
        }
        if (ch == ' ' || ch == '\t') {
            if (!cleaned.empty()) {
                pending_space = true;
            }
            continue;
        }
        if (pending_space) {
            cleaned.push_back(' ');
            pending_space = false;
        }
        cleaned.push_back(static_cast<char>(ch));
    }
    while (!cleaned.empty() && (cleaned.back() == ' ' || cleaned.back() == '.')) {
        cleaned.pop_back();
    }
    std::size_t start = 0;
    while (start < cleaned.size() && (cleaned[start] == ' ' || cleaned[start] == '.')) {
        ++start;
    }
    if (start > 0) {
        cleaned.erase(0, start);
    }
    if (cleaned.empty()) {
        cleaned = "city";
    }
    if (!path_has_cty_suffix(cleaned)) {
        cleaned += ".cty";
    }
    return cleaned;
}

// The welcome flag and the last save folder live here, never under an XFCE
// config tree. XDG_CONFIG_HOME is ignored on purpose.
inline std::string city_config_dir()
{
    const char *home = std::getenv("HOME");
    if (home == nullptr || home[0] != '/') {
        return {};
    }
    const std::string dir = std::string(home) + "/.config/lunduke-city";
    if (dir.find("/xfce") != std::string::npos) {
        return {};
    }
    return dir;
}

// Last folder used by Save or Load. Empty when none has been stored, or when
// the stored path is not an absolute directory.
inline std::string remembered_city_folder()
{
    const std::string dir = city_config_dir();
    if (dir.empty()) {
        return {};
    }
    std::ifstream in(dir + "/last-folder");
    std::string line;
    if (!std::getline(in, line)) {
        return {};
    }
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
        line.pop_back();
    }
    while (line.size() > 1 && line.back() == '/') {
        line.pop_back();
    }
    if (line.empty() || line[0] != '/' || line.find("/xfce") != std::string::npos) {
        return {};
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(line, ec)) {
        return {};
    }
    return line;
}

// Remember the directory of a city file, or a directory itself. Refuses an
// XFCE path and a relative path.
inline void remember_city_folder(const std::string &path)
{
    if (path.empty() || path.find("/xfce") != std::string::npos) {
        return;
    }
    std::string folder = path;
    std::error_code ec;
    if (!std::filesystem::is_directory(folder, ec)) {
        const auto slash = folder.find_last_of('/');
        if (slash == std::string::npos) {
            return;
        }
        folder = slash == 0 ? std::string("/") : folder.substr(0, slash);
    }
    while (folder.size() > 1 && folder.back() == '/') {
        folder.pop_back();
    }
    if (folder.empty() || folder[0] != '/' || folder.find("/xfce") != std::string::npos) {
        return;
    }
    const std::string dir = city_config_dir();
    if (dir.empty()) {
        return;
    }
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return;
    }
    std::ofstream out(dir + "/last-folder", std::ios::trunc);
    if (!out) {
        return;
    }
    out << folder << '\n';
}
