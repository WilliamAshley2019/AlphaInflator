//==============================================================================
// Alpha Inflator  v1.2 - PluginEditor.cpp  (JUCE 8)
//==============================================================================

#include "PluginEditor.h"

using juce::Colour;
using juce::Rectangle;

// Convenience: build a Colour from a Pal constant
static inline Colour pal(uint32_t c) { return Colour(c); }

//==============================================================================
// InflatorLAF
//==============================================================================
InflatorLAF::InflatorLAF()
{
    setColour(juce::ComboBox::backgroundColourId, pal(Pal::PANEL));
    setColour(juce::ComboBox::outlineColourId, pal(Pal::RIM));
    setColour(juce::ComboBox::textColourId, pal(Pal::TEXT));
    setColour(juce::ComboBox::arrowColourId, pal(Pal::ACCENT));
    setColour(juce::PopupMenu::backgroundColourId, pal(Pal::PANEL));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, pal(Pal::ACCENT).withAlpha(0.3f));
    setColour(juce::PopupMenu::textColourId, pal(Pal::TEXT));
    setColour(juce::PopupMenu::highlightedTextColourId, pal(Pal::TEXT));
    setColour(juce::Slider::textBoxTextColourId, pal(Pal::TEXT_DIM));
    setColour(juce::Slider::textBoxBackgroundColourId, Colour(0x00000000u));
    setColour(juce::Slider::textBoxOutlineColourId, Colour(0x00000000u));
    setColour(juce::TextButton::buttonColourId, pal(Pal::PANEL_LIGHT));
    setColour(juce::TextButton::textColourOffId, pal(Pal::TEXT_DIM));
    setColour(juce::TextButton::buttonOnColourId, pal(Pal::ACCENT));
    setColour(juce::TextButton::textColourOnId, pal(Pal::BG));
}

//------------------------------------------------------------------------------
void InflatorLAF::drawRotarySlider(juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos,
    float startAngle, float endAngle,
    juce::Slider&)
{
    const float diam = static_cast<float> (juce::jmin(w, h)) - 10.0f;
    const float radius = diam * 0.5f;
    const float cx = (float)x + (float)w * 0.5f;
    const float cy = (float)y + (float)h * 0.5f - 6.0f;

    // Rim
    g.setColour(pal(Pal::RIM));
    g.fillEllipse(cx - radius - 2.5f, cy - radius - 2.5f, diam + 5.0f, diam + 5.0f);

    // Body gradient
    juce::ColourGradient grad(pal(Pal::KNOB_BODY).brighter(0.12f),
        cx - radius * 0.5f, cy - radius * 0.5f,
        pal(Pal::KNOB_BODY).darker(0.25f),
        cx + radius * 0.4f, cy + radius * 0.4f, true);
    g.setGradientFill(grad);
    g.fillEllipse(cx - radius, cy - radius, diam, diam);

    // Track background
    const float arcR = radius + 4.0f;
    juce::Path bgArc;
    bgArc.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f, startAngle, endAngle, true);
    g.setColour(Colour(0xFF1A1A2Au));
    g.strokePath(bgArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));

    // Track active
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    juce::Path activeArc;
    activeArc.addArc(cx - arcR, cy - arcR, arcR * 2.0f, arcR * 2.0f, startAngle, angle, true);
    g.setColour(pal(Pal::ACCENT));
    g.strokePath(activeArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
        juce::PathStrokeType::rounded));

    // Indicator dot
    const float dotR = radius * 0.115f;
    const float dotX = cx + (radius - dotR * 2.2f) * std::sin(angle);
    const float dotY = cy - (radius - dotR * 2.2f) * std::cos(angle);
    g.setColour(pal(Pal::ACCENT).brighter(0.25f));
    g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);

    // Sheen
    g.setColour(Colour(0x18FFFFFFu));
    g.fillEllipse(cx - radius * 0.38f, cy - radius * 0.48f,
        radius * 0.38f, radius * 0.22f);
}

