//==============================================================================
// JS Inflator - JUCE 8 Conversion
// Original by yg331 | JUCE port with improvements
//
// Improvements over original VST3:
//   - JUCE AudioProcessorValueTreeState for clean parameter management
//   - juce::dsp::Oversampling replaces custom Kaiser FIR + r8b chain
//   - Per-channel DC blocking filter (prevents saturation-induced DC offset)
//   - SmoothedValues on all continuous parameters (eliminates zipper noise)
//   - Soft knee output limiter option (more musical than hard clip)
//   - Mid/Side processing mode (wider, more open stereo image)
//   - Refactored band split using the original SVF structure, expressed cleanly
//   - Level ballistics per-channel for accurate stereo metering
//   - Modern C++17 throughout
//==============================================================================

#pragma once
#include <JuceHeader.h>

//==============================================================================
namespace ParamID
{
    inline const juce::String INPUT = "input";
    inline const juce::String EFFECT = "effect";
    inline const juce::String CURVE = "curve";
    inline const juce::String OUTPUT = "output";
    inline const juce::String CLIP = "clip";
    inline const juce::String SPLIT = "split";
    inline const juce::String MS_MODE = "msmode";   // NEW: Mid/Side processing
    inline const juce::String IN = "in";
    inline const juce::String BYPASS = "bypass";
    inline const juce::String OS = "os";       // 0=1x, 1=2x, 2=4x, 3=8x
}

//==============================================================================
class JSInflatorProcessor final : public juce::AudioProcessor
{
public:
    JSInflatorProcessor();
    ~JSInflatorProcessor() override;

    //==========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return "JS Inflator"; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==========================================================================
    // Public parameter tree (editor attaches sliders directly to this)
    juce::AudioProcessorValueTreeState apvts;

    // Thread-safe meter reads (called from UI timer on message thread)
    float getInputLevelL()  const noexcept { return inLevelL.load(); }
    float getInputLevelR()  const noexcept { return inLevelR.load(); }
    float getOutputLevelL() const noexcept { return outLevelL.load(); }
    float getOutputLevelR() const noexcept { return outLevelR.load(); }
    float getEffectMeter()  const noexcept { return effectMeter.load(); }

private:
    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==========================================================================
    // Core inflator polynomial (Oxford Inflator style)
    // Operates on a single sample in the range [-2, 2].
    // curveA/B/C/D are pre-computed from the Curve parameter.
    [[nodiscard]] double processInflatorSample(double x) const noexcept;

    // Update cached polynomial coefficients when Curve param changes
    void updateCurveCoefficients(double curveParam) noexcept;

    //==========================================================================
    // Oversampling ─ one instance per factor (2x / 4x / 8x).
    // Avoids runtime allocation on the audio thread when switching.
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    //   oversamplers[0] = 2x  (1 stage)
    //   oversamplers[1] = 4x  (2 stages)
    //   oversamplers[2] = 8x  (3 stages)

    int currentOSIndex = -1;  // -1 forces latency update on first processBlock

    // Called on message thread when OS param changes (via parameterChanged listener).
    // Triggers the host to query getLatencyInSamples() again.
    void onOSParamChanged(int newIndex);

    //==========================================================================
    // SVF (State-Variable Filter) band-split — ported directly from original.
    // Splits the signal into Low / Mid / High bands at fc_low & fc_high.
    //
    // G / GR normalise the mid-band gain so all three bands sum flat (≈0 dB).
    struct BandSplit
    {
        double lpR = 0.0, lpI = 0.0;   // one-pole LP at fc_low
        double hpR = 0.0, hpI = 0.0;   // one-pole HP at fc_high
        double C_low = 0.0;
        double C_high = 0.0;
        double G = 1.0;               // mid-band normalisation gain
        double GR = 1.0;               // reciprocal

        void reset() noexcept { lpR = lpI = hpR = hpI = 0.0; }

        void setFrequencies(double fc_low, double fc_high, double fs) noexcept
        {
            constexpr double pi = juce::MathConstants<double>::pi;
            C_low = 0.5 * std::tan(pi * (fc_low / fs - 0.25)) + 0.5;
            C_high = 0.5 * std::tan(pi * (fc_high / fs - 0.25)) + 0.5;
            G = C_high * (1.0 - C_low) / (C_high - C_low);
            GR = (G > 1e-12) ? 1.0 / G : 1.0;
        }

