#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"
#include "dsp/FastMath.h"
#include "dsp/Darkener.h"
#include "dsp/PressureComp.h"
#include "dsp/PressureDrive.h"
#include "presets/PresetManager.h"
#include "ui/DepthMapping.h"
#include "ui/IndexedImage.h"
#include "ui/PixelScaler.h"
#include "ui/PressureFx.h"
#include "ui/SpriteBank.h"

#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

#include <malloc.h>
#include <windows.h>
#include <psapi.h>

namespace
{
int gFails = 0;
thread_local int gAllocWatch = 0;
thread_local int gAllocCount = 0;
std::string gFilter;

void fail (const char* expr, const char* file, int line)
{
    std::printf ("FAIL %s:%d  %s\n", file, line, expr);
    ++gFails;
}

#define CHECK(expr) do { if (! (expr)) fail (#expr, __FILE__, __LINE__); } while (0)

void section (const char* name)
{
    std::printf ("\n== %s\n", name);
    std::fflush (stdout);
}

bool want (const char* name)
{
    return gFilter.empty() || std::string (name).find (gFilter) != std::string::npos;
}

float db (float peak)
{
    if (peak <= 0.0f)
        return -240.0f;
    return 20.0f * std::log10 (peak);
}

float ampFromDb (float dbfs)
{
    return std::pow (10.0f, dbfs / 20.0f);
}

struct Pink
{
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
    std::uint32_t rng = 0x12345678u;

    float white()
    {
        rng = rng * 1664525u + 1013904223u;
        return ((int) (rng >> 9) - 8388608) / 8388608.0f;
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
        const float p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        return p;
    }
};

struct Sine
{
    double phase = 0.0;
    double step = 0.0;
    float amp = 0.25f;

    void set (double freq, double sr, float amplitude)
    {
        step = freq / sr;
        amp = amplitude;
    }

    float next()
    {
        const float y = amp * (float) std::sin (phase * 6.28318530717958647692);
        phase += step;
        if (phase >= 1.0)
            phase -= 1.0;
        return y;
    }
};

class Host
{
public:
    abyss::AbyssAudioProcessor proc;
    double sr = 48000.0;
    int block = 512;
    int channels = 2;

    void set (const char* paramId, float real)
    {
        if (auto* p = proc.apvts.getParameter (paramId))
            p->setValueNotifyingHost (p->convertTo0to1 (real));
    }

    void prepare (double sampleRate, int blockSize, int ch)
    {
        sr = sampleRate;
        block = blockSize;
        channels = ch;
        juce::AudioProcessor::BusesLayout layout;
        const auto setCh = ch == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        layout.inputBuses.add (setCh);
        layout.outputBuses.add (setCh);
        CHECK (proc.setBusesLayout (layout));
        proc.prepareToPlay (sr, block);
    }

    void resetAudio()
    {
        proc.reset();
        proc.prepareToPlay (sr, block);
    }

    void silence (double seconds)
    {
        juce::AudioBuffer<float> buf (channels, block);
        juce::MidiBuffer midi;
        const int total = (int) std::lround (seconds * sr);
        for (int done = 0; done < total; done += block)
        {
            buf.clear();
            proc.processBlock (buf, midi);
        }
    }

    void process (juce::AudioBuffer<float>& buf)
    {
        juce::MidiBuffer midi;
        proc.processBlock (buf, midi);
    }

    void render (const std::vector<float>& input, std::vector<float>& output, std::vector<float>* tap)
    {
        output.assign (input.size(), 0.0f);
        const int frames = (int) input.size() / channels;
        if (tap != nullptr)
            tap->assign ((size_t) frames + (size_t) block, 0.0f);
        juce::AudioBuffer<float> buf (channels, block);
        int frame = 0;
        while (frame < frames)
        {
            const int n = std::min (block, frames - frame);
            buf.clear();
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < channels; ++c)
                    buf.setSample (c, i, input[(size_t) (frame + i) * (size_t) channels + (size_t) c]);

            if (tap != nullptr)
                proc.engine.tapSub = tap->data() + (size_t) frame;
            process (buf);
            proc.engine.tapSub = nullptr;

            for (int i = 0; i < n; ++i)
                for (int c = 0; c < channels; ++c)
                    output[(size_t) (frame + i) * (size_t) channels + (size_t) c] = buf.getSample (c, i);
            frame += n;
        }
    }
};

void highpassInPlace (float* x, int n, float fc, float sr);

std::vector<float> makePink (int frames, int channels, float peakDb)
{
    Pink pink;
    std::vector<float> mono ((size_t) frames);
    for (int i = 0; i < frames; ++i)
        mono[(size_t) i] = pink.next();
    if (frames > 64)
        highpassInPlace (mono.data(), frames, 20.0f, 48000.0f);
    float peak = 1.0e-12f;
    for (float s : mono)
        peak = std::max (peak, std::abs (s));
    const float g = ampFromDb (peakDb) / peak;
    std::vector<float> data ((size_t) frames * (size_t) channels);
    for (int i = 0; i < frames; ++i)
        for (int c = 0; c < channels; ++c)
            data[(size_t) i * (size_t) channels + (size_t) c] = mono[(size_t) i] * g * (c == 0 ? 1.0f : 0.97f);
    return data;
}

std::vector<float> makeSine (int frames, int channels, double freq, double sr, float dbfs)
{
    Sine osc;
    osc.set (freq, sr, ampFromDb (dbfs));
    std::vector<float> data ((size_t) frames * (size_t) channels);
    for (int i = 0; i < frames; ++i)
    {
        const float s = osc.next();
        for (int c = 0; c < channels; ++c)
            data[(size_t) i * (size_t) channels + (size_t) c] = s;
    }
    return data;
}

float peakAbs (const float* a, const float* b, int n)
{
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
        peak = std::max (peak, std::abs (a[i] - (b != nullptr ? b[i] : 0.0f)));
    return peak;
}

float goertzelAmp (const float* x, int n, double freq, double sr)
{
    double re = 0.0, im = 0.0, win = 0.0;
    const double w = 6.28318530717958647692 * freq / sr;
    for (int i = 0; i < n; ++i)
    {
        const double hann = 0.5 * (1.0 - std::cos (6.28318530717958647692 * (double) i / (double) (n - 1)));
        win += hann;
        const double s = (double) x[i] * hann;
        re += s * std::cos (w * i);
        im -= s * std::sin (w * i);
    }
    if (win <= 0.0)
        return 0.0f;
    return (float) (2.0 * std::hypot (re, im) / win);
}

void highpassInPlace (float* x, int n, float fc, float sr)
{
    const float a = std::exp (-6.28318530718f * fc / sr);
    float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f, x3 = 0.0f, y3 = 0.0f, x4 = 0.0f, y4 = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float s0 = x[i];
        const float s1 = a * (y1 + s0 - x1); x1 = s0; y1 = s1;
        const float s2 = a * (y2 + s1 - x2); x2 = s1; y2 = s2;
        const float s3 = a * (y3 + s2 - x3); x3 = s2; y3 = s3;
        const float s4 = a * (y4 + s3 - x4); x4 = s3; y4 = s4;
        x[i] = s4;
    }
}

