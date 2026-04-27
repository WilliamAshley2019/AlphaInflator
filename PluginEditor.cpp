//==============================================================================
// JS Inflator - PluginEditor.cpp
//==============================================================================

#include "PluginEditor.h"

using juce::Colour;
using juce::Rectangle;
using juce::Graphics;

//==============================================================================
// InflatorLookAndFeel
//==============================================================================
InflatorLookAndFeel::InflatorLookAndFeel()
{
    // Combo box colours
    setColour (juce::ComboBox::backgroundColourId,   Colour (COL_PANEL));
    setColour (juce::ComboBox::outlineColourId,       Colour (COL_KNOB_RIM));
    setColour (juce::ComboBox::textColourId,          Colour (COL_TEXT));
    setColour (juce::ComboBox::arrowColourId,         Colour (COL_ACCENT));
    setColour (juce::PopupMenu::backgroundColourId,   Colour (COL_PANEL));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colour (COL_ACCENT).withAlpha (0.4f));
    setColour (juce::PopupMenu::textColourId,         Colour (COL_TEXT));
    setColour (juce::PopupMenu::highlightedTextColourId, Colour (COL_TEXT));

    // Slider text box
    setColour (juce::Slider::textBoxTextColourId,     Colour (COL_TEXTDIM));
    setColour (juce::Slider::textBoxBackgroundColourId, Colour (0x00000000));
    setColour (juce::Slider::textBoxOutlineColourId,  Colour (0x00000000));
    setColour (juce::Slider::textBoxHighlightColourId, Colour (COL_ACCENT).withAlpha (0.5f));
}