//------------------------------------------------------------------------------
void InflatorLAF::drawToggleButton(juce::Graphics& g, juce::ToggleButton& btn,
    bool highlighted, bool)
{
    const bool on = btn.getToggleState();
    const bool isWarn = warningSet.contains(&btn) && warningSet[&btn];
    const auto bounds = btn.getLocalBounds().toFloat().reduced(1.5f);
    const float corner = 5.0f;

    const Colour onCol = isWarn ? pal(Pal::BTN_WARN) : pal(Pal::BTN_ON);

    g.setColour(on ? onCol.withAlpha(0.88f) : pal(Pal::BTN_OFF));
    g.fillRoundedRectangle(bounds, corner);

    g.setColour(on ? onCol : pal(Pal::RIM).withAlpha(highlighted ? 0.9f : 0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), corner, 1.2f);

    g.setColour(on ? pal(Pal::BG) : pal(Pal::TEXT_DIM));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.5f).withStyle("Bold")));
    g.drawFittedText(btn.getButtonText(), btn.getLocalBounds(),
        juce::Justification::centred, 1);
}

//------------------------------------------------------------------------------
void InflatorLAF::drawLinearSlider(juce::Graphics& g,
    int x, int y, int w, int h,
    float sliderPos, float, float,
    const juce::Slider::SliderStyle style,
    juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, sliderPos, 0.0f, 1.0f, style, slider);
        return;
    }

    const float trackH = 4.0f;
    const float trackY = (float)y + (float)h * 0.5f - trackH * 0.5f;

    g.setColour(Colour(0xFF1A1A2Au));
    g.fillRoundedRectangle((float)x, trackY, (float)w, trackH, 2.0f);

    g.setColour(pal(Pal::ACCENT));
    g.fillRoundedRectangle((float)x, trackY, sliderPos - (float)x, trackH, 2.0f);

    const float thumbR = 6.0f;
    const float thumbY = (float)y + (float)h * 0.5f - thumbR;
    g.setColour(pal(Pal::TEXT));
    g.fillEllipse(sliderPos - thumbR, thumbY, thumbR * 2.0f, thumbR * 2.0f);
    g.setColour(pal(Pal::RIM));
    g.drawEllipse(sliderPos - thumbR + 0.5f, thumbY + 0.5f,
        thumbR * 2.0f - 1.0f, thumbR * 2.0f - 1.0f, 1.0f);
}

//------------------------------------------------------------------------------
void InflatorLAF::drawComboBox(juce::Graphics& g, int w, int h,
    bool, int, int, int, int, juce::ComboBox&)
{
    const auto b = Rectangle<float>(0.0f, 0.0f, (float)w, (float)h);
    g.setColour(pal(Pal::PANEL));
    g.fillRoundedRectangle(b, 5.0f);
    g.setColour(pal(Pal::RIM));
    g.drawRoundedRectangle(b.reduced(0.5f), 5.0f, 1.0f);

    const float ax = (float)w - 13.0f, ay = (float)h * 0.5f;
    juce::Path arrow;
    arrow.addTriangle(ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 3.0f);
    g.setColour(pal(Pal::ACCENT));
    g.fillPath(arrow);
}

//------------------------------------------------------------------------------
void InflatorLAF::drawPopupMenuItem(juce::Graphics& g, const Rectangle<int>& area,
    bool isSeparator, bool isActive, bool isHighlighted,
    bool isTicked, bool, const juce::String& text,
    const juce::String&, const juce::Drawable*,
    const Colour*)
{
    if (isSeparator)
    {
        g.setColour(pal(Pal::RIM));
        g.fillRect(area.getX() + 4, area.getCentreY(), area.getWidth() - 8, 1);
        return;
    }
    if (isHighlighted)
    {
        g.setColour(pal(Pal::ACCENT).withAlpha(0.22f));
        g.fillRect(area);
    }
    g.setColour(isTicked ? pal(Pal::ACCENT)
        : (isActive ? pal(Pal::TEXT) : pal(Pal::TEXT_DIM)));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(13.0f)));
    g.drawFittedText(text, area.reduced(8, 0), juce::Justification::centredLeft, 1);
}

//==============================================================================
// VuMeterBar
//==============================================================================
VuMeterBar::VuMeterBar(const juce::String& lbl) : label(lbl) {}

float VuMeterBar::gainToPos(float gain) noexcept
{
    if (gain <= 0.0f) return 0.0f;
    const float dB = juce::Decibels::gainToDecibels(gain);
    return juce::jlimit(0.0f, 1.0f, (dB + 60.0f) / 66.0f);
}

void VuMeterBar::setDataL(const MeterData& d)
{
    dataL = d;
    totalOversL += d.overs;
    repaint();
}

