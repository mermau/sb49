#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/PerformerEngine.h"
#include "MidiCCMap.h"

class PerformerAudioProcessor : public juce::AudioProcessor
{
public:
    PerformerAudioProcessor();
    ~PerformerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    MidiCCMap ccMap;

    float getLfoMonitor() const { return engine.getLfoMonitor(); }
    juce::RangedAudioParameter* getParam (int index) const { return params[(size_t) index]; }

private:
    void handleMidi (const juce::MidiMessage& m);
    void applyCC (int cc, int value);
    perf::Params readParams() const;

    perf::Engine engine;
    std::array<juce::RangedAudioParameter*, pid::numMappable> params {};
    std::array<std::atomic<float>*, pid::numMappable> raw {};
    std::atomic<float>* rawStereo = nullptr;
    std::atomic<float>* rawSeparate = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformerAudioProcessor)
};