//------------------------------------------------------------------------------
void InflatorLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                             float sliderPos, float startAngle,
                                             float endAngle, juce::Slider& slider)
{
    const float diameter = static_cast<float> (juce::jmin (w, h)) - 8.0f;
    const float radius   = diameter * 0.5f;
    const float centreX  = static_cast<float> (x) + static_cast<float> (w) * 0.5f;
    const float centreY  = static_cast<float> (y) + static_cast<float> (h) * 0.5f - 6.0f;

    // Outer rim
    g.setColour (Colour (COL_KNOB_RIM));
    g.fillEllipse (centreX - radius - 2.0f, centreY - radius - 2.0f,
                   diameter + 4.0f, diameter + 4.0f);

    // Knob body gradient
    juce::ColourGradient grad (Colour (COL_KNOB_BODY).brighter (0.15f),
                               centreX - radius * 0.4f, centreY - radius * 0.4f,
                               Colour (COL_KNOB_BODY).darker (0.3f),
                               centreX + radius * 0.4f, centreY + radius * 0.4f,
                               true);
    g.setGradientFill (grad);
    g.fillEllipse (centreX - radius, centreY - radius, diameter, diameter);

    // Arc track (background)
    const float arcR = radius + 3.5f;
    juce::Path arcBg;
    arcBg.addArc (centreX - arcR, centreY - arcR, arcR * 2.0f, arcR * 2.0f,
                  startAngle, endAngle, true);
    g.setColour (Colour (0xFF2A2A3A));
    g.strokePath (arcBg, juce::PathStrokeType (3.5f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    // Arc track (active, teal)
    const float sliderAngle = startAngle + sliderPos * (endAngle - startAngle);
    juce::Path arcActive;
    arcActive.addArc (centreX - arcR, centreY - arcR, arcR * 2.0f, arcR * 2.0f,
                      startAngle, sliderAngle, true);
    g.setColour (Colour (COL_ACCENT));
    g.strokePath (arcActive, juce::PathStrokeType (3.5f,
                                                    juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

    // Indicator dot
    const float dotR  = radius * 0.12f;
    const float dotDx = (radius - dotR * 1.8f) * std::sin (sliderAngle);
    const float dotDy = -(radius - dotR * 1.8f) * std::cos (sliderAngle);
    g.setColour (Colour (COL_ACCENT).brighter (0.2f));
    g.fillEllipse (centreX + dotDx - dotR, centreY + dotDy - dotR,
                   dotR * 2.0f, dotR * 2.0f);

    // Subtle centre highlight
    g.setColour (Colour (0x20FFFFFF));
    g.fillEllipse (centreX - radius * 0.35f, centreY - radius * 0.45f,
                   radius * 0.35f, radius * 0.25f);

    juce::ignoreUnused (slider);
}

//------------------------------------------------------------------------------
void InflatorLookAndFeel::drawToggleButton (Graphics& g, juce::ToggleButton& btn,
                                             bool highlighted, bool /*down*/)
{
    const bool on      = btn.getToggleState();
    const auto bounds  = btn.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = 5.0f;

    // Background
    g.setColour (on ? Colour (COL_BTN_ON).withAlpha (0.85f)
                    : Colour (COL_BTN_OFF));
    g.fillRoundedRectangle (bounds, corner);

    // Rim
    g.setColour (on ? Colour (COL_ACCENT)
                    : Colour (COL_KNOB_RIM).withAlpha (highlighted ? 0.9f : 0.5f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

    // Text
    g.setColour (on ? Colour (COL_BG) : Colour (COL_TEXTDIM));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.5f)
                                              .withStyle ("Bold")));
    g.drawFittedText (btn.getButtonText(), btn.getLocalBounds(),
                      juce::Justification::centred, 1);
}

//------------------------------------------------------------------------------
void InflatorLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool,
                                         int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = Rectangle<float> (0, 0, (float)w, (float)h);
    g.setColour (Colour (COL_PANEL));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (Colour (COL_KNOB_RIM));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    // Arrow
    const float arrowX = w - 14.0f, arrowY = h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (arrowX - 4.0f, arrowY - 2.0f,
                       arrowX + 4.0f, arrowY - 2.0f,
                       arrowX,        arrowY + 3.0f);
    g.setColour (Colour (COL_ACCENT));
    g.fillPath (arrow);

    juce::ignoreUnused (box);
}

//------------------------------------------------------------------------------
void InflatorLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area,
                                              bool isSeparator, bool isActive,
                                              bool isHighlighted, bool isTicked,
                                              bool, const juce::String& text,
                                              const juce::String&, const juce::Drawable*,
                                              const Colour*)
{
    if (isSeparator)
    {
        g.setColour (Colour (COL_KNOB_RIM));
        g.fillRect (area.getX() + 4, area.getCentreY(), area.getWidth() - 8, 1);
        return;
    }

    if (isHighlighted)
    {
        g.setColour (Colour (COL_ACCENT).withAlpha (0.25f));
        g.fillRect (area);
    }

    g.setColour (isTicked   ? Colour (COL_ACCENT)
                            : (isActive ? Colour (COL_TEXT) : Colour (COL_TEXTDIM)));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (13.0f)));
    g.drawFittedText (text, area.reduced (8, 0), juce::Justification::centredLeft, 1);

    if (isTicked)
    {
        g.setColour (Colour (COL_ACCENT));
        g.fillRect (area.getRight() - 6, area.getY() + 6, 2, area.getHeight() - 12);
    }
}

//==============================================================================
// VuMeterComponent
//==============================================================================
VuMeterComponent::VuMeterComponent (const juce::String& labelText)
    : label (labelText)
{}

void VuMeterComponent::setLevels (float left, float right)
{
    levelL = left;
    levelR = right;
    repaint();
}

float VuMeterComponent::gainToBarPos (float gain) noexcept
{
    if (gain <= 0.0f) return 0.0f;
    const float dB  = juce::Decibels::gainToDecibels (gain);
    const float pos = (dB + 60.0f) / 66.0f;  // -60 dBFS = 0, +6 dBFS = 1
    return juce::jlimit (0.0f, 1.0f, pos);
}

