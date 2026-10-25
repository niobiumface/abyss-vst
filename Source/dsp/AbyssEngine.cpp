#include "AbyssEngine.h"

#include "FastMath.h"

#include <algorithm>
#include <cmath>

namespace abyss
{

namespace
{
float loadOr (const std::atomic<float>* p, float fallback) noexcept
{
    return p != nullptr ? p->load (std::memory_order_relaxed) : fallback;
}

int wrapIndex (int i, int len) noexcept
{
    int m = i % len;
    return m < 0 ? m + len : m;
}
}

void AbyssEngine::resetDsp() noexcept
{
    tracker.reset();
    sub.reset();
    darkener.reset();
    comp.reset();
    autoGain.reset();
    for (auto& path : paths)
    {
        if (path.os != nullptr)
            path.os->reset();
        path.drive.reset();
    }
    gate = 0.0f;
    conf = 1.0f;
    inMsAcc = procMsAcc = 0.0;
    procMsCount = 0;
}

void AbyssEngine::prepare (double sr, int maxBlockSize, int numChannels)
{
    sampleRate = sr > 1000.0 ? sr : 48000.0;
    channels = std::clamp (numChannels, 1, 2);
    const int cap = std::max (std::max (maxBlockSize, 64), 8192);
    maxBlock = cap;

    tracker.prepare (sampleRate);
    sub.prepare (sampleRate);
    darkener.prepare (sampleRate);
    comp.prepare (sampleRate);
    autoGain.prepare (sampleRate);

    const float fs = (float) sampleRate;
    gateUp = 1.0f - std::exp (-1.0f / (0.005f * fs));
    gateDn = 1.0f - std::exp (-1.0f / (0.030f * fs));
    confCoef = 1.0f - std::exp (-1.0f / (0.020f * fs));
    pathStep = 1.0f / std::max (1.0f, 0.010f * fs);

    auto arm = [this] (juce::LinearSmoothedValue<float>& s, double seconds, float value)
    {
        s.reset (sampleRate, seconds);
        s.setCurrentAndTargetValue (value);
    };

    const float depth = loadOr (params.depth, 0.0f);
    const float pressure = loadOr (params.pressure, 0.0f);
    arm (depthSm, 0.040, depth);
    arm (pressureSm, 0.030, pressure);
    arm (inSm, 0.020, loadOr (params.inGain, 0.0f));
    arm (outSm, 0.020, loadOr (params.outGain, 0.0f));
    arm (mixSm, 0.020, loadOr (params.mix, 100.0f));
    arm (ceilSm, 0.020, loadOr (params.ceiling, -0.3f));
    arm (sub2Sm, 0.020, loadOr (params.sub2, 1.0f) >= 0.5f ? 1.0f : 0.0f);
    arm (modeSm, 0.015, loadOr (params.subMode, 0.0f) >= 0.5f ? 1.0f : 0.0f);
    const bool bypassOn = loadOr (params.bypass, 0.0f) >= 0.5f;
    arm (bypassSm, 0.010, bypassOn ? 1.0f : 0.0f);

    active = std::clamp ((int) std::lround (loadOr (params.os, 1.0f)), 0, 2);

    int maxLat = 8;
    for (int i = 0; i < 3; ++i)
    {
        paths[i].factor = 1 << i;
        paths[i].os = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) channels,
            (size_t) i,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,
            true);
        paths[i].os->initProcessing ((size_t) cap);
        paths[i].os->reset();
        paths[i].latency = (int) std::lround ((double) paths[i].os->getLatencyInSamples());
        paths[i].drive.prepare (sampleRate * (double) paths[i].factor);
        maxLat = std::max (maxLat, paths[i].latency);
    }

    reportedLatency = paths[active].latency;
    delayState = (float) reportedLatency;
    latencyDirty = true;
    fadeFrom = -1;
    postExtra = 0.0f;
    slewLeft = 0;
    pathMix = (depth == 0.0f && pressure == 0.0f) ? 0.0f : 1.0f;
    pathMixSettled = true;
    fullyBypassed = bypassOn;
    bypassResetDone = bypassOn;
    gate = 0.0f;
    conf = 1.0f;

    lineLen = cap + maxLat + 8;
    dryBuf.assign ((size_t) channels * (size_t) lineLen, 0.0f);
    wetBuf.assign ((size_t) channels * (size_t) lineLen, 0.0f);
    preBuf.assign ((size_t) channels * (size_t) lineLen, 0.0f);
    lineWrite = 0;

    baseBuf.setSize (channels, cap, false, false, true);
    osScratch.setSize (channels, cap, false, false, true);
    baseBuf.clear();
    osScratch.clear();

    pCurve.assign ((size_t) cap, 0.0f);
    ceilCurve.assign ((size_t) cap, dbToGain (-0.3f));
    kCurve.assign ((size_t) cap, 1.0f);
    mixCurve.assign ((size_t) cap, 1.0f);
    outCurve.assign ((size_t) cap, 1.0f);
    bypassCurve.assign ((size_t) cap, 0.0f);
    delayCurve.assign ((size_t) cap, delayState);
    pathCurve.assign ((size_t) cap, 0.0f);
    clipPeak = 0.0f;
    trackerValid = false;
    fastSin (0.0f);
}