float bandDb (const float* x, int n, double sr, float f0, float f1)
{
    const int order = 15;
    const int N = 1 << order;
    std::vector<float> fft (2 * (size_t) N, 0.0f);
    const int use = std::min (n, N);
    for (int i = 0; i < use; ++i)
    {
        const float hann = 0.5f * (1.0f - std::cos (6.28318530717958647692f * (float) i / (float) (use - 1)));
        fft[(size_t) i] = x[i] * hann;
    }
    juce::dsp::FFT transform (order);
    transform.performFrequencyOnlyForwardTransform (fft.data());
    const int a = std::max (1, (int) std::floor (f0 * N / sr));
    const int b = std::min (N / 2, (int) std::ceil (f1 * N / sr));
    double energy = 0.0;
    for (int i = a; i <= b; ++i)
        energy += (double) fft[(size_t) i] * (double) fft[(size_t) i];
    return db ((float) std::sqrt (energy));
}

float rms (const float* x, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; ++i)
        s += (double) x[i] * (double) x[i];
    return (float) std::sqrt (s / std::max (1, n));
}

bool badSample (float x)
{
    return ! std::isfinite (x) || (x != 0.0f && std::abs (x) < 1.0e-30f);
}

void neutral (Host& host)
{
    host.set (abyss::id::depth, 0.0f);
    host.set (abyss::id::pressure, 0.0f);
    host.set (abyss::id::inGain, 0.0f);
    host.set (abyss::id::outGain, 0.0f);
    host.set (abyss::id::mix, 100.0f);
    host.set (abyss::id::subMode, 0.0f);
    host.set (abyss::id::sub2, 1.0f);
    host.set (abyss::id::ceiling, -0.3f);
    host.set (abyss::id::autoGain, 1.0f);
    host.set (abyss::id::os, 0.0f);
    host.set (abyss::id::bypass, 0.0f);
}

void testMath()
{
    if (! want ("math"))
        return;
    section ("fast math");
    CHECK (abyss::dbToGain (0.0f) == 1.0f);
    float prev = abyss::fastTanh (-3.0f);
    float tanhErr = 0.0f;
    for (int i = -300; i <= 300; ++i)
    {
        const float x = (float) i / 100.0f;
        const float y = abyss::fastTanh (x);
        tanhErr = std::max (tanhErr, std::abs (y - std::tanh (x)));
        CHECK (y + 1.0e-6f >= prev);
        prev = y;
    }
    std::printf ("fastTanh max error %g\n", tanhErr);
    CHECK (tanhErr < 1.0e-3f);

    float sinErr = 0.0f;
    for (int i = 0; i <= 4000; ++i)
    {
        const float x = (float) i / 4000.0f * 12.5663706f;
        sinErr = std::max (sinErr, std::abs (abyss::fastSin (x) - std::sin (x)));
    }
    std::printf ("fastSin max error %g\n", sinErr);
    CHECK (sinErr < 1.0e-4f);
}

void testMapping()
{
    if (! want ("map"))
        return;
    section ("depthToLevel");
    const float values[] = { 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100,
                             4.9f, 14.9f, 49.9f, 94.9f };
    for (float v : values)
    {
        const int expect = (int) std::lround (v / 10.0f);
        CHECK (abyss::depthToLevel (v) == expect);
    }
    CHECK (abyss::pressureStage (0.0f) == 0);
    CHECK (abyss::pressureStage (33.0f) == 1);
    CHECK (abyss::pressureStage (67.0f) == 2);
    CHECK (abyss::pressureStage (100.0f) == 3);
}

void testNull (double sr, int channels)
{
    section (channels == 1 ? "null mono" : "null stereo");
    const int frames = (int) sr;
    for (int pass = 0; pass < 2; ++pass)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::os, 0.0f);
        host.prepare (sr, 512, channels);
        auto input = pass == 0 ? makePink (frames, channels, -6.0f) : makeSine (frames, channels, 110.0, sr, -12.0f);
        std::vector<float> output;
        host.render (input, output, nullptr);
        const bool same = std::memcmp (input.data(), output.data(), input.size() * sizeof (float)) == 0;
        if (! same)
            std::printf ("identity residual %.2f dBFS\n", db (peakAbs (input.data(), output.data(), (int) input.size())));
        CHECK (same);

        for (int os = 1; os <= 2; ++os)
        {
            Host osHost;
            neutral (osHost);
            osHost.set (abyss::id::os, (float) os);
            osHost.prepare (sr, 256, channels);
            osHost.silence (0.2);
            std::vector<float> delayed;
            osHost.render (input, delayed, nullptr);
            const int lat = osHost.proc.getLatencySamples();
            const int n = frames - lat - 8;
            float rest = 0.0f;
            for (int i = 0; i < n; ++i)
                for (int c = 0; c < channels; ++c)
                {
                    const float a = input[(size_t) i * (size_t) channels + (size_t) c];
                    const float b = delayed[(size_t) (i + lat) * (size_t) channels + (size_t) c];
                    rest = std::max (rest, std::abs (a - b));
                }
            std::printf ("OS %dx %s residual %.2f dBFS latency %d\n", os == 1 ? 2 : 4, pass ? "sine" : "pink", db (rest), lat);
            CHECK (db (rest) < -90.0f);
        }
    }

    Host mix0;
    neutral (mix0);
    mix0.set (abyss::id::depth, 100.0f);
    mix0.set (abyss::id::pressure, 100.0f);
    mix0.set (abyss::id::mix, 0.0f);
    mix0.set (abyss::id::os, 1.0f);
    mix0.prepare (sr, 256, channels);
    auto input = makePink (frames, channels, -6.0f);
    std::vector<float> output;
    mix0.render (input, output, nullptr);
    const int lat = mix0.proc.getLatencySamples();
    float rest = 0.0f;
    for (int i = 0; i < frames - lat; ++i)
        for (int c = 0; c < channels; ++c)
            rest = std::max (rest, std::abs (input[(size_t) i * channels + c] - output[(size_t) (i + lat) * channels + c]));
    std::printf ("mix 0 residual %.2f dBFS\n", db (rest));
    CHECK (db (rest) < -120.0f);

    for (int os = 0; os <= 2; ++os)
    {
        Host bypass;
        neutral (bypass);
        bypass.set (abyss::id::depth, 100.0f);
        bypass.set (abyss::id::pressure, 100.0f);
        bypass.set (abyss::id::os, (float) os);
        bypass.set (abyss::id::bypass, 1.0f);
        bypass.prepare (sr, 256, channels);
        bypass.silence (0.05);
        std::vector<float> delayed;
        bypass.render (input, delayed, nullptr);
        const int blat = bypass.proc.getLatencySamples();
        float bres = 0.0f;
        for (int i = 0; i < frames - blat; ++i)
            for (int c = 0; c < channels; ++c)
                bres = std::max (bres, std::abs (input[(size_t) i * channels + c] - delayed[(size_t) (i + blat) * channels + c]));
        std::printf ("bypass os %d residual %.2f dBFS latency %d\n", os, db (bres), blat);
        CHECK (db (bres) < -120.0f);
    }
}