void VuMeterBar::setDataR(const MeterData& d)
{
    dataR = d;
    totalOversR += d.overs;
    repaint();
}

void VuMeterBar::resetOvers()
{
    totalOversL = totalOversR = 0;
    repaint();
}

void VuMeterBar::drawChannel(juce::Graphics& g, Rectangle<float> b,
    const MeterData& data, bool) const
{
    const float H = b.getHeight();

    g.setColour(pal(Pal::METER_OFF));
    g.fillRect(b);

    const float ppmPos = gainToPos(data.ppm);
    const float greenEnd = 0.757f;
    const float yellowEnd = 0.908f;

    auto seg = [&](float f0, float f1, Colour col)
        {
            const float lo = juce::jlimit(0.0f, ppmPos, f0);
            const float hi = juce::jlimit(0.0f, ppmPos, f1);
            if (hi <= lo) return;
            g.setColour(col);
            g.fillRect(b.getX(), b.getBottom() - hi * H, b.getWidth(), (hi - lo) * H);
        };
    seg(0.0f, greenEnd, pal(Pal::METER_GRN));
    seg(greenEnd, yellowEnd, pal(Pal::METER_YEL));
    seg(yellowEnd, 1.0f, pal(Pal::METER_RED));

    // RMS overlay — semi-transparent white line
    const float rmsY = b.getBottom() - gainToPos(data.rms) * H;
    g.setColour(Colour(0x60FFFFFFu));
    g.fillRect(b.getX(), rmsY - 1.0f, b.getWidth(), 2.0f);

    // Peak hold tick
    const float peakY = b.getBottom() - gainToPos(data.peak) * H;
    const bool  hasOvers = (data.peak >= 1.0f);
    g.setColour(hasOvers ? pal(Pal::ACCENT3) : pal(Pal::TEXT));
    g.fillRect(b.getX(), peakY - 1.5f, b.getWidth(), 2.5f);

    // Tick marks
    g.setColour(Colour(0x30000000u));
    for (float dBtick : { -48.0f, -24.0f, -12.0f, -6.0f, 0.0f, 3.0f })
    {
        const float p = (dBtick + 60.0f) / 66.0f;
        g.fillRect(b.getX(), b.getBottom() - p * H - 0.5f, b.getWidth(), 1.0f);
    }
}

void VuMeterBar::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(2.0f);

    g.setColour(pal(Pal::TEXT_DIM));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f).withStyle("Bold")));
    g.drawText(label, b.withHeight(12.0f), juce::Justification::centred);

    const float barTop = 13.0f;
    const float gap = 3.0f;
    const float barW = (b.getWidth() - gap) * 0.5f;
    const float barH = b.getHeight() - barTop;

    drawChannel(g, { b.getX(),              b.getY() + barTop, barW, barH }, dataL, true);
    drawChannel(g, { b.getX() + barW + gap, b.getY() + barTop, barW, barH }, dataR, false);

    const int overs = totalOversL + totalOversR;
    if (overs > 0)
    {
        g.setColour(pal(Pal::ACCENT3));
        g.setFont(juce::Font(juce::FontOptions{}.withHeight(8.5f).withStyle("Bold")));
        g.drawText(juce::String(overs) + "!", b, juce::Justification::bottomRight);
    }
}

//==============================================================================
// GRMeter
//==============================================================================
void GRMeter::setGRdB(float dB)
{
    grDB = juce::jlimit(-40.0f, 0.0f, dB);
    repaint();
}

void GRMeter::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(pal(Pal::METER_OFF));
    g.fillRoundedRectangle(b, 2.0f);

    if (grDB < -0.01f)
    {
        const float frac = juce::jlimit(0.0f, 1.0f, -grDB / 20.0f);
        const float fillW = frac * b.getWidth();
        g.setColour(pal(Pal::ACCENT3).withAlpha(0.85f));
        g.fillRoundedRectangle(b.getRight() - fillW, b.getY(), fillW, b.getHeight(), 2.0f);
    }

    g.setColour(pal(Pal::RIM).withAlpha(0.5f));
    g.drawRoundedRectangle(b.reduced(0.5f), 2.0f, 1.0f);
}