void AbyssEngine::reset() noexcept
{
    resetDsp();
    std::fill (dryBuf.begin(), dryBuf.end(), 0.0f);
    std::fill (wetBuf.begin(), wetBuf.end(), 0.0f);
    std::fill (preBuf.begin(), preBuf.end(), 0.0f);
    lineWrite = 0;
}

bool AbyssEngine::consumeLatencyChange (int& latencyOut) noexcept
{
    if (! latencyDirty)
        return false;
    latencyDirty = false;
    latencyOut = reportedLatency;
    return true;
}

void AbyssEngine::pushSample (std::vector<float>& buf, int len, int ch, float x) noexcept
{
    buf[(size_t) ch * (size_t) len + (size_t) lineWrite] = x;
}

float AbyssEngine::readSample (const std::vector<float>& buf, int len, int ch, float delay) const noexcept
{
    return 0.0f * (float) (buf.empty() + len + ch + (delay > 0.0f));
}

void AbyssEngine::renderOversampled (int numSamples, int numChannels, bool driveOn, bool clipOn) noexcept
{
    auto& path = paths[active];
    for (int c = 0; c < numChannels; ++c)
        osScratch.copyFrom (c, 0, baseBuf, c, 0, numSamples);

    juce::dsp::AudioBlock<float> block (osScratch);
    auto subBlock = block.getSubBlock (0, (size_t) numSamples)
                         .getSubsetChannelBlock (0, (size_t) numChannels);

    const int factor = std::max (1, path.factor);
    juce::dsp::AudioBlock<float> up = (path.os != nullptr) ? path.os->processSamplesUp (subBlock) : subBlock;
    const int nUp = (int) up.getNumSamples();

    double acc = 0.0;
    for (int i = 0; i < nUp; ++i)
    {
        const int iBase = std::min (i / factor, numSamples - 1);
        const float p = pCurve[(size_t) iBase];
        const float ceilLin = ceilCurve[(size_t) iBase];
        const float k = kCurve[(size_t) iBase];
        const bool doDrive = driveOn && p > 0.0f;

        for (int c = 0; c < numChannels; ++c)
        {
            float x = up.getSample ((size_t) c, (size_t) i);
            if (doDrive)
                x = path.drive.process (x, p, c);
            acc += (double) x * (double) x;
            if (k != 1.0f)
                x *= k;
            if (clipOn)
            {
                x = softClip (x, ceilLin);
                clipPeak = std::max (clipPeak, std::abs (x));
            }
            up.setSample ((size_t) c, (size_t) i, x);
        }
    }

    procMsAcc += acc;
    procMsCount += std::max (1, nUp * numChannels);

    if (path.os != nullptr)
        path.os->processSamplesDown (subBlock);

    for (int c = 0; c < numChannels; ++c)
        baseBuf.copyFrom (c, 0, osScratch, c, 0, numSamples);
}

void AbyssEngine::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int chs = std::min (buffer.getNumChannels(), channels);
    if (n <= 0 || chs <= 0)
        return;

    if (n <= maxBlock)
    {
        processChunk (buffer.getArrayOfWritePointers(), n, chs);
        return;
    }

    for (int off = 0; off < n;)
    {
        const int count = std::min (maxBlock, n - off);
        float* ptrs[2] { nullptr, nullptr };
        for (int c = 0; c < chs; ++c)
            ptrs[c] = buffer.getWritePointer (c) + off;
        processChunk (ptrs, count, chs);
        off += count;
    }
}