void VuMeterComponent::drawBar (Graphics& g, Rectangle<float> bounds, float pos) const
{
    const float totalH = bounds.getHeight();

    // Off (background) segments
    g.setColour (Colour (InflatorLookAndFeel::COL_METER_OFF));
    g.fillRect (bounds);

    // Coloured segments (bottom-up): green / yellow / red
    // Green: 0..0.76 of bar (0..0 dBFS), Yellow: 0.76..0.91, Red: 0.91..1.0
    const float greenTop  = 0.76f;
    const float yellowTop = 0.91f;

    auto drawSegment = [&] (float yFrac0, float yFrac1, Colour col)
    {
        const float frac0 = juce::jlimit (0.0f, pos, yFrac0);
        const float frac1 = juce::jlimit (0.0f, pos, yFrac1);
        if (frac1 <= frac0) return;
        const float y1 = bounds.getBottom() - frac1 * totalH;
        const float h  = (frac1 - frac0) * totalH;
        g.setColour (col);
        g.fillRect (Rectangle<float> (bounds.getX(), y1, bounds.getWidth(), h));
    };

    drawSegment (0.0f,    greenTop,  Colour (InflatorLookAndFeel::COL_METER_GREEN));
    drawSegment (greenTop,  yellowTop, Colour (InflatorLookAndFeel::COL_METER_YELLOW));
    drawSegment (yellowTop, 1.0f,      Colour (InflatorLookAndFeel::COL_METER_RED));

    // Tick marks at -12, -6, 0, +3, +6 dB
    g.setColour (Colour (0xFF101018));
    for (float dBTick : { -12.0f, -6.0f, 0.0f, 3.0f })
    {
        const float p = (dBTick + 60.0f) / 66.0f;
        const float y = bounds.getBottom() - p * totalH;
        g.fillRect (bounds.getX(), y - 0.5f, bounds.getWidth(), 1.0f);
    }
}

void VuMeterComponent::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (2.0f);

    // Label
    g.setColour (Colour (InflatorLookAndFeel::COL_TEXTDIM));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f).withStyle ("Bold")));
    g.drawText (label, b.withHeight (12.0f), juce::Justification::centred);

    const float barTop   = 14.0f;
    const float barW     = (b.getWidth() - 4.0f) * 0.5f;
    const float barGap   = 4.0f;
    const float barH     = b.getHeight() - barTop;

    Rectangle<float> leftBar  (b.getX(),          b.getY() + barTop, barW, barH);
    Rectangle<float> rightBar (b.getX() + barW + barGap, b.getY() + barTop, barW, barH);

    drawBar (g, leftBar,  gainToBarPos (levelL));
    drawBar (g, rightBar, gainToBarPos (levelR));
}

//==============================================================================
// EffectMeterComponent
//==============================================================================
void EffectMeterComponent::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (2.0f);

    // Background
    g.setColour (Colour (InflatorLookAndFeel::COL_METER_OFF));
    g.fillRoundedRectangle (b, 3.0f);

    // Active fill — teal gradient
    if (level > 0.001f)
    {
        const float fillW = level * b.getWidth();
        juce::ColourGradient grad (Colour (InflatorLookAndFeel::COL_ACCENT),
                                   b.getX(), 0,
                                   Colour (InflatorLookAndFeel::COL_ACCENT2),
                                   b.getX() + fillW, 0, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b.withWidth (fillW), 3.0f);
    }

    // Border
    g.setColour (Colour (InflatorLookAndFeel::COL_KNOB_RIM).withAlpha (0.5f));
    g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, 1.0f);
}

