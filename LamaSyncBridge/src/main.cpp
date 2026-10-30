// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Protocol.h"

#include "Design.h"
#include "DeviceManager.h"
#include "MemoryPool.h"
#include "SineWave.h"
#include "StereoOut.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

constexpr unsigned kSampleRate = 48000;
constexpr unsigned kBlockSize = 960;
constexpr unsigned kPoolBlocks = 32;

struct Options {
    bool help = false;
    bool listPorts = false;
    bool protocolOnly = false;
    std::string comPort;
};

Options parseOptions(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string argument(argv[index]);
        if (argument == "--help" || argument == "-h") {
            options.help = true;
        } else if (argument == "--list-ports") {
            options.listPorts = true;
        } else if (argument == "--protocol-only") {
            options.protocolOnly = true;
        } else if (argument == "--com") {
            if (++index >= argc) throw std::runtime_error("--com requires a port such as COM3");
            options.comPort = argv[index];
        } else {
            throw std::runtime_error("unknown argument: " + argument);
        }
    }
    return options;
}

void printUsage() {
    std::cout
        << "LamaSyncAudIO [--com COM3] [--protocol-only]\n"
        << "LamaSyncAudIO --list-ports\n\n"
        << "Commands: START, STOP, LOOP ON, LOOP OFF, STATUS, QUIT\n"
        << "--protocol-only tests the command/serial path without opening audio.\n";
}

class Engine {
public:
    Engine()
        : samples_(kPoolBlocks * kBlockSize),
          output_(std::make_unique<AudIO::StereoOut>()),
          graph_(0, 2) {
        AudIO::MemoryPool<float>::get().initialize(
            samples_.data(), static_cast<uint16_t>(kPoolBlocks), kBlockSize);
        if (!manager_.addAudioDevice("StereoOut", output_.get(), true)) {
            throw std::runtime_error("cannot register AudIO stereo output");
        }

        const auto oscillator = graph_.createObject(std::make_unique<AudIO::SineWave>());
        if (!graph_.setDesignOutput(oscillator)) {
            throw std::runtime_error("AudIO cannot select the oscillator as output");
        }
        auto* sine = dynamic_cast<AudIO::SineWave*>(graph_.getObject(oscillator));
        if (!sine) throw std::runtime_error("AudIO oscillator unavailable");
        sine->setParameter(AudIO::SineWave::kParamFreq, 220.0f);
        sine->setParameter(AudIO::SineWave::kParamAmplitude, -12.0f);
        graph_.setBypass(false);
        manager_.setAudioCallback(&graph_);
        manager_.start(kSampleRate, kBlockSize);
        started_ = output_->isReady();
        if (!started_) {
            manager_.stop();
            throw std::runtime_error("Windows audio output could not be opened");
        }
        manager_.pause();
    }

    ~Engine() {
        if (started_) manager_.stop();
    }

    void setPlaying(bool playing) {
        if (playing) manager_.resume();
        else manager_.pause();
    }

private:
    std::vector<float> samples_;
    std::unique_ptr<AudIO::StereoOut> output_;
    AudIO::Design graph_;
    AudIO::DeviceManager manager_;
    bool started_ = false;
};

#ifdef _WIN32
std::string windowsError(const std::string& operation) {
    const auto error = GetLastError();
    return operation + " failed (Windows error " + std::to_string(error) + ")";
}

class SerialPort {
public:
    explicit SerialPort(const std::string& name) {
        std::string path = name;
        if (path.rfind("\\\\.\\", 0) != 0) path = "\\\\.\\" + path;
        handle_ = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, 0, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) {
            throw std::runtime_error(windowsError("opening " + name));
        }