void testLatency()
{
    if (! want ("latency"))
        return;
    section ("latency");
    for (int os = 0; os <= 2; ++os)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::os, (float) os);
        host.prepare (48000.0, 128, 2);
        host.silence (0.3);
        CHECK (host.proc.engine.isSettled());
        const int lat = host.proc.getLatencySamples();
        std::vector<float> input ((size_t) 48000 * 2, 0.0f);
        input[0] = 1.0f;
        input[1] = 1.0f;
        std::vector<float> output;
        host.render (input, output, nullptr);
        int peakAt = 0;
        float peak = 0.0f;
        for (int i = 0; i < 4000; ++i)
        {
            const float a = std::abs (output[(size_t) i * 2]);
            if (a > peak)
            {
                peak = a;
                peakAt = i;
            }
        }
        std::printf ("OS %d reported %d measured %d peak %.4f\n", os, lat, peakAt, peak);
        CHECK (peakAt == lat);
        CHECK (std::abs (peak - 1.0f) < 1.0e-4f);
    }
}

void testSub()
{
    if (! want ("sub"))
        return;
    section ("sub");
    const double freqs[] = { 55.0, 110.0, 196.0 };
    for (double freq : freqs)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::depth, 100.0f);
        host.set (abyss::id::pressure, 0.0f);
        host.set (abyss::id::autoGain, 0.0f);
        host.set (abyss::id::os, 0.0f);
        host.set (abyss::id::sub2, 1.0f);
        host.prepare (48000.0, 256, 2);
        const int settleN = 24000;
        const int measN = 48000;
        auto tone = makeSine (settleN + measN, 2, freq, 48000.0, -12.0f);
        std::vector<float> output, tap;
        host.render (tone, output, &tap);
        std::vector<float> left (measN);
        for (int i = 0; i < measN; ++i)
            left[(size_t) i] = output[(size_t) (settleN + i) * 2];
        const float half = db (goertzelAmp (left.data(), measN, freq * 0.5, 48000.0));
        const float quarter = db (goertzelAmp (left.data(), measN, freq * 0.25, 48000.0));
        std::printf ("clean %.1f Hz f/2 %.2f dBFS f/4 %.2f dBFS\n", freq, half, quarter);
        CHECK (half >= -24.0f);
        CHECK (quarter >= -30.0f);

        int crossings = 0;
        double last = -1.0;
        double sum = 0.0;
        for (int i = settleN + 1; i < settleN + measN; ++i)
        {
            const float a = tap[(size_t) i - 1];
            const float b = tap[(size_t) i];
            if (a <= 0.0f && b > 0.0f)
            {
                const float frac = a / (a - b);
                const double at = (double) (i - 1) + (double) frac;
                if (last >= 0.0)
                {
                    sum += at - last;
                    ++crossings;
                }
                last = at;
            }
        }
        if (crossings > 0)
        {
            const double measured = 48000.0 / (sum / crossings);
            const double err = std::abs (measured - freq * 0.5) / (freq * 0.5);
            std::printf ("  sub freq %.2f Hz err %.3f%%\n", measured, err * 100.0);
            CHECK (err < 0.01);
        }
        else
            CHECK (false);
    }

    Host no2;
    neutral (no2);
    no2.set (abyss::id::depth, 100.0f);
    no2.set (abyss::id::autoGain, 0.0f);
    no2.set (abyss::id::os, 0.0f);
    no2.set (abyss::id::sub2, 0.0f);
    no2.prepare (48000.0, 256, 2);
    auto tone = makeSine (72000, 2, 110.0, 48000.0, -12.0f);
    std::vector<float> output;
    no2.render (tone, output, nullptr);
    std::vector<float> left (48000);
    for (int i = 0; i < 48000; ++i)
        left[(size_t) i] = output[(size_t) (24000 + i) * 2];
    const float q = db (goertzelAmp (left.data(), 48000, 27.5, 48000.0));
    std::printf ("sub2 off f/4 %.2f dBFS\n", q);
    CHECK (q < -60.0f);

    Host grit;
    neutral (grit);
    grit.set (abyss::id::depth, 100.0f);
    grit.set (abyss::id::autoGain, 0.0f);
    grit.set (abyss::id::os, 0.0f);
    grit.set (abyss::id::subMode, 1.0f);
    grit.prepare (48000.0, 256, 2);
    grit.render (tone, output, nullptr);
    for (int i = 0; i < 48000; ++i)
        left[(size_t) i] = output[(size_t) (24000 + i) * 2];
    const float gritHalf = db (goertzelAmp (left.data(), 48000, 55.0, 48000.0));
    std::printf ("grit f/2 %.2f dBFS\n", gritHalf);
    CHECK (gritHalf >= -30.0f);

    Host dry;
    neutral (dry);
    dry.set (abyss::id::depth, 0.0f);
    dry.set (abyss::id::autoGain, 0.0f);
    dry.prepare (48000.0, 256, 2);
    dry.render (tone, output, nullptr);
    for (int i = 0; i < 48000; ++i)
        left[(size_t) i] = output[(size_t) (24000 + i) * 2];
    const float leak = db (goertzelAmp (left.data(), 48000, 55.0, 48000.0));
    std::printf ("depth 0 f/2 %.2f dBFS\n", leak);
    CHECK (leak < -100.0f);
}

void testClicks()
{
    if (! want ("click"))
        return;
    section ("clicks");
    Host host;
    neutral (host);
    host.set (abyss::id::depth, 100.0f);
    host.set (abyss::id::pressure, 0.0f);
    host.set (abyss::id::autoGain, 0.0f);
    host.set (abyss::id::os, 0.0f);
    host.prepare (48000.0, 256, 2);
    auto tone = makeSine (96000, 2, 110.0, 48000.0, -12.0f);
    std::vector<float> output, tap;
    host.render (tone, output, &tap);
    float amp = 0.0f;
    float maxDy = 0.0f;
    for (int i = 48000; i < 96000; ++i)
    {
        amp = std::max (amp, std::abs (tap[(size_t) i]));
        maxDy = std::max (maxDy, std::abs (tap[(size_t) i] - tap[(size_t) i - 1]));
    }
    const float limit = 1.5f * 6.28318530718f * 55.0f / 48000.0f * std::max (amp, 1.0e-6f);
    std::printf ("steady sub dy %.6f limit %.6f amp %.4f\n", maxDy, limit, amp);
    CHECK (maxDy <= limit + 1.0e-5f);

    Host jump;
    neutral (jump);
    jump.set (abyss::id::depth, 100.0f);
    jump.set (abyss::id::autoGain, 0.0f);
    jump.set (abyss::id::os, 0.0f);
    jump.prepare (48000.0, 128, 2);
    Sine osc;
    osc.set (110.0, 48000.0, ampFromDb (-12.0f));
    juce::AudioBuffer<float> buf (2, 128);
    juce::MidiBuffer midi;
    std::vector<float> y;
    bool switched = false;
    for (int n = 0; n < 48000; ++n)
    {
        if (! switched && n == 24000)
        {
            osc.set (146.8, 48000.0, ampFromDb (-12.0f));
            switched = true;
        }
        const int i = n % 128;
        const float s = osc.next();
        buf.setSample (0, i, s);
        buf.setSample (1, i, s);
        if (i == 127)
        {
            jump.process (buf);
            for (int k = 0; k < 128; ++k)
                y.push_back (buf.getSample (0, k));
        }
    }
    float steady = 0.0f;
    for (int i = 8000; i < 23000; ++i)
        steady = std::max (steady, std::abs (y[(size_t) i] - y[(size_t) i - 1]));
    float burst = 0.0f;
    float peak = 0.0f;
    for (int i = 24000; i < 36000; ++i)
    {
        burst = std::max (burst, std::abs (y[(size_t) i] - y[(size_t) i - 1]));
        peak = std::max (peak, std::abs (y[(size_t) i]));
    }
    const float ceiling = abyss::dbToGain (-0.3f);
    std::printf ("tone jump dy %.5f steady %.5f peak %.4f\n", burst, steady, peak);
    CHECK (burst <= 3.0f * std::max (steady, 1.0e-6f) + 1.0e-4f);
    CHECK (peak <= ceiling + 0.002f);
}

