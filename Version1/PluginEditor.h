//==============================================================================
// JS Inflator - PluginEditor.h
// Professional dark UI with VU metering, custom knobs and look-and-feel
//==============================================================================

#pragma once
#include "PluginProcessor.h"

//==============================================================================
// Custom LookAndFeel for the dark professional theme
//==============================================================================
class InflatorLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Colour palette (u suffix = explicitly unsigned, suppresses MSVC C4309/overflow warnings)
    static constexpr uint32_t COL_BG = 0xFF1A1A1Fu;  // near-black background
    static constexpr uint32_t COL_PANEL = 0xFF252530u;  // slightly lighter panels
    static constexpr uint32_t COL_ACCENT = 0xFF4ECDC4u;  // teal accent
    static constexpr uint32_t COL_ACCENT2 = 0xFFFFE66Du;  // yellow-gold secondary
    static constexpr uint32_t COL_KNOB_BODY = 0xFF313140u;  // knob face
    static constexpr uint32_t COL_KNOB_RIM = 0xFF4A4A60u;  // knob rim
    static constexpr uint32_t COL_TEXT = 0xFFE0E0EEu;  // main text
    static constexpr uint32_t COL_TEXTDIM = 0xFF7070A0u;  // dimmed label text
    static constexpr uint32_t COL_METER_GREEN = 0xFF2ECC71u;
    static constexpr uint32_t COL_METER_YELLOW = 0xFFF0C040u;
    static constexpr uint32_t COL_METER_RED = 0xFFE74C3Cu;
    static constexpr uint32_t COL_METER_OFF = 0xFF1A2A1Au;
    static constexpr uint32_t COL_BTN_ON = 0xFF4ECDC4u;
    static constexpr uint32_t COL_BTN_OFF = 0xFF2A2A38u;

    InflatorLookAndFeel();

    //--- Rotary knob ---------------------------------------------------------
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
        float sliderPos, float rotaryStartAngle,
        float rotaryEndAngle, juce::Slider&) override;

    //--- Toggle button -------------------------------------------------------
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;

    //--- ComboBox ------------------------------------------------------------
    void drawComboBox(juce::Graphics&, int w, int h, bool isButtonDown,
        int buttonX, int buttonY, int buttonW, int buttonH,
        juce::ComboBox&) override;

    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
        bool isSeparator, bool isActive, bool isHighlighted,
        bool isTicked, bool hasSubMenu,
        const juce::String& text,
        const juce::String& shortcutKeyText,
        const juce::Drawable* icon,
        const juce::Colour* textColour) override;
};

//==============================================================================
// Stereo VU meter component — draws segmented bar with PPM-style colouring
//==============================================================================
class VuMeterComponent : public juce::Component
{
public:
    VuMeterComponent(const juce::String& labelText);

    void setLevels(float left, float right);  // call from timer, range [0..1]
    void paint(juce::Graphics&) override;
    void resized() override {}

private:
    juce::String label;
    float levelL = 0.0f, levelR = 0.0f;

    // Convert linear gain to a normalised bar position using a dB scale
    static float gainToBarPos(float gain) noexcept;

    void drawBar(juce::Graphics&, juce::Rectangle<float> bounds,
        float pos) const;
};

//==============================================================================
// Single-channel effect meter (shows how much the inflator is working)
//==============================================================================
class EffectMeterComponent : public juce::Component
{
public:
    void setLevel(float v) { level = v; repaint(); }
    void paint(juce::Graphics&) override;

private:
    float level = 0.0f;
};

//==============================================================================
// Main plugin editor
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
    JSInflatorProcessor& processor;
    InflatorLookAndFeel  laf;

    //--- Knobs ---------------------------------------------------------------
    juce::Slider inputKnob{ juce::Slider::Rotary, juce::Slider::TextBoxBelow };
    juce::Slider effectKnob{ juce::Slider::Rotary, juce::Slider::TextBoxBelow };
    juce::Slider curveKnob{ juce::Slider::Rotary, juce::Slider::TextBoxBelow };
    juce::Slider outputKnob{ juce::Slider::Rotary, juce::Slider::TextBoxBelow };

    juce::Label inputLabel, effectLabel, curveLabel, outputLabel;

    //--- Toggle buttons ------------------------------------------------------
    juce::ToggleButton inButton{ "IN" };
    juce::ToggleButton clipButton{ "CLIP" };
    juce::ToggleButton splitButton{ "SPLIT" };
    juce::ToggleButton msButton{ "M/S" };
    juce::ToggleButton bypassButton{ "BYPASS" };

    //--- Oversampling combo --------------------------------------------------
    juce::ComboBox osCombo;
    juce::Label    osLabel;

    //--- Metering ------------------------------------------------------------
    VuMeterComponent  inputMeter{ "IN" };
    VuMeterComponent  outputMeter{ "OUT" };
    EffectMeterComponent effectMeterComp;
    juce::Label effectMeterLabel;

    //--- APVTS Attachments ---------------------------------------------------
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttachment> inputAtt, effectAtt, curveAtt, outputAtt;
    std::unique_ptr<ButtonAttachment> inAtt, clipAtt, splitAtt, msAtt, bypassAtt;
    std::unique_ptr<ComboAttachment>  osAtt;

    //--- Helpers -------------------------------------------------------------
    void setupKnob(juce::Slider& s, juce::Label& l, const juce::String& text);
    void setupButton(juce::ToggleButton& b);

    void timerCallback() override;  // drives meter updates at 30 Hz

    // Painting helpers
    void paintBackground(juce::Graphics&);
    void paintHeaderBand(juce::Graphics&);
    void paintSectionDividers(juce::Graphics&);

    static constexpr int WIDTH = 480;
    static constexpr int HEIGHT = 340;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JSInflatorEditor)
};