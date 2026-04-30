//==============================================================================
// JS Inflator  v1.2 - PluginProcessor.cpp  (JUCE 8)
//==============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
JSInflatorProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    using Param = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto dBStr = [](float v, int) { return juce::String(v, 1) + " dB"; };
    auto pctStr = [](float v, int) { return juce::String(v, 1) + " %"; };

    // ── Original ─────────────────────────────────────────────────────────────
    layout.add(std::make_unique<Param>(ParamID::INPUT, "Input Gain",
        Range(-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(dBStr)));

    layout.add(std::make_unique<Param>(ParamID::EFFECT, "Effect",
        Range(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(pctStr)));

    layout.add(std::make_unique<Param>(ParamID::CURVE, "Curve",
        Range(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1); })));

    layout.add(std::make_unique<Param>(ParamID::OUTPUT, "Output Gain",
        Range(-12.0f, 0.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(dBStr)));

    layout.add(std::make_unique<Choice>(ParamID::OS, "Oversampling",
        juce::StringArray{ "1x", "2x", "4x", "8x" }, 0));

    layout.add(std::make_unique<Bool>(ParamID::IN, "In", true));
    layout.add(std::make_unique<Bool>(ParamID::CLIP, "Clip", false));
    layout.add(std::make_unique<Bool>(ParamID::SPLIT, "Band Split", false));
    layout.add(std::make_unique<Bool>(ParamID::MS_MODE, "Mid/Side", false));
    layout.add(std::make_unique<Bool>(ParamID::BYPASS, "Bypass", false));

    // ── New ──────────────────────────────────────────────────────────────────
    layout.add(std::make_unique<Choice>(ParamID::AGC_MODE, "Auto Gain Comp",
        juce::StringArray{ "Off", "Static", "Dynamic" }, 0));

    layout.add(std::make_unique<Bool>(ParamID::DELTA, "Delta Monitor", false));

    layout.add(std::make_unique<Param>(ParamID::TONE, "Tone",
        Range(-50.0f, 50.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int)
            {
                if (v < -5.0f) return juce::String("Warm ") + juce::String(std::fabs(v), 0);
                if (v > 5.0f) return juce::String("Bright ") + juce::String(v, 0);
                return juce::String("Neutral");
            })));

    layout.add(std::make_unique<Choice>(ParamID::FOCUS, "Frequency Focus",
        juce::StringArray{ "Full", "Low", "Mid", "High" }, 0));

    layout.add(std::make_unique<Choice>(ParamID::DYN_MODE, "Dynamics Sensitivity",
        juce::StringArray{ "Smooth", "Neutral", "Punch", "Dense" }, 1));

    layout.add(std::make_unique<Bool>(ParamID::LIMITER, "Safety Limiter", false));

    layout.add(std::make_unique<Param>(ParamID::LIM_CEIL, "Limiter Ceiling",
        Range(-6.0f, -0.1f, 0.05f), -0.3f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(dBStr)));

    return layout;
}

//==============================================================================
JSInflatorProcessor::JSInflatorProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "JSInflator", createParameterLayout())
{
    apvts.addParameterListener(ParamID::OS, &osListener);
    updateCurveCoefficients(0.5);
}

JSInflatorProcessor::~JSInflatorProcessor()
{
    apvts.removeParameterListener(ParamID::OS, &osListener);
}

