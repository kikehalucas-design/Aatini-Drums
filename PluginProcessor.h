#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>

#include "TR8Sequencer.h"

// Identifiants des paramètres d'un instrument : "bd_tune", "sd_snappy", "oh_level"...
inline juce::String instId (int inst, const char* suffix)
{
    return juce::String (tr8::kInstNames[inst]).toLowerCase() + "_" + suffix;
}

namespace ParamIDs
{
    inline constexpr const char* volume  = "volume";
    inline constexpr const char* tempo   = "tempo";
    inline constexpr const char* shuffle = "shuffle";
    inline constexpr const char* accent  = "accent";
    inline constexpr const char* sync    = "sync";
}

class AatiniAudioProcessor : public juce::AudioProcessor
{
public:
    AatiniAudioProcessor();
    ~AatiniAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- partagé avec l'éditeur -------------------------------------------------
    tr8::Pattern& getPattern() { return seq.pattern; }

    std::atomic<int>      editVar      { 0 };      // variation affichée et modifiée (0..7)
    std::atomic<int>      editInst     { 0 };      // instrument modifié (0..10), 11 = piste d'accent
    std::atomic<uint8_t>  varMask      { 1 };      // variations jouées (bits A..H)
    std::atomic<bool>     internalPlay { false };  // lecture interne (quand l'hôte est arrêté)
    std::atomic<int>      fillRequest  { -1 };     // fill-in demandé (0 ou 1)
    std::atomic<uint32_t> uiTriggers   { 0 };      // instruments déclenchés depuis l'interface
    std::atomic<int>      currentStep  { -1 };     // pas en cours (-1 = arrêté)
    std::atomic<int>      currentSlot  { 0 };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateEngineParams();
    void loadDefaultPattern();

    tr8::Engine engine;
    tr8::Sequencer seq;
    bool hostWasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AatiniAudioProcessor)
};
