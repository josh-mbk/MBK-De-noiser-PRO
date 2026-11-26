#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

namespace
{
    struct Preset { const char* name; float reduction, sensitivity, smoothing, low, high; };

    const Preset presets[] =
    {
        { "Default",              20.0f, 40.0f, 50.0f,   20.0f, 20000.0f },
        { "Voice - Light",        12.0f, 30.0f, 55.0f,   80.0f, 12000.0f },
        { "Voice - Strong",       28.0f, 55.0f, 65.0f,   80.0f, 12000.0f },
        { "Podcast / Dialogue",   22.0f, 45.0f, 60.0f,   70.0f, 14000.0f },
        { "Hiss Remover",         18.0f, 45.0f, 50.0f, 2000.0f, 20000.0f },
        { "Rumble / Room Tone",   22.0f, 50.0f, 60.0f,   20.0f,   500.0f },
        { "Music - Gentle",        8.0f, 25.0f, 70.0f,   20.0f, 20000.0f },
        { "Max Cleanup",          40.0f, 75.0f, 70.0f,   20.0f, 20000.0f }
    };
    constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));

    inline float toDb (float mag) noexcept
    {
        // 0 dB = sinus pleine échelle (somme de la fenêtre de Hann = N/2 -> amplitude N/4)
        return 20.0f * std::log10 (juce::jmax (mag * (4.0f / (float) MBKDenoiserProAudioProcessor::fftSize), 1.0e-6f));
    }
}

//==============================================================================
void MBKDenoiserProAudioProcessor::Channel::reset()
{
    inBuf.fill (0.0f); outBuf.fill (0.0f); work.fill (0.0f);
    mag.fill (0.0f); magSmooth.fill (0.0f); noiseAdaptive.fill (0.0f); noiseSum.fill (0.0f);
    target.fill (1.0f); gain.fill (1.0f); prevClean2.fill (0.0f);
    pos = hopCount = learnCount = 0;
    firstFrame = true;
    wasLearning = false;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MBKDenoiserProAudioProcessor::createLayout()
{
    using juce::AudioParameterFloat;
    using juce::NormalisableRange;
    using juce::ParameterID;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::reduction, 1 },   "Reduction",
                 NormalisableRange<float> (0.0f, 60.0f, 0.1f), 20.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::sensitivity, 1 }, "Sensitivity",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 40.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::smoothing, 1 },   "Smoothing",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::lowFreq, 1 },     "Band Low",
                 NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.4f), 20.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::highFreq, 1 },    "Band High",
                 NormalisableRange<float> (1000.0f, 20000.0f, 1.0f, 0.4f), 20000.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::mix, 1 },         "Mix",
                 NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ids::output, 1 },      "Output",
                 NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (ParameterID { ids::mode, 1 }, "Mode",
                 juce::StringArray { "Auto", "Learned" }, 0));
    p.push_back (std::make_unique<juce::AudioParameterBool> (ParameterID { ids::bypass, 1 }, "Bypass", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (ParameterID { ids::listen, 1 }, "Noise Only", false));

    return { p.begin(), p.end() };
}

//==============================================================================
MBKDenoiserProAudioProcessor::MBKDenoiserProAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MBK_DENOISER_PRO", createLayout())
{
    for (int i = 0; i < fftSize; ++i)   // fenêtre de Hann périodique
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) fftSize);

    for (auto& a : specIn)  a.store (-120.0f);
    for (auto& a : specOut) a.store (-120.0f);
    for (auto& a : specThr) a.store (-120.0f);
    for (auto& v : learnedValid) v.store (false);

    pReduction   = apvts.getRawParameterValue (ids::reduction);
    pSensitivity = apvts.getRawParameterValue (ids::sensitivity);
    pSmoothing   = apvts.getRawParameterValue (ids::smoothing);
    pLow         = apvts.getRawParameterValue (ids::lowFreq);
    pHigh        = apvts.getRawParameterValue (ids::highFreq);
    pMix         = apvts.getRawParameterValue (ids::mix);
    pOutput      = apvts.getRawParameterValue (ids::output);
    pMode        = apvts.getRawParameterValue (ids::mode);
    pBypass      = apvts.getRawParameterValue (ids::bypass);
    pListen      = apvts.getRawParameterValue (ids::listen);
}

bool MBKDenoiserProAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto& out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void MBKDenoiserProAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate.store (sampleRate);

    for (auto& c : channels)
        c.reset();

    smMix.reset (sampleRate, 0.05);     smMix.setCurrentAndTargetValue (pMix->load() * 0.01f);
    smOut.reset (sampleRate, 0.05);     smOut.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    smBypass.reset (sampleRate, 0.03);  smBypass.setCurrentAndTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    learnActive.store (false);
    learnSeconds.store (0.0f);
    setLatencySamples (fftSize);
}

