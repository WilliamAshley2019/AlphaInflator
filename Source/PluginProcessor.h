//==============================================================================
// JS Inflator  v1.3 - PluginProcessor.h  (JUCE 8 / VS2022+)
// Original DSP by yg331  |  JUCE port + fixes + additions
//
// v1.3 changes vs v1.2:
//   FIX  - Delta monitoring now uses a proper dry-signal delay line so the
//           unprocessed signal is correctly time-aligned with the OS output.
//   FIX  - Output VU/RMS meters now read AFTER AGC gain is applied.
//   FIX  - Delta extraction moved before the safety limiter so the delta
//           shows effect-only content, not inverted limiter gain-reduction.
//   NEW  - SPLIT_TYPE: choose between the original yg331 SVF allpass
//           crossover or the simple one-pole (new) crossover.
//   NEW  - CLIP_MODE: Off / Hard (original ±1.0) / Soft (Padé tanh) /
//           Hard+Soft (soft saturation with a guaranteed ±1.0 ceiling).
//           Hard mode also restores the pre-inflator hard clip the original had.
//   NEW  - OS_QUAL: Min Phase IIR (low latency) or Linear Phase FIR
//           (higher quality, higher latency).  Both sets are pre-built in
//           prepareToPlay so switching doesn't allocate on the audio thread.
//   NEW  - DC_BLOCK toggle: the always-on DC blocker can now be disabled
//           for a completely transparent signal path when effect is bypassed.
//==============================================================================

#pragma once
#include <JuceHeader.h>

//==============================================================================
// Parameter IDs — single source of truth
//==============================================================================
namespace ParamID
{
    // ── Original ─────────────────────────────────────────────────────────────
    inline const juce::String INPUT = "input";
    inline const juce::String EFFECT = "effect";
    inline const juce::String CURVE = "curve";
    inline const juce::String OUTPUT = "output";
    inline const juce::String SPLIT = "split";
    inline const juce::String MS_MODE = "msmode";
    inline const juce::String IN = "in";
    inline const juce::String BYPASS = "bypass";
    // ── Upgraded ─────────────────────────────────────────────────────────────
    inline const juce::String CLIP_MODE = "clipMode";   // 0=Off 1=Hard 2=Soft 3=Hard+Soft
    inline const juce::String OS = "os";          // 0=1x 1=2x 2=4x 3=8x
    inline const juce::String OS_QUAL = "osQual";      // 0=MinPhase IIR  1=LinearPhase FIR
    inline const juce::String SPLIT_TYPE = "splitType";   // 0=Simple  1=Original SVF
    // ── New features ─────────────────────────────────────────────────────────
    inline const juce::String AGC_MODE = "agcMode";    // 0=Off 1=Static 2=Dynamic
    inline const juce::String DELTA = "delta";
    inline const juce::String TONE = "tone";        // -50..+50 tilt
    inline const juce::String FOCUS = "focus";       // 0=Full 1=Low 2=Mid 3=High
    inline const juce::String DYN_MODE = "dynMode";     // 0=Smooth 1=Neutral 2=Punch 3=Dense
    inline const juce::String LIMITER = "limiter";
    inline const juce::String LIM_CEIL = "limCeil";     // -6..-0.1 dBFS
    inline const juce::String DC_BLOCK = "dcBlock";     // bool
}

//==============================================================================
// Colour palette — shared between processor and editor headers
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
// ── DSP structs (header-only, used only inside PluginProcessor.cpp) ──────────
//==============================================================================

//------------------------------------------------------------------------------
// Original yg331 SVF allpass band-split — ported verbatim from JSIF_processor.cpp
//
//   LP:  R = I + C*(x - I)          HP:  R = (1-C)*I + C*x
//        I = 2*R - I                      I = 2*R - I
//
// Coefficient formula (bilinear-tan warping):
//   C = 0.5 * tan(π*(fc/Fs - 0.25)) + 0.5
//
// This produces a feedforward blend R = (1-C)*I + C*x  rather than a
// pure state-variable output, giving the original its unique crossover character.
//------------------------------------------------------------------------------
struct OriginalSVFBandSplit
{
    double LP_C = 0, LP_R = 0, LP_I = 0;
    double HP_C = 0, HP_R = 0, HP_I = 0;
    double G = 1.0, GR = 1.0;