        try {
            DCB config{};
            config.DCBlength = sizeof(config);
            if (!GetCommState(handle_, &config)) {
                throw std::runtime_error(windowsError("GetCommState"));
            }
            config.BaudRate = CBR_115200;
            config.ByteSize = 8;
            config.Parity = NOPARITY;
            config.StopBits = ONESTOPBIT;
            config.fBinary = TRUE;
            config.fOutxCtsFlow = FALSE;
            config.fOutxDsrFlow = FALSE;
            config.fDtrControl = DTR_CONTROL_DISABLE;
            config.fRtsControl = RTS_CONTROL_DISABLE;
            config.fOutX = FALSE;
            config.fInX = FALSE;
            if (!SetCommState(handle_, &config)) {
                throw std::runtime_error(windowsError("SetCommState"));
            }

            COMMTIMEOUTS timeouts{};
            timeouts.ReadIntervalTimeout = 50;
            timeouts.ReadTotalTimeoutConstant = 100;
            if (!SetCommTimeouts(handle_, &timeouts)) {
                throw std::runtime_error(windowsError("SetCommTimeouts"));
            }
            if (!SetupComm(handle_, 4096, 4096)) {
                throw std::runtime_error(windowsError("SetupComm"));
            }
            PurgeComm(handle_, PURGE_RXCLEAR | PURGE_TXCLEAR);
        } catch (...) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
            throw;
        }
    }

    ~SerialPort() {
        if (handle_ != INVALID_HANDLE_VALUE) CloseHandle(handle_);
    }

    bool readLine(std::string& line) {
        line.clear();
        for (;;) {
            char character{};
            DWORD count = 0;
            if (!ReadFile(handle_, &character, 1, &count, nullptr)) {
                throw std::runtime_error(windowsError("serial read"));
            }
            if (count == 0) continue;
            if (character == '\n') return true;
            if (character == '\r') continue;
            if (line.size() >= 255) {
                line.clear();
                return false;
            }
            line.push_back(character);
        }
    }

    void writeLine(const std::string& line) {
        const auto data = line + "\n";
        DWORD written = 0;
        if (!WriteFile(handle_, data.data(), static_cast<DWORD>(data.size()), &written, nullptr)
            || written != data.size()) {
            throw std::runtime_error(windowsError("serial write"));
        }
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

int listSerialPorts() {
    bool found = false;
    std::vector<char> target(32768);
    for (int number = 1; number <= 256; ++number) {
        const auto name = std::string("COM") + std::to_string(number);
        SetLastError(ERROR_SUCCESS);
        const auto length = QueryDosDeviceA(name.c_str(), target.data(),
                                             static_cast<DWORD>(target.size()));
        if (length != 0) {
            std::cout << name << "\t" << target.data() << '\n';
            found = true;
        }
    }
    if (!found) std::cout << "No accessible COM ports found.\n";
    return 0;
}
#endif

class Runtime {
public:
    explicit Runtime(bool protocolOnly) {
        if (!protocolOnly) engine_ = std::make_unique<Engine>();
    }

    bool handle(const std::string& line, std::string& response) {
        const auto command = LamaSync::parse(line);
        const bool keepRunning = controller_.execute(command);
        if (engine_ && (command == LamaSync::Command::Start || command == LamaSync::Command::Stop)) {
            engine_->setPlaying(controller_.playing());
        }
        response = LamaSync::reply(command, controller_.playing(), controller_.loop());
        return keepRunning;
    }

private:
    LamaSync::Controller controller_;
    std::unique_ptr<Engine> engine_;
};

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parseOptions(argc, argv);
        if (options.help) {
            printUsage();
            return 0;
        }
        if (options.listPorts) {
#ifdef _WIN32
            return listSerialPorts();
#else
            std::cerr << "--list-ports is only implemented for Windows.\n";
            return 2;
#endif
        }

        Runtime runtime(options.protocolOnly);
        std::cout << "LamaSyncAudIO ready; audio="
                  << (options.protocolOnly ? "disabled" : "48kHz/220Hz") << ".\n";

        if (!options.comPort.empty()) {
#ifdef _WIN32
            SerialPort serial(options.comPort);
            std::cout << "Serial " << options.comPort
                      << " open at 115200 8N1; DTR/RTS disabled.\n";
            for (;;) {
                std::string line;
                if (!serial.readLine(line)) {
                    serial.writeLine("ERR LINE_TOO_LONG");
                    continue;
                }
                std::string response;
                const bool keepRunning = runtime.handle(line, response);
                std::cout << options.comPort << ": " << line << " -> " << response << '\n';
                serial.writeLine(response);
                if (!keepRunning) break;
            }
            return 0;
#else
            std::cerr << "--com is only implemented for Windows.\n";
            return 2;
#endif
        }

        std::cout << "Type START / STOP / LOOP ON / LOOP OFF / STATUS / QUIT:\n";
        for (std::string line; std::getline(std::cin, line);) {
            std::string response;
            const bool keepRunning = runtime.handle(line, response);
            std::cout << response << '\n';
            if (!keepRunning) break;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "ERROR " << error.what() << '\n';
        printUsage();
        return 1;
    }
}