//==============================================================================
// EffectBar
//==============================================================================
void EffectBar::paint(juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(pal(Pal::METER_OFF));
    g.fillRoundedRectangle(b, 3.0f);

    if (level > 0.001f)
    {
        const float fw = level * b.getWidth();
        juce::ColourGradient grad(pal(Pal::ACCENT), b.getX(), 0.0f,
            pal(Pal::ACCENT2), b.getX() + fw, 0.0f, false);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(b.withWidth(fw), 3.0f);
    }

    g.setColour(pal(Pal::RIM).withAlpha(0.5f));
    g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);
}

//==============================================================================
// JSInflatorEditor
//==============================================================================
JSInflatorEditor::JSInflatorEditor(JSInflatorProcessor& p)
    : AudioProcessorEditor(p), proc(p)
{
    setLookAndFeel(&laf);
    setResizable(true, false);
    setResizeLimits(BASE_W, BASE_H, BASE_W * 2, BASE_H * 2);
    setSize(BASE_W, BASE_H);

    // Scale combo
    scaleCombo.addItem("100%", 1);
    scaleCombo.addItem("125%", 2);
    scaleCombo.addItem("150%", 3);
    scaleCombo.addItem("200%", 4);
    scaleCombo.setSelectedId(1, juce::dontSendNotification);
    scaleCombo.onChange = [this]
        {
            const float scales[] = { 1.0f, 1.25f, 1.5f, 2.0f };
            applyScale(scales[juce::jlimit(0, 3, scaleCombo.getSelectedId() - 1)]);
        };
    addAndMakeVisible(scaleCombo);

    // Knobs
    setupKnob(inputKnob, inputLbl, "INPUT");
    setupKnob(effectKnob, effectLbl, "EFFECT");
    setupKnob(curveKnob, curveLbl, "CURVE");
    setupKnob(outputKnob, outputLbl, "OUTPUT");
    setupKnob(toneKnob, toneLbl, "TONE");

    // Limiter ceiling slider
    limCeilSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    limCeilSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 16);
    limCeilSlider.setColour(juce::Slider::textBoxTextColourId, pal(Pal::TEXT_DIM));
    limCeilSlider.setColour(juce::Slider::textBoxBackgroundColourId, Colour(0x00000000u));
    limCeilSlider.setColour(juce::Slider::textBoxOutlineColourId, Colour(0x00000000u));
    addAndMakeVisible(limCeilSlider);

    limCeilLbl.setText("CEILING", juce::dontSendNotification);
    limCeilLbl.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f).withStyle("Bold")));
    limCeilLbl.setColour(juce::Label::textColourId, pal(Pal::TEXT_DIM));
    addAndMakeVisible(limCeilLbl);

    // Toggle buttons
    setupToggle(inBtn, "IN");
    setupToggle(clipBtn, "CLIP");
    setupToggle(splitBtn, "SPLIT");
    setupToggle(msBtn, "M/S");
    setupToggle(deltaBtn, "DELTA", true);
    setupToggle(limiterBtn, "LIMIT");
    setupToggle(bypassBtn, "BYPASS", true);

    // Combos
    setupCombo(osCombo, osLbl, "OVERSAMP", { "1x", "2x", "4x", "8x" });
    setupCombo(agcCombo, agcLbl, "AUTO GAIN", { "Off", "Static", "Dynamic" });
    setupCombo(focusCombo, focusLbl, "FOCUS", { "Full", "Low", "Mid", "High" });
    setupCombo(dynCombo, dynLbl, "DYNAMICS", { "Smooth", "Neutral", "Punch", "Dense" });

    // A/B buttons
    for (auto* b : { &abSaveABtn, &abSaveBBtn, &abToABtn, &abToBBtn })
        setupTextBtn(*b);

    abSaveABtn.onClick = [this] { saveSnap(snapA); };
    abSaveBBtn.onClick = [this] { saveSnap(snapB); };
    abToABtn.onClick = [this] { onAB(true);  };
    abToBBtn.onClick = [this] { onAB(false); };

    // Metering
    addAndMakeVisible(inMeter);
    addAndMakeVisible(outMeter);
    addAndMakeVisible(effectBar);
    addAndMakeVisible(grMeter);

    auto setupSmallLabel = [&](juce::Label& l, const juce::String& text)
        {
            l.setText(text, juce::dontSendNotification);
            l.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f).withStyle("Bold")));
            l.setColour(juce::Label::textColourId, pal(Pal::TEXT_DIM));
            addAndMakeVisible(l);
        };
    setupSmallLabel(effectBarLbl, "EFFECT");
    setupSmallLabel(grMeterLbl, "GR");

    // APVTS Attachments
    auto& apvts = proc.apvts;
    attInput = std::make_unique<SlAtt>(apvts, ParamID::INPUT, inputKnob);
    attEffect = std::make_unique<SlAtt>(apvts, ParamID::EFFECT, effectKnob);
    attCurve = std::make_unique<SlAtt>(apvts, ParamID::CURVE, curveKnob);
    attOutput = std::make_unique<SlAtt>(apvts, ParamID::OUTPUT, outputKnob);
    attTone = std::make_unique<SlAtt>(apvts, ParamID::TONE, toneKnob);
    attLimCeil = std::make_unique<SlAtt>(apvts, ParamID::LIM_CEIL, limCeilSlider);

    attIn = std::make_unique<BtnAtt>(apvts, ParamID::IN, inBtn);
    attClip = std::make_unique<BtnAtt>(apvts, ParamID::CLIP_MODE, clipBtn);
    attSplit = std::make_unique<BtnAtt>(apvts, ParamID::SPLIT, splitBtn);
    attMS = std::make_unique<BtnAtt>(apvts, ParamID::MS_MODE, msBtn);
    attBypass = std::make_unique<BtnAtt>(apvts, ParamID::BYPASS, bypassBtn);
    attDelta = std::make_unique<BtnAtt>(apvts, ParamID::DELTA, deltaBtn);
    attLimiter = std::make_unique<BtnAtt>(apvts, ParamID::LIMITER, limiterBtn);

    attOS = std::make_unique<CbAtt>(apvts, ParamID::OS, osCombo);
    attAGC = std::make_unique<CbAtt>(apvts, ParamID::AGC_MODE, agcCombo);
    attFocus = std::make_unique<CbAtt>(apvts, ParamID::FOCUS, focusCombo);
    attDyn = std::make_unique<CbAtt>(apvts, ParamID::DYN_MODE, dynCombo);

    startTimerHz(30);
}

