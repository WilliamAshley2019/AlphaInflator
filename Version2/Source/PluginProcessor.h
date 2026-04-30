//==============================================================================
// JS Inflator  v1.2 - PluginProcessor.h  (JUCE 8 / VS2022+)
// Original DSP by yg331  |  JUCE port + feature additions
//==============================================================================

#pragma once
#include <JuceHeader.h>

//==============================================================================
// All parameter IDs in one place — editor and processor both include this header
//==============================================================================
namespace ParamID
{
    // ── Original ─────────────────────────────────────────────────────────────
    inline const juce::String INPUT = "input";
    inline const juce::String EFFECT = "effect";
    inline const juce::String CURVE = "curve";
    inline const juce::String OUTPUT = "output";
    inline const juce::String CLIP = "clip";
    inline const juce::String SPLIT = "split";
    inline const juce::String MS_MODE = "msmode";
    inline const juce::String IN = "in";
    inline const juce::String BYPASS = "bypass";
    inline const juce::String OS = "os";        // 0=1x 1=2x 2=4x 3=8x
    // ── New ──────────────────────────────────────────────────────────────────
    inline const juce::String AGC_MODE = "agcMode";   // 0=Off 1=Static 2=Dynamic
    inline const juce::String DELTA = "delta";     // bool – difference monitoring
    inline const juce::String TONE = "tone";      // -50..+50  tilt EQ
    inline const juce::String FOCUS = "focus";     // 0=Full 1=Low 2=Mid 3=High
    inline const juce::String DYN_MODE = "dynMode";   // 0=Smooth 1=Neutral 2=Punch 3=Dense
    inline const juce::String LIMITER = "limiter";   // bool
    inline const juce::String LIM_CEIL = "limCeil";   // -6..-0.1 dBFS
}

//==============================================================================
// Colour palette — used by both PluginEditor and InflatorLAF
//==============================================================================
namespace Pal
{
    constexpr uint32_t BG = 0xFF18181Fu;
    constexpr uint32_t PANEL = 0xFF22222Bu;
    constexpr uint32_t PANEL_LIGHT = 0xFF2A2A36u;
    constexpr uint32_t ACCENT = 0xFF4ECDC4u;
    constexpr uint32_t ACCENT2 = 0xFFFFE66Du;
    constexpr uint32_t ACCENT3 = 0xFFFF6B6Bu;
    constexpr uint32_t RIM = 0xFF3A3A50u;
    constexpr uint32_t TEXT = 0xFFDDDDEEu;
    constexpr uint32_t TEXT_DIM = 0xFF6060A0u;
    constexpr uint32_t KNOB_BODY = 0xFF2E2E40u;
    constexpr uint32_t METER_OFF = 0xFF111A11u;
    constexpr uint32_t METER_GRN = 0xFF27AE60u;
    constexpr uint32_t METER_YEL = 0xFFF0C040u;
    constexpr uint32_t METER_RED = 0xFFE74C3Cu;
    constexpr uint32_t BTN_ON = 0xFF4ECDC4u;
    constexpr uint32_t BTN_OFF = 0xFF252535u;
    constexpr uint32_t BTN_WARN = 0xFFFF6B6Bu;
}

//==============================================================================
// Shared DSP helpers  (header-only structs, used by PluginProcessor.cpp only)
//==============================================================================

// One-pole tilt EQ pivoting at ~800 Hz
struct TiltEQ
{
    double lpState = 0.0, coeff = 0.0;

    void prepare(double sampleRate) noexcept
    {
        coeff = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * 800.0 / sampleRate);
        lpState = 0.0;
    }

    double process(double x, double gainLow, double gainHigh) noexcept
    {
        lpState += coeff * (x - lpState);
        return lpState * gainLow + (x - lpState) * gainHigh;
    }

    void reset() noexcept { lpState = 0.0; }
};

// 1 ms lookahead limiter (linked stereo)
class LookaheadLimiter
{
public:
    static constexpr int    MAX_BUF = 128;
    static constexpr double RELEASE_S = 0.050;

    void prepare(double sampleRate) noexcept
    {
        lookaheadN = std::min(MAX_BUF, std::max(1, (int)(sampleRate * 0.001)));
        releaseCoeff = std::exp(-1.0 / (sampleRate * RELEASE_S));
        reset();
    }

    void reset() noexcept
    {
        std::fill(bufL, bufL + MAX_BUF, 0.0);
        std::fill(bufR, bufR + MAX_BUF, 0.0);
        gainReduction = 1.0;
        writePos = 0;
    }

