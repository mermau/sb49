#pragma once
#include "PluginProcessor.h"
#include "PerformerLookAndFeel.h"

// Slider con menu MIDI Learn (tasto destro / ctrl-clic)
class LearnSlider : public juce::Slider
{
public:
    LearnSlider (PerformerAudioProcessor& p, int index) : proc (p), paramIndex (index) {}
    void mouseDown (const juce::MouseEvent& e) override;
    void paint (juce::Graphics& g) override;
    bool learning = false;
private:
    PerformerAudioProcessor& proc;
    const int paramIndex;
};

// Pulsante a cappuccio quadrato con LED sopra
class PerfButton : public juce::Button
{
public:
    PerfButton (PerformerAudioProcessor& p, int index) : juce::Button ({}), proc (p), paramIndex (index)
    {
        setClickingTogglesState (true);
    }
    void mouseDown (const juce::MouseEvent& e) override;
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override;
    bool learning = false;
    float ledOverride = -1.0f; // >= 0: luminosità imposta (LED di velocità modulazione)
    juce::String onText, offText; // scritta opzionale sul cappuccio
private:
    PerformerAudioProcessor& proc;
    const int paramIndex;
};

class PerformerPanel : public juce::Component
{
public:
    explicit PerformerPanel (PerformerAudioProcessor& p);
    ~PerformerPanel() override;
    void paint (juce::Graphics& g) override;
    void resized() override;
    void refresh(); // chiamato dal timer dell'editor

    static constexpr int W = 1200, H = 520;

private:
    struct Slot
    {
        juce::String label;
        juce::Component* comp = nullptr;
        int paramIndex = -1;
        juce::Rectangle<int> area;
    };
    struct Section
    {
        juce::String title;
        int first = 0, count = 0;
        juce::Rectangle<int> box;
    };

    PerformerAudioProcessor& proc;
    std::vector<std::unique_ptr<LearnSlider>> sliders;
    std::vector<std::unique_ptr<PerfButton>> buttons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> bAtt;
    std::vector<Slot> slots;
    std::vector<Section> sections;

    std::unique_ptr<LearnSlider> pitchKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAtt;
    juce::ToggleButton stereoToggle, retrigToggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stereoAtt, retrigAtt;
    PerfButton* modLedButton = nullptr;

    juce::MidiKeyboardComponent keyboard;
    int lastMapVersion = -1;

    void addSlider (int paramIndex, const juce::String& label);
    void addButton (int paramIndex, const juce::String& label);
    void drawWood (juce::Graphics& g, juce::Rectangle<float> r);
};

class PerformerEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PerformerEditor (PerformerAudioProcessor&);
    ~PerformerEditor() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void resized() override;
private:
    void timerCallback() override { panel.refresh(); }
    PerformerLookAndFeel lnf;
    PerformerPanel panel;
    juce::TooltipWindow tooltips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PerformerEditor)
};