//==============================================================================
void MBKDenoiserProAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh      = juce::jmin (buffer.getNumChannels(), maxChannels);
    const int numSamples = buffer.getNumSamples();

    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    const float sr = (float) currentSampleRate.load();

    floorGain   = juce::Decibels::decibelsToGain (-pReduction->load());
    overFactor  = 1.0f + 0.025f * pSensitivity->load();
    const float s = pSmoothing->load() * 0.01f;
    alpha       = 0.80f + 0.18f * s;
    attackCoef  = 0.1f;
    releaseCoef = 0.4f + 0.2f * s;
    learnedMode = pMode->load() > 0.5f;

    const float lo = pLow->load();
    const float hi = juce::jmax (pHigh->load(), lo * 1.5f);
    kLo = juce::jlimit (0, numBins - 1, juce::roundToInt (lo * (float) fftSize / sr));
    kHi = juce::jlimit (0, numBins - 1, juce::roundToInt (hi * (float) fftSize / sr));

    const bool listen = pListen->load() > 0.5f;
    smMix.setTargetValue (pMix->load() * 0.01f);
    smOut.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    smBypass.setTargetValue (pBypass->load() > 0.5f ? 1.0f : 0.0f);

    float* data[maxChannels] = {};
    for (int ch = 0; ch < numCh; ++ch)
        data[ch] = buffer.getWritePointer (ch);

    float peakIn = 0.0f, peakOut = 0.0f;
    constexpr int mask = fftSize - 1;

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = smMix.getNextValue();
        const float og  = smOut.getNextValue();
        const float byp = smBypass.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& c = channels[(size_t) ch];
            const float in = data[ch][i];

            const float dry = c.inBuf[(size_t) c.pos];     // échantillon d'il y a fftSize (aligné sur la latence)
            c.inBuf[(size_t) c.pos] = in;
            const float wet = c.outBuf[(size_t) c.pos];
            c.outBuf[(size_t) c.pos] = 0.0f;
            c.pos = (c.pos + 1) & mask;

            if (++c.hopCount >= hopSize)
            {
                c.hopCount = 0;
                processFrame (c, ch);
            }

            const float processed = listen ? (dry - wet) * og
                                           : (dry * (1.0f - mix) + wet * mix) * og;
            const float out = processed * (1.0f - byp) + dry * byp;
            data[ch][i] = out;

            peakIn  = juce::jmax (peakIn,  std::abs (in));
            peakOut = juce::jmax (peakOut, std::abs (out));
        }
    }

    inputLevel.store  (juce::jmax (inputLevel.load(),  peakIn));
    outputLevel.store (juce::jmax (outputLevel.load(), peakOut));
}