void testGate()
{
    if (! want ("gate"))
        return;
    section ("gate");
    auto runTap = [] (const std::vector<float>& input, bool& valid, float& tapPeak)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::depth, 100.0f);
        host.set (abyss::id::autoGain, 0.0f);
        host.set (abyss::id::os, 0.0f);
        host.prepare (48000.0, 256, 2);
        std::vector<float> output, tap;
        host.render (input, output, &tap);
        valid = host.proc.engine.trackerValid;
        tapPeak = 0.0f;
        const int n0 = (int) tap.size() - 4800;
        for (int i = n0; i < (int) tap.size(); ++i)
            tapPeak = std::max (tapPeak, std::abs (tap[(size_t) i]));
    };

    bool valid = true;
    float peak = 1.0f;
    runTap (makePink (48000, 2, -70.0f), valid, peak);
    std::printf ("pink -70 sub %.2f dBFS\n", db (peak));
    CHECK (db (peak) < -90.0f);

    runTap (std::vector<float> (48000 * 2, 0.0f), valid, peak);
    std::printf ("silence sub %.2f dBFS\n", db (peak));
    CHECK (db (peak) < -90.0f);

    runTap (makeSine (48000, 2, 20000.0, 48000.0, -12.0f), valid, peak);
    std::printf ("20 kHz valid %d\n", valid ? 1 : 0);
    CHECK (! valid);

    runTap (std::vector<float> (48000 * 2, 0.2f), valid, peak);
    std::printf ("dc valid %d\n", valid ? 1 : 0);
    CHECK (! valid);
}

void testDarkener()
{
    if (! want ("dark"))
        return;
    section ("darkener");
    {
        abyss::Darkener dkr;
        dkr.prepare (48000.0);
        auto src = makePink (48000, 1, -18.0f);
        const float depthsD[] = { 0, 25, 50, 75, 100 };
        std::vector<float> ref (src.begin() + 8000, src.begin() + 40000);
        highpassInPlace (ref.data(), (int) ref.size(), 5000.0f, 48000.0f);
        highpassInPlace (ref.data(), (int) ref.size(), 5000.0f, 48000.0f);
        const float inDb = db (rms (ref.data() + 2000, (int) ref.size() - 2000));
        for (float depth : depthsD)
        {
            dkr.reset();
            std::vector<float> y (src.size());
            for (size_t n = 0; n < src.size(); ++n)
            {
                float oL, oR;
                dkr.process (depth / 100.0f, src[n], src[n], 1, oL, oR);
                y[n] = oL;
            }
            std::vector<float> seg (y.begin() + 8000, y.begin() + 40000);
            highpassInPlace (seg.data(), (int) seg.size(), 5000.0f, 48000.0f);
            highpassInPlace (seg.data(), (int) seg.size(), 5000.0f, 48000.0f);
            std::printf ("darkener unit depth %.0f HF %.2f dB\n", depth,
                         db (rms (seg.data() + 2000, (int) seg.size() - 2000)) - inDb);
        }
    }
    const float depths[] = { 0, 25, 50, 75, 100 };
    float ratios[5] {};
    auto pink = makePink (48000, 2, -18.0f);
    std::vector<float> inLeft (32768);
    for (int n = 0; n < 32768; ++n)
        inLeft[(size_t) n] = pink[(size_t) (8000 + n) * 2];
    highpassInPlace (inLeft.data(), 32768, 5000.0f, 48000.0f);
    highpassInPlace (inLeft.data(), 32768, 5000.0f, 48000.0f);
    const float inBand = db (rms (inLeft.data() + 2000, 30000));
    for (int i = 0; i < 5; ++i)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::depth, depths[i]);
        host.set (abyss::id::pressure, 0.0f);
        host.set (abyss::id::autoGain, 0.0f);
        host.set (abyss::id::os, 0.0f);
        host.prepare (48000.0, 256, 2);
        std::vector<float> output;
        host.render (pink, output, nullptr);
        std::vector<float> left (32768);
        for (int n = 0; n < 32768; ++n)
            left[(size_t) n] = output[(size_t) (8000 + n) * 2];
        highpassInPlace (left.data(), 32768, 5000.0f, 48000.0f);
        highpassInPlace (left.data(), 32768, 5000.0f, 48000.0f);
        const float outBand = db (rms (left.data() + 2000, 30000));
        ratios[i] = outBand - inBand;
        std::printf ("depth %.0f  HF %.2f dB\n", depths[i], ratios[i]);
    }
    CHECK (std::abs (ratios[0]) < 0.5f);
    CHECK (ratios[1] <= -3.0f);
    CHECK (ratios[2] <= -12.0f);
    CHECK (ratios[3] <= -20.0f);
    CHECK (ratios[4] <= -30.0f);
    CHECK (ratios[0] > ratios[1] && ratios[1] > ratios[2] && ratios[2] > ratios[3] && ratios[3] > ratios[4]);
}

