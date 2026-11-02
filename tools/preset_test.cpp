#include "PluginProcessor.h"
#include <cstdio>
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    setenv ("HOME", "/tmp/claude-0/-home-claude/3bc91c29-bcf3-5bf4-98ce-7c9784618128/scratchpad/fakehome", 1);
    auto proc = std::make_unique<PerformerAudioProcessor>();
    auto& pm = proc->presets;
    int fails = 0;
    std::printf ("factory=%d entries=%d current=%s\n", PresetManager::numFactory(), (int) pm.entries().size(), pm.getCurrentName().toRawUTF8());
    // preset 0 == default
    for (int k = 0; k < pid::numMappable; ++k)
    {
        auto* p = proc->getParam (k);
        if (std::abs (p->getValue() - p->getDefaultValue()) > 1e-6f) { std::printf ("default mismatch %s\n", pid::all[k].id); }
    }
    pm.load (0);
    for (int k = 0; k < pid::numMappable; ++k)
        if (std::abs (proc->getParam (k)->getValue() - proc->getParam (k)->getDefaultValue()) > 1e-6f) { std::printf ("preset0 != default: %s\n", pid::all[k].id); ++fails; }
    pm.load (13); // Wah Brass
    std::printf ("loaded %s modToVcf=%g brassOn=%g\n", pm.getCurrentName().toRawUTF8(), proc->getParam (pid::modToVcf)->getValue(), proc->getParam (pid::brassOn)->getValue());
    proc->getParam (pid::brassResonance)->setValueNotifyingHost (0.33f);
    proc->getParam (pid::transpose)->setValueNotifyingHost (1.0f);
    if (! pm.saveAs ("Mio Test")) { std::puts ("saveAs FAILED"); ++fails; }
    std::printf ("after save: entries=%d current=%s idx=%d factory=%d\n", (int) pm.entries().size(), pm.getCurrentName().toRawUTF8(), pm.getCurrentIndex(), pm.currentIsFactory());
    const int userIdx = pm.getCurrentIndex();
    pm.load (0);
    pm.load (userIdx);
    std::printf ("reloaded res=%g transpose=%g\n", proc->getParam (pid::brassResonance)->getValue(), proc->getParam (pid::transpose)->getValue());
    if (std::abs (proc->getParam (pid::brassResonance)->getValue() - 0.33f) > 1e-4f) ++fails;
    // state roundtrip of preset name
    juce::MemoryBlock mb; proc->getStateInformation (mb);
    if (auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize()))
        std::printf ("state presetName=%s hasMIDIMAP=%d\n", xml->getStringAttribute ("presetName").toRawUTF8(), xml->getChildByName ("MIDIMAP") != nullptr);
    // step wraps
    pm.load (0); pm.step (-1); std::printf ("step -1 from 0 -> %s\n", pm.getCurrentName().toRawUTF8());
    pm.step (+1); std::printf ("step +1 -> %s\n", pm.getCurrentName().toRawUTF8());
    pm.remove (userIdx);
    std::printf ("after remove entries=%d\nFAILS=%d\n", (int) pm.entries().size(), fails);
    return fails;
}
