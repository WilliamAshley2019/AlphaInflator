//==============================================================================
// Alpha Inflator  v1.2 - PluginEditor.h  (JUCE 8)
//
// NOTE: Pal:: colour constants live in PluginProcessor.h only.
//       This file includes PluginProcessor.h and uses Pal:: directly.
//       Never define Pal:: here — that caused the redefinition errors.
//==============================================================================

#pragma once
#include "PluginProcessor.h"

//==============================================================================
// InflatorLAF  — custom look-and-feel (dark teal/gold theme)
//==============================================================================
class InflatorLAF : public juce::LookAndFeel_V4
{
public:
    InflatorLAF();

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
        float sliderPos, float startAngle, float endAngle,
        juce::Slider&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
        bool highlighted, bool down) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h,
        float sliderPos, float minPos, float maxPos,
        const juce::Slider::SliderStyle, juce::Slider&) override;

    void drawComboBox(juce::Graphics&, int w, int h, bool isDown,
        int bx, int by, int bw, int bh, juce::ComboBox&) override;

    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>&,
        bool isSeparator, bool isActive, bool isHighlighted,
        bool isTicked, bool hasSubMenu,
        const juce::String& text, const juce::String& shortcut,
        const juce::Drawable* icon,
        const juce::Colour* textColour) override;

    // Mark a toggle button to render in warning/coral colour when ON
    void setWarning(juce::ToggleButton& btn, bool isWarning)
    {
        warningSet.set(&btn, isWarning);
    }

private:
    juce::HashMap<juce::Component*, bool> warningSet;
};

//==============================================================================
// VuMeterBar  — stereo bar with PPM, RMS overlay, peak-hold tick, overs badge
//==============================================================================
class VuMeterBar : public juce::Component
{
public:
    struct MeterData
    {
        float ppm = 0.0f;   // linear peak
        float rms = 0.0f;   // linear RMS
        float peak = 0.0f;   // held peak (linear)
        int   overs = 0;      // new overs since last read
    };

    explicit VuMeterBar(const juce::String& label);

    void setDataL(const MeterData& d);
    void setDataR(const MeterData& d);
    void resetOvers();
    void paint(juce::Graphics&) override;

private:
    juce::String label;
    MeterData    dataL, dataR;
    int          totalOversL = 0, totalOversR = 0;

    static float gainToPos(float gain) noexcept;
    void drawChannel(juce::Graphics&, juce::Rectangle<float> bounds,
        const MeterData& data, bool leftChannel) const;
};

//==============================================================================
// GRMeter  — horizontal gain-reduction bar for the limiter
//==============================================================================
class GRMeter : public juce::Component
{
public:
    void setGRdB(float dB);
    void paint(juce::Graphics&) override;
private:
    float grDB = 0.0f;
};

//==============================================================================
// EffectBar  — teal→gold horizontal amount bar
//==============================================================================
class EffectBar : public juce::Component
{
public:
    void setLevel(float v) { level = juce::jlimit(0.0f, 1.0f, v); repaint(); }
    void paint(juce::Graphics&) override;
private:
    float level = 0.0f;
};

//==============================================================================
// ABSnapshot  — stores a full copy of every parameter value (editor-side only)
//==============================================================================
struct ABSnapshot
{
    juce::HashMap<juce::String, float> values;
    bool valid = false;
};

//==============================================================================
// JSInflatorEditor
//==============================================================================
class JSInflatorEditor : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit JSInflatorEditor(JSInflatorProcessor&);
    ~JSInflatorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    JSInflatorProcessor& proc;
    InflatorLAF          laf;

    // ── Geometry constants ────────────────────────────────────────────────────
    static constexpr int BASE_W = 560;
    static constexpr int BASE_H = 400;
    float uiScale = 1.0f;

    // ── Scale selector ────────────────────────────────────────────────────────
    juce::ComboBox scaleCombo;

    // ── Core knobs ────────────────────────────────────────────────────────────
    juce::Slider inputKnob, effectKnob, curveKnob, outputKnob, toneKnob;
    juce::Label  inputLbl, effectLbl, curveLbl, outputLbl, toneLbl;

    // ── Limiter ceiling slider ─────────────────────────────────────────────────
    juce::Slider limCeilSlider;
    juce::Label  limCeilLbl;

    // ── Toggle buttons ─────────────────────────────────────────────────────────
    juce::ToggleButton inBtn, clipBtn, splitBtn, msBtn;
    juce::ToggleButton deltaBtn, limiterBtn, bypassBtn;   // warning-coloured

    // ── Combo boxes + labels ──────────────────────────────────────────────────
    juce::ComboBox osCombo, agcCombo, focusCombo, dynCombo;
    juce::Label    osLbl, agcLbl, focusLbl, dynLbl;

    // ── A/B Compare buttons ───────────────────────────────────────────────────
    juce::TextButton abSaveABtn{ "SAVE A" };
    juce::TextButton abSaveBBtn{ "SAVE B" };
    juce::TextButton abToABtn{ "A" };
    juce::TextButton abToBBtn{ "B" };
    ABSnapshot       snapA, snapB;

    void saveSnap(ABSnapshot& snap);
    void loadSnap(const ABSnapshot& snap);
    void onAB(bool loadA);

    // ── Metering ──────────────────────────────────────────────────────────────
    VuMeterBar inMeter{ "IN" };
    VuMeterBar outMeter{ "OUT" };
    EffectBar  effectBar;
    GRMeter    grMeter;
    juce::Label effectBarLbl, grMeterLbl;

    // ── APVTS attachments ─────────────────────────────────────────────────────
    using SlAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BtnAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CbAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SlAtt>  attInput, attEffect, attCurve, attOutput, attTone, attLimCeil;
    std::unique_ptr<BtnAtt> attIn, attClip, attSplit, attMS, attBypass, attDelta, attLimiter;
    std::unique_ptr<CbAtt>  attOS, attAGC, attFocus, attDyn;

    // ── Setup helpers ─────────────────────────────────────────────────────────
    void setupKnob(juce::Slider& s, juce::Label& l, const juce::String& text);
    void setupToggle(juce::ToggleButton& b, const juce::String& text, bool warn = false);
    void setupCombo(juce::ComboBox& c, juce::Label& l,
        const juce::String& labelText, const juce::StringArray& items);
    void setupTextBtn(juce::TextButton& b);
    void applyScale(float newScale);

    // ── Timer (30 Hz meter updates) ───────────────────────────────────────────
    void timerCallback() override;

    // ── Paint helpers ─────────────────────────────────────────────────────────
    void paintBackground(juce::Graphics&);
    void paintHeader(juce::Graphics&);
    void paintSectionLabels(juce::Graphics&);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JSInflatorEditor)
};