void AbyssEngine::processBypassed (juce::AudioBuffer<float>& buffer) noexcept
{
    bypassSm.setCurrentAndTargetValue (1.0f);
    fullyBypassed = true;
    if (! bypassResetDone)
    {
        resetDsp();
        bypassResetDone = true;
    }
    process (buffer);
}

void AbyssEngine::processChunk (float* const* data, int numSamples, int numChannels) noexcept
{
    const float depthT = loadOr (params.depth, 0.0f);
    const float pressureT = loadOr (params.pressure, 0.0f);
    const bool bypassT = loadOr (params.bypass, 0.0f) >= 0.5f;
    const bool agOn = loadOr (params.autoGain, 1.0f) >= 0.5f;
    const int osMode = std::clamp ((int) std::lround (loadOr (params.os, 1.0f)), 0, 2);

    if (osMode != active)
    {
        active = osMode;
        if (reportedLatency != paths[active].latency)
        {
            reportedLatency = paths[active].latency;
            latencyDirty = true;
        }
    }

    depthSm.setTargetValue (depthT);
    pressureSm.setTargetValue (pressureT);
    inSm.setTargetValue (loadOr (params.inGain, 0.0f));
    outSm.setTargetValue (loadOr (params.outGain, 0.0f));
    mixSm.setTargetValue (loadOr (params.mix, 100.0f));
    ceilSm.setTargetValue (loadOr (params.ceiling, -0.3f));
    sub2Sm.setTargetValue (loadOr (params.sub2, 1.0f) >= 0.5f ? 1.0f : 0.0f);
    modeSm.setTargetValue (loadOr (params.subMode, 0.0f) >= 0.5f ? 1.0f : 0.0f);
    bypassSm.setTargetValue (bypassT ? 1.0f : 0.0f);

    if (! bypassT && (fullyBypassed || bypassResetDone))
    {
        resetDsp();
        fullyBypassed = false;
        bypassResetDone = false;
    }

    const bool bypassNow = bypassT && bypassSm.getCurrentValue() >= 1.0f;
    if (bypassNow)
    {
        fullyBypassed = true;
        if (! bypassResetDone)
        {
            resetDsp();
            bypassResetDone = true;
        }

        const int start = lineWrite;
        for (int i = 0; i < numSamples; ++i)
        {
            const float targetDelay = (float) paths[active].latency;
            const float delta = targetDelay - delayState;
            if (delta > 0.25f) delayState += 0.25f;
            else if (delta < -0.25f) delayState -= 0.25f;
            else delayState = targetDelay;

            const int slot = wrapIndex (start + i, lineLen);
            for (int c = 0; c < numChannels; ++c)
            {
                dryBuf[(size_t) c * (size_t) lineLen + (size_t) slot] = data[c][i];
                preBuf[(size_t) c * (size_t) lineLen + (size_t) slot] = data[c][i];
                data[c][i] = flushTiny (readAt (dryBuf, c, slot, delayState));
            }
        }
        lineWrite = wrapIndex (start + numSamples, lineLen);
        trackerValid = false;
        if (tapSub != nullptr)
            std::fill (tapSub, tapSub + numSamples, 0.0f);
        return;
    }

    const bool identity = depthT == 0.0f && pressureT == 0.0f
                       && depthSm.getCurrentValue() == 0.0f
                       && pressureSm.getCurrentValue() == 0.0f
                       && pathMix <= 0.0f
                       && delayState == 0.0f
                       && paths[active].latency == 0
                       && loadOr (params.inGain, 0.0f) == 0.0f && inSm.getCurrentValue() == 0.0f
                       && loadOr (params.outGain, 0.0f) == 0.0f && outSm.getCurrentValue() == 0.0f
                       && loadOr (params.mix, 100.0f) == 100.0f && mixSm.getCurrentValue() == 100.0f
                       && ! bypassT && bypassSm.getCurrentValue() == 0.0f;

    if (identity)
    {
        const int start = lineWrite;
        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = data[0][i];
            const float inR = numChannels > 1 ? data[1][i] : inL;
            const float side = numChannels > 1 ? 0.5f * (inL + inR) : inL;
            tracker.process (side);
            depthSm.getNextValue();
            pressureSm.getNextValue();
            inSm.getNextValue();
            outSm.getNextValue();
            mixSm.getNextValue();
            ceilSm.getNextValue();
            sub2Sm.getNextValue();
            modeSm.getNextValue();
            bypassSm.getNextValue();

            const int slot = wrapIndex (start + i, lineLen);
            for (int c = 0; c < numChannels; ++c)
            {
                const float x = flushTiny (data[c][i]);
                data[c][i] = x;
                dryBuf[(size_t) c * (size_t) lineLen + (size_t) slot] = x;
                preBuf[(size_t) c * (size_t) lineLen + (size_t) slot] = x;
                wetBuf[(size_t) c * (size_t) lineLen + (size_t) slot] = x;
            }
            if (tapSub != nullptr)
                tapSub[i] = 0.0f;
        }
        lineWrite = wrapIndex (start + numSamples, lineLen);
        trackerValid = tracker.isValid();
        pathMix = 0.0f;
        return;
    }

    const int start = lineWrite;
    bool anyProcess = false;
    double inAcc = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float targetDelay = (float) paths[active].latency;
        const float delta = targetDelay - delayState;
        if (delta > 0.25f) delayState += 0.25f;
        else if (delta < -0.25f) delayState -= 0.25f;
        else delayState = targetDelay;
        delayCurve[(size_t) i] = delayState;

        const float dPct = depthSm.getNextValue();
        const float pPct = pressureSm.getNextValue();
        const float inDb = inSm.getNextValue();
        const float outDb = outSm.getNextValue();
        const float mixPct = mixSm.getNextValue();
        const float sub2 = sub2Sm.getNextValue();
        const float mode = modeSm.getNextValue();

        const float d = dPct <= 0.0f ? 0.0f : (dPct >= 100.0f ? 1.0f : dPct * 0.01f);
        const float p = pPct <= 0.0f ? 0.0f : (pPct >= 100.0f ? 1.0f : pPct * 0.01f);
        pCurve[(size_t) i] = p;
        ceilCurve[(size_t) i] = dbToGain (ceilSm.getNextValue());
        mixCurve[(size_t) i] = mixPct >= 100.0f ? 1.0f : (mixPct <= 0.0f ? 0.0f : mixPct * 0.01f);
        outCurve[(size_t) i] = dbToGain (outDb);
        bypassCurve[(size_t) i] = bypassSm.getNextValue();

        const float inG = dbToGain (inDb);
        float wetL = data[0][i];
        float wetR = numChannels > 1 ? data[1][i] : wetL;
        const int slot = wrapIndex (start + i, lineLen);
        for (int c = 0; c < numChannels; ++c)
            dryBuf[(size_t) c * (size_t) lineLen + (size_t) slot] = data[c][i];

        if (inG != 1.0f)
        {
            wetL *= inG;
            if (numChannels > 1)
                wetR *= inG;
        }

        preBuf[(size_t) 0 * (size_t) lineLen + (size_t) slot] = wetL;
        if (numChannels > 1)
            preBuf[(size_t) 1 * (size_t) lineLen + (size_t) slot] = wetR;

        const float side = numChannels > 1 ? 0.5f * (wetL + wetR) : wetL;
        inAcc += (double) wetL * (double) wetL;
        if (numChannels > 1)
            inAcc += (double) wetR * (double) wetR;

        const PitchFrame frame = tracker.process (side);
        const float gateTarget = (frame.valid && frame.env > 0.0017782794f) ? 1.0f : 0.0f;
        gate += (gateTarget > gate ? gateUp : gateDn) * (gateTarget - gate);
        const float confTarget = frame.confidence <= 1 ? 0.5f : 1.0f;
        conf += confCoef * (confTarget - conf);

        float add = 0.0f;
        if (d > 0.0f)
        {
            float c1, c2, g1s, g2s;
            sub.process (frame, d, mode >= 0.5f ? 1 : 0, c1, c2, g1s, g2s);
            const float s1 = (c1 + mode * (g1s - c1)) * frame.envSub * gate * conf;
            const float s2 = (c2 + mode * (g2s - c2)) * frame.envSub * gate * conf;
            const float gv1 = 1.1f * smoothstep (0.0f, 0.55f, d);
            const float gv2 = 0.75f * smoothstep (0.45f, 1.0f, d) * sub2;
            add = s1 * gv1 + s2 * gv2;

            float mL, mR;
            darkener.process (d, wetL, wetR, numChannels, mL, mR);
            const float m = 1.0f - 0.30f * d;
            wetL = mL * m + add;
            wetR = numChannels > 1 ? (mR * m + add) : wetL;
            anyProcess = true;
        }

        if (p > 0.0f)
        {
            comp.setPressure (p);
            comp.process (wetL, wetR, numChannels);
            anyProcess = true;
        }

        if (tapSub != nullptr)
            tapSub[i] = add;

        baseBuf.setSample (0, i, wetL);
        if (numChannels > 1)
            baseBuf.setSample (1, i, wetR);

        const bool wantProc = d > 0.0f || p > 0.0f;
        if (wantProc) pathMix = std::min (1.0f, pathMix + pathStep);
        else          pathMix = std::max (0.0f, pathMix - pathStep);
        pathCurve[(size_t) i] = pathMix;
    }

    trackerValid = tracker.isValid();
    pathMixSettled = (pathMix <= 0.0f) || (pathMix >= 1.0f);
    lineWrite = wrapIndex (start + numSamples, lineLen);

    const float blockSeconds = (float) numSamples / (float) sampleRate;
    const float meanIn = (float) (inAcc / (double) std::max (1, numSamples * numChannels));
    const bool osLive = paths[active].latency > 0;
    const bool needProc = anyProcess || pathMix > 0.0f || osLive;
    const int inherent = needProc ? paths[active].latency : 0;
    const bool effectOn = anyProcess || pathMix > 0.0f;

    if (! needProc)
    {
        autoGain.updateTargets (meanIn, meanIn, blockSeconds, false, true);
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
            kCurve[(size_t) i] = (agOn && effectOn) ? autoGain.next() : 1.0f;

        renderOversampled (numSamples, numChannels, effectOn, effectOn);
        const float meanProc = procMsCount > 0 ? (float) (procMsAcc / (double) procMsCount) : meanIn;
        autoGain.updateTargets (meanProc, meanIn, blockSeconds, agOn && effectOn, ! effectOn);
        procMsAcc = 0.0;
        procMsCount = 0;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        const int slot = wrapIndex (start + i, lineLen);
        const float extra = needProc ? std::max (0.0f, delayCurve[(size_t) i] - (float) inherent)
                                     : delayCurve[(size_t) i];
        const float mix = mixCurve[(size_t) i];
        const float og = outCurve[(size_t) i];
        const float b = bypassCurve[(size_t) i];
        const float pm = pathCurve[(size_t) i];

        for (int c = 0; c < numChannels; ++c)
        {
            const float dry = readAt (dryBuf, c, slot, delayCurve[(size_t) i]);
            const float neutral = readAt (preBuf, c, slot, delayCurve[(size_t) i]);
            float wet = neutral;

            if (needProc)
            {
                wetBuf[(size_t) c * (size_t) lineLen + (size_t) slot] = baseBuf.getSample (c, i);
                const float processed = readAt (wetBuf, c, slot, extra);
                if (pm >= 1.0f)
                    wet = processed;
                else if (pm > 0.0f)
                    wet = neutral + pm * (processed - neutral);
            }

            float y = mix >= 1.0f ? wet : (mix <= 0.0f ? dry : (dry + mix * (wet - dry)));
            if (og != 1.0f)
                y *= og;
            if (b >= 1.0f)
                y = dry;
            else if (b > 0.0f)
                y += b * (dry - y);

            if (! identity)
                y = flushTiny (y);
            data[c][i] = y;
        }
    }

    if (bypassCurve[(size_t) numSamples - 1] >= 1.0f && bypassT)
    {
        fullyBypassed = true;
        if (! bypassResetDone)
        {
            resetDsp();
            bypassResetDone = true;
        }
    }
}

float AbyssEngine::readAt (const std::vector<float>& buf, int ch, int writePos, float delay) const noexcept
{
    if (delay < 0.0f)
        delay = 0.0f;
    float r = (float) writePos - delay;
    const float lf = (float) lineLen;
    while (r < 0.0f)
        r += lf;
    while (r >= lf)
        r -= lf;
    const int i0 = (int) r;
    const float frac = r - (float) i0;
    const float* b = buf.data() + (size_t) ch * (size_t) lineLen;
    const float a = b[i0];
    if (frac == 0.0f)
        return a;
    const int i1 = i0 + 1 < lineLen ? i0 + 1 : 0;
    return a + frac * (b[i1] - a);
}

} // namespace abyss