void testPressure()
{
    if (! want ("pressure"))
        return;
    section ("pressure");
    {
        abyss::PressureDrive drive;
        drive.prepare (48000.0);
        std::vector<float> y (48000);
        Sine osc;
        osc.set (100.0, 48000.0, ampFromDb (-12.0f));
        for (int i = 0; i < 48000; ++i)
            y[(size_t) i] = drive.process (osc.next(), 1.0f, 0);
        const float h1 = goertzelAmp (y.data(), 48000, 100.0, 48000.0);
        double harm = 0.0;
        for (int k = 2; k <= 10; ++k)
        {
            const float hk = goertzelAmp (y.data(), 48000, 100.0 * k, 48000.0);
            harm += (double) hk * (double) hk;
        }
        const float thd = h1 > 1.0e-8f ? (float) (std::sqrt (harm) / h1) : 0.0f;
        std::printf ("drive alone THD %.2f%% h1 %.2f dBFS\n", thd * 100.0f, db (h1));
    }
    const float pressures[] = { 0, 25, 50, 75, 100 };
    float thd[5] {};
    for (int i = 0; i < 5; ++i)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::depth, 0.0f);
        host.set (abyss::id::pressure, pressures[i]);
        host.set (abyss::id::autoGain, 0.0f);
        host.set (abyss::id::os, 0.0f);
        host.prepare (48000.0, 256, 2);
        auto tone = makeSine (96000, 2, 100.0, 48000.0, -12.0f);
        std::vector<float> output;
        host.render (tone, output, nullptr);
        std::vector<float> left (48000);
        for (int n = 0; n < 48000; ++n)
            left[(size_t) n] = output[(size_t) (48000 + n) * 2];
        const float h1 = goertzelAmp (left.data(), 48000, 100.0, 48000.0);
        double harm = 0.0;
        for (int k = 2; k <= 10; ++k)
        {
            const float hk = goertzelAmp (left.data(), 48000, 100.0 * k, 48000.0);
            harm += (double) hk * (double) hk;
        }
        thd[i] = h1 > 1.0e-8f ? (float) (std::sqrt (harm) / h1) : 0.0f;
        std::printf ("pressure %.0f THD %.2f%%  h1 %.2f dBFS rms %.2f dBFS\n",
                     pressures[i], thd[i] * 100.0f, db (h1), db (rms (left.data(), 48000)));
    }
    CHECK (thd[4] >= 0.05f);
    CHECK (thd[0] < thd[1] && thd[1] < thd[2] && thd[2] < thd[3] && thd[3] < thd[4]);

    float crest[5] {};
    std::vector<float> drum (48000 * 2);
    std::uint32_t rng = 0xA5A5u;
    for (int i = 0; i < 48000; ++i)
    {
        rng = rng * 1664525u + 1013904223u;
        const float white = ((int) (rng >> 9) - 8388608) / 8388608.0f;
        const float t = std::fmod ((float) i / 48000.0f, 0.25f);
        const float env = std::exp (-t / 0.03f);
        const float s = (env * std::sin (6.28318530718f * 60.0f * (float) i / 48000.0f) + 0.05f * white) * 1.5f;
        drum[(size_t) i * 2] = drum[(size_t) i * 2 + 1] = s;
    }
    for (int i = 0; i < 5; ++i)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::depth, 0.0f);
        host.set (abyss::id::pressure, pressures[i]);
        host.set (abyss::id::autoGain, 0.0f);
        host.set (abyss::id::os, 0.0f);
        host.prepare (48000.0, 256, 2);
        std::vector<float> output;
        host.render (drum, output, nullptr);
        float peak = 0.0f;
        double e = 0.0;
        for (int n = 4000; n < 48000; ++n)
        {
            const float s = output[(size_t) n * 2];
            peak = std::max (peak, std::abs (s));
            e += (double) s * (double) s;
        }
        const float r = std::sqrt ((float) (e / 44000.0));
        crest[i] = r > 1.0e-8f ? peak / r : 0.0f;
        std::printf ("pressure %.0f crest %.3f peak %.4f rms %.4f\n", pressures[i], crest[i], peak, r);
    }
    CHECK (crest[0] + 0.02f >= crest[1] && crest[1] + 0.02f >= crest[2]
           && crest[2] + 0.02f >= crest[3] && crest[3] + 0.02f >= crest[4]);
}

void testAutoGain()
{
    if (! want ("autogain"))
        return;
    section ("autogain");
    const float vals[] = { 0, 50, 100 };
    auto pink = makePink (48000 * 3, 2, -18.0f);
    float worst = 0.0f;
    for (float d : vals)
        for (float p : vals)
        {
            Host host;
            neutral (host);
            host.set (abyss::id::depth, d);
            host.set (abyss::id::pressure, p);
            host.set (abyss::id::autoGain, 1.0f);
            host.set (abyss::id::os, 0.0f);
            host.prepare (48000.0, 256, 2);
            std::vector<float> output;
            host.render (pink, output, nullptr);
            const int a = 48000 * 2;
            const float inR = rms (pink.data() + a * 2, 48000 * 2);
            const float outR = rms (output.data() + a * 2, 48000 * 2);
            const float delta = std::abs (db (outR) - db (inR));
            worst = std::max (worst, delta);
            std::printf ("D %.0f P %.0f  |dRMS| %.2f dB\n", d, p, delta);
            CHECK (delta <= 1.5f);
        }
    std::printf ("autogain worst %.2f dB\n", worst);
}

void testRobust()
{
    if (! want ("robust"))
        return;
    section ("robust");
    const float vals[] = { 0, 50, 100 };
    for (float d : vals)
        for (float p : vals)
            for (int mode = 0; mode < 2; ++mode)
                for (int os = 0; os < 3; ++os)
                {
                    Host host;
                    neutral (host);
                    host.set (abyss::id::depth, d);
                    host.set (abyss::id::pressure, p);
                    host.set (abyss::id::subMode, (float) mode);
                    host.set (abyss::id::os, (float) os);
                    host.prepare (48000.0, 128, 2);
                    auto feed = [&] (std::vector<float> input)
                    {
                        std::vector<float> output;
                        host.render (input, output, nullptr);
                        for (float s : output)
                            if (badSample (s))
                                return false;
                        return true;
                    };
                    CHECK (feed (std::vector<float> (4096, 0.0f)));
                    CHECK (feed (makePink (4096, 2, 40.0f)));
                    CHECK (feed (std::vector<float> (4096, 0.25f)));
                    std::vector<float> impulse (4096, 0.0f);
                    impulse[0] = impulse[1] = 1.0f;
                    CHECK (feed (impulse));
                    std::vector<float> sweep (8192);
                    Sine osc;
                    for (int i = 0; i < 4096; ++i)
                    {
                        const double f = 20.0 * std::pow (1000.0, (double) i / 4095.0);
                        osc.set (f, 48000.0, 0.2f);
                        const float s = osc.next();
                        sweep[(size_t) i * 2] = sweep[(size_t) i * 2 + 1] = s;
                    }
                    CHECK (feed (sweep));
                    Pink tiny;
                    std::vector<float> den (4096);
                    for (int i = 0; i < 2048; ++i)
                    {
                        const float s = tiny.white() * 1.0e-30f;
                        den[(size_t) i * 2] = den[(size_t) i * 2 + 1] = s;
                    }
                    CHECK (feed (den));
                }
    std::printf ("robust matrix done\n");
}

void testGain()
{
    if (! want ("gain"))
        return;
    section ("gain");
    auto checkGain = [] (float inDb, float outDb)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::inGain, inDb);
        host.set (abyss::id::outGain, outDb);
        host.set (abyss::id::os, 0.0f);
        host.prepare (48000.0, 256, 2);
        auto tone = makeSine (48000, 2, 1000.0, 48000.0, -18.0f);
        std::vector<float> output;
        host.render (tone, output, nullptr);
        const float inR = rms (tone.data() + 24000 * 2, 12000 * 2);
        const float outR = rms (output.data() + 24000 * 2, 12000 * 2);
        const float got = db (outR) - db (inR);
        const float expect = inDb + outDb;
        std::printf ("in %+.1f out %+.1f -> %+.3f dB\n", inDb, outDb, got);
        CHECK (std::abs (got - expect) <= 0.01f);
    };
    checkGain (6.0f, 0.0f);
    checkGain (-6.0f, 0.0f);
    checkGain (0.0f, 6.0f);
    checkGain (0.0f, -6.0f);

    for (int os = 0; os <= 2; ++os)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::depth, 100.0f);
        host.set (abyss::id::pressure, 100.0f);
        host.set (abyss::id::os, (float) os);
        host.set (abyss::id::mix, 100.0f);
        host.set (abyss::id::outGain, 0.0f);
        host.set (abyss::id::ceiling, -1.0f);
        host.prepare (48000.0, 128, 2);
        host.proc.engine.clipPeak = 0.0f;
        auto hot = makePink ((int) 48000, 2, -6.0f);
        std::vector<float> output;
        host.render (hot, output, nullptr);
        float peak = 0.0f;
        for (float s : output)
            peak = std::max (peak, std::abs (s));
        const float clipDb = db (host.proc.engine.clipPeak);
        const float outDb = db (peak);
        const float slack = os == 0 ? 0.05f : 0.5f;
        std::printf ("os %d clip %.2f dBFS out %.2f dBFS\n", os, clipDb, outDb);
        CHECK (clipDb <= -1.0f + 0.05f);
        CHECK (outDb <= -1.0f + slack);
    }
}

