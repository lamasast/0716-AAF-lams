// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace LamaSync {

enum class Command { Start, Stop, LoopOn, LoopOff, Status, Quit, Invalid };

inline std::string normalize(std::string_view line) {
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = line.find_last_not_of(" \t\r\n");
    std::string text(line.substr(first, last - first + 1));
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

inline Command parse(std::string_view raw) {
    const auto command = normalize(raw);
    if (command == "START") return Command::Start;
    if (command == "STOP") return Command::Stop;
    if (command == "LOOP ON" || command == "LOOP=ON" || command == "LOOP 1") return Command::LoopOn;
    if (command == "LOOP OFF" || command == "LOOP=OFF" || command == "LOOP 0") return Command::LoopOff;
    if (command == "STATUS") return Command::Status;
    if (command == "QUIT") return Command::Quit;
    return Command::Invalid;
}

class Controller {
public:
    bool execute(Command command) {
        switch (command) {
        case Command::Start: playing_ = true; break;
        case Command::Stop: playing_ = false; break;
        case Command::LoopOn: loop_ = true; break;
        case Command::LoopOff: loop_ = false; break;
        case Command::Quit: return false;
        case Command::Status:
        case Command::Invalid: break;
        }
        return true;
    }

    bool playing() const { return playing_; }
    bool loop() const { return loop_; }

private:
    bool playing_ = false;
    bool loop_ = false;
};

inline std::string reply(Command command, bool playing, bool loop) {
    if (command == Command::Invalid) return "ERR UNKNOWN_COMMAND";
    if (command == Command::Quit) return "ACK QUIT";
    if (command == Command::Status) {
        return std::string("STATUS playing=") + (playing ? "1" : "0")
             + " loop=" + (loop ? "1" : "0");
    }
    if (command == Command::Start) return "ACK START";
    if (command == Command::Stop) return "ACK STOP";
    if (command == Command::LoopOn) return "ACK LOOP ON";
    return "ACK LOOP OFF";
}

} // namespace LamaSync
