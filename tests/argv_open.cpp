// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Lunduke City authors
// See COPYING and NOTICE.

// argv open of a saved city loads that city. A corrupt or missing file
// reports in a dialog and does not exit 1.

#include "city_session.hpp"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

int fail(int code, const char *message)
{
    std::cerr << message << "\n";
    return code;
}

std::string window_tree()
{
    // Bound the query. A stuck xwininfo must not freeze the test until the
    // meson timeout.
    FILE *pipe = popen("timeout 2 xwininfo -root -tree 2>/dev/null", "r");
    if (pipe == nullptr) {
        return {};
    }
    std::string text;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        text += buf;
    }
    pclose(pipe);
    return text;
}

struct Child {
    pid_t pid = -1;
    int stderr_fd = -1;
};

Child launch(const std::string &binary, const std::string &path)
{
    int pipes[2];
    if (pipe(pipes) != 0) {
        return {};
    }
    const pid_t pid = fork();
    if (pid < 0) {
        close(pipes[0]);
        close(pipes[1]);
        return {};
    }
    if (pid == 0) {
        // Own process group so the city, dbus-run-session, and the bus can
        // be signalled together. dbus-run-session otherwise leaves the city
        // running after this test moves on.
        setpgid(0, 0);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[0]);
        close(pipes[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }
        // A private bus, so a leftover city on the session bus cannot take
        // the open or stall registration.
        unsetenv("DBUS_SESSION_BUS_ADDRESS");
        execlp("dbus-run-session", "dbus-run-session", "--", binary.c_str(), path.c_str(),
               static_cast<char *>(nullptr));
        _exit(127);
    }
    setpgid(pid, pid);
    close(pipes[1]);
    const int flags = fcntl(pipes[0], F_GETFL, 0);
    if (flags >= 0) {
        fcntl(pipes[0], F_SETFL, flags | O_NONBLOCK);
    }
    return Child{pid, pipes[0]};
}

bool still_running(pid_t pid, int *status)
{
    const pid_t got = waitpid(pid, status, WNOHANG);
    return got == 0;
}

std::string read_fd(int fd)
{
    std::string text;
    char buf[256];
    ssize_t n = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        text.append(buf, buf + n);
    }
    return text;
}

void stop_child(Child &child)
{
    if (child.pid > 0) {
        kill(-child.pid, SIGTERM);
        kill(child.pid, SIGTERM);
        for (int i = 0; i < 50; ++i) {
            int status = 0;
            if (waitpid(child.pid, &status, WNOHANG) == child.pid) {
                child.pid = -1;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (child.pid > 0) {
            kill(-child.pid, SIGKILL);
            kill(child.pid, SIGKILL);
            int status = 0;
            waitpid(child.pid, &status, 0);
            child.pid = -1;
        }
    }
    if (child.stderr_fd >= 0) {
        close(child.stderr_fd);
        child.stderr_fd = -1;
    }
}

int wait_for_title(Child &child, const std::string &title, std::string &err)
{
    for (int i = 0; i < 40; ++i) {
        int status = 0;
        if (!still_running(child.pid, &status)) {
            err = read_fd(child.stderr_fd);
            std::cerr << "exited early status " << status << " stderr " << err << "\n";
            if (WIFEXITED(status) && WEXITSTATUS(status) == 1) {
                return 2;
            }
            return 3;
        }
        // Drain so a chatty child cannot fill the pipe and stall.
        read_fd(child.stderr_fd);
        const std::string tree = window_tree();
        if (tree.find(title) != std::string::npos) {
            err = read_fd(child.stderr_fd);
            return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    err = read_fd(child.stderr_fd);
    std::cerr << "windows:\n" << window_tree() << "\nstderr:\n" << err << "\n";
    return 4;
}

} // namespace

int main(int argc, char **argv)
{
    const char *display = std::getenv("DISPLAY");
    if (display == nullptr || display[0] == '\0') {
        return fail(1, "no DISPLAY; this GUI test must fail rather than skip or start its own server");
    }
    if (argc < 2) {
        return fail(1, "usage: argv_open <lunduke-city>");
    }
    const std::string binary = argv[1];

    char dir_template[] = "/tmp/lunduke-argv-open-XXXXXX";
    char *dir = mkdtemp(dir_template);
    if (dir == nullptr) {
        return fail(1, "could not make a directory for the city file");
    }
    const std::string good = std::string(dir) + "/harbor.cty";
    const std::string corrupt = std::string(dir) + "/corrupt.cty";
    const std::string missing = std::string(dir) + "/missing.cty";

    {
        CitySession session;
        session.new_city("Harbor Town", 7);
        if (session.city_name() != "Harbor Town" || !session.save_city_as(good)) {
            return fail(2, "could not write a city fixture");
        }
    }
    {
        std::ofstream out(corrupt, std::ios::binary);
        out << "this is not a city file\n";
        if (!out) {
            return fail(2, "could not write a corrupt city file");
        }
    }

    Child loaded = launch(binary, good);
    if (loaded.pid <= 0) {
        return fail(3, "could not start lunduke-city with a city file");
    }
    std::string err;
    const int loaded_rc = wait_for_title(loaded, "Lunduke City - Harbor Town", err);
    stop_child(loaded);
    if (loaded_rc == 2 || err.find("can not open files") != std::string::npos) {
        return fail(4, "lunduke-city rejected a saved city on the command line");
    }
    if (loaded_rc != 0) {
        return fail(5, "a saved city passed on the command line did not load");
    }

    Child bad = launch(binary, corrupt);
    if (bad.pid <= 0) {
        return fail(6, "could not start lunduke-city with a corrupt file");
    }
    err.clear();
    const int bad_rc = wait_for_title(bad, "Could not load city", err);
    stop_child(bad);
    if (bad_rc == 2 || err.find("can not open files") != std::string::npos) {
        return fail(7, "a corrupt city file made lunduke-city exit 1");
    }
    if (bad_rc != 0) {
        return fail(8, "a corrupt city file did not show an error dialog");
    }

    Child gone = launch(binary, missing);
    if (gone.pid <= 0) {
        return fail(9, "could not start lunduke-city with a missing file");
    }
    err.clear();
    const int gone_rc = wait_for_title(gone, "Could not load city", err);
    stop_child(gone);
    if (gone_rc == 2 || err.find("can not open files") != std::string::npos) {
        return fail(10, "a missing city file made lunduke-city exit 1");
    }
    if (gone_rc != 0) {
        return fail(11, "a missing city file did not show an error dialog");
    }
    return 0;
}