void testAutomation()
{
    if (! want ("auto"))
        return;
    section ("automation");
    auto run = [] (const char* paramId, float low, float high, double freq)
    {
        Host host;
        neutral (host);
        host.set (abyss::id::autoGain, 0.0f);
        host.set (abyss::id::os, 1.0f);
        host.set (paramId, low);
        host.prepare (48000.0, 256, 2);
        host.silence (0.2);
        Sine osc;
        osc.set (freq, 48000.0, ampFromDb (-12.0f));
        const int total = 48000;
        std::vector<float> y (total);
        juce::AudioBuffer<float> buf (2, 256);
        bool highNow = false;
        for (int n = 0; n < total; ++n)
        {
            if (n % 2048 == 0)
            {
                highNow = ! highNow;
                host.set (paramId, highNow ? high : low);
            }
            const int i = n % 256;
            const float s = osc.next();
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
            if (i == 255 || n + 1 == total)
            {
                const int count = i + 1;
                juce::AudioBuffer<float> block (2, count);
                for (int c = 0; c < 2; ++c)
                    block.copyFrom (c, 0, buf, c, 0, count);
                host.process (block);
                for (int k = 0; k < count; ++k)
                    y[(size_t) (n - i + k)] = block.getSample (0, k);
            }
        }
        auto measureSteady = [&] (float setting)
        {
            host.set (paramId, setting);
            host.silence (0.25);
            Sine ref;
            ref.set (freq, 48000.0, ampFromDb (-12.0f));
            float prev = 0.0f;
            float steady = 0.0f;
            for (int n = 0; n < 8192; ++n)
            {
                const int i = n % 256;
                const float s = ref.next();
                buf.setSample (0, i, s);
                buf.setSample (1, i, s);
                if (i == 255)
                {
                    host.process (buf);
                    for (int k = 0; k < 256; ++k)
                    {
                        const float now = buf.getSample (0, k);
                        if (n > 2048)
                            steady = std::max (steady, std::abs (now - prev));
                        prev = now;
                    }
                }
            }
            return steady;
        };
        const float steadyHi = measureSteady (high);
        const float steadyLo = measureSteady (low);
        const float steady = std::max (steadyHi, steadyLo);
        float burst = 0.0f;
        float peak = 0.0f;
        int burstAt = 0;
        const float ceiling = abyss::dbToGain (-0.3f);
        for (int n = 512; n < total; ++n)
        {
            const float dy = std::abs (y[(size_t) n] - y[(size_t) n - 1]);
            if (dy > burst)
            {
                burst = dy;
                burstAt = n;
            }
            peak = std::max (peak, std::abs (y[(size_t) n]));
        }
        std::printf ("%s dy %.5f at %d mod %d  [%.3f %.3f %.3f %.3f %.3f] steady %.5f peak %.3f\n",
                     paramId, burst, burstAt, burstAt % 2048,
                     y[(size_t) burstAt - 2], y[(size_t) burstAt - 1], y[(size_t) burstAt],
                     y[(size_t) std::min (burstAt + 1, total - 1)],
                     y[(size_t) std::min (burstAt + 2, total - 1)],
                     steady, peak);
        CHECK (burst <= 1.5f * std::max (steady, 1.0e-5f) + 1.0e-4f);
        CHECK (db (peak) <= -0.3f + 0.5f);
        juce::ignoreUnused (ceiling);
    };
    run (abyss::id::depth, 0.0f, 100.0f, 1000.0);
    run (abyss::id::depth, 0.0f, 100.0f, 110.0);
    run (abyss::id::pressure, 0.0f, 100.0f, 110.0);
    run (abyss::id::subMode, 0.0f, 1.0f, 110.0);
    run (abyss::id::sub2, 0.0f, 1.0f, 110.0);
    run (abyss::id::inGain, -12.0f, 12.0f, 110.0);
    run (abyss::id::outGain, -12.0f, 12.0f, 110.0);
    run (abyss::id::mix, 0.0f, 100.0f, 110.0);
    run (abyss::id::bypass, 0.0f, 1.0f, 110.0);
    run (abyss::id::os, 0.0f, 2.0f, 110.0);
}

void testAlloc()
{
    if (! want ("alloc"))
        return;
    section ("alloc");
    Host host;
    neutral (host);
    host.prepare (48000.0, 128, 2);
    host.silence (0.05);
    auto* depth = host.proc.apvts.getRawParameterValue (abyss::id::depth);
    auto* pressure = host.proc.apvts.getRawParameterValue (abyss::id::pressure);
    auto* mode = host.proc.apvts.getRawParameterValue (abyss::id::subMode);
    auto* os = host.proc.apvts.getRawParameterValue (abyss::id::os);
    juce::AudioBuffer<float> buf (2, 128);
    juce::MidiBuffer midi;
    Sine osc;
    osc.set (80.0, 48000.0, 0.1f);
    gAllocCount = 0;
    gAllocWatch = 1;
    const int total = 48000 * 10;
    for (int n = 0; n < total; n += 128)
    {
        if ((n % 2048) == 0)
        {
            depth->store ((n / 2048) % 2 == 0 ? 0.0f : 100.0f);
            pressure->store ((n / 4096) % 2 == 0 ? 0.0f : 100.0f);
            mode->store ((n / 8192) % 2 == 0 ? 0.0f : 1.0f);
            os->store ((float) ((n / 16384) % 3));
        }
        for (int i = 0; i < 128; ++i)
        {
            const float s = osc.next();
            buf.setSample (0, i, s);
            buf.setSample (1, i, s);
        }
        host.proc.processBlock (buf, midi);
    }
    gAllocWatch = 0;
    std::printf ("audio-thread allocations %d\n", gAllocCount);
    CHECK (gAllocCount == 0);
}