JSInflatorEditor::~JSInflatorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void JSInflatorEditor::setupKnob(juce::Slider& s, juce::Label& l, const juce::String& text)
{
    s.setSliderStyle(juce::Slider::Rotary);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 16);
    addAndMakeVisible(s);

    l.setText(text, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
    l.setColour(juce::Label::textColourId, pal(Pal::TEXT_DIM));
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void JSInflatorEditor::setupToggle(juce::ToggleButton& b, const juce::String& text, bool warn)
{
    b.setButtonText(text);
    b.setClickingTogglesState(true);
    if (warn) laf.setWarning(b, true);
    addAndMakeVisible(b);
}

void JSInflatorEditor::setupCombo(juce::ComboBox& c, juce::Label& l,
    const juce::String& labelText,
    const juce::StringArray& items)
{
    c.addItemList(items, 1);
    c.setScrollWheelEnabled(true);
    addAndMakeVisible(c);

    l.setText(labelText, juce::dontSendNotification);
    l.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f).withStyle("Bold")));
    l.setColour(juce::Label::textColourId, pal(Pal::TEXT_DIM));
    l.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(l);
}

void JSInflatorEditor::setupTextBtn(juce::TextButton& b)
{
    addAndMakeVisible(b);
}

void JSInflatorEditor::applyScale(float newScale)
{
    uiScale = newScale;
    setSize(static_cast<int> (BASE_W * uiScale), static_cast<int> (BASE_H * uiScale));
}

