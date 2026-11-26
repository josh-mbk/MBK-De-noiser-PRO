#include "PluginEditor.h"

namespace
{
    float dbToMeter (float gain)
    {
        return juce::jmap (juce::Decibels::gainToDecibels (gain, -60.0f), -60.0f, 0.0f, 0.0f, 1.0f);
    }

    juce::Font boldFont (float size)
    {
        return juce::Font (juce::FontOptions (size, juce::Font::bold));
    }

    juce::Font plainFont (float size)
    {
        return juce::Font (juce::FontOptions (size));
    }
}

//==============================================================================
// LookAndFeel
//==============================================================================
MBKLookAndFeel::MBKLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       mbk::text);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               mbk::muted);
    setColour (juce::TextButton::buttonColourId,        mbk::panel);
    setColour (juce::TextButton::textColourOffId,       mbk::text);
    setColour (juce::TextButton::textColourOnId,        mbk::bgTop);
    setColour (juce::ComboBox::backgroundColourId,      mbk::panel);
    setColour (juce::ComboBox::textColourId,            mbk::text);
    setColour (juce::ComboBox::outlineColourId,         mbk::accent.withAlpha (0.45f));
    setColour (juce::ComboBox::arrowColourId,           mbk::accent);
    setColour (juce::PopupMenu::backgroundColourId,     mbk::panel);
    setColour (juce::PopupMenu::textColourId,           mbk::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, mbk::accent2);
    setColour (juce::TooltipWindow::backgroundColourId, mbk::panel);
    setColour (juce::TooltipWindow::textColourId,       mbk::text);
    setColour (juce::TooltipWindow::outlineColourId,    mbk::accent.withAlpha (0.5f));
}

void MBKLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float arcR = radius - 3.0f;
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float thick = juce::jmax (3.0f, radius * 0.11f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (mbk::panel.brighter (0.15f));
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setGradientFill (juce::ColourGradient (mbk::accent2, centre.x - arcR, centre.y,
                                             mbk::accent,  centre.x + arcR, centre.y, false));
    g.strokePath (value, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = arcR - thick - 3.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF2B3D5E), centre.x, centre.y - knobR,
                                             juce::Colour (0xFF121C30), centre.x, centre.y + knobR, false));
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (s.isMouseOverOrDragging() ? 0.22f : 0.08f));
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.75f, -knobR + 4.0f, 3.5f, knobR * 0.42f, 1.75f);
    g.setColour (mbk::accent);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void MBKLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                           bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const bool isWarn = b.getProperties().contains ("warn");
    const auto onColour = isWarn ? mbk::warn : mbk::accent;

    auto c = on ? onColour : bg;
    if (down) c = c.darker (0.2f); else if (over) c = c.brighter (0.12f);

    g.setColour (c);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour ((on ? onColour : mbk::accent).withAlpha (0.7f));
    g.drawRoundedRectangle (r, 7.0f, 1.2f);
}

juce::Font MBKLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return boldFont (12.5f);
}

//==============================================================================
// SpectrumView
//==============================================================================
SpectrumView::SpectrumView (MBKDenoiserProAudioProcessor& p) : proc (p)
{
    dIn.fill (-120.0f); dOut.fill (-120.0f); dThr.fill (-120.0f);
}

float SpectrumView::xForFreq (float f, juce::Rectangle<float> r) const
{
    return r.getX() + r.getWidth() * std::log (f / fMin) / std::log (fMax / fMin);
}

float SpectrumView::yForDb (float db, juce::Rectangle<float> r) const
{
    return juce::jmap (juce::jlimit (minDb, maxDb, db), minDb, maxDb, r.getBottom(), r.getY());
}

void SpectrumView::update (float reductionDb)
{
    reduction = reductionDb;

    const double sr = proc.getDisplaySampleRate();
    fMax = (float) juce::jmin (20000.0, sr * 0.5 * 0.98);
    const float binHz = (float) sr / (float) MBKDenoiserProAudioProcessor::fftSize;
    constexpr int numBins = MBKDenoiserProAudioProcessor::numBins;

    for (int c = 0; c < numCols; ++c)
    {
        const float f0 = fMin * std::pow (fMax / fMin, (float) c / (float) numCols);
        const float f1 = fMin * std::pow (fMax / fMin, (float) (c + 1) / (float) numCols);
        const int k0 = juce::jlimit (1, numBins - 1, (int) std::floor (f0 / binHz));
        const int k1 = juce::jlimit (k0, numBins - 1, (int) std::ceil (f1 / binHz));

        float mi = -120.0f, mo = -120.0f, mt = -120.0f;
        for (int k = k0; k <= k1; ++k)
        {
            mi = juce::jmax (mi, proc.specIn[(size_t) k].load (std::memory_order_relaxed));
            mo = juce::jmax (mo, proc.specOut[(size_t) k].load (std::memory_order_relaxed));
            mt = juce::jmax (mt, proc.specThr[(size_t) k].load (std::memory_order_relaxed));
        }

        auto& a = dIn[(size_t) c];  a = mi > a ? mi : a * 0.8f + mi * 0.2f;
        auto& b = dOut[(size_t) c]; b = mo > b ? mo : b * 0.8f + mo * 0.2f;
        auto& t = dThr[(size_t) c]; t = t * 0.6f + mt * 0.4f;
    }
    repaint();
}