//==============================================================================
// JSInflatorEditor
//==============================================================================
JSInflatorEditor::JSInflatorEditor (JSInflatorProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&laf);
    setSize (WIDTH, HEIGHT);
    setResizable (false, false);

    //--- Knob setup ----------------------------------------------------------
    setupKnob (inputKnob,  inputLabel,  "INPUT");
    setupKnob (effectKnob, effectLabel, "EFFECT");
    setupKnob (curveKnob,  curveLabel,  "CURVE");
    setupKnob (outputKnob, outputLabel, "OUTPUT");

    //--- Button setup --------------------------------------------------------
    for (auto* b : { &inButton, &clipButton, &splitButton, &msButton, &bypassButton })
        setupButton (*b);

    //--- OS Combo ------------------------------------------------------------
    osCombo.addItemList ({ "1x", "2x", "4x", "8x" }, 1);
    osCombo.setScrollWheelEnabled (true);
    addAndMakeVisible (osCombo);

    osLabel.setText ("OVERSAMP", juce::dontSendNotification);
    osLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f).withStyle ("Bold")));
    osLabel.setColour (juce::Label::textColourId, Colour (InflatorLookAndFeel::COL_TEXTDIM));
    osLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (osLabel);

    //--- Meters --------------------------------------------------------------
    addAndMakeVisible (inputMeter);
    addAndMakeVisible (outputMeter);
    addAndMakeVisible (effectMeterComp);

    effectMeterLabel.setText ("EFFECT", juce::dontSendNotification);
    effectMeterLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f).withStyle ("Bold")));
    effectMeterLabel.setColour (juce::Label::textColourId, Colour (InflatorLookAndFeel::COL_TEXTDIM));
    effectMeterLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (effectMeterLabel);

    //--- APVTS Attachments ---------------------------------------------------
    auto& apvts = processor.apvts;
    inputAtt  = std::make_unique<SliderAttachment> (apvts, ParamID::INPUT,  inputKnob);
    effectAtt = std::make_unique<SliderAttachment> (apvts, ParamID::EFFECT, effectKnob);
    curveAtt  = std::make_unique<SliderAttachment> (apvts, ParamID::CURVE,  curveKnob);
    outputAtt = std::make_unique<SliderAttachment> (apvts, ParamID::OUTPUT, outputKnob);

    inAtt     = std::make_unique<ButtonAttachment> (apvts, ParamID::IN,      inButton);
    clipAtt   = std::make_unique<ButtonAttachment> (apvts, ParamID::CLIP,    clipButton);
    splitAtt  = std::make_unique<ButtonAttachment> (apvts, ParamID::SPLIT,   splitButton);
    msAtt     = std::make_unique<ButtonAttachment> (apvts, ParamID::MS_MODE, msButton);
    bypassAtt = std::make_unique<ButtonAttachment> (apvts, ParamID::BYPASS,  bypassButton);
    osAtt     = std::make_unique<ComboAttachment>  (apvts, ParamID::OS,      osCombo);

    startTimerHz (30);
}

JSInflatorEditor::~JSInflatorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void JSInflatorEditor::setupKnob (juce::Slider& s, juce::Label& l,
                                   const juce::String& text)
{
    s.setSliderStyle (juce::Slider::Rotary);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    s.setColour (juce::Slider::rotarySliderFillColourId,  Colour (InflatorLookAndFeel::COL_ACCENT));
    s.setColour (juce::Slider::rotarySliderOutlineColourId, Colour (InflatorLookAndFeel::COL_KNOB_RIM));
    addAndMakeVisible (s);

    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions{}.withHeight (10.5f).withStyle ("Bold")));
    l.setColour (juce::Label::textColourId, Colour (InflatorLookAndFeel::COL_TEXTDIM));
    l.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (l);
}

void JSInflatorEditor::setupButton (juce::ToggleButton& b)
{
    b.setClickingTogglesState (true);
    addAndMakeVisible (b);
}

//==============================================================================
void JSInflatorEditor::timerCallback()
{
    inputMeter.setLevels  (processor.getInputLevelL(),  processor.getInputLevelR());
    outputMeter.setLevels (processor.getOutputLevelL(), processor.getOutputLevelR());
    effectMeterComp.setLevel (processor.getEffectMeter());
}

//==============================================================================
void JSInflatorEditor::paint (Graphics& g)
{
    paintBackground (g);
    paintHeaderBand (g);
    paintSectionDividers (g);
}

//------------------------------------------------------------------------------
void JSInflatorEditor::paintBackground (Graphics& g)
{
    // Deep dark gradient background
    juce::ColourGradient bgGrad (Colour (0xFF1C1C24), 0, 0,
                                  Colour (0xFF13131A), 0, (float)HEIGHT, false);
    g.setGradientFill (bgGrad);
    g.fillAll();

    // Subtle noise texture via tiny semi-transparent rectangles
    // (lightweight, no external images needed)
    g.setColour (Colour (0x04FFFFFF));
    for (int y = 0; y < HEIGHT; y += 2)
        g.fillRect (0, y, WIDTH, 1);
}