void testState()
{
    if (! want ("state"))
        return;
    section ("state");
    Host host;
    neutral (host);
    host.prepare (48000.0, 128, 2);
    host.set (abyss::id::depth, 42.0f);
    host.set (abyss::id::pressure, 17.0f);
    host.set (abyss::id::inGain, -3.5f);
    host.set (abyss::id::mix, 63.0f);
    host.set (abyss::id::subMode, 1.0f);
    host.set (abyss::id::sub2, 0.0f);
    host.set (abyss::id::os, 2.0f);
    host.set (abyss::id::ceiling, -2.5f);
    host.proc.uiScale.store (4);
    host.proc.animate.store (false);
    host.proc.crtFx.store (false);
    juce::MemoryBlock block;
    host.proc.getStateInformation (block);
    Host copy;
    copy.prepare (48000.0, 128, 2);
    copy.proc.setStateInformation (block.getData(), (int) block.getSize());
    auto almost = [] (float a, float b) { return std::abs (a - b) < 0.05f; };
    CHECK (almost (copy.proc.apvts.getRawParameterValue (abyss::id::depth)->load(), 42.0f));
    CHECK (almost (copy.proc.apvts.getRawParameterValue (abyss::id::pressure)->load(), 17.0f));
    CHECK (almost (copy.proc.apvts.getRawParameterValue (abyss::id::inGain)->load(), -3.5f));
    CHECK (almost (copy.proc.apvts.getRawParameterValue (abyss::id::mix)->load(), 63.0f));
    CHECK (almost (copy.proc.apvts.getRawParameterValue (abyss::id::subMode)->load(), 1.0f));
    CHECK (copy.proc.apvts.getRawParameterValue (abyss::id::sub2)->load() < 0.5f);
    CHECK (almost (copy.proc.apvts.getRawParameterValue (abyss::id::os)->load(), 2.0f));
    CHECK (copy.proc.uiScale.load() == 4);
    CHECK (! copy.proc.animate.load());
    CHECK (! copy.proc.crtFx.load());

    copy.proc.setStateInformation (nullptr, 0);
    const char junk[] = "not-a-state";
    copy.proc.setStateInformation (junk, (int) sizeof junk);
    CHECK (copy.proc.getName() == "Abyss");

    const float expectD[] = { 0, 15, 30, 45, 55, 65, 80, 60, 90, 100 };
    const float expectP[] = { 0, 10, 20, 30, 15, 50, 40, 90, 75, 100 };
    const int expectM[] = { 0, 0, 0, 0, 0, 0, 0, 1, 1, 1 };
    for (int i = 0; i < abyss::kNumFactoryPresets; ++i)
    {
        host.proc.setCurrentProgram (i);
        CHECK (host.proc.getProgramName (i) == abyss::factoryPreset (i).name);
        CHECK (almost (host.proc.apvts.getRawParameterValue (abyss::id::depth)->load(), expectD[i]));
        CHECK (almost (host.proc.apvts.getRawParameterValue (abyss::id::pressure)->load(), expectP[i]));
        CHECK ((int) std::lround (host.proc.apvts.getRawParameterValue (abyss::id::subMode)->load()) == expectM[i]);
    }
}

bool rectHas (int x, int y, int rx, int ry, int rw, int rh)
{
    return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

bool excludedPixel (int x, int y)
{
    return rectHas (x, y, 43, 132, 24, 24)
        || rectHas (x, y, 133, 132, 24, 24)
        || rectHas (x, y, 79, 140, 12, 12)
        || rectHas (x, y, 101, 140, 12, 12)
        || rectHas (x, y, 182, 136, 20, 16)
        || rectHas (x, y, 74, 113, 92, 10)
        || rectHas (x, y, abyss::layout::kCreditX, abyss::layout::kCreditY, 59, 7);
}

bool sameRgb (juce::Colour a, juce::Colour b)
{
    return a.getRed() == b.getRed() && a.getGreen() == b.getGreen() && a.getBlue() == b.getBlue();
}

void testAssetsAndUi()
{
    if (! want ("ui") && ! want ("asset"))
        return;
    section ("assets");
    juce::SharedResourcePointer<abyss::SpriteBank> bank;
    CHECK (bank->loaded);
    CHECK (bank->cabinet.w == 240 && bank->cabinet.h == 186);
    CHECK (bank->screenOff.w == 112 && bank->screenOff.h == 74);
    CHECK (bank->depthStrip.w == 24 && bank->depthStrip.h == 1536);
    CHECK (bank->pressureStrip.w == 24 && bank->pressureStrip.h == 1536);
    CHECK (bank->smallStrip.w == 12 && bank->smallStrip.h == 384);
    CHECK (bank->bypassOff.w == 20 && bank->bypassOff.h == 16);
    CHECK (bank->bypassOn.w == 20 && bank->bypassOn.h == 16);
    CHECK (bank->cabinet.countTransparent() == 8268);
    for (int y = 0; y < bank->cabinet.h; ++y)
        for (int x = 0; x < bank->cabinet.w; ++x)
            if (bank->cabinet.at (x, y) == 255)
                CHECK (rectHas (x, y, 64, 38, 112, 74));

    int prevRed = 0;
    for (int level = 0; level < 11; ++level)
    {
        CHECK (bank->anim[level][0].w == 112 && bank->anim[level][0].h == 74);
        CHECK (! bank->anim[level][0].samePixels (bank->anim[level][1]));
        if (level <= 3)
            CHECK (bank->anim[level][2].samePixels (bank->anim[level][0]));
        else
            CHECK (! bank->anim[level][2].samePixels (bank->anim[level][0]));
        const int red = bank->anim[level][0].countIndexRange (13, 15);
        if (level < 4)
            CHECK (red == 0);
        else
            CHECK (red > prevRed);
        prevRed = red;
        for (int frame = 0; frame < 3; ++frame)
            for (auto v : bank->anim[level][frame].idx)
                CHECK (v <= 18);
    }

    Host host;
    neutral (host);
    host.proc.animate.store (false);
    host.proc.crtFx.store (true);
    host.prepare (48000.0, 128, 2);
    auto editor = std::unique_ptr<abyss::AbyssAudioProcessorEditor> (static_cast<abyss::AbyssAudioProcessorEditor*> (host.proc.createEditor()));
    editor->setUiScale (1);

    auto compareDepth = [&] (float depth, bool bypass, float pressure, bool expectFx)
    {
        host.set (abyss::id::depth, depth);
        host.set (abyss::id::pressure, pressure);
        host.set (abyss::id::bypass, bypass ? 1.0f : 0.0f);
        editor->pumpUiForTest();
        auto snap = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        const int level = abyss::depthToLevel (depth);
        const int stage = expectFx ? abyss::pressureStage (pressure) : 0;
        abyss::IndexedImage screen = bypass ? bank->screenOff : bank->anim[level][0];
        if (! bypass && stage > 0)
            screen = abyss::applyPressureFx (bank->anim[level][0], stage, 0);
        int mismatches = 0;
        for (int y = 0; y < 186 && mismatches < 6; ++y)
            for (int x = 0; x < 240 && mismatches < 6; ++x)
            {
                if (excludedPixel (x, y))
                    continue;
                juce::Colour expect;
                if (bank->cabinet.at (x, y) == 255)
                {
                    const int sx = x - 64;
                    const int sy = y - 38;
                    const auto idx = screen.at (sx, sy);
                    if (idx == 255)
                        continue;
                    expect = abyss::colourForIndex (idx);
                }
                else
                    expect = abyss::colourForIndex (bank->cabinet.at (x, y));
                const auto got = snap.getPixelAt (x, y);
                if (! sameRgb (got, expect))
                {
                    std::printf ("pixel %d,%d got %02X%02X%02X expect %02X%02X%02X depth %.0f\n",
                                 x, y, got.getRed(), got.getGreen(), got.getBlue(),
                                 expect.getRed(), expect.getGreen(), expect.getBlue(), depth);
                    ++mismatches;
                }
            }
        CHECK (mismatches == 0);
    };

    for (int d = 0; d <= 100; d += 5)
        compareDepth ((float) d, false, 0.0f, false);
    compareDepth (0.0f, true, 0.0f, false);
    host.proc.crtFx.store (true);
    const float stagePressure[] = { 0.0f, 33.0f, 67.0f, 100.0f };
    for (float pressure : stagePressure)
        compareDepth (60.0f, false, pressure, true);
    host.proc.crtFx.store (false);
    compareDepth (60.0f, false, 100.0f, false);

    auto snap1 = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    for (int s = 2; s <= 5; ++s)
    {
        editor->setUiScale (s);
        auto snap = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        auto up = abyss::scaleNearest (snap1, s);
        CHECK (snap.getWidth() == up.getWidth() && snap.getHeight() == up.getHeight());
        int bad = 0;
        for (int y = 0; y < up.getHeight() && bad < 4; ++y)
            for (int x = 0; x < up.getWidth() && bad < 4; ++x)
                if (! sameRgb (snap.getPixelAt (x, y), up.getPixelAt (x, y)))
                {
                    std::printf ("scale %d mismatch %d,%d\n", s, x, y);
                    ++bad;
                }
        CHECK (bad == 0);
    }

    editor->setUiScale (1);
    host.proc.animate.store (true);
    host.set (abyss::id::bypass, 0.0f);
    editor->addToDesktop (0);
    editor->setVisible (true);
    CHECK (editor->isShowing());
    CHECK (editor->isTimerRunning());
    const int seq[] = { 0, 1, 0, 1, 0, 1, 0, 2 };
    for (int i = 0; i < 8; ++i)
    {
        editor->advanceAnimationForTest();
        CHECK (editor->animationFrame() == seq[i]);
    }
    host.proc.animate.store (false);
    editor->setVisible (false);
    editor->setVisible (true);
    CHECK (! editor->isTimerRunning());
    host.proc.animate.store (true);
    editor->setVisible (true);
    host.set (abyss::id::bypass, 1.0f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
    CHECK (! editor->isTimerRunning());
    editor->setVisible (false);
    CHECK (! editor->isTimerRunning());
    editor.reset();
}

void testMemory()
{
    if (! want ("memory"))
        return;
    section ("memory");
    Host host;
    neutral (host);
    host.prepare (48000.0, 128, 2);
    auto bytes = [] {
        PROCESS_MEMORY_COUNTERS_EX info {};
        GetProcessMemoryInfo (GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*) &info, sizeof info);
        return info.PrivateUsage;
    };
    std::uint64_t after5 = 0;
    std::uint64_t after50 = 0;
    for (int i = 0; i < 50; ++i)
    {
        auto editor = std::unique_ptr<abyss::AbyssAudioProcessorEditor> (
            static_cast<abyss::AbyssAudioProcessorEditor*> (host.proc.createEditor()));
        editor->setUiScale (1);
        host.proc.animate.store (true);
        editor->addToDesktop (0);
        editor->setVisible (true);
        const auto start = juce::Time::getMillisecondCounter();
        while (juce::Time::getMillisecondCounter() - start < 1000)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        editor->setVisible (false);
        editor.reset();
        if (i == 4)
            after5 = bytes();
        if (i == 49)
            after50 = bytes();
    }
    const double growthMb = (double) ((std::int64_t) after50 - (std::int64_t) after5) / (1024.0 * 1024.0);
    std::printf ("editor memory growth %.2f MB\n", growthMb);
    CHECK (growthMb < 5.0);

    auto editor = std::unique_ptr<abyss::AbyssAudioProcessorEditor> (
        static_cast<abyss::AbyssAudioProcessorEditor*> (host.proc.createEditor()));
    editor->setUiScale (5);
    editor->addToDesktop (0);
    editor->setVisible (true);
    const auto peak = bytes();
    std::printf ("editor 5x private %.2f MB\n", peak / (1024.0 * 1024.0));
    editor.reset();
}

void testEditorCpu()
{
    if (! want ("cpu"))
        return;
    section ("editor cpu");
    Host host;
    neutral (host);
    host.prepare (48000.0, 128, 2);
    auto editor = std::unique_ptr<abyss::AbyssAudioProcessorEditor> (
        static_cast<abyss::AbyssAudioProcessorEditor*> (host.proc.createEditor()));
    editor->setUiScale (3);
    editor->addToDesktop (0);

    auto sample = [] (double seconds, bool animate, abyss::AbyssAudioProcessorEditor& ed, abyss::AbyssAudioProcessor& proc)
    {
        proc.animate.store (animate);
        ed.setVisible (true);
        FILETIME c {}, e {}, k0 {}, u0 {}, k1 {}, u1 {};
        GetProcessTimes (GetCurrentProcess(), &c, &e, &k0, &u0);
        const auto start = juce::Time::getMillisecondCounter();
        while (juce::Time::getMillisecondCounter() - start < (juce::uint32) (seconds * 1000.0))
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);
        GetProcessTimes (GetCurrentProcess(), &c, &e, &k1, &u1);
        auto to100 = [] (const FILETIME& a, const FILETIME& b)
        {
            ULARGE_INTEGER ua, ub;
            ua.LowPart = a.dwLowDateTime;
            ua.HighPart = a.dwHighDateTime;
            ub.LowPart = b.dwLowDateTime;
            ub.HighPart = b.dwHighDateTime;
            return (double) (ub.QuadPart - ua.QuadPart) * 1.0e-7;
        };
        return 100.0 * (to100 (k0, k1) + to100 (u0, u1)) / seconds;
    };

    host.proc.animate.store (false);
    editor->setVisible (false);
    editor->setVisible (true);
    const double idle = sample (10.0, false, *editor, host.proc);
    const double moving = sample (10.0, true, *editor, host.proc);
    std::printf ("editor idle %.2f%% animating %.2f%%\n", idle, moving);
    CHECK (idle < 1.0);
    CHECK (moving < 2.0);
    editor.reset();
}
}