//==============================================================================
// A/B Compare
//==============================================================================
void JSInflatorEditor::saveSnap(ABSnapshot& snap)
{
    snap.values.clear();
    for (auto* param : proc.apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            snap.values.set(rp->getParameterID(), rp->getValue());
    snap.valid = true;
}

void JSInflatorEditor::loadSnap(const ABSnapshot& snap)
{
    if (!snap.valid) return;
    for (auto* param : proc.apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (snap.values.contains(rp->getParameterID()))
                rp->setValueNotifyingHost(snap.values[rp->getParameterID()]);
}

void JSInflatorEditor::onAB(bool loadA)
{
    ABSnapshot& snap = loadA ? snapA : snapB;
    if (!snap.valid) { saveSnap(snap); return; }
    loadSnap(snap);

    abToABtn.setColour(juce::TextButton::buttonColourId,
        loadA ? pal(Pal::ACCENT) : pal(Pal::PANEL_LIGHT));
    abToBBtn.setColour(juce::TextButton::buttonColourId,
        !loadA ? pal(Pal::ACCENT) : pal(Pal::PANEL_LIGHT));
    repaint();
}

//==============================================================================
// Timer
//==============================================================================
void JSInflatorEditor::timerCallback()
{
    VuMeterBar::MeterData inL, inR, outL, outR;

    inL.ppm = proc.getInLevelL();    inR.ppm = proc.getInLevelR();
    outL.ppm = proc.getOutLevelL();   outR.ppm = proc.getOutLevelR();
    inL.rms = proc.getInRmsL();      inR.rms = proc.getInRmsR();
    outL.rms = proc.getOutRmsL();     outR.rms = proc.getOutRmsR();
    inL.peak = proc.getInPeakL();     inR.peak = proc.getInPeakR();
    outL.peak = proc.getOutPeakL();    outR.peak = proc.getOutPeakR();
    inL.overs = proc.getAndClearInOversL();
    inR.overs = proc.getAndClearInOversR();
    outL.overs = proc.getAndClearOutOversL();
    outR.overs = proc.getAndClearOutOversR();

    inMeter.setDataL(inL);    inMeter.setDataR(inR);
    outMeter.setDataL(outL);   outMeter.setDataR(outR);
    effectBar.setLevel(proc.getEffectMeter());
    grMeter.setGRdB(proc.getLimiterGR());
}

//==============================================================================
// Paint
//==============================================================================
void JSInflatorEditor::paint(juce::Graphics& g)
{
    paintBackground(g);
    paintHeader(g);
    paintSectionLabels(g);
}

void JSInflatorEditor::paintBackground(juce::Graphics& g)
{
    const float H = (float)getHeight();
    juce::ColourGradient bg(Colour(0xFF1A1A22u), 0.0f, 0.0f,
        Colour(0xFF111118u), 0.0f, H, false);
    g.setGradientFill(bg);
    g.fillAll();

    g.setColour(Colour(0x03FFFFFFu));
    for (int y = 0; y < (int)H; y += 2)
        g.fillRect(0, y, getWidth(), 1);
}

void JSInflatorEditor::paintHeader(juce::Graphics& g)
{
    const float W = (float)getWidth();
    const float s = uiScale;

    juce::ColourGradient hg(pal(Pal::ACCENT).withAlpha(0.95f), 0.0f, 0.0f,
        pal(Pal::ACCENT2).withAlpha(0.75f), W, 0.0f, false);
    g.setGradientFill(hg);
    g.fillRect(0.0f, 0.0f, W, 3.0f * s);

    g.setColour(pal(Pal::TEXT));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(19.0f * s).withStyle("Bold")));
    g.drawText("Alpha Inflator",
        Rectangle<float>(12.0f * s, 5.0f * s, 220.0f * s, 26.0f * s),
        juce::Justification::centredLeft);

    g.setColour(pal(Pal::TEXT_DIM));
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.5f * s)));
    g.drawText("Oxford-Style Harmonic Exciter",
        Rectangle<float>(12.0f * s, 27.0f * s, 320.0f * s, 13.0f * s),
        juce::Justification::centredLeft);

    g.setColour(pal(Pal::RIM).withAlpha(0.6f));
    g.fillRect(0.0f, 44.0f * s, W, 1.0f);
}

void JSInflatorEditor::paintSectionLabels(juce::Graphics& g)
{
    const float s = uiScale;
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(9.0f * s).withStyle("Bold")));
    g.setColour(pal(Pal::TEXT_DIM).withAlpha(0.7f));

    struct LP { const char* t; float x, y; };
    for (auto& lp : { LP{"CORE",    12.0f,  46.0f},
                      LP{"TONE",   340.0f,  46.0f},
                      LP{"ROUTING", 12.0f, 188.0f},
                      LP{"ENGINE",  12.0f, 280.0f},
                      LP{"A/B",    420.0f, 188.0f} })
        g.drawText(lp.t,
            Rectangle<float>(lp.x * s, lp.y * s, 80.0f * s, 12.0f * s),
            juce::Justification::centredLeft);

    g.setColour(pal(Pal::RIM).withAlpha(0.25f));
    g.fillRect(10.0f * s, 184.0f * s, (float)getWidth() - 20.0f * s, 1.0f);
    g.fillRect(10.0f * s, 276.0f * s, (float)getWidth() - 20.0f * s, 1.0f);
    g.fillRect(330.0f * s, 46.0f * s, 1.0f, 135.0f * s);
    g.fillRect(415.0f * s, 184.0f * s, 1.0f, 90.0f * s);
}