//------------------------------------------------------------------------------
void JSInflatorEditor::paintHeaderBand (Graphics& g)
{
    // Accent band at top
    juce::ColourGradient hGrad (Colour (InflatorLookAndFeel::COL_ACCENT).withAlpha (0.9f), 0, 0,
                                 Colour (InflatorLookAndFeel::COL_ACCENT2).withAlpha (0.7f),
                                 (float)WIDTH, 0, false);
    g.setGradientFill (hGrad);
    g.fillRect (0, 0, WIDTH, 3);

    // Plugin name
    g.setColour (Colour (InflatorLookAndFeel::COL_TEXT));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (18.0f).withStyle ("Bold")));
    g.drawText ("JS INFLATOR", Rectangle<int> (12, 6, 180, 24),
                juce::Justification::centredLeft);

    // Subtitle
    g.setColour (Colour (InflatorLookAndFeel::COL_TEXTDIM));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
    g.drawText ("Oxford-Style Harmonic Exciter", Rectangle<int> (12, 25, 220, 14),
                juce::Justification::centredLeft);

    // Author / version badge
    g.setColour (Colour (InflatorLookAndFeel::COL_ACCENT).withAlpha (0.6f));
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
    g.drawText ("yg331  |  JUCE 8", Rectangle<int> (WIDTH - 110, 12, 100, 14),
                juce::Justification::centredRight);
}

//------------------------------------------------------------------------------
void JSInflatorEditor::paintSectionDividers (Graphics& g)
{
    g.setColour (Colour (InflatorLookAndFeel::COL_KNOB_RIM).withAlpha (0.3f));

    // Horizontal line below knob row
    g.fillRect (10, 220, WIDTH - 20, 1);

    // Vertical lines between knobs (subtle separators)
    for (int x : { 130, 250, 355 })
        g.fillRect (x, 48, 1, 165);
}

//==============================================================================
void JSInflatorEditor::resized()
{
    // Header row
    const int headerH = 44;

    //--- Meters (top right) --------------------------------------------------
    const int meterW    = 54;
    const int meterH    = 110;
    const int meterTop  = headerH + 4;
    const int meterGap  = 6;

    inputMeter.setBounds  (WIDTH - 2 * (meterW + meterGap) - 6, meterTop, meterW, meterH);
    outputMeter.setBounds (WIDTH - (meterW + meterGap) - 6,     meterTop, meterW, meterH);

    // Effect meter (horizontal, below the two VU meters)
    effectMeterLabel.setBounds (WIDTH - 2 * (meterW + meterGap) - 6, meterTop + meterH + 2,
                                 2 * meterW + meterGap, 12);
    effectMeterComp.setBounds  (WIDTH - 2 * (meterW + meterGap) - 6, meterTop + meterH + 14,
                                 2 * meterW + meterGap, 12);

    //--- Knobs ---------------------------------------------------------------
    const int knobTop = headerH + 8;
    const int knobW   = 110;
    const int knobH   = 90;
    const int labelH  = 14;

    auto placeKnob = [&] (juce::Slider& s, juce::Label& l, int col)
    {
        const int kx = col * knobW + 6;
        l.setBounds (kx, knobTop, knobW - 4, labelH);
        s.setBounds (kx, knobTop + labelH, knobW - 4, knobH);
    };

    placeKnob (inputKnob,  inputLabel,  0);
    placeKnob (effectKnob, effectLabel, 1);
    placeKnob (curveKnob,  curveLabel,  2);
    placeKnob (outputKnob, outputLabel, 3);

    //--- Buttons row ---------------------------------------------------------
    const int btnTop  = 226;
    const int btnH    = 28;
    const int btnW    = 66;
    const int btnGap  = 6;

    auto placeBtn = [&] (juce::ToggleButton& b, int col)
    {
        b.setBounds (10 + col * (btnW + btnGap), btnTop, btnW, btnH);
    };

    placeBtn (inButton,     0);
    placeBtn (clipButton,   1);
    placeBtn (splitButton,  2);
    placeBtn (msButton,     3);
    placeBtn (bypassButton, 4);

    //--- Oversampling combo --------------------------------------------------
    const int comboTop = 267;
    osLabel.setBounds (10, comboTop, 70, 14);
    osCombo.setBounds (82, comboTop, 68, 22);

    //--- Meter labels --------------------------------------------------------
    // (already set above)
}
