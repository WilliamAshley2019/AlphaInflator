//==============================================================================
// JS Inflator - PluginProcessor.cpp
// JUCE 8 conversion of the original VST3 by yg331
//==============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout JSInflatorProcessor::createParameterLayout()
{
    using Range = juce::NormalisableRange<float>;
    using Param = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Input gain: -12 to +12 dB (matches original: 24*norm - 12)
    layout.add(std::make_unique<Param>(
        ParamID::INPUT, "Input Gain",
        Range(-12.0f, 12.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " dB"; })));

    // Effect (wet/dry): 0..100 %
    layout.add(std::make_unique<Param>(
        ParamID::EFFECT, "Effect",
        Range(0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " %"; })));

    // Curve: 0..100 (50 = neutral, matches original's 0..1 range)
    layout.add(std::make_unique<Param>(
        ParamID::CURVE, "Curve",
        Range(0.0f, 100.0f, 0.1f), 50.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1); })));

    // Output gain: -12 to 0 dB (matches original: 12*norm - 12)
    layout.add(std::make_unique<Param>(
        ParamID::OUTPUT, "Output Gain",
        Range(-12.0f, 0.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes{}.withStringFromValueFunction(
            [](float v, int) { return juce::String(v, 1) + " dB"; })));

    // Oversampling: 1x / 2x / 4x / 8x
    layout.add(std::make_unique<Choice>(
        ParamID::OS, "Oversampling",
        juce::StringArray{ "1x", "2x", "4x", "8x" }, 0));

    // Toggles
    layout.add(std::make_unique<Bool>(ParamID::IN, "In", true));
    layout.add(std::make_unique<Bool>(ParamID::CLIP, "Clip", false));
    layout.add(std::make_unique<Bool>(ParamID::SPLIT, "Band Split", false));
    layout.add(std::make_unique<Bool>(ParamID::MS_MODE, "Mid/Side", false));
    layout.add(std::make_unique<Bool>(ParamID::BYPASS, "Bypass", false));

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

    // Initialise curve coefficients to default (Curve = 50 => curvepct = 0)
    updateCurveCoefficients(0.5);
}

JSInflatorProcessor::~JSInflatorProcessor()
{
    apvts.removeParameterListener(ParamID::OS, &osListener);
}

//==============================================================================
bool JSInflatorProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Accept Mono->Mono and Stereo->Stereo
    const auto& in = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono()
        || in == juce::AudioChannelSet::stereo();
}

//==============================================================================
void JSInflatorProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;

    const int numCh = getTotalNumInputChannels();

    //--- Oversamplers (2x / 4x / 8x) ----------------------------------------
    for (int i = 0; i < 3; ++i)
    {
        oversamplers[i] = std::make_unique<juce::dsp::Oversampling<float>>(
            numCh,
            i + 1,  // numStages: 1=2x, 2=4x, 3=8x
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,   // maximise quality
            false   // non-interleaved
        );
        oversamplers[i]->initProcessing(static_cast<size_t>(samplesPerBlock));
    }

    // Set latency from whichever factor is currently selected
    const int osIdx = static_cast<int> (
        apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
    currentOSIndex = osIdx;

    if (osIdx == 0)
        setLatencySamples(0);
    else
        setLatencySamples(static_cast<int> (
            oversamplers[osIdx - 1]->getLatencyInSamples()));

    //--- Band splits ---------------------------------------------------------
    for (int ch = 0; ch < 2; ++ch)
    {
        bandSplit[ch].setFrequencies(240.0, 2400.0, sampleRate);
        bandSplit[ch].reset();

        // OS-domain splits are set in processBlock when OS rate is known
        bandSplitOS[ch].reset();
    }

    //--- DC blockers ---------------------------------------------------------
    for (auto& dc : dcBlocker)
        dc.reset();

    //--- Smoothed parameters (20 ms ramp, prevents zipper noise) -----------
    const double rampSec = 0.020;
    inputGain.reset(sampleRate, rampSec);
    outputGain.reset(sampleRate, rampSec);
    effectWet.reset(sampleRate, rampSec);
    curveSmoother.reset(sampleRate, rampSec);

    // Set initial targets from current parameter values
    auto dB2gain = [](float dB) { return juce::Decibels::decibelsToGain<double>(dB); };
    inputGain.setCurrentAndTargetValue(dB2gain(apvts.getRawParameterValue(ParamID::INPUT)->load()));
    outputGain.setCurrentAndTargetValue(dB2gain(apvts.getRawParameterValue(ParamID::OUTPUT)->load()));
    effectWet.setCurrentAndTargetValue(apvts.getRawParameterValue(ParamID::EFFECT)->load() / 100.0);
    curveSmoother.setCurrentAndTargetValue(
        apvts.getRawParameterValue(ParamID::CURVE)->load() / 100.0);

    //--- Level followers -----------------------------------------------------
    for (auto& f : inputFollower)  f.prepare(sampleRate);
    for (auto& f : outputFollower) f.prepare(sampleRate);

    inLevelL = 0.0f;  inLevelR = 0.0f;
    outLevelL = 0.0f;  outLevelR = 0.0f;
    effectMeter = 0.0f;
}