void* operator new (std::size_t n)
{
    if (gAllocWatch)
        ++gAllocCount;
    if (void* p = std::malloc (n == 0 ? 1 : n))
        return p;
    throw std::bad_alloc();
}

void* operator new[] (std::size_t n) { return ::operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

void* operator new (std::size_t n, const std::nothrow_t&) noexcept
{
    try { return ::operator new (n); } catch (...) { return nullptr; }
}
void* operator new[] (std::size_t n, const std::nothrow_t&) noexcept
{
    return ::operator new (n, std::nothrow);
}
void operator delete (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }

void* operator new (std::size_t n, std::align_val_t align)
{
    if (gAllocWatch)
        ++gAllocCount;
    const auto a = std::max<std::size_t> ((std::size_t) align, sizeof (void*));
    if (void* p = _aligned_malloc (n == 0 ? 1 : n, a))
        return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n, std::align_val_t align) { return ::operator new (n, align); }
void operator delete (void* p, std::align_val_t) noexcept { _aligned_free (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { _aligned_free (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { _aligned_free (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { _aligned_free (p); }

int main (int argc, char** argv)
{
    if (argc > 1)
        gFilter = argv[1];

    juce::ScopedJuceInitialiser_GUI gui;
    testMath();
    testMapping();
    if (want ("null"))
    {
        section ("null");
        testNull (48000.0, 2);
        testNull (48000.0, 1);
        testNull (44100.0, 2);
        testNull (44100.0, 1);
    }
    testLatency();
    testSub();
    testClicks();
    testGate();
    testDarkener();
    testPressure();
    testAutoGain();
    testRobust();
    testGain();
    testAutomation();
    testAlloc();
    testState();
    testAssetsAndUi();
    testMemory();
    testEditorCpu();

    std::printf ("\n%s  %d failed\n", gFails == 0 ? "PASS" : "FAIL", gFails);
    return gFails == 0 ? 0 : 1;
}
