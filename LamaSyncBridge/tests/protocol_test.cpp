// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Protocol.h"

#include <cassert>
#include <iostream>

using LamaSync::Command;

int main() {
    assert(LamaSync::parse(" start\r\n") == Command::Start);
    assert(LamaSync::parse("STOP") == Command::Stop);
    assert(LamaSync::parse("loop on") == Command::LoopOn);
    assert(LamaSync::parse("LOOP=OFF") == Command::LoopOff);
    assert(LamaSync::parse("LOOP 0") == Command::LoopOff);
    assert(LamaSync::parse(" STATUS ") == Command::Status);
    assert(LamaSync::parse("QUIT") == Command::Quit);
    assert(LamaSync::parse("CNC G0 X10") == Command::Invalid);
    assert(LamaSync::parse("UNKNOWN") == Command::Invalid);

    LamaSync::Controller controller;
    assert(controller.execute(Command::Start));
    assert(controller.playing());
    assert(controller.execute(Command::LoopOn));
    assert(controller.loop());
    assert(LamaSync::reply(Command::Status, controller.playing(), controller.loop())
           == "STATUS playing=1 loop=1");
    assert(controller.execute(Command::Stop));
    assert(!controller.playing());
    assert(!controller.execute(Command::Quit));
    assert(LamaSync::reply(Command::Invalid, false, false) == "ERR UNKNOWN_COMMAND");

    std::cout << "LamaSync protocol tests: PASS\n";
}