    void setFrequencies(double fcLow, double fcHigh, double fs) noexcept
    {
        constexpr double pi = juce::MathConstants<double>::pi;
        LP_C = 0.5 * std::tan(pi * (fcLow / fs - 0.25)) + 0.5;
        HP_C = 0.5 * std::tan(pi * (fcHigh / fs - 0.25)) + 0.5;
        G = HP_C * (1.0 - LP_C) / std::max(1e-12, HP_C - LP_C);
        GR = (G > 1e-12) ? 1.0 / G : 1.0;
    }

    void process(double x, double& low, double& mid, double& high) noexcept
    {
        // LP path (feedforward)
        LP_R = LP_I + LP_C * (x - LP_I);
        LP_I = 2.0 * LP_R - LP_I;
        // HP path (feedforward)
        HP_R = (1.0 - HP_C) * HP_I + HP_C * x;
        HP_I = 2.0 * HP_R - HP_I;

        low = LP_R;
        high = x - HP_R;
        mid = HP_R - LP_R;
    }

    void reset() noexcept { LP_R = LP_I = HP_R = HP_I = 0.0; }
};

//------------------------------------------------------------------------------
// Simple one-pole band-split (new, less CPU, slightly different crossover slope)
//   Coefficient:  c = 1 - exp(-2π·fc/Fs)
//   State update: s += c*(x - s)
//------------------------------------------------------------------------------
struct SimpleBandSplit
{
    double lpState = 0, hpState = 0;
    double coLow = 0, coHigh = 0;
    double G = 1.0, GR = 1.0;