//==============================================================================
void MBKDenoiserProAudioProcessor::processFrame (Channel& c, int chIndex)
{
    constexpr int mask = fftSize - 1;
    const bool isMain = (chIndex == 0);

    // 1) fenêtrage (le plus ancien échantillon est à c.pos) + FFT
    for (int i = 0; i < fftSize; ++i)
        c.work[(size_t) i] = c.inBuf[(size_t) ((c.pos + i) & mask)] * window[(size_t) i];
    std::fill (c.work.begin() + fftSize, c.work.end(), 0.0f);

    c.fft.performRealOnlyForwardTransform (c.work.data(), true);

    // 2) apprentissage du profil de bruit (début)
    const bool learnNow = learnActive.load();
    if (learnNow && ! c.wasLearning)
    {
        c.noiseSum.fill (0.0f);
        c.learnCount = 0;
        c.wasLearning = true;
    }

    const int  src = learnedValid[(size_t) chIndex].load() ? chIndex : 0;
    const bool useLearned = learnedMode && learnedValid[(size_t) src].load();
    const auto& lp = learned[(size_t) src];

    // 3) estimation du bruit + gain de Wiener à décision dirigée
    constexpr float riseCoef = 0.0015f, adaptiveBias = 1.65f, magSmoothCoef = 0.8f;
    const float oneMinusAlpha = 1.0f - alpha;

    for (int k = 0; k < numBins; ++k)
    {
        const float re = c.work[(size_t) (2 * k)], im = c.work[(size_t) (2 * k + 1)];
        const float mag = std::sqrt (re * re + im * im);
        c.mag[(size_t) k] = mag;

        float& ms = c.magSmooth[(size_t) k];
        ms = c.firstFrame ? mag : magSmoothCoef * ms + (1.0f - magSmoothCoef) * mag;

        float& na = c.noiseAdaptive[(size_t) k];
        if (c.firstFrame)      na = ms;
        else if (ms < na)      na = 0.5f * (na + ms);
        else                   na += (ms - na) * riseCoef;

        if (learnNow)
            c.noiseSum[(size_t) k] += mag;

        const float noise = useLearned ? lp[(size_t) k] : na * adaptiveBias;
        const float nOver = noise * overFactor;
        const float n2    = juce::jmax (nOver * nOver, 1.0e-18f);

        float g = 1.0f;
        if (k >= kLo && k <= kHi)
        {
            float xi = alpha * c.prevClean2[(size_t) k] / n2
                       + oneMinusAlpha * juce::jmax (mag * mag / n2 - 1.0f, 0.0f);
            xi = juce::jmin (xi, 1.0e8f);
            g = juce::jlimit (floorGain, 1.0f, xi / (1.0f + xi));
        }
        c.target[(size_t) k] = g;

        if (isMain)
        {
            specIn[(size_t) k].store (toDb (mag), std::memory_order_relaxed);
            specThr[(size_t) k].store (toDb (nOver), std::memory_order_relaxed);
        }
    }
    c.firstFrame = false;

    // apprentissage : suivi / fin
    if (learnNow)
    {
        ++c.learnCount;
        if (isMain)
        {
            const float sec = (float) c.learnCount * (float) hopSize / (float) currentSampleRate.load();
            learnSeconds.store (sec);
            if (sec >= maxLearnSeconds)
                learnActive.store (false);
        }
    }
    else if (c.wasLearning)
    {
        c.wasLearning = false;
        if (c.learnCount >= 4)
        {
            const float inv = 1.0f / (float) c.learnCount;
            for (int k = 0; k < numBins; ++k)
                learned[(size_t) chIndex][(size_t) k] = c.noiseSum[(size_t) k] * inv;
            learnedValid[(size_t) chIndex].store (true);
        }
        if (isMain)
            learnSeconds.store (0.0f);
    }

    // 4) lissage fréquentiel (3 points) puis temporel, application du gain
    float sumDb = 0.0f;
    for (int k = 0; k < numBins; ++k)
    {
        const float prev = c.target[(size_t) juce::jmax (0, k - 1)];
        const float next = c.target[(size_t) juce::jmin (numBins - 1, k + 1)];
        const float gt   = 0.25f * prev + 0.5f * c.target[(size_t) k] + 0.25f * next;

        float& gs = c.gain[(size_t) k];
        const float coef = gt > gs ? attackCoef : releaseCoef;
        gs = coef * gs + (1.0f - coef) * gt;

        c.work[(size_t) (2 * k)]     *= gs;
        c.work[(size_t) (2 * k + 1)] *= gs;

        const float cleaned = gs * c.mag[(size_t) k];
        c.prevClean2[(size_t) k] = cleaned * cleaned;

        if (isMain)
        {
            specOut[(size_t) k].store (toDb (cleaned), std::memory_order_relaxed);
            sumDb += 20.0f * std::log10 (juce::jmax (gs, 1.0e-5f));
        }
    }

    if (isMain)
        reductionDb.store (-sumDb / (float) numBins);

    // 5) resynthèse + overlap-add (somme des Hann^2 à 75 % = 1.5)
    c.fft.performRealOnlyInverseTransform (c.work.data());

    constexpr float scale = 1.0f / (0.375f * (float) overlap);
    for (int i = 0; i < fftSize; ++i)
        c.outBuf[(size_t) ((c.pos + i) & mask)] += c.work[(size_t) i] * window[(size_t) i] * scale;
}

//==============================================================================
int MBKDenoiserProAudioProcessor::getNumPrograms() { return numPresets; }

const juce::String MBKDenoiserProAudioProcessor::getProgramName (int index)
{
    return (index >= 0 && index < numPresets) ? juce::String (presets[index].name) : juce::String();
}

void MBKDenoiserProAudioProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= numPresets)
        return;

    currentProgram = index;
    const auto& p = presets[index];

    auto set = [this] (const char* id, float v)
    {
        if (auto* prm = apvts.getParameter (id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    };

    set (ids::reduction,   p.reduction);
    set (ids::sensitivity, p.sensitivity);
    set (ids::smoothing,   p.smoothing);
    set (ids::lowFreq,     p.low);
    set (ids::highFreq,    p.high);
}

//==============================================================================
juce::AudioProcessorEditor* MBKDenoiserProAudioProcessor::createEditor()
{
    return new MBKDenoiserProAudioProcessorEditor (*this);
}

void MBKDenoiserProAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);

    for (int ch = 0; ch < maxChannels; ++ch)
        if (learnedValid[(size_t) ch].load())
            state.setProperty (juce::Identifier ("noise" + juce::String (ch)),
                               juce::Base64::toBase64 (learned[(size_t) ch].data(), sizeof (float) * (size_t) numBins),
                               nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MBKDenoiserProAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        if (! xml->hasTagName (apvts.state.getType()))
            return;

        auto vt = juce::ValueTree::fromXml (*xml);
        apvts.replaceState (vt);

        currentProgram = juce::jlimit (0, numPresets - 1, (int) vt.getProperty ("program", 0));

        for (int ch = 0; ch < maxChannels; ++ch)
        {
            const auto text = vt.getProperty (juce::Identifier ("noise" + juce::String (ch))).toString();
            if (text.isEmpty())
                continue;

            juce::MemoryOutputStream mo;
            if (juce::Base64::convertFromBase64 (mo, text) && mo.getDataSize() == sizeof (float) * (size_t) numBins)
            {
                std::memcpy (learned[(size_t) ch].data(), mo.getData(), mo.getDataSize());
                learnedValid[(size_t) ch].store (true);
            }
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MBKDenoiserProAudioProcessor();
}
