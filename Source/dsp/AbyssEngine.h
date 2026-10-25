#pragma once

#include <atomic>
#include <memory>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "AutoGain.h"
#include "Darkener.h"
#include "PitchTracker.h"
#include "PressureComp.h"
#include "PressureDrive.h"
#include "SubOscillator.h"

namespace abyss
{

struct ParamPointers
{
    std::atomic<float>* depth = nullptr;
    std::atomic<float>* pressure = nullptr;
    std::atomic<float>* inGain = nullptr;
    std::atomic<float>* outGain = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* subMode = nullptr;
    std::atomic<float>* sub2 = nullptr;
    std::atomic<float>* ceiling = nullptr;
    std::atomic<float>* autoGain = nullptr;
    std::atomic<float>* os = nullptr;
    std::atomic<float>* bypass = nullptr;
};

class AbyssEngine
{
public:
    void setParameters (ParamPointers p) noexcept { params = p; }

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;

    void process (juce::AudioBuffer<float>& buffer) noexcept;
    void processBypassed (juce::AudioBuffer<float>& buffer) noexcept;

    int getLatencySamples() const noexcept { return reportedLatency; }
    bool consumeLatencyChange (int& latencyOut) noexcept;

    // Test hooks. tapSub receives the mono sub contribution added to the wet path.
    float* tapSub = nullptr;
    float clipPeak = 0.0f;
    bool trackerValid = false;

    bool isSettled() const noexcept
    {
        const float d = delayState - (float) reportedLatency;
        return slewLeft <= 0 && d <= 1.0e-3f && d >= -1.0e-3f && pathMixSettled;
    }

private:
    struct Path
    {
        std::unique_ptr<juce::dsp::Oversampling<float>> os;
        PressureDrive drive;
        int latency = 0;
        int factor = 1;
    };

    void processChunk (float* const* data, int numSamples, int numChannels) noexcept;
    void renderOversampled (int numSamples, int numChannels, bool driveOn, bool clipOn) noexcept;
    void pushSample (std::vector<float>& buf, int len, int ch, float x) noexcept;
    float readSample (const std::vector<float>& buf, int len, int ch, float delay) const noexcept;
    float readAt (const std::vector<float>& buf, int ch, int writePos, float delay) const noexcept;

    ParamPointers params;
    double sampleRate = 48000.0;
    int maxBlock = 0;
    int channels = 2;

    PitchTracker tracker;
    SubOscillator sub;
    Darkener darkener;
    PressureComp comp;
    AutoGain autoGain;

    Path paths[3];
    int active = 1;          // default 2x
    int fadeFrom = -1;
    float postExtra = 0.0f;
    float delayState = 0.0f;
    int reportedLatency = 0;
    bool latencyDirty = false;
    int slewLeft = 0;

    juce::LinearSmoothedValue<float> depthSm, pressureSm, inSm, outSm, mixSm, ceilSm, sub2Sm, bypassSm, modeSm;
    float pathMix = 0.0f;    // 0 = neutral delay, 1 = processed
    bool pathMixSettled = true;
    bool fullyBypassed = false;
    bool bypassResetDone = false;

    float gate = 0.0f;
    float conf = 1.0f;
    float gateUp = 0.0f, gateDn = 0.0f, confCoef = 0.0f, pathStep = 0.0f;

    double inMsAcc = 0.0;
    double procMsAcc = 0.0;
    int procMsCount = 0;

    std::vector<float> dryBuf;
    std::vector<float> wetBuf;
    std::vector<float> preBuf;
    int lineLen = 0;
    int lineWrite = 0;

    juce::AudioBuffer<float> baseBuf;
    juce::AudioBuffer<float> osScratch;

    std::vector<float> pCurve;
    std::vector<float> ceilCurve;
    std::vector<float> kCurve;
    std::vector<float> mixCurve;
    std::vector<float> outCurve;
    std::vector<float> bypassCurve;
    std::vector<float> delayCurve;
    std::vector<float> pathCurve;

    void resetDsp() noexcept;
};

} // namespace abyss