    void setFrequencies(double fcLow, double fcHigh, double fs) noexcept
    {
        constexpr double twopi = 2.0 * juce::MathConstants<double>::pi;
        coLow = 1.0 - std::exp(-twopi * fcLow / fs);
        coHigh = 1.0 - std::exp(-twopi * fcHigh / fs);
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

//------------------------------------------------------------------------------
// TiltEQ — one-pole shelving tilt around ~800 Hz
//   tone +1 → +6 dB high / -6 dB low;   tone -1 → warm
//------------------------------------------------------------------------------
struct TiltEQ
{
    double lpState = 0, coeff = 0;

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

//------------------------------------------------------------------------------
// DC Blocker — first-order HP at ~5 Hz
//------------------------------------------------------------------------------
struct DCBlocker
{
    double x1 = 0, y1 = 0;

    double process(double x) noexcept
    {
        y1 = x - x1 + 0.99975 * y1;
        x1 = x;
        return y1;
    }

    void reset() noexcept { x1 = y1 = 0.0; }
};

//------------------------------------------------------------------------------
// 1 ms lookahead limiter (linked stereo, circular buffer)
//------------------------------------------------------------------------------
class LookaheadLimiter
{
public:
    static constexpr int    MAX_BUF = 128;
    static constexpr double RELEASE_S = 0.050;

    void prepare(double fs) noexcept
    {
        lookaheadN = std::min(MAX_BUF, std::max(1, (int)(fs * 0.001)));
        releaseCoeff = std::exp(-1.0 / (fs * RELEASE_S));
        reset();
    }

    void reset() noexcept
    {
        std::fill(bufL, bufL + MAX_BUF, 0.0);
        std::fill(bufR, bufR + MAX_BUF, 0.0);
        gainRed = 1.0;  writePos = 0;
    }

    void processFrame(double& L, double& R, double ceiling) noexcept
    {
        bufL[writePos] = L;  bufR[writePos] = R;
        const double pk = std::max(std::fabs(L), std::fabs(R));
        if (pk > ceiling && pk > 1e-12)
            gainRed = std::min(gainRed, ceiling / pk);
        gainRed += (1.0 - gainRed) * (1.0 - releaseCoeff);
        gainRed = std::min(gainRed, 1.0);
        const int rp = (writePos - lookaheadN + MAX_BUF) % MAX_BUF;
        L = bufL[rp] * gainRed;  R = bufR[rp] * gainRed;
        writePos = (writePos + 1) % MAX_BUF;
    }

    int    getLatencySamples()  const noexcept { return lookaheadN; }
    double getGainReductionDB() const noexcept
    {
        return juce::Decibels::gainToDecibels(gainRed);
    }

private:
    double bufL[MAX_BUF] = {}, bufR[MAX_BUF] = {};
    int    writePos = 0, lookaheadN = 48;
    double gainRed = 1.0, releaseCoeff = 0.999;
};

//------------------------------------------------------------------------------
// Transient detector — fast/slow envelope ratio
//------------------------------------------------------------------------------
struct TransientDetector
{
    double fast = 0, slow = 0, fc = 0, sc = 0;

    void prepare(double fs) noexcept
    {
        fc = std::exp(-1.0 / (fs * 0.005));
        sc = std::exp(-1.0 / (fs * 0.100));
        fast = slow = 0.0;
    }

    double process(double x) noexcept
    {
        const double lv = std::fabs(x);
        fast = fc * fast + (1.0 - fc) * lv;
        slow = sc * slow + (1.0 - sc) * lv;
        const double d = fast - slow;
        return (slow > 1e-10) ? juce::jlimit(0.0, 1.0, d / slow) : 0.0;
    }

    void reset() noexcept { fast = slow = 0.0; }
};

//------------------------------------------------------------------------------
// PPM level follower (1 ms attack, 300 ms release)
//------------------------------------------------------------------------------
struct LevelFollower
{
    double state = 0, atk = 0, rel = 0;

    void prepare(double fs) noexcept
    {
        atk = std::exp(-1.0 / (fs * 0.001));
        rel = std::exp(-1.0 / (fs * 0.300));
        state = 0.0;
    }

    void update(double x) noexcept
    {
        const double lv = std::fabs(x);
        state = ((lv > state) ? atk : rel) * state
            + (1.0 - ((lv > state) ? atk : rel)) * lv;
    }

    float  getValue() const noexcept { return static_cast<float> (state); }
    void   reset()    noexcept { state = 0.0; }
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
    bool   hasEditor()           const override { return true; }
    const  juce::String getName() const override { return "JS Inflator"; }
    bool   acceptsMidi()          const override { return false; }
    bool   producesMidi()         const override { return false; }
    bool   isMidiEffect()         const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int    getNumPrograms()    override { return 1; }
    int    getCurrentProgram() override { return 0; }
    void   setCurrentProgram(int) override {}
    const  juce::String getProgramName(int) override { return {}; }
    void   changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int)   override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Thread-safe meter reads (message thread, 30 Hz)
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
    int   getAndClearInOversL()  noexcept { return inOversL.exchange(0); }
    int   getAndClearInOversR()  noexcept { return inOversR.exchange(0); }
    int   getAndClearOutOversL() noexcept { return outOversL.exchange(0); }
    int   getAndClearOutOversR() noexcept { return outOversR.exchange(0); }
    float getEffectMeter() const noexcept { return effectMeter.load(); }
    float getLimiterGR()   const noexcept { return limiterGR.load(); }

private:
    //==========================================================================
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Inflator polynomial (original yg331 algorithm — identical to original)
    double processInflatorSample(double x) const noexcept;
    void   updateCurveCoefficients(double curveParam) noexcept;
    double curveA = 1.5, curveB = 0.0, curveC = -0.5, curveD = 0.0625;

    // Soft-knee clip (Padé tanh, accurate ±0.0005 for |x|<4)
    static double softClip(double x) noexcept
    {
        if (x > 4.0) return  1.0;
        if (x < -4.0) return -1.0;
        const double x2 = x * x;
        return x * (27.0 + x2) / (27.0 + 9.0 * x2);
    }

    // Hard clip — matches original verbatim
    static double hardClip(double x) noexcept
    {
        return juce::jlimit(-1.0, 1.0, x);
    }

    // M/S helpers
    static void encodeMS(float& L, float& R) noexcept { const float  m = (L + R) * .5f, s = (L - R) * .5f; L = m; R = s; }
    static void decodeMS(float& M, float& S) noexcept { const float  l = M + S, r = M - S;              M = l; S = r; }
    static void encodeMS(double& L, double& R) noexcept { const double m = (L + R) * .5, s = (L - R) * .5;   L = m; R = s; }
    static void decodeMS(double& M, double& S) noexcept { const double l = M + S, r = M - S;              M = l; S = r; }

    //==========================================================================
    // Oversampling — IIR (min phase) and FIR (linear phase) variants
    // Both pre-built in prepareToPlay; switched via OS_QUAL param.
    // [0..2] = 2x/4x/8x IIR,   [3..5] = 2x/4x/8x FIR
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 6> oversamplers;

    std::atomic<int> requestedOSIndex{ 0 };   // 0=1x 1=2x 2=4x 3=8x
    std::atomic<int> requestedOSQual{ 0 };   // 0=IIR  1=FIR
    int              currentOSIndex = -1;
    int              currentOSQual = -1;

    void onOSParamChanged(int newIdx, int newQual);
    int  getOversamplerArrayIndex(int osIdx, int osQual) const noexcept
    {
        // 0 = 1x (no oversampler used)
        // osIdx 1..3 + qual 0..1 → array index 0..5
        return (osIdx - 1) + osQual * 3;
    }

    //==========================================================================
    // Band split — one instance per channel, two topologies
    std::array<OriginalSVFBandSplit, 2> svfSplit, svfSplitOS;
    std::array<SimpleBandSplit, 2> simpleSplit, simpleSplitOS;

    // Helper: process one band-split sample using whichever topology is selected
    void processBandSplit(int ch, bool useOS, bool useSVF,
        double x, double& low, double& mid, double& high,
        double& G, double& GR) noexcept;

    //==========================================================================
    // Other DSP objects (per-channel)
    std::array<DCBlocker, 2> dcBlocker;
    std::array<TiltEQ, 2> tiltEQ;
    std::array<TransientDetector, 2> transientDet;
    LookaheadLimiter                  lookaheadLim;

    //==========================================================================
    // Dry delay line — fixes delta monitoring time-alignment when OS is active.
    // Size = enough for max OS latency (FIR 8x) + limiter lookahead + headroom.
    static constexpr int DRY_DELAY_MAX = 16384;
    float dryDelayL[DRY_DELAY_MAX] = {};
    float dryDelayR[DRY_DELAY_MAX] = {};
    int   dryDelayWrite = 0;
    int   dryDelayLen = 0;  // = totalLatencySamples, set in prepareToPlay

    void  pushDry(float l, float r) noexcept;
    void  peekDry(float& l, float& r) const noexcept;  // reads delayed sample

    //==========================================================================
    // AGC
    double agcRmsIn = 0, agcRmsOut = 0, agcCoeffSlow = 0, agcCoeffFast = 0;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> agcGainSmooth;

    //==========================================================================
    // Smoothed parameters
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> inputGain, outputGain;
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Linear>         effectWet, curveSmoother, toneSmoother;

    //==========================================================================
    // Level followers and RMS
    std::array<LevelFollower, 2> inputFollower, outputFollower;
    std::array<double, 2>        inRmsAcc = { 0.0, 0.0 };
    std::array<double, 2>        outRmsAcc = { 0.0, 0.0 };
    double rmsCoeff = 0.0;

    float peakHoldInL = 0, peakHoldInR = 0, peakHoldOutL = 0, peakHoldOutR = 0;
    int   peakHoldCounter = 0, peakHoldSamples = 0;

    // Delta dry buffer (block-level copy at base rate, used when OS=1x only)
    juce::AudioBuffer<float> dryBuffer;

    //==========================================================================
    // Atomic meter outputs
    std::atomic<float> inLevelL{ 0 }, inLevelR{ 0 }, outLevelL{ 0 }, outLevelR{ 0 };
    std::atomic<float> inPeakL{ 0 }, inPeakR{ 0 }, outPeakL{ 0 }, outPeakR{ 0 };
    std::atomic<float> inRmsL{ 0 }, inRmsR{ 0 }, outRmsL{ 0 }, outRmsR{ 0 };
    std::atomic<int>   inOversL{ 0 }, inOversR{ 0 }, outOversL{ 0 }, outOversR{ 0 };
    std::atomic<float> effectMeter{ 0 }, limiterGR{ 0 };

    double currentSampleRate = 44100.0;
    int    currentBlockSize = 512;

    //==========================================================================
    struct OSListener : public juce::AudioProcessorValueTreeState::Listener
    {
        explicit OSListener(JSInflatorProcessor& p) : proc(p) {}
        void parameterChanged(const juce::String& id, float v) override
        {
            const int idx = static_cast<int> (
                proc.apvts.getRawParameterValue(ParamID::OS)->load() + 0.5f);
            const int qual = static_cast<int> (
                proc.apvts.getRawParameterValue(ParamID::OS_QUAL)->load() + 0.5f);
            proc.onOSParamChanged(idx, qual);
            juce::ignoreUnused(id, v);
        }
        JSInflatorProcessor& proc;
    } osListener{ *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JSInflatorProcessor)
};