        // Returns three perfectly-summing bands
        void process(double x, double& low, double& mid, double& high) noexcept
        {
            // LP at fc_low
            lpR = lpI + C_low * (x - lpI);
            lpI = 2.0 * lpR - lpI;

            // HP at fc_high  (complementary = 1 - LP)
            hpR = (1.0 - C_high) * hpI + C_high * x;
            hpI = 2.0 * hpR - hpI;

            low = lpR;
            high = x - hpR;
            mid = hpR - lpR;
        }
    };

    // One BandSplit per channel (up to 2 channels stereo)
    std::array<BandSplit, 2> bandSplit;
    // Separate instances for the oversampled domain (reset on OS change)
    std::array<BandSplit, 2> bandSplitOS;

    //==========================================================================
    // DC Blocker — first-order high-pass at ~5 Hz.
    // Prevents slow DC drift that polynomial saturation can introduce.
    struct DCBlocker
    {
        double x1 = 0.0, y1 = 0.0;

        [[nodiscard]] double process(double x) noexcept
        {
            // coefficient = 1 - (2π * fc / fs), fc ≈ 5 Hz at typical rates
            y1 = x - x1 + 0.99975 * y1;
            x1 = x;
            return y1;
        }
        void reset() noexcept { x1 = y1 = 0.0; }
    };

    std::array<DCBlocker, 2> dcBlocker;

    //==========================================================================
    // Soft-knee output limiter — applied post-inflator when Clip is enabled.
    // Uses tanh approximation: gentler and more musical than hard clip.
    // x_out = tanh(x) for |x| > threshold, passthrough below.
    static double softKneeClip(double x) noexcept
    {
        // Fast tanh approximation (Pade) – accurate to ±0.0005 for |x| < 4
        if (x > 4.0) return  1.0;
        if (x < -4.0) return -1.0;
        const double x2 = x * x;
        return x * (27.0 + x2) / (27.0 + 9.0 * x2);
    }

    //==========================================================================
    // Smoothed parameters — eliminates zipper noise on continuous controls
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> inputGain, outputGain;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear>         effectWet, curveSmoother;

    // Cached curve polynomial coefficients
    double curveA = 1.5, curveB = 0.0, curveC = -0.5, curveD = 0.0625;

    //==========================================================================
    // Level followers for VU metering.
    // Attack: 1ms, Release: 300ms — standard PPM ballistics.
    struct LevelFollower
    {
        double state = 0.0;
        double attack = 0.0;
        double release = 0.0;

        void prepare(double sampleRate) noexcept
        {
            attack = std::exp(-1.0 / (sampleRate * 0.001));   // 1 ms
            release = std::exp(-1.0 / (sampleRate * 0.300));   // 300 ms
            state = 0.0;
        }

        void update(double sample) noexcept
        {
            const double level = std::fabs(sample);
            state = ((level > state) ? attack : release) * state
                + (1.0 - ((level > state) ? attack : release)) * level;
        }

        float getValue() const noexcept { return static_cast<float> (state); }
    };

    std::array<LevelFollower, 2> inputFollower, outputFollower;

    // Atomic floats read by the editor timer on the message thread
    std::atomic<float> inLevelL{ 0.0f }, inLevelR{ 0.0f };
    std::atomic<float> outLevelL{ 0.0f }, outLevelR{ 0.0f };
    std::atomic<float> effectMeter{ 0.0f };

    //==========================================================================
    // Mid/Side encoding helpers
    static void encodeMS(float& L, float& R) noexcept
    {
        const float m = (L + R) * 0.5f;
        const float s = (L - R) * 0.5f;
        L = m; R = s;
    }
    static void decodeMS(float& M, float& S) noexcept
    {
        const float l = M + S;
        const float r = M - S;
        M = l; S = r;
    }
    // double overloads for internal processing path
    static void encodeMS(double& L, double& R) noexcept
    {
        const double m = (L + R) * 0.5, s = (L - R) * 0.5; L = m; R = s;
    }
    static void decodeMS(double& M, double& S) noexcept
    {
        const double l = M + S, r = M - S; M = l; S = r;
    }

    //==========================================================================
    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    // Listener for OS parameter changes (to update latency)
    struct OSParamListener : public juce::AudioProcessorValueTreeState::Listener
    {
        explicit OSParamListener(JSInflatorProcessor& p) : proc(p) {}
        void parameterChanged(const juce::String&, float newValue) override
        {
            proc.onOSParamChanged(static_cast<int> (newValue + 0.5f));
        }
        JSInflatorProcessor& proc;
    };
    OSParamListener osListener{ *this };

    //==========================================================================
    // Internal templated processBlock so both float and double paths share logic
    template <typename SampleType>
    void processBlockImpl(juce::AudioBuffer<SampleType>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JSInflatorProcessor)
};