// chime: runs a command, waits for it to end, and chimes, so a long build, deploy or
// upload can be left alone until it's done.
//
// Copyright (C) 2026 Daher Alfawares
// SPDX-License-Identifier: GPL-3.0-or-later
//
// This program is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later version.
// It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
// without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
// PURPOSE. See the GNU General Public License for more details. You should have
// received a copy of it along with this program; if not, see
// <https://www.gnu.org/licenses/>.
//
//   chime make -j16                 runs it, then chimes: the desktop's "complete"
//                                   sound if it succeeded, its error sound if not,
//                                   with a notification saying how it ended and how
//                                   long it took
//   chime --name "Deploy" …         names the job in the notification: "Deploy
//                                   finished", or "Deploy failed with 1"
//   chime                           chimes now
//   chime --error --message "Out"   chimes the error sound now, saying that
//
// It exits with the command's own status, so it can sit inside scripts and && chains.
// Ctrl-C reaches the command, and chime still says how it ended. The sound and the
// notification are best-effort: a stalled audio sink or notification daemon is given
// a second and never fails it.

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

#ifndef CHIME_VERSION
#define CHIME_VERSION "0.0.0"
#endif

namespace {

using namespace std::chrono_literals;
using clock_type = std::chrono::steady_clock;

// Runs a helper, its output dropped, for at most limit; whether it finished well or
// was still going, as a sound playing is, when it was stopped.
bool helper(std::vector<std::string> words, std::chrono::milliseconds limit = 1000ms) {
    pid_t pid = fork();
    if (pid < 0) return false;
    if (pid == 0) {
        int null = open("/dev/null", O_RDWR);
        if (null >= 0) {
            dup2(null, STDIN_FILENO);
            dup2(null, STDOUT_FILENO);
            dup2(null, STDERR_FILENO);
        }
        std::vector<char*> arguments;
        for (auto& word : words) arguments.push_back(word.data());
        arguments.push_back(nullptr);
        execvp(arguments[0], arguments.data());
        _exit(127);
    }
    auto until = clock_type::now() + limit;
    int status = 0;
    while (clock_type::now() < until) {
        if (waitpid(pid, &status, WNOHANG) == pid) return WIFEXITED(status) && WEXITSTATUS(status) == 0;
        std::this_thread::sleep_for(20ms);
    }
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    return true;
}

// Whether a program is on the PATH.
bool found(std::string_view program) {
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::string_view rest = path;
    while (!rest.empty()) {
        auto colon = rest.find(':');
        auto directory = rest.substr(0, colon);
        if (!directory.empty() && access((std::string(directory) + "/" + std::string(program)).c_str(), X_OK) == 0) return true;
        if (colon == std::string_view::npos) break;
        rest.remove_prefix(colon + 1);
    }
    return false;
}

// The desktop's sound for the event, else the freedesktop sound file played directly,
// else its bell; then the terminal's bell and a notification.
void chime(bool failed, const std::string& title, const std::string& body) {
    std::string event = failed ? "dialog-error" : "complete";
    bool played = found("canberra-gtk-play") && helper({"canberra-gtk-play", "-i", event});
    const std::filesystem::path sounds = "/usr/share/sounds/freedesktop/stereo";
    for (auto file : {sounds / (event + ".oga"), sounds / "bell.oga"}) {
        if (played || !std::filesystem::exists(file)) continue;
        for (auto player : {"pw-play", "paplay", "aplay"}) {
            if (found(player) && helper({player, file.string()})) {
                played = true;
                break;
            }
        }
    }
    if (FILE* terminal = std::fopen("/dev/tty", "w")) {
        std::fputs("\a", terminal);
        std::fclose(terminal);
    }
    if (found("notify-send")) {
        std::vector<std::string> words{"notify-send", "--app-name", "chime"};
        if (failed) words.insert(words.end(), {"--urgency", "critical"});
        words.push_back(title);
        if (!body.empty()) words.push_back(body);
        helper(words);
    }
}

std::string spent(clock_type::duration took) {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(took).count();
    if (seconds < 60) return std::format("{}s", seconds);
    if (seconds < 3600) return std::format("{}m{:02}s", seconds / 60, seconds % 60);
    return std::format("{}h{:02}m{:02}s", seconds / 3600, seconds / 60 % 60, seconds % 60);
}

std::string shown(const std::vector<std::string>& words) {
    std::string line;
    for (auto& word : words) {
        if (!line.empty()) line += ' ';
        line += word;
    }
    return line.size() > 80 ? line.substr(0, 77) + "..." : line;
}

// A signal by its name, like SIGINT, where the C library knows it.
std::string signal_name(int signal) {
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 32))
    if (const char* abbreviation = sigabbrev_np(signal)) return std::string("SIG") + abbreviation;