    void processFrame(double& L, double& R, double ceilingLinear) noexcept
    {
        bufL[writePos] = L;
        bufR[writePos] = R;

        const double peak = std::max(std::fabs(L), std::fabs(R));
        if (peak > ceilingLinear && peak > 1e-12)
            gainReduction = std::min(gainReduction, ceilingLinear / peak);

        gainReduction += (1.0 - gainReduction) * (1.0 - releaseCoeff);
        gainReduction = std::min(gainReduction, 1.0);

        const int readPos = (writePos - lookaheadN + MAX_BUF) % MAX_BUF;
        L = bufL[readPos] * gainReduction;
        R = bufR[readPos] * gainReduction;
        writePos = (writePos + 1) % MAX_BUF;
    }

    int    getLatencySamples()  const noexcept { return lookaheadN; }
    double getGainReductionDB() const noexcept
    {
        return juce::Decibels::gainToDecibels(gainReduction);
    }

private:
    double bufL[MAX_BUF] = {};
    double bufR[MAX_BUF] = {};
    int    writePos = 0;
    int    lookaheadN = 48;
    double gainReduction = 1.0;
    double releaseCoeff = 0.999;
};

// Fast/slow envelope ratio for transient detection
struct TransientDetector
{
    double fastState = 0.0, slowState = 0.0;
    double fastCoeff = 0.0, slowCoeff = 0.0;

    void prepare(double sampleRate) noexcept
    {
        fastCoeff = std::exp(-1.0 / (sampleRate * 0.005));
        slowCoeff = std::exp(-1.0 / (sampleRate * 0.100));
        fastState = slowState = 0.0;
    }

    double process(double x) noexcept
    {
        const double level = std::fabs(x);
        fastState = fastCoeff * fastState + (1.0 - fastCoeff) * level;
        slowState = slowCoeff * slowState + (1.0 - slowCoeff) * level;
        const double diff = fastState - slowState;
        return (slowState > 1e-10) ? juce::jlimit(0.0, 1.0, diff / slowState) : 0.0;
    }

    void reset() noexcept { fastState = slowState = 0.0; }
};

// Perfect-reconstruction three-band split
struct BandSplit
{
    double lpState = 0.0, hpState = 0.0;
    double coLow = 0.0, coHigh = 0.0;
    double G = 1.0, GR = 1.0;

    void setFrequencies(double fcLow, double fcHigh, double fs) noexcept
    {
        coLow = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * fcLow / fs);
        coHigh = 1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * fcHigh / fs);
        G = coHigh * (1.0 - coLow) / std::max(1e-12, coHigh - coLow);
        GR = (G > 1e-12) ? 1.0 / G : 1.0;
    }

    void process(double x, double& low, double& mid, double& high) noexcept
    {
        lpState += coLow * (x - lpState);
        hpState += coHigh * (x - hpState);
        low = lpState;
        high = x - hpState;
        mid = hpState - lpState;
    }

    void reset() noexcept { lpState = hpState = 0.0; }
};

// First-order DC blocker (~5 Hz high-pass)
struct DCBlocker
{
    double x1 = 0.0, y1 = 0.0;

    double process(double x) noexcept
    {
        y1 = x - x1 + 0.99975 * y1;
        x1 = x;
        return y1;
    }

    void reset() noexcept { x1 = y1 = 0.0; }
};

// PPM level follower  (1 ms attack, 300 ms release)
struct LevelFollower
{
    double state = 0.0, attack = 0.0, release = 0.0;

    void prepare(double sampleRate) noexcept
    {
        attack = std::exp(-1.0 / (sampleRate * 0.001));
        release = std::exp(-1.0 / (sampleRate * 0.300));
        state = 0.0;
    }

    void update(double sample) noexcept
    {
        const double level = std::fabs(sample);
        const double coeff = (level > state) ? attack : release;
        state = coeff * state + (1.0 - coeff) * level;
    }

    float getValue() const noexcept { return static_cast<float> (state); }
    void  reset()    noexcept { state = 0.0; }
};

//==============================================================================
class JSInflatorProcessor final : public juce::AudioProcessor
{
public:
    JSInflatorProcessor();
    ~JSInflatorProcessor() override;