//==============================================================================
// resized
//==============================================================================
void JSInflatorEditor::resized()
{
    uiScale = static_cast<float> (getWidth()) / static_cast<float> (BASE_W);
    const float s = uiScale;

    auto sc = [&](float v) { return static_cast<int> (v * s); };
    auto sr = [&](float x, float y, float w, float h) -> Rectangle<int>
        { return { sc(x), sc(y), sc(w), sc(h) }; };

    // Scale combo — top right corner
    scaleCombo.setBounds(sr((float)BASE_W - 62.0f, 9.0f, 58.0f, 22.0f));

    // Core knobs
    const float kTop = 56.0f, kW = 76.0f, kH = 90.0f, lH = 13.0f;
    juce::Slider* knobs[] = { &inputKnob, &effectKnob, &curveKnob, &outputKnob };
    juce::Label* klabels[] = { &inputLbl,  &effectLbl,  &curveLbl,  &outputLbl };
    for (int i = 0; i < 4; ++i)
    {
        const float kx = 12.0f + (float)i * (kW + 4.0f);
        klabels[i]->setBounds(sr(kx, kTop, kW, lH));
        knobs[i]->setBounds(sr(kx, kTop + lH, kW, kH));
    }

    // Tone knob
    toneLbl.setBounds(sr(340.0f, kTop, 76.0f, lH));
    toneKnob.setBounds(sr(340.0f, kTop + lH, 76.0f, kH));

    // VU meters (right side)
    const float mTop = 48.0f, mH = 130.0f, mW = 44.0f;
    inMeter.setBounds(sr(434.0f, mTop, mW, mH));
    outMeter.setBounds(sr(482.0f, mTop, mW, mH));

    // Effect bar + GR meter under VU meters
    effectBarLbl.setBounds(sr(434.0f, mTop + mH + 2.0f, 90.0f, 11.0f));
    effectBar.setBounds(sr(434.0f, mTop + mH + 14.0f, 90.0f, 10.0f));
    grMeterLbl.setBounds(sr(434.0f, mTop + mH + 27.0f, 90.0f, 11.0f));
    grMeter.setBounds(sr(434.0f, mTop + mH + 39.0f, 90.0f, 10.0f));

    // Routing toggle buttons
    const float btnTop = 196.0f, btnW = 62.0f, btnH = 26.0f, btnGap = 5.0f;
    juce::ToggleButton* btns[] = { &inBtn, &clipBtn, &splitBtn, &msBtn,
                                    &deltaBtn, &limiterBtn, &bypassBtn };
    for (int i = 0; i < 7; ++i)
        btns[i]->setBounds(sr(12.0f + (float)i * (btnW + btnGap), btnTop, btnW, btnH));

    // A/B buttons
    const float abTop = 196.0f, abW = 44.0f, abH = 26.0f;
    abSaveABtn.setBounds(sr(420.0f, abTop, abW, abH));
    abSaveBBtn.setBounds(sr(420.0f, abTop + abH + 4.0f, abW, abH));
    abToABtn.setBounds(sr(468.0f, abTop, abW, abH));
    abToBBtn.setBounds(sr(468.0f, abTop + abH + 4.0f, abW, abH));

    // Engine combos
    const float cbTop = 288.0f, cbW = 90.0f, cbH = 22.0f, cbLH = 12.0f;
    struct CL { juce::ComboBox* c; juce::Label* l; float x; };
    for (auto& cl : { CL{&osCombo,    &osLbl,    12.0f },
                      CL{&agcCombo,   &agcLbl,   108.0f},
                      CL{&focusCombo, &focusLbl, 204.0f},
                      CL{&dynCombo,   &dynLbl,   300.0f} })
    {
        cl.l->setBounds(sr(cl.x, cbTop - cbLH, cbW, cbLH));
        cl.c->setBounds(sr(cl.x, cbTop, cbW, cbH));
    }

    // Limiter ceiling slider
    limCeilLbl.setBounds(sr(400.0f, cbTop - cbLH, 60.0f, cbLH));
    limCeilSlider.setBounds(sr(396.0f, cbTop, 160.0f, cbH));
}