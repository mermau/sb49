#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "dsp/PerformerEngine.h"
#include "MidiCCMap.h"
#include "PresetManager.h"

class PerformerAudioProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
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

    // i "program" esposti all'host sono i preset di fabbrica (elenco fisso)
    int getNumPrograms() override { return PresetManager::numFactory(); }
    int getCurrentProgram() override { return presets.currentIsFactory() ? presets.getCurrentIndex() : 0; }
    void setCurrentProgram (int i) override { presets.load (i); }
    const juce::String getProgramName (int i) override { return PresetManager::factoryName (i); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    MidiCCMap ccMap;
    PresetManager presets { apvts };

    float getLfoMonitor() const { return engine.getLfoMonitor(); }
    juce::RangedAudioParameter* getParam (int index) const { return params[(size_t) index]; }

private:
    void handleMidi (const juce::MidiMessage& m);
    void handleAsyncUpdate() override;
    std::atomic<int> pendingProgram { -1 }; // Program Change MIDI, applicato sul message thread
    juce::SpinLock presetNameLock;
    juce::String pendingPresetName;         // nome del preset letto dallo stato della DAW
    void applyCC (int cc, int value);
    perf::Params readParams() const;

    perf::Engine engine;
    std::array<juce::RangedAudioParameter*, pid::numMappable> params {};
    std::array<std::atomic<float>*, pid::numMappable> raw {};
    std::atomic<float>* rawStereo = nullptr;
    std::atomic<float>* rawSeparate = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformerAudioProcessor)
};
