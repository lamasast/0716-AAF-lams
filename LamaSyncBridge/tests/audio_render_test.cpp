// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudIOBuffer.h"
#include "Design.h"
#include "MemoryPool.h"
#include "SineWave.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kSampleRate = 48000;
constexpr uint32_t kBlockSize = 480;
constexpr uint32_t kSeconds = 1;
constexpr uint16_t kChannels = 2;
constexpr uint16_t kBitsPerSample = 16;
constexpr uint16_t kPoolBlocks = 64;

void writeU16(std::ofstream& file, uint16_t value) {
    const char bytes[] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff)
    };
    file.write(bytes, sizeof(bytes));
}

void writeU32(std::ofstream& file, uint32_t value) {
    const char bytes[] = {
        static_cast<char>(value & 0xff),
        static_cast<char>((value >> 8) & 0xff),
        static_cast<char>((value >> 16) & 0xff),
        static_cast<char>((value >> 24) & 0xff)
    };
    file.write(bytes, sizeof(bytes));
}

void writeWave(const std::filesystem::path& path,
               const std::vector<float>& left,
               const std::vector<float>& right) {
    if (left.size() != right.size()) throw std::runtime_error("stereo sample count mismatch");
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot create WAV output");

    const auto dataBytes = static_cast<uint32_t>(left.size() * kChannels * sizeof(int16_t));
    file.write("RIFF", 4);
    writeU32(file, 36 + dataBytes);
    file.write("WAVEfmt ", 8);
    writeU32(file, 16);
    writeU16(file, 1);
    writeU16(file, kChannels);
    writeU32(file, kSampleRate);
    writeU32(file, kSampleRate * kChannels * kBitsPerSample / 8);
    writeU16(file, kChannels * kBitsPerSample / 8);
    writeU16(file, kBitsPerSample);
    file.write("data", 4);
    writeU32(file, dataBytes);

    for (size_t index = 0; index < left.size(); ++index) {
        for (const float sample : {left[index], right[index]}) {
            const auto limited = std::clamp(sample, -1.0f, 1.0f);
            const auto pcm = static_cast<int16_t>(std::lround(limited * 32767.0f));
            writeU16(file, static_cast<uint16_t>(pcm));
        }
    }
    if (!file) throw std::runtime_error("writing WAV output failed");
}

double estimateFrequency(const std::vector<float>& samples, size_t first, size_t last) {
    size_t positiveCrossings = 0;
    size_t firstCrossing = 0;
    size_t lastCrossing = 0;
    for (size_t index = first + 1; index < last; ++index) {
        if (samples[index - 1] <= 0.0f && samples[index] > 0.0f) {
            if (positiveCrossings == 0) firstCrossing = index;
            lastCrossing = index;
            ++positiveCrossings;
        }
    }
    if (positiveCrossings < 2 || lastCrossing == firstCrossing) return 0.0;
    return static_cast<double>(positiveCrossings - 1) * kSampleRate
         / static_cast<double>(lastCrossing - firstCrossing);
}

double rms(const std::vector<float>& samples, size_t first, size_t last) {
    double sum = 0.0;
    for (size_t index = first; index < last; ++index) {
        sum += static_cast<double>(samples[index]) * samples[index];
    }
    return std::sqrt(sum / static_cast<double>(last - first));
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path outputPath = "lamasync-220hz-48k.wav";
        if (argc == 3 && std::string(argv[1]) == "--output") {
            outputPath = argv[2];
        } else if (argc != 1) {
            throw std::runtime_error("usage: LamaSyncAudioRenderTest [--output file.wav]");
        }

        std::vector<float> pool(static_cast<size_t>(kPoolBlocks) * kBlockSize);
        AudIO::MemoryPool<float>::get().initialize(pool.data(), kPoolBlocks, kBlockSize);

        AudIO::Design design(0, kChannels);
        const auto oscillator = design.createObject(std::make_unique<AudIO::SineWave>());
        if (!design.setDesignOutput(oscillator)) {
            throw std::runtime_error("cannot select AudIO oscillator output");
        }
        auto* sine = dynamic_cast<AudIO::SineWave*>(design.getObject(oscillator));
        if (!sine) throw std::runtime_error("AudIO oscillator unavailable");
        sine->setParameter(AudIO::SineWave::kParamFreq, 220.0f);
        sine->setParameter(AudIO::SineWave::kParamAmplitude, -12.0f);
        design.setBypass(false);
        design.prepareToPlay(kSampleRate, kBlockSize);

        AudIO::Buffer<float> block(0, false, kChannels, true);
        std::vector<float> left;
        std::vector<float> right;
        left.reserve(kSampleRate * kSeconds);
        right.reserve(kSampleRate * kSeconds);

        const auto blockCount = kSampleRate * kSeconds / kBlockSize;
        for (uint32_t blockIndex = 0; blockIndex < blockCount; ++blockIndex) {
            block.clearOutputs();
            design.process(block);
            const auto* leftBlock = block.getOutputBuffer(0);
            const auto* rightBlock = block.getOutputBuffer(1);
            if (!leftBlock || !rightBlock) throw std::runtime_error("AudIO output buffer missing");
            left.insert(left.end(), leftBlock, leftBlock + kBlockSize);
            right.insert(right.end(), rightBlock, rightBlock + kBlockSize);
        }
        design.teardown();

        if (left.size() != kSampleRate || right.size() != kSampleRate) {
            throw std::runtime_error("unexpected rendered sample count");
        }

        const size_t analysisStart = kSampleRate / 10;
        const size_t analysisEnd = left.size();
        const auto frequency = estimateFrequency(left, analysisStart, analysisEnd);
        const auto level = rms(left, analysisStart, analysisEnd);
        double maxStereoDifference = 0.0;
        for (size_t index = analysisStart; index < analysisEnd; ++index) {
            maxStereoDifference = std::max(
                maxStereoDifference,
                std::abs(static_cast<double>(left[index] - right[index])));
        }

        if (std::abs(frequency - 220.0) > 2.0) {
            throw std::runtime_error("dominant zero-crossing frequency is not 220 Hz");
        }
        if (level < 0.05 || level > 0.9) {
            throw std::runtime_error("rendered RMS level is outside the expected range");
        }
        if (maxStereoDifference > 0.00001) {
            throw std::runtime_error("left and right test-tone channels differ");
        }

        writeWave(outputPath, left, right);
        const auto expectedSize = 44 + left.size() * kChannels * sizeof(int16_t);
        if (std::filesystem::file_size(outputPath) != expectedSize) {
            throw std::runtime_error("WAV file size/header validation failed");
        }

        std::cout << "AUDIO_TEST PASS sample_rate=" << kSampleRate
                  << " channels=" << kChannels
                  << " samples=" << left.size()
                  << " measured_hz=" << frequency
                  << " rms=" << level
                  << " wav=" << outputPath.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "AUDIO_TEST FAIL " << error.what() << '\n';
        return 1;
    }
}