void SpectrumView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (mbk::screen);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (mbk::accent.withAlpha (0.18f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    auto plot = b.reduced (8.0f, 8.0f);
    plot.removeFromLeft (26.0f);
    plot.removeFromBottom (14.0f);

    g.setFont (plainFont (10.0f));

    // grille dB
    for (float db : { -20.0f, -40.0f, -60.0f, -80.0f })
    {
        const float y = yForDb (db, plot);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (mbk::muted);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (b.getX() + 2.0f, y - 7.0f, 26.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    // grille fréquences
    struct FreqMark { float f; const char* label; };
    static const FreqMark marks[] = { { 50.0f, "" }, { 100.0f, "100" }, { 200.0f, "" }, { 500.0f, "500" },
                                      { 1000.0f, "1k" }, { 2000.0f, "" }, { 5000.0f, "5k" }, { 10000.0f, "10k" } };
    for (const auto& m : marks)
    {
        if (m.f >= fMax) continue;
        const float x = xForFreq (m.f, plot);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (mbk::muted);
        g.drawText (m.label, juce::Rectangle<float> (x - 20.0f, plot.getBottom() + 1.0f, 40.0f, 13.0f),
                    juce::Justification::centred);
    }

    g.saveState();
    g.reduceClipRegion (plot.toNearestInt());

    auto build = [&] (const std::array<float, numCols>& a, bool close)
    {
        juce::Path p;
        for (int c = 0; c < numCols; ++c)
        {
            const float x = plot.getX() + plot.getWidth() * (float) c / (float) (numCols - 1);
            const float y = yForDb (a[(size_t) c], plot);
            if (c == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (close)
        {
            p.lineTo (plot.getRight(), plot.getBottom());
            p.lineTo (plot.getX(), plot.getBottom());
            p.closeSubPath();
        }
        return p;
    };

    // entrée
    g.setColour (juce::Colour (0xFF8FA3C4).withAlpha (0.14f));
    g.fillPath (build (dIn, true));
    g.setColour (juce::Colour (0xFF8FA3C4).withAlpha (0.9f));
    g.strokePath (build (dIn, false), juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));

    // sortie
    g.setGradientFill (juce::ColourGradient (mbk::accent.withAlpha (0.55f), 0.0f, plot.getY(),
                                             mbk::accent.withAlpha (0.08f), 0.0f, plot.getBottom(), false));
    g.fillPath (build (dOut, true));
    g.setColour (mbk::accent);
    g.strokePath (build (dOut, false), juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));

    // seuil de bruit
    g.setColour (mbk::warn.withAlpha (0.95f));
    g.strokePath (build (dThr, false), juce::PathStrokeType (1.3f, juce::PathStrokeType::curved));

    // zone hors bande de traitement (grisée)
    const float lo = proc.apvts.getRawParameterValue (ids::lowFreq)->load();
    const float hi = proc.apvts.getRawParameterValue (ids::highFreq)->load();
    const float xLo = juce::jlimit (plot.getX(), plot.getRight(), xForFreq (juce::jmax (lo, fMin), plot));
    const float xHi = juce::jlimit (plot.getX(), plot.getRight(), xForFreq (juce::jmin (juce::jmax (hi, lo * 1.5f), fMax), plot));
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (plot.getX(), plot.getY(), xLo - plot.getX(), plot.getHeight());
    g.fillRect (xHi, plot.getY(), plot.getRight() - xHi, plot.getHeight());
    g.setColour (mbk::accent.withAlpha (0.6f));
    if (xLo > plot.getX() + 1.0f)      g.drawVerticalLine ((int) xLo, plot.getY(), plot.getBottom());
    if (xHi < plot.getRight() - 1.0f)  g.drawVerticalLine ((int) xHi, plot.getY(), plot.getBottom());

    g.restoreState();

    // légende
    auto legend = juce::Rectangle<float> (plot.getX() + 8.0f, plot.getY() + 4.0f, 300.0f, 14.0f);
    auto item = [&] (juce::Colour c, const char* label, float w)
    {
        auto cell = legend.removeFromLeft (w);
        g.setColour (c);
        g.fillEllipse (cell.getX(), cell.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (mbk::muted);
        g.setFont (boldFont (10.0f));
        g.drawText (label, cell.withTrimmedLeft (11.0f), juce::Justification::centredLeft);
    };
    item (juce::Colour (0xFF8FA3C4), "INPUT",     58.0f);
    item (mbk::accent,               "OUTPUT",    64.0f);
    item (mbk::warn,                 "THRESHOLD", 80.0f);

    // réduction moyenne
    g.setColour (mbk::text);
    g.setFont (boldFont (13.0f));
    g.drawText ("REDUCTION  -" + juce::String (reduction, 1) + " dB",
                juce::Rectangle<float> (plot.getRight() - 200.0f, plot.getY() + 3.0f, 192.0f, 16.0f),
                juce::Justification::centredRight);
}

//==============================================================================
// VMeter
//==============================================================================
void VMeter::setLevel (float target)
{
    level = target > level ? target : level * 0.88f + target * 0.12f;
    repaint();
}

void VMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    auto labelArea = b.removeFromTop (14.0f);
    g.setColour (mbk::muted);
    g.setFont (boldFont (10.0f));
    g.drawText (name, labelArea, juce::Justification::centred);

    auto bar = b.withSizeKeepingCentre (10.0f, b.getHeight() - 2.0f);
    g.setColour (mbk::screen);
    g.fillRoundedRectangle (bar, 4.0f);

    const float h = bar.getHeight() * juce::jlimit (0.0f, 1.0f, level);
    auto fill = bar.withTop (bar.getBottom() - h);
    g.setGradientFill (juce::ColourGradient (mbk::accent, 0.0f, bar.getBottom(),
                                             mbk::warn,   0.0f, bar.getY(), false));
    g.fillRoundedRectangle (fill, 4.0f);
}

//==============================================================================
// Editor
//==============================================================================
MBKDenoiserProAudioProcessorEditor::MBKDenoiserProAudioProcessorEditor (MBKDenoiserProAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), spectrum (p)
{
    setLookAndFeel (&laf);

    initKnob (knobs[0], "REDUCTION",   ids::reduction,   " dB", 1,
              "Maximum attenuation applied to detected noise. Higher = cleaner but riskier for the signal.");
    initKnob (knobs[1], "SENSITIVITY", ids::sensitivity, " %",  0,
              "How aggressively noise is detected. Raise it if noise remains, lower it if the sound becomes dull or watery.");
    initKnob (knobs[2], "SMOOTHING",   ids::smoothing,   " %",  0,
              "Smooths the gain over time. Higher = fewer 'musical noise' artifacts, slightly softer transients.");
    initKnob (knobs[3], "BAND LOW",    ids::lowFreq,     "",    0,
              "Noise reduction only acts above this frequency. Everything below is left untouched.");
    initKnob (knobs[4], "BAND HIGH",   ids::highFreq,    "",    0,
              "Noise reduction only acts below this frequency. Everything above is left untouched.");
    initKnob (knobs[5], "MIX",         ids::mix,         " %",  0,
              "Dry / wet balance. 100 % = fully processed.");
    initKnob (knobs[6], "OUTPUT",      ids::output,      " dB", 1,
              "Output gain.");

    for (int i : { 3, 4 })
    {
        auto& s = knobs[(size_t) i].slider;
        s.textFromValueFunction = [] (double v)
        {
            return v >= 1000.0 ? juce::String (v / 1000.0, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        };
        s.valueFromTextFunction = [] (const juce::String& t)
        {
            const double v = t.getDoubleValue();
            return t.containsIgnoreCase ("k") ? v * 1000.0 : v;
        };
        s.updateText();
    }

    // presets
    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.setTooltip ("Factory presets. They set Reduction, Sensitivity, Smoothing and the frequency range.");
    presetBox.onChange = [this] { processor.setCurrentProgram (presetBox.getSelectedId() - 1); };
    addAndMakeVisible (presetBox);

    prevButton.setTooltip ("Previous preset");
    prevButton.onClick = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id > 1) presetBox.setSelectedId (id - 1);
    };
    nextButton.setTooltip ("Next preset");
    nextButton.onClick = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id < presetBox.getNumItems()) presetBox.setSelectedId (id + 1);
    };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    // bypass / écoute du bruit
    bypassButton.getProperties().set ("warn", true);
    bypassButton.setTooltip ("Bypass the plugin (latency stays compensated).");
    listenButton.setTooltip ("Listen to what is being removed. Useful to check that no wanted signal is lost.");
    addAndMakeVisible (bypassButton);
    addAndMakeVisible (listenButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ids::bypass, bypassButton);
    listenAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, ids::listen, listenButton);

    // mode + learn
    modeLabel.setText ("MODE", juce::dontSendNotification);
    modeLabel.setFont (boldFont (11.0f));
    addAndMakeVisible (modeLabel);

    modeBox.addItemList ({ "Auto", "Learned" }, 1);
    modeBox.setTooltip ("Auto: the noise is tracked continuously. Learned: uses the noise profile you captured (more precise).");
    addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, ids::mode, modeBox);

    learnButton.setTooltip ("Press, play ONLY the noise (e.g. silence between words), then press again to stop. "
                            "The profile is saved with your project.");
    learnButton.onClick = [this] { processor.setLearning (! processor.isLearning()); };
    addAndMakeVisible (learnButton);

    statusLabel.setFont (plainFont (12.0f));
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    addAndMakeVisible (spectrum);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    setResizable (true, true);
    setResizeLimits (640, 448, 1280, 896);
    getConstrainer()->setFixedAspectRatio (800.0 / 560.0);
    setSize (800, 560);

    startTimerHz (30);
}

MBKDenoiserProAudioProcessorEditor::~MBKDenoiserProAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MBKDenoiserProAudioProcessorEditor::initKnob (Knob& k, const juce::String& title, const char* paramId,
                                                   const juce::String& suffix, int decimals, const juce::String& tip)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 20);
    k.slider.setTextValueSuffix (suffix);
    k.slider.setNumDecimalPlacesToDisplay (decimals);
    k.slider.setMouseDragSensitivity (160);
    k.slider.setTooltip (tip + "  (double-click = reset)");
    if (auto* prm = processor.apvts.getParameter (paramId))
        k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    addAndMakeVisible (k.slider);

    k.label.setText (title, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (boldFont (11.0f));
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, paramId, k.slider);
}

//==============================================================================
void MBKDenoiserProAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 800.0f;

    g.setGradientFill (juce::ColourGradient (mbk::bgTop, 0.0f, 0.0f, mbk::bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // logo
    auto logo = logoRect.toFloat();
    auto title = logo.removeFromTop (logo.getHeight() * 0.64f);
    g.setFont (boldFont (24.0f * s));
    g.setColour (mbk::accent);
    g.drawText ("MBK", title.removeFromLeft (58.0f * s), juce::Justification::centredLeft);
    g.setColour (mbk::text);
    g.drawText ("DE-NOISER", title.removeFromLeft (150.0f * s), juce::Justification::centredLeft);

    auto badge = title.removeFromLeft (50.0f * s).withSizeKeepingCentre (44.0f * s, 20.0f * s);
    g.setColour (mbk::accent);
    g.fillRoundedRectangle (badge, 5.0f * s);
    g.setColour (mbk::bgTop);
    g.setFont (boldFont (12.0f * s));
    g.drawText ("PRO", badge, juce::Justification::centred);

    g.setColour (mbk::muted);
    g.setFont (plainFont (11.0f * s));
    g.drawText (juce::String ("Spectral noise reduction  |  v") + JucePlugin_VersionString,
                logo, juce::Justification::centredLeft);

    g.setColour (mbk::accent.withAlpha (0.22f));
    g.fillRect (20.0f * s, 72.0f * s, (float) getWidth() - 40.0f * s, 1.0f);

    // panneaux de réglages
    static const char* titles[3] = { "NOISE REDUCTION", "FREQUENCY RANGE", "OUTPUT" };
    for (size_t i = 0; i < groupRects.size(); ++i)
    {
        auto r = groupRects[i].toFloat();
        g.setColour (mbk::panel.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 10.0f * s);
        g.setColour (mbk::accent.withAlpha (0.9f));
        g.setFont (boldFont (11.0f * s));
        g.drawText (titles[i], r.removeFromTop (24.0f * s).withTrimmedLeft (12.0f * s), juce::Justification::centredLeft);
    }

    g.setColour (mbk::panel.withAlpha (0.55f));
    g.fillRoundedRectangle (bottomRect.toFloat(), 10.0f * s);
}

void MBKDenoiserProAudioProcessorEditor::resized()
{
    const float s = (float) getWidth() / 800.0f;
    auto S = [s] (int v) { return juce::roundToInt ((float) v * s); };
    auto place = [] (juce::Component& c, juce::Rectangle<int> r, int h)
    {
        c.setBounds (r.withSizeKeepingCentre (r.getWidth(), h));
    };

    auto area = getLocalBounds();

    // en-tête
    auto header = area.removeFromTop (S (72)).reduced (S (20), S (12));
    logoRect = header.removeFromLeft (S (280));
    place (bypassButton, header.removeFromRight (S (90)), S (30));
    header.removeFromRight (S (8));
    place (listenButton, header.removeFromRight (S (112)), S (30));
    header.removeFromRight (S (16));
    place (prevButton, header.removeFromLeft (S (28)), S (30));
    header.removeFromLeft (S (4));
    place (nextButton, header.removeFromRight (S (28)), S (30));
    header.removeFromRight (S (4));
    place (presetBox, header, S (30));

    // spectre + vu-mètres
    area.removeFromTop (S (6));
    auto specRow = area.removeFromTop (S (250)).reduced (S (16), 0);
    inMeter.setBounds (specRow.removeFromLeft (S (28)));
    specRow.removeFromLeft (S (6));
    outMeter.setBounds (specRow.removeFromRight (S (28)));
    specRow.removeFromRight (S (6));
    spectrum.setBounds (specRow);

    // barre du bas
    area.removeFromTop (S (8));
    auto bottomArea = area.removeFromBottom (S (68));
    bottomRect = bottomArea.reduced (S (16), S (6));
    auto bottom = bottomRect.reduced (S (14), S (12));
    modeLabel.setBounds (bottom.removeFromLeft (S (46)));
    place (modeBox, bottom.removeFromLeft (S (120)), S (30));
    bottom.removeFromLeft (S (14));
    place (learnButton, bottom.removeFromLeft (S (200)), S (30));
    bottom.removeFromLeft (S (14));
    statusLabel.setBounds (bottom);

    // groupes de boutons
    auto knobsArea = area.reduced (S (16), 0);
    const int gw = knobsArea.getWidth();
    groupRects[0] = knobsArea.removeFromLeft (gw * 3 / 7).reduced (S (4), 0);
    groupRects[1] = knobsArea.removeFromLeft (gw * 2 / 7).reduced (S (4), 0);
    groupRects[2] = knobsArea.reduced (S (4), 0);

    auto layoutGroup = [&] (juce::Rectangle<int> r, std::initializer_list<int> idx)
    {
        r.removeFromTop (S (24));
        const int colW = r.getWidth() / (int) idx.size();
        for (int i : idx)
        {
            auto col = r.removeFromLeft (colW);
            knobs[(size_t) i].label.setBounds (col.removeFromTop (S (16)));
            knobs[(size_t) i].slider.setBounds (col);
        }
    };
    layoutGroup (groupRects[0], { 0, 1, 2 });
    layoutGroup (groupRects[1], { 3, 4 });
    layoutGroup (groupRects[2], { 5, 6 });
}

//==============================================================================
void MBKDenoiserProAudioProcessorEditor::updateLearnUi()
{
    const bool learning = processor.isLearning();

    if (wasLearning && ! learning && processor.hasLearnedProfile())
        if (auto* prm = processor.apvts.getParameter (ids::mode))
            prm->setValueNotifyingHost (prm->convertTo0to1 (1.0f));   // bascule en mode Learned
    wasLearning = learning;

    juce::String status;
    if (learning)
    {
        learnButton.setButtonText ("STOP  (" + juce::String (processor.getLearnSeconds(), 1) + " s)");
        learnButton.setColour (juce::TextButton::buttonColourId, mbk::warn.withAlpha (0.75f));
        status = "Learning... play only the noise, then press STOP.";
    }
    else
    {
        learnButton.setButtonText ("LEARN NOISE");
        learnButton.setColour (juce::TextButton::buttonColourId, mbk::panel);

        const bool learnedMode = modeBox.getSelectedId() == 2;
        if (learnedMode && processor.hasLearnedProfile())  status = "Using your learned noise profile.";
        else if (learnedMode)                              status = "No profile yet - press LEARN NOISE on a noise-only passage.";
        else                                               status = "Auto: background noise is tracked automatically.";
    }

    if (status != lastStatus)
    {
        lastStatus = status;
        statusLabel.setText (status, juce::dontSendNotification);
    }
}

void MBKDenoiserProAudioProcessorEditor::timerCallback()
{
    inMeter.setLevel  (dbToMeter (processor.inputLevel.exchange  (0.0f)));
    outMeter.setLevel (dbToMeter (processor.outputLevel.exchange (0.0f)));
    spectrum.update (processor.reductionDb.load());
    updateLearnUi();
}