#endif
    switch (signal) {
        case SIGHUP: return "SIGHUP";
        case SIGINT: return "SIGINT";
        case SIGQUIT: return "SIGQUIT";
        case SIGABRT: return "SIGABRT";
        case SIGKILL: return "SIGKILL";
        case SIGSEGV: return "SIGSEGV";
        case SIGPIPE: return "SIGPIPE";
        case SIGTERM: return "SIGTERM";
        default: return std::format("signal {}", signal);
    }
}

void usage(FILE* to) {
    std::println(to, "usage: chime [--name JOB] command [argument...]");
    std::println(to, "       chime [--error] [--message TEXT]");
    std::println(to, "");
    std::println(to, "Runs the command, waits for it to end and chimes: the desktop's complete");
    std::println(to, "sound if it succeeded, its error sound if not, with a notification saying");
    std::println(to, "how it ended and how long it took. Exits with the command's status.");
    std::println(to, "Without a command, chimes now.");
    std::println(to, "");
    std::println(to, "  --name JOB      name the job in the notification, like \"Deploy finished\"");
    std::println(to, "  --message TEXT  what the notification says when chiming now");
    std::println(to, "  --error         chime the error sound when chiming now");
    std::println(to, "  --              the command starts here, even if it starts with --");
    std::println(to, "  -h, --help      show this");
    std::println(to, "  --version       show chime's version");
}

}  // namespace

int main(int count, char** given) {
    bool error = false;
    std::optional<std::string> message, name;
    int at = 1;
    for (; at < count; ++at) {
        std::string_view word = given[at];
        if (word == "--") {
            ++at;
            break;
        }
        if (word == "--error") error = true;
        else if ((word == "--message" || word == "--name") && at + 1 < count) (word == "--name" ? name : message) = given[++at];
        else if (word == "-h" || word == "--help") {
            usage(stdout);
            return 0;
        } else if (word == "--version") {
            std::println("chime {}", CHIME_VERSION);
            return 0;
        } else if (word.starts_with("-")) {
            std::println(stderr, "chime: there's no option {}", word);
            usage(stderr);
            return 2;
        } else break;
    }

    std::vector<std::string> command(given + at, given + count);
    if (command.empty()) {
        chime(error, message.value_or(error ? "Failed" : "Done"), "");
        return 0;
    }

    // Given a command, a message names it, as --name does.
    if (!name) name = message;

    // The command gets the terminal's Ctrl-C; chime waits it out and reports.
    std::signal(SIGINT, SIG_IGN);
    std::signal(SIGQUIT, SIG_IGN);
    auto start = clock_type::now();
    pid_t pid = fork();
    if (pid < 0) {
        std::println(stderr, "chime: couldn't start {}: {}", command[0], std::strerror(errno));
        return 127;
    }
    if (pid == 0) {
        std::signal(SIGINT, SIG_DFL);
        std::signal(SIGQUIT, SIG_DFL);
        std::vector<char*> arguments;
        for (auto& word : command) arguments.push_back(word.data());
        arguments.push_back(nullptr);
        execvp(arguments[0], arguments.data());
        std::println(stderr, "chime: couldn't run {}: {}", command[0], std::strerror(errno));
        _exit(errno == ENOENT ? 127 : 126);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    auto took = spent(clock_type::now() - start);

    int code;
    std::string ended;
    if (WIFSIGNALED(status)) {
        code = 128 + WTERMSIG(status);
        ended = "stopped by " + signal_name(WTERMSIG(status));
    } else {
        code = WEXITSTATUS(status);
        ended = code == 0 ? "finished" : std::format("failed with {}", code);
    }
    std::string title = std::format("{} {}", name.value_or(shown(command)), ended);
    std::string body = name ? std::format("{} in {}", shown(command), took) : std::format("in {}", took);
    std::println(stderr, "chime: {} {} in {}", shown(command), ended, took);
    chime(error || code != 0, title, body);
    return code;
}
