#pragma once

#include "PluginProcessor.h"

namespace mbk
{
    inline const juce::Colour bgTop    { 0xFF0B1220 };
    inline const juce::Colour bgBottom { 0xFF152238 };
    inline const juce::Colour panel    { 0xFF1B2A44 };
    inline const juce::Colour screen   { 0xFF0A101C };
    inline const juce::Colour accent   { 0xFF21D4C0 };
    inline const juce::Colour accent2  { 0xFF3B82F6 };
    inline const juce::Colour text     { 0xFFE6EDF7 };
    inline const juce::Colour muted    { 0xFF7B8AA3 };
    inline const juce::Colour warn     { 0xFFF59E0B };
}

//==============================================================================
class MBKLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MBKLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
};

//==============================================================================
/** Analyseur : entrée (gris-bleu), sortie (turquoise), seuil de bruit (orange). */
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (MBKDenoiserProAudioProcessor& p);
    void update (float reductionDb);
    void paint (juce::Graphics&) override;

private:
    static constexpr int   numCols = 200;
    static constexpr float minDb = -100.0f, maxDb = 0.0f, fMin = 20.0f;

    float xForFreq (float f, juce::Rectangle<float> r) const;
    float yForDb (float db, juce::Rectangle<float> r) const;

    MBKDenoiserProAudioProcessor& proc;
    std::array<float, numCols> dIn {}, dOut {}, dThr {};
    float reduction = 0.0f;
    float fMax = 20000.0f;
};

//==============================================================================
class VMeter : public juce::Component
{
public:
    explicit VMeter (juce::String n) : name (std::move (n)) {}
    void setLevel (float target);
    void paint (juce::Graphics&) override;

private:
    juce::String name;
    float level = 0.0f;
};

//==============================================================================
class MBKDenoiserProAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit MBKDenoiserProAudioProcessorEditor (MBKDenoiserProAudioProcessor&);
    ~MBKDenoiserProAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void initKnob (Knob&, const juce::String& title, const char* paramId, const juce::String& suffix,
                   int decimals, const juce::String& tip);
    void updateLearnUi();

    MBKDenoiserProAudioProcessor& processor;
    MBKLookAndFeel laf;
    juce::TooltipWindow tooltipWindow { this, 500 };

    SpectrumView spectrum;
    VMeter inMeter { "IN" }, outMeter { "OUT" };

    std::array<Knob, 7> knobs;   // 0 reduction, 1 sensitivity, 2 smoothing, 3 low, 4 high, 5 mix, 6 output

    juce::ComboBox presetBox, modeBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TextButton bypassButton { "BYPASS" }, listenButton { "NOISE ONLY" }, learnButton { "LEARN NOISE" };
    juce::Label modeLabel, statusLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   bypassAttachment, listenAttachment;

    juce::Rectangle<int> logoRect, bottomRect;
    std::array<juce::Rectangle<int>, 3> groupRects;
    bool wasLearning = false;
    juce::String lastStatus;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MBKDenoiserProAudioProcessorEditor)
};