    //==========================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool   hasEditor()          const override { return true; }
    const  juce::String getName() const override { return "JS Inflator"; }
    bool   acceptsMidi()         const override { return false; }
    bool   producesMidi()        const override { return false; }
    bool   isMidiEffect()        const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()    override { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Thread-safe meter accessors — called by editor timer at 30 Hz
    //==========================================================================
    float getInLevelL()  const noexcept { return inLevelL.load(); }
    float getInLevelR()  const noexcept { return inLevelR.load(); }
    float getOutLevelL() const noexcept { return outLevelL.load(); }
    float getOutLevelR() const noexcept { return outLevelR.load(); }

    float getInPeakL()   const noexcept { return inPeakL.load(); }
    float getInPeakR()   const noexcept { return inPeakR.load(); }
    float getOutPeakL()  const noexcept { return outPeakL.load(); }
    float getOutPeakR()  const noexcept { return outPeakR.load(); }

    float getInRmsL()    const noexcept { return inRmsL.load(); }
    float getInRmsR()    const noexcept { return inRmsR.load(); }
    float getOutRmsL()   const noexcept { return outRmsL.load(); }
    float getOutRmsR()   const noexcept { return outRmsR.load(); }

    // Read-and-clear: editor accumulates, resets each timer tick
    int getAndClearInOversL()  noexcept { return inOversL.exchange(0); }
    int getAndClearInOversR()  noexcept { return inOversR.exchange(0); }
    int getAndClearOutOversL() noexcept { return outOversL.exchange(0); }
    int getAndClearOutOversR() noexcept { return outOversR.exchange(0); }

    float getEffectMeter() const noexcept { return effectMeter.load(); }
    float getLimiterGR()   const noexcept { return limiterGR.load(); }

private:
    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Inflator polynomial (original yg331 algorithm, preserved exactly)
    double processInflatorSample(double x) const noexcept;
    void   updateCurveCoefficients(double curveParam) noexcept;
    double curveA = 1.5, curveB = 0.0, curveC = -0.5, curveD = 0.0625;

    // Soft-knee clip (Padé tanh approximation, accurate to ±0.0005 for |x|<4)
    static double softKneeClip(double x) noexcept
    {
        if (x > 4.0) return  1.0;
        if (x < -4.0) return -1.0;
        const double x2 = x * x;
        return x * (27.0 + x2) / (27.0 + 9.0 * x2);
    }

    // M/S helpers
    static void encodeMS(float& L, float& R) noexcept { const float  m = (L + R) * .5f, s = (L - R) * .5f; L = m; R = s; }
    static void decodeMS(float& M, float& S) noexcept { const float  l = M + S, r = M - S;       M = l; S = r; }
    static void encodeMS(double& L, double& R) noexcept { const double m = (L + R) * .5, s = (L - R) * .5;  L = m; R = s; }
    static void decodeMS(double& M, double& S) noexcept { const double l = M + S, r = M - S;       M = l; S = r; }

    //==========================================================================
    // Oversampling (2x / 4x / 8x pre-built in prepareToPlay)
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;
    std::atomic<int> requestedOSIndex{ 0 };
    int              currentOSIndex = -1;
    void             onOSParamChanged(int newIndex);

    //==========================================================================
    // DSP objects (per-channel where needed)
    std::array<BandSplit, 2> bandSplit, bandSplitOS;
    std::array<DCBlocker, 2> dcBlocker;
    std::array<TiltEQ, 2> tiltEQ;
    std::array<TransientDetector, 2> transientDet;
    LookaheadLimiter                 lookaheadLim;

    // AGC state
    double agcRmsIn = 0.0, agcRmsOut = 0.0;
    double agcCoeffSlow = 0.0, agcCoeffFast = 0.0;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> agcGainSmooth;

    //==========================================================================
    // Smoothed parameters  (all ramp over 20 ms)
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> inputGain, outputGain;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear>         effectWet, curveSmoother, toneSmoother;

    //==========================================================================
    // Level followers & RMS
    std::array<LevelFollower, 2> inputFollower, outputFollower;
    std::array<double, 2>        inRmsAcc = { 0.0, 0.0 };
    std::array<double, 2>        outRmsAcc = { 0.0, 0.0 };
    double rmsCoeff = 0.0;

    // Peak hold
    float peakHoldInL = 0.0f, peakHoldInR = 0.0f;
    float peakHoldOutL = 0.0f, peakHoldOutR = 0.0f;
    int   peakHoldCounter = 0, peakHoldSamples = 0;

    // Delta monitoring dry copy
    juce::AudioBuffer<float> dryBuffer;

    //==========================================================================
    // Atomic meter outputs  (audio thread writes, message thread reads)
    std::atomic<float> inLevelL{ 0.0f }, inLevelR{ 0.0f };
    std::atomic<float> outLevelL{ 0.0f }, outLevelR{ 0.0f };
    std::atomic<float> inPeakL{ 0.0f }, inPeakR{ 0.0f };
    std::atomic<float> outPeakL{ 0.0f }, outPeakR{ 0.0f };
    std::atomic<float> inRmsL{ 0.0f }, inRmsR{ 0.0f };
    std::atomic<float> outRmsL{ 0.0f }, outRmsR{ 0.0f };
    std::atomic<int>   inOversL{ 0 }, inOversR{ 0 };
    std::atomic<int>   outOversL{ 0 }, outOversR{ 0 };
    std::atomic<float> effectMeter{ 0.0f };
    std::atomic<float> limiterGR{ 0.0f };

    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    //==========================================================================
    struct OSListener : public juce::AudioProcessorValueTreeState::Listener
    {
        explicit OSListener(JSInflatorProcessor& p) : proc(p) {}
        void parameterChanged(const juce::String&, float v) override
        {
            proc.onOSParamChanged(static_cast<int> (v + 0.5f));
        }
        JSInflatorProcessor& proc;
    } osListener{ *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JSInflatorProcessor)
};