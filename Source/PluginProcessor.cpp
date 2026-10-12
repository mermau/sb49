#include "PluginProcessor.h"
#include "PluginEditor.h"

PerformerAudioProcessor::PerformerAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < pid::numMappable; ++i)
    {
        params[(size_t) i] = apvts.getParameter (pid::all[(size_t) i].id);
        raw[(size_t) i] = apvts.getRawParameterValue (pid::all[(size_t) i].id);
    }
    rawStereo = apvts.getRawParameterValue (pid::stereoEnsembleId);
}

juce::AudioProcessorValueTreeState::ParameterLayout PerformerAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& p : pid::all)
    {
        const juce::ParameterID id { p.id, 1 };
        if (p.isBool)
            layout.add (std::make_unique<juce::AudioParameterBool> (id, p.name, p.defaultValue > 0.5f));
        else if (juce::String (p.id) == "pitch")
            layout.add (std::make_unique<juce::AudioParameterFloat> (id, p.name,
                juce::NormalisableRange<float> (-15.0f, 15.0f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("st")));
        else
            layout.add (std::make_unique<juce::AudioParameterFloat> (id, p.name,
                juce::NormalisableRange<float> (0.0f, 1.0f), p.defaultValue,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                    [] (float v, int) { return juce::String (v * 10.0f, 1); })));
    }
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { pid::stereoEnsembleId, 1 },
                                                            "Stereo Ensemble", false));
    return layout;
}

bool PerformerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void PerformerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    keyboardState.reset();
}

perf::Params PerformerAudioProcessor::readParams() const
{
    auto v = [this] (int i) { return raw[(size_t) i]->load(); };
    perf::Params p;
    p.brassAttack = v (pid::brassAttack);   p.brassDecay = v (pid::brassDecay);
    p.brassRange = v (pid::brassRange);     p.brassResonance = v (pid::brassResonance);
    p.brassOn = v (pid::brassOn) > 0.5f;    p.brassVolume = v (pid::brassVolume);
    p.stringsLow = v (pid::stringsLow);     p.stringsMid = v (pid::stringsMid);
    p.stringsHigh = v (pid::stringsHigh);
    p.strings16 = v (pid::strings16) > 0.5f; p.strings8 = v (pid::strings8) > 0.5f;
    p.crescendo = v (pid::crescendo);       p.sustain = v (pid::sustain);
    p.stringsVolume = v (pid::stringsVolume);
    p.modDelay = v (pid::modDelay);         p.modRate = v (pid::modRate);
    p.modDepth = v (pid::modDepth);         p.modToVcf = v (pid::modToVcf) > 0.5f;
    p.masterVolume = v (pid::masterVolume); p.pitchSemitones = v (pid::pitch);
    p.stereoEnsemble = rawStereo->load() > 0.5f;
    return p;
}

void PerformerAudioProcessor::applyCC (int cc, int value)
{
    if (ccMap.learnParam.load() >= 0 && MidiCCMap::isLearnable (cc))
    {
        ccMap.assign (cc, ccMap.learnParam.exchange (-1));
    }
    const int p = ccMap.paramForCC (cc);
    if (p < 0) return;
    auto* param = params[(size_t) p];
    const float norm = pid::all[(size_t) p].isBool ? (value >= 64 ? 1.0f : 0.0f) : (float) value / 127.0f;
    if (std::abs (param->getValue() - norm) > 1.0e-4f)
        param->setValueNotifyingHost (norm);
}

void PerformerAudioProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                 engine.noteOn (m.getNoteNumber());
    else if (m.isNoteOff())           engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())        engine.setPitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f);
    else if (m.isSustainPedalOn())    engine.setSustainPedal (true);
    else if (m.isSustainPedalOff())   engine.setSustainPedal (false);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isController())
    {
        if (m.getControllerNumber() == 1) engine.setModWheel ((float) m.getControllerValue() / 127.0f);
        applyCC (m.getControllerNumber(), m.getControllerValue());
    }
}

void PerformerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, n, true); // unisce la tastiera a schermo

    engine.setParams (readParams());

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    int pos = 0;
    for (const auto meta : midi) // rendering accurato al campione tra un evento e l'altro
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > pos)
        {
            engine.process (L + pos, R ? R + pos : nullptr, at - pos);
            pos = at;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < n) engine.process (L + pos, R ? R + pos : nullptr, n - pos);
    midi.clear();
}

void PerformerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto old = state.getChildWithName ("MIDIMAP"); old.isValid()) state.removeChild (old, nullptr);
    state.appendChild (ccMap.toValueTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void PerformerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.hasType (apvts.state.getType())) return;
        if (auto map = state.getChildWithName ("MIDIMAP"); map.isValid())
        {
            ccMap.fromValueTree (map);
            state.removeChild (map, nullptr);
        }
        apvts.replaceState (state);
    }
}

juce::AudioProcessorEditor* PerformerAudioProcessor::createEditor() { return new PerformerEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PerformerAudioProcessor(); }
