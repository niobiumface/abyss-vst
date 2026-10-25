#include "Parameters.h"
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace
{

double fileTimeSeconds (const FILETIME& t)
{
    ULARGE_INTEGER u;
    u.LowPart = t.dwLowDateTime;
    u.HighPart = t.dwHighDateTime;
    return (double) u.QuadPart * 1.0e-7;
}

double processCpuSeconds()
{
    FILETIME created, exited, kernel, user;
    GetProcessTimes (GetCurrentProcess(), &created, &exited, &kernel, &user);
    return fileTimeSeconds (kernel) + fileTimeSeconds (user);
}

void setParam (abyss::AbyssAudioProcessor& proc, const char* id, float real)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (real));
}

struct Pink
{
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    std::uint32_t rng = 0xA5A5u;

    float white()
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) ((int) (rng >> 9) - 8388608) * (1.0f / 8388608.0f);
    }

    float next()
    {
        const float w = white();
        b0 = 0.99886f * b0 + w * 0.0555179f;
        b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f;
        b5 = -0.7616f * b5 - w * 0.0168980f;
        const float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        return pink;
    }
};

void highpass (std::vector<float>& x, double sr, float fc)
{
    const float a = std::exp (-6.28318530718f * fc / (float) sr);
    float state = 0.0f;
    float prev = 0.0f;
    for (float& s : x)
    {
        const float y = a * (state + s - prev);
        prev = s;
        state = y;
        s = y;
    }
}

void normalise (std::vector<float>& x, float peak)
{
    float m = 1.0e-12f;
    for (float s : x)
        m = std::max (m, std::abs (s));
    const float g = peak / m;
    for (float& s : x)
        s *= g;
}

float limitForOs (int os)
{
    if (os == 0) return 0.40f;
    if (os == 1) return 0.80f;
    return 1.40f;
}

const char* osName (int os)
{
    return os == 0 ? "off" : (os == 1 ? "2x" : "4x");
}

float runSignal (abyss::AbyssAudioProcessor& proc, const std::vector<float>& signal, int block, float& sink)
{
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    const int total = (int) signal.size();
    const double t0 = processCpuSeconds();

    for (int n = 0; n < total; n += block)
    {
        const int count = std::min (block, total - n);
        for (int i = 0; i < count; ++i)
        {
            const float s = signal[(size_t) (n + i)];
            buf.setSample (0, i, s);
            buf.setSample (1, i, s * 0.97f);
        }
        proc.processBlock (buf, midi);
        sink += buf.getSample (0, 0);
    }

    const double cpu = processCpuSeconds() - t0;
    const double audio = (double) total / 48000.0;
    return (float) (100.0 * cpu / audio);
}

} // namespace

int main()
{
    constexpr double sr = 48000.0;
    constexpr int seconds = 60;
    const int frames = (int) (sr * seconds);

    std::vector<float> pink ((size_t) frames);
    std::vector<float> bass ((size_t) frames);
    {
        Pink gen;
        for (int i = 0; i < frames; ++i)
            pink[(size_t) i] = gen.next();
        highpass (pink, sr, 20.0f);
        normalise (pink, 0.125f);

        const float notes[] = { 41.20f, 55.00f, 65.41f, 82.41f, 98.00f, 110.00f, 130.81f, 164.81f };
        const float amp = 0.25f;
        double phase = 0.0;
        for (int i = 0; i < frames; ++i)
        {
            const int note = (i * 8) / frames;
            phase += notes[note] / sr;
            if (phase >= 1.0)
                phase -= 1.0;
            bass[(size_t) i] = amp * (float) std::sin (phase * 6.283185307179586);
        }
    }

    std::printf ("block  os   depth  pressure   pink%%   bass%%    max%%   limit  status\n");
    int failed = 0;
    float sink = 0.0f;

    const int blocks[] = { 64, 128, 512 };
    const int osModes[] = { 0, 1, 2 };
    const float levels[] = { 0.0f, 50.0f, 100.0f };

    for (int block : blocks)
    {
        for (int os : osModes)
        {
            for (float depth : levels)
            {
                for (float pressure : levels)
                {
                    abyss::AbyssAudioProcessor proc;
                    juce::AudioProcessor::BusesLayout layout;
                    layout.inputBuses.add (juce::AudioChannelSet::stereo());
                    layout.outputBuses.add (juce::AudioChannelSet::stereo());
                    proc.setBusesLayout (layout);
                    setParam (proc, abyss::id::os, (float) os);
                    setParam (proc, abyss::id::depth, depth);
                    setParam (proc, abyss::id::pressure, pressure);
                    setParam (proc, abyss::id::autoGain, 1.0f);
                    setParam (proc, abyss::id::mix, 100.0f);
                    proc.prepareToPlay (sr, block);

                    juce::AudioBuffer<float> warm (2, block);
                    juce::MidiBuffer midi;
                    warm.clear();
                    for (int i = 0; i < 8; ++i)
                        proc.processBlock (warm, midi);

                    const float pinkPct = runSignal (proc, pink, block, sink);
                    const float bassPct = runSignal (proc, bass, block, sink);
                    const float worst = std::max (pinkPct, bassPct);
                    const float limit = limitForOs (os);
                    const bool check = block == 128;
                    const bool over = check && worst > limit;
                    if (over)
                        ++failed;

                    std::printf ("%5d  %-3s  %5.0f  %8.0f  %6.3f  %6.3f  %6.3f  %5.2f  %s\n",
                                 block, osName (os), depth, pressure,
                                 pinkPct, bassPct, worst, check ? limit : 0.0f,
                                 over ? "FAIL" : (check ? "ok" : "-"));
                    std::fflush (stdout);
                }
            }
        }
    }

    std::printf ("sink %g\n", sink);
    std::printf ("%s  %d block-128 cases over the limit\n", failed == 0 ? "PASS" : "FAIL", failed);
    return failed == 0 ? 0 : 1;
}