//==============================================================================
void JSInflatorProcessor::releaseResources()
{
    for (auto& os : oversamplers) os.reset();
}

//==============================================================================
void JSInflatorProcessor::onOSParamChanged(int newIndex)
{
    // Called on the message thread when OS choice changes.
    // We just record the new index; processBlock will pick it up.
    currentOSIndex = newIndex;

    // Notify the host that latency has changed (host will re-query getLatencyInSamples)
    if (newIndex == 0)
        setLatencySamples(0);
    else if (oversamplers[newIndex - 1] != nullptr)
        setLatencySamples(static_cast<int> (
            oversamplers[newIndex - 1]->getLatencyInSamples()));

    updateHostDisplay(juce::AudioProcessorListener::ChangeDetails()
        .withLatencyChanged(true));
}

//==============================================================================
// Core inflator polynomial — preserves original algorithm exactly.
// Operates on |x|; sign is restored at the end.
//
// With curvepct = Curve - 0.5:
//   curveA = 1.5 + curvepct      curveB = -2 * curvepct
//   curveC = curvepct - 0.5      curveD = 0.0625 - 0.25*cp + 0.25*cp²
//
// The polynomial is a smooth approximation of a tape-style compander.
// For |x| > 1 it folds with 2x - x², a mild wave-folder that avoids
// hard discontinuities.  For |x| >= 2 the signal is zeroed (full fold).
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
    // curveParam is normalised [0, 1], where 0.5 = neutral
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

    const bool bypass = apvts.getRawParameterValue(ParamID::BYPASS)->load() > 0.5f;
    const bool isIn = apvts.getRawParameterValue(ParamID::IN)->load() > 0.5f;
    const bool doClip = apvts.getRawParameterValue(ParamID::CLIP)->load() > 0.5f;
    const bool doSplit = apvts.getRawParameterValue(ParamID::SPLIT)->load() > 0.5f;
    const bool doMS = apvts.getRawParameterValue(ParamID::MS_MODE)->load() > 0.5f;
    const int  osIdx = currentOSIndex;  // written on message thread, read here

    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    // Smooth target updates (once per block is fine for these params)
    auto dB2gain = [](float dB) { return juce::Decibels::decibelsToGain<double>(dB); };
    inputGain.setTargetValue(dB2gain(apvts.getRawParameterValue(ParamID::INPUT)->load()));
    outputGain.setTargetValue(dB2gain(apvts.getRawParameterValue(ParamID::OUTPUT)->load()));
    effectWet.setTargetValue(apvts.getRawParameterValue(ParamID::EFFECT)->load() / 100.0);

    const double curveParam = apvts.getRawParameterValue(ParamID::CURVE)->load() / 100.0;
    curveSmoother.setTargetValue(curveParam);

    //--- Bypass path ---------------------------------------------------------
    if (bypass)
    {
        // Update meters from bypassed signal
        for (int ch = 0; ch < std::min(numCh, 2); ++ch)
        {
            const float* src = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                inputFollower[ch].update(static_cast<double>(src[i]));
                outputFollower[ch].update(static_cast<double>(src[i]));
            }
        }
        inLevelL = inputFollower[0].getValue();
        inLevelR = (numCh > 1) ? inputFollower[1].getValue() : inLevelL.load();
        outLevelL = outputFollower[0].getValue();
        outLevelR = (numCh > 1) ? outputFollower[1].getValue() : outLevelL.load();
        effectMeter = 0.0f;
        return;
    }

    //--- Mid/Side encode (stereo only) ----------------------------------------
    if (doMS && numCh == 2)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
            encodeMS(L[i], R[i]);
    }

    //--- Update curve coefficients once per block ----------------------------
    updateCurveCoefficients(curveSmoother.getCurrentValue());

    //--- Track INPUT levels (pre-processing) ---------------------------------
    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        const float* src = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            inputFollower[ch].update(static_cast<double>(src[i]));
    }

    //--- Oversampled / non-oversampled processing ----------------------------
    juce::dsp::AudioBlock<float> block(buffer);

    // If OS is active, upsample the block
    juce::dsp::AudioBlock<float> oversampledBlock;
    if (osIdx > 0 && oversamplers[osIdx - 1] != nullptr)
    {
        // Do NOT reset the oversampler every block — it must maintain its
        // internal filter state for continuous operation.
        oversampledBlock = oversamplers[osIdx - 1]->processSamplesUp(block);
    }
    else
    {
        oversampledBlock = block;
    }

    const int   osNumSamples = static_cast<int> (oversampledBlock.getNumSamples());
    const double osSampleRate = currentSampleRate * (osIdx == 0 ? 1 : (1 << osIdx));

    // Update OS-domain band splits if sample rate changed
    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
        bandSplitOS[ch].setFrequencies(240.0, 2400.0, osSampleRate);

    //--- Sample-by-sample processing -----------------------------------------
    double effectSum = 0.0;

    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        float* samples = oversampledBlock.getChannelPointer(ch);

        // Update input level follower (pre-inflator, pre-input-gain)
        // We'll do it on the non-oversampled signal below.

        for (int i = 0; i < osNumSamples; ++i)
        {
            // Advance smoothers every sample (oversampled domain)
            const double inGain = inputGain.getNextValue();
            const double outGain = outputGain.getNextValue();
            const double wet = effectWet.getNextValue();

            double x = static_cast<double>(samples[i]);

            // Apply input gain
            x *= inGain;

            // Hard-clip guard before inflator (keeps signal in [-2, 2])
            x = juce::jlimit(-2.0, 2.0, x);

            const double dry = x;

            // Inflator
            double processed = x;
            if (isIn)
            {
                if (doSplit)
                {
                    double low, mid, high;
                    bandSplitOS[ch].process(x, low, mid, high);

                    // Mid band is normalised so all three bands sum flat;
                    // apply the gain/gain-reciprocal for correct loudness.
                    processed = processInflatorSample(low)
                        + processInflatorSample(mid * bandSplitOS[ch].G)
                        * bandSplitOS[ch].GR
                        + processInflatorSample(high);
                }
                else
                {
                    processed = processInflatorSample(x);
                }
            }

            // Soft-knee or hard clip
            if (doClip)
                processed = softKneeClip(processed);

            // Wet/dry mix (effect parameter)
            processed = dry * (1.0 - wet) + processed * wet;

            // Accumulate effect meter (difference signal energy)
            effectSum += std::fabs(processed) - std::fabs(dry);

            // DC block (improvement over original)
            processed = dcBlocker[ch].process(processed);

            // Apply output gain
            processed *= outGain;

            samples[i] = static_cast<float> (processed);
        }
    }

    //--- Downsample back to host rate ----------------------------------------
    if (osIdx > 0 && oversamplers[osIdx - 1] != nullptr)
        oversamplers[osIdx - 1]->processSamplesDown(block);

    //--- Mid/Side decode (stereo only) ----------------------------------------
    if (doMS && numCh == 2)
    {
        float* L = buffer.getWritePointer(0);
        float* R = buffer.getWritePointer(1);
        for (int i = 0; i < numSamples; ++i)
            decodeMS(L[i], R[i]);
    }

    //--- Level follower updates (output) ------------------------------------
    for (int ch = 0; ch < std::min(numCh, 2); ++ch)
    {
        const float* out = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
            outputFollower[ch].update(static_cast<double>(out[i]));
    }

    inLevelL = inputFollower[0].getValue();
    inLevelR = (numCh > 1) ? inputFollower[1].getValue() : inLevelL.load();
    outLevelL = outputFollower[0].getValue();
    outLevelR = (numCh > 1) ? outputFollower[1].getValue() : outLevelL.load();

    // Effect meter: average difference signal across channels/samples
    const double avgEffect = effectSum / static_cast<double> (
        std::max(1, osNumSamples * std::min(numCh, 2)));
    effectMeter = static_cast<float> (juce::jlimit(0.0, 1.0,
        0.5 + juce::Decibels::gainToDecibels(std::fabs(avgEffect) + 1e-12) / 40.0));
}

//==============================================================================
// Double-precision path — identical logic, JUCE routes 64-bit hosts here.
void JSInflatorProcessor::processBlock(juce::AudioBuffer<double>& buffer,
    juce::MidiBuffer& midi)
{
    // Downmix to float, process, upmix back — keeps one code path.
    // For truly double-precision hosts this is a minor compromise;
    // the inflator core runs at double internally anyway.
    juce::AudioBuffer<float> floatBuf(buffer.getNumChannels(), buffer.getNumSamples());
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const double* src = buffer.getReadPointer(ch);
        float* dst = floatBuf.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            dst[i] = static_cast<float>(src[i]);
    }

    processBlock(floatBuf, midi);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const float* src = floatBuf.getReadPointer(ch);
        double* dst = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            dst[i] = static_cast<double>(src[i]);
    }
}

//==============================================================================
juce::AudioProcessorEditor* JSInflatorProcessor::createEditor()
{
    return new JSInflatorEditor(*this);
}

//==============================================================================
void JSInflatorProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void JSInflatorProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
    {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));

        // Reflect the OS choice immediately
        const int newIdx = static_cast<int> (
            apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
        onOSParamChanged(newIdx);
    }
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JSInflatorProcessor();
}
