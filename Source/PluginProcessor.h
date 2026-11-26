#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace ids
{
    inline constexpr const char* reduction   = "reduction";
    inline constexpr const char* sensitivity = "sensitivity";
    inline constexpr const char* smoothing   = "smoothing";
    inline constexpr const char* lowFreq     = "lowfreq";
    inline constexpr const char* highFreq    = "highfreq";
    inline constexpr const char* mix         = "mix";
    inline constexpr const char* output      = "output";
    inline constexpr const char* mode        = "mode";
    inline constexpr const char* bypass      = "bypass";
    inline constexpr const char* listen      = "listen";
}

/**
 *  MBK De-noiser PRO
 *  Réducteur de bruit spectral : STFT 2048 points / recouvrement 75 %, filtre de Wiener
 *  à décision dirigée (peu de "bruit musical"), estimation du bruit adaptative ou apprise.
 */
class MBKDenoiserProAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int   fftOrder        = 11;
    static constexpr int   fftSize         = 1 << fftOrder;   // 2048
    static constexpr int   numBins         = fftSize / 2 + 1;
    static constexpr int   overlap         = 4;
    static constexpr int   hopSize         = fftSize / overlap;
    static constexpr int   maxChannels     = 2;
    static constexpr float maxLearnSeconds = 10.0f;

    MBKDenoiserProAudioProcessor();
    ~MBKDenoiserProAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MBK De-noiser PRO"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (ids::bypass); }

    // Presets d'usine
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- API pour l'interface ---
    void  setLearning (bool shouldLearn) noexcept { learnActive.store (shouldLearn); }
    bool  isLearning() const noexcept             { return learnActive.load(); }
    float getLearnSeconds() const noexcept        { return learnSeconds.load(); }
    bool  hasLearnedProfile() const noexcept      { return learnedValid[0].load() || learnedValid[1].load(); }
    double getDisplaySampleRate() const noexcept  { return currentSampleRate.load(); }

    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f }, reductionDb { 0.0f };
    std::array<std::atomic<float>, numBins> specIn, specOut, specThr;   // en dB (affichage)

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    struct Channel
    {
        Channel() : fft (fftOrder) {}
        void reset();

        juce::dsp::FFT fft;
        std::array<float, fftSize>     inBuf {}, outBuf {};
        std::array<float, fftSize * 2> work {};
        std::array<float, numBins>     mag {}, magSmooth {}, noiseAdaptive {}, noiseSum {},
                                       target {}, gain {}, prevClean2 {};
        int  pos = 0, hopCount = 0, learnCount = 0;
        bool firstFrame = true, wasLearning = false;
    };

    void processFrame (Channel& c, int chIndex);

    std::array<Channel, maxChannels> channels;
    std::array<float, fftSize> window {};

    // profil de bruit appris (écrit par le thread audio ou par setStateInformation)
    std::array<std::array<float, numBins>, maxChannels> learned {};
    std::array<std::atomic<bool>, maxChannels> learnedValid;

    std::atomic<float> *pReduction, *pSensitivity, *pSmoothing, *pLow, *pHigh,
                       *pMix, *pOutput, *pMode, *pBypass, *pListen;
    juce::SmoothedValue<float> smMix, smOut, smBypass;

    // paramètres du bloc courant (thread audio)
    float floorGain = 0.1f, overFactor = 2.0f, alpha = 0.9f, attackCoef = 0.1f, releaseCoef = 0.5f;
    bool  learnedMode = false;
    int   kLo = 0, kHi = numBins - 1;

    std::atomic<bool>   learnActive { false };
    std::atomic<float>  learnSeconds { 0.0f };
    std::atomic<double> currentSampleRate { 44100.0 };
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MBKDenoiserProAudioProcessor)
};