//==============================================================================
bool JSInflatorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    if (in != layouts.getMainOutputChannelSet()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void JSInflatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    const int numCh = getTotalNumInputChannels();

    // Oversamplers
    for (int i = 0; i < 3; ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>>(
            numCh, i + 1,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true, false);
        oversamplers[i]->initProcessing(static_cast<size_t>(samplesPerBlock));
    }

    const int osIdx = static_cast<int>(apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
    currentOSIndex = osIdx;
    requestedOSIndex.store(osIdx);

    const int osLat = (osIdx == 0) ? 0
        : static_cast<int> (oversamplers[osIdx - 1]->getLatencyInSamples());
    setLatencySamples(osLat + lookaheadLim.getLatencySamples());

    // Band splits
    for (int ch = 0; ch < 2; ++ch)
    {
        bandSplit[ch].setFrequencies(240.0, 2400.0, sampleRate);
        bandSplit[ch].reset();
        bandSplitOS[ch].reset();
    }

    // DSP objects
    for (auto& d : dcBlocker)    d.reset();
    for (auto& t : tiltEQ)       t.prepare(sampleRate);
    for (auto& td : transientDet) td.prepare(sampleRate);
    lookaheadLim.prepare(sampleRate);

    // Smoothers
    const double ramp = 0.020;
    inputGain.reset(sampleRate, ramp);
    outputGain.reset(sampleRate, ramp);
    effectWet.reset(sampleRate, ramp);
    curveSmoother.reset(sampleRate, ramp);
    toneSmoother.reset(sampleRate, ramp);
    agcGainSmooth.reset(sampleRate, 0.500);

    auto dB2g = [](float dB) { return juce::Decibels::decibelsToGain<double>(dB); };
    inputGain.setCurrentAndTargetValue(dB2g(apvts.getRawParameterValue(ParamID::INPUT)->load()));
    outputGain.setCurrentAndTargetValue(dB2g(apvts.getRawParameterValue(ParamID::OUTPUT)->load()));
    effectWet.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::EFFECT)->load() / 100.0);
    curveSmoother.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::CURVE)->load() / 100.0);
    toneSmoother.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::TONE)->load() / 50.0);
    agcGainSmooth.setCurrentAndTargetValue(1.0);

    // AGC
    agcCoeffSlow = std::exp(-1.0 / (sampleRate * 5.0));
    agcCoeffFast = std::exp(-1.0 / (sampleRate * 0.5));
    agcRmsIn = agcRmsOut = 0.0;

    // RMS
    rmsCoeff = std::exp(-1.0 / (sampleRate * 0.300));
    inRmsAcc.fill(0.0);
    outRmsAcc.fill(0.0);

    // Level followers
    for (auto& f : inputFollower) { f.prepare(sampleRate); f.reset(); }
    for (auto& f : outputFollower) { f.prepare(sampleRate); f.reset(); }

    peakHoldSamples = static_cast<int> (sampleRate * 2.0);
    peakHoldCounter = 0;
    peakHoldInL = peakHoldInR = 0.0f;
    peakHoldOutL = peakHoldOutR = 0.0f;

    dryBuffer.setSize(numCh, samplesPerBlock);

    // Reset atomics
    inLevelL = inLevelR = outLevelL = outLevelR = 0.0f;
    inPeakL = inPeakR = outPeakL = outPeakR = 0.0f;
    inRmsL = inRmsR = outRmsL = outRmsR = 0.0f;
    inOversL = inOversR = outOversL = outOversR = 0;
    effectMeter = 0.0f;
    limiterGR = 0.0f;
}

//==============================================================================
void JSInflatorProcessor::releaseResources()
{
    for (auto& os : oversamplers) os.reset();
}

//==============================================================================
void JSInflatorProcessor::onOSParamChanged(int newIndex)
{
    requestedOSIndex.store(newIndex);
    const int osLat = (newIndex == 0 || !oversamplers[newIndex - 1]) ? 0
        : static_cast<int> (oversamplers[newIndex - 1]->getLatencyInSamples());
    setLatencySamples(osLat + lookaheadLim.getLatencySamples());
    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails().withLatencyChanged(true));
}

//==============================================================================
double JSInflatorProcessor::processInflatorSample(double x) const noexcept
{
    const double sign = (x >= 0.0) ? 1.0 : -1.0;
    const double s1 = std::fabs(x);
    const double s2 = s1 * s1;
    const double s3 = s2 * s1;
    const double s4 = s2 * s2;

    double out;
    if (s1 >= 2.0) out = 0.0;
    else if (s1 > 1.0) out = 2.0 * s1 - s2;
    else                out = curveA * s1 + curveB * s2 + curveC * s3
        - curveD * (s2 - 2.0 * s3 + s4);
    return out * sign;
}

void JSInflatorProcessor::updateCurveCoefficients(double curveParam) noexcept
{
    const double cp = curveParam - 0.5;
    curveA = 1.5 + cp;
    curveB = -(cp + cp);
    curveC = cp - 0.5;
    curveD = 0.0625 - cp * 0.25 + (cp * cp) * 0.25;
}

//==============================================================================
void JSInflatorProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Read parameters
    auto raw = [&](const juce::String& id) { return apvts.getRawParameterValue(id)->load(); };

    const bool bypass = raw(ParamID::BYPASS) > 0.5f;
    const bool isIn = raw(ParamID::IN) > 0.5f;
    const bool doClip = raw(ParamID::CLIP) > 0.5f;
    const bool doSplit = raw(ParamID::SPLIT) > 0.5f;
    const bool doMS = raw(ParamID::MS_MODE) > 0.5f;
    const bool doDelta = raw(ParamID::DELTA) > 0.5f;
    const bool doLim = raw(ParamID::LIMITER) > 0.5f;
    const int  agcMode = static_cast<int> (raw(ParamID::AGC_MODE) + 0.5f);
    const int  focus = static_cast<int> (raw(ParamID::FOCUS) + 0.5f);
    const int  dynMode = static_cast<int> (raw(ParamID::DYN_MODE) + 0.5f);
    const double limCeilLin = juce::Decibels::decibelsToGain(raw(ParamID::LIM_CEIL));

    const int osIdx = requestedOSIndex.load();
    currentOSIndex = osIdx;
    const int numCh = buffer.getNumChannels();
    const int numS = buffer.getNumSamples();

    // Update smoother targets
    auto dB2g = [](float dB) { return juce::Decibels::decibelsToGain<double>(dB); };
    inputGain.setTargetValue(dB2g(raw(ParamID::INPUT)));
    outputGain.setTargetValue(dB2g(raw(ParamID::OUTPUT)));
    effectWet.setTargetValue(raw(ParamID::EFFECT) / 100.0);
    curveSmoother.setTargetValue(raw(ParamID::CURVE) / 100.0);
    toneSmoother.setTargetValue(raw(ParamID::TONE) / 50.0);

    // Dynamics mode wet modifiers
    double dynTransBoost = 0.0, dynSustReduce = 0.0;
    if (dynMode == 0) dynTransBoost = -0.30;   // Smooth
    else if (dynMode == 2) dynTransBoost = 0.40;   // Punch
    else if (dynMode == 3) dynSustReduce = 0.35;   // Dense

    // ── Input level tracking (pre-processing) ─────────────────────────────────
    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        for (int i = 0; i < numS; ++i)
        {
            const double s = static_cast<double>(src[i]);
            inputFollower[ch].update(s);
            inRmsAcc[ch] = rmsCoeff * inRmsAcc[ch] + (1.0 - rmsCoeff) * s * s;
            if (src[i] >= 1.0f || src[i] <= -1.0f)
            {
                if (ch == 0) inOversL.fetch_add(1, std::memory_order_relaxed);
                else         inOversR.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    peakHoldInL = std::max(peakHoldInL, inputFollower[0].getValue());
    peakHoldInR = std::max(peakHoldInR, (numCh > 1) ? inputFollower[1].getValue() : peakHoldInL);

    // ── Delta dry copy ────────────────────────────────────────────────────────
    if (doDelta)
        dryBuffer.makeCopyOf(buffer, true);

    // ── Bypass ────────────────────────────────────────────────────────────────
    if (bypass)
    {
        for (int ch = 0; ch < std::min(numCh, 2); ++ch)
        {
            const float* src = buffer.getReadPointer(ch);
            for (int i = 0; i < numS; ++i)
                outputFollower[ch].update(static_cast<double>(src[i]));
        }
        outLevelL = outputFollower[0].getValue();
        outLevelR = (numCh > 1) ? outputFollower[1].getValue() : outLevelL.load();
        inLevelL = inputFollower[0].getValue();
        inLevelR = (numCh > 1) ? inputFollower[1].getValue() : inLevelL.load();
        effectMeter = 0.0f;
        return;
    }

    // ── M/S encode ────────────────────────────────────────────────────────────
    if (doMS && numCh == 2)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i) encodeMS(L[i], R[i]);
    }

    // ── Upsample ─────────────────────────────────────────────────────────────
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::AudioBlock<float> osBlock;

    if (osIdx > 0 && oversamplers[osIdx - 1])
        osBlock = oversamplers[osIdx - 1]->processSamplesUp(block);
    else
        osBlock = block;

    const int    osN = static_cast<int> (osBlock.getNumSamples());
    const double osFS = currentSampleRate * (osIdx == 0 ? 1 : (1 << osIdx));

    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
        bandSplitOS[ch].setFrequencies(240.0, 2400.0, osFS);

    // Pre-compute tilt gains once per block
    updateCurveCoefficients(curveSmoother.getCurrentValue());
    const double toneNorm = toneSmoother.getCurrentValue();   // -1..+1
    const double tiltLow = juce::Decibels::decibelsToGain(-toneNorm * 6.0);
    const double tiltHigh = juce::Decibels::decibelsToGain(toneNorm * 6.0);

    double effectAccum = 0.0;

    // ── Per-sample loop ───────────────────────────────────────────────────────
    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        float* samples = osBlock.getChannelPointer(ch);

        for (int i = 0; i < osN; ++i)
        {
            const double inG = inputGain.getNextValue();
            const double outG = outputGain.getNextValue();
            const double wet = effectWet.getNextValue();

            double x = static_cast<double>(samples[i]) * inG;
            x = juce::jlimit(-2.0, 2.0, x);
            const double dry = x;

            // Dynamics sensitivity — modulate wet based on transient content
            const double transAmt = transientDet[ch].process(x);
            double dynWet = juce::jlimit(0.0, 1.0,
                wet + wet * dynTransBoost * transAmt
                - wet * dynSustReduce * (1.0 - transAmt));

            double processed = x;
            if (isIn)
            {
                double low, mid, high;
                bandSplitOS[ch].process(x, low, mid, high);

                // Frequency Focus: only process selected band(s)
                const double midNorm = mid * bandSplitOS[ch].G;
                if (focus == 0)   // Full
                {
                    low = processInflatorSample(low);
                    mid = processInflatorSample(midNorm) * bandSplitOS[ch].GR;
                    high = processInflatorSample(high);
                }
                else if (focus == 1)  // Low
                {
                    low = processInflatorSample(low);
                    mid = midNorm * bandSplitOS[ch].GR;
                }
                else if (focus == 2)  // Mid
                {
                    mid = processInflatorSample(midNorm) * bandSplitOS[ch].GR;
                }
                else                   // High
                {
                    high = processInflatorSample(high);
                    mid = midNorm * bandSplitOS[ch].GR;
                }

                processed = low + mid + high;
            }

            if (doClip)
                processed = softKneeClip(processed);

            processed = dry * (1.0 - dynWet) + processed * dynWet;

            effectAccum += std::fabs(processed) - std::fabs(dry);

            processed = dcBlocker[ch].process(processed);
            processed = tiltEQ[ch].process(processed, tiltLow, tiltHigh);
            processed *= outG;

            samples[i] = static_cast<float> (processed);
        }
    }

    // ── Downsample ────────────────────────────────────────────────────────────
    if (osIdx > 0 && oversamplers[osIdx - 1])
        oversamplers[osIdx - 1]->processSamplesDown(block);

    // ── Safety Limiter ────────────────────────────────────────────────────────
    if (doLim && numCh >= 2)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i)
        {
            double dL = L[i], dR = R[i];
            lookaheadLim.processFrame(dL, dR, limCeilLin);
            L[i] = static_cast<float>(dL);
            R[i] = static_cast<float>(dR);
        }
        limiterGR = static_cast<float>(lookaheadLim.getGainReductionDB());
    }
    else
    {
        limiterGR = 0.0f;
    }

    // ── M/S decode ───────────────────────────────────────────────────────────
    if (doMS && numCh == 2)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numS; ++i) decodeMS(L[i], R[i]);
    }

    // ── Delta monitor ─────────────────────────────────────────────────────────
    if (doDelta && dryBuffer.getNumSamples() == numS)
    {
        for (int ch = 0; ch < std::min(numCh, 2); ++ch)
        {
            float* dst = buffer.getWritePointer(ch);
            const float* dry = dryBuffer.getReadPointer(ch);
            for (int i = 0; i < numS; ++i) dst[i] -= dry[i];
        }
    }

    // ── Auto Gain Compensation ────────────────────────────────────────────────
    if (agcMode > 0)
    {
        const double coeff = (agcMode == 2) ? agcCoeffFast : agcCoeffSlow;
        const double inPow = (inRmsAcc[0] + (numCh > 1 ? inRmsAcc[1] : inRmsAcc[0])) * 0.5;
        double outPow = 0.0;
        for (int ch = 0; ch < std::min(numCh, 2); ++ch)
        {
            const float* src = buffer.getReadPointer(ch);
            for (int i = 0; i < numS; ++i)
                outPow += static_cast<double>(src[i]) * static_cast<double>(src[i]);
        }
        outPow /= static_cast<double>(numS * std::min(numCh, 2));

        agcRmsIn = coeff * agcRmsIn + (1.0 - coeff) * inPow;
        agcRmsOut = coeff * agcRmsOut + (1.0 - coeff) * outPow;

        if (agcRmsOut > 1e-12 && agcRmsIn > 1e-12)
            agcGainSmooth.setTargetValue(juce::jlimit(0.1, 4.0, std::sqrt(agcRmsIn / agcRmsOut)));

        buffer.applyGain(static_cast<float> (agcGainSmooth.getNextValue()));
    }

    // ── Output level tracking ─────────────────────────────────────────────────
    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        for (int i = 0; i < numS; ++i)
        {
            const double s = static_cast<double>(src[i]);
            outputFollower[ch].update(s);
            outRmsAcc[ch] = rmsCoeff * outRmsAcc[ch] + (1.0 - rmsCoeff) * s * s;
            if (src[i] >= 1.0f || src[i] <= -1.0f)
            {
                if (ch == 0) outOversL.fetch_add(1, std::memory_order_relaxed);
                else         outOversR.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    peakHoldOutL = std::max(peakHoldOutL, outputFollower[0].getValue());
    peakHoldOutR = std::max(peakHoldOutR, (numCh > 1) ? outputFollower[1].getValue() : peakHoldOutL);

    peakHoldCounter += numS;
    if (peakHoldCounter >= peakHoldSamples)
    {
        peakHoldCounter = 0;
        peakHoldInL *= 0.95f;  peakHoldInR *= 0.95f;
        peakHoldOutL *= 0.95f;  peakHoldOutR *= 0.95f;
    }

    // Push atomics
    inLevelL = inputFollower[0].getValue();
    inLevelR = (numCh > 1) ? inputFollower[1].getValue() : inLevelL.load();
    outLevelL = outputFollower[0].getValue();
    outLevelR = (numCh > 1) ? outputFollower[1].getValue() : outLevelL.load();
    inPeakL = peakHoldInL;   inPeakR = peakHoldInR;
    outPeakL = peakHoldOutL;  outPeakR = peakHoldOutR;
    inRmsL = static_cast<float> (std::sqrt(inRmsAcc[0]));
    inRmsR = static_cast<float> (std::sqrt((numCh > 1) ? inRmsAcc[1] : inRmsAcc[0]));
    outRmsL = static_cast<float> (std::sqrt(outRmsAcc[0]));
    outRmsR = static_cast<float> (std::sqrt((numCh > 1) ? outRmsAcc[1] : outRmsAcc[0]));

    const double avgEffect = effectAccum / static_cast<double> (std::max(1, osN * std::min(numCh, 2)));
    effectMeter = static_cast<float> (juce::jlimit(0.0, 1.0,
        0.5 + juce::Decibels::gainToDecibels(std::fabs(avgEffect) + 1e-12) / 40.0));
}

//==============================================================================
void JSInflatorProcessor::processBlock(juce::AudioBuffer<double>& buffer,
    juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> fb(buffer.getNumChannels(), buffer.getNumSamples());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const double* s = buffer.getReadPointer(ch);
        float* d = fb.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i) d[i] = static_cast<float>(s[i]);
    }
    processBlock(fb, midi);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* s = fb.getReadPointer(ch);
        double* d = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i) d[i] = static_cast<double>(s[i]);
    }
}

//==============================================================================
juce::AudioProcessorEditor* JSInflatorProcessor::createEditor()
{
    return new JSInflatorEditor(*this);
}

void JSInflatorProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto xml = apvts.copyState().createXml();
    copyXmlToBinary(*xml, destData);
}

void JSInflatorProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
        onOSParamChanged(static_cast<int> (apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JSInflatorProcessor();
}