#include "PluginEditor.h"

namespace
{
void showLearnMenu (PerformerAudioProcessor& proc, int paramIndex, juce::Component* target)
{
    juce::PopupMenu m;
    const int cc = proc.ccMap.ccForParam (paramIndex);
    m.addSectionHeader (juce::String (pid::all[(size_t) paramIndex].name)
                        + (cc >= 0 ? "  (CC " + juce::String (cc) + ")" : "  (nessun CC)"));
    m.addItem (1, "MIDI Learn: muovi un controllo della tastiera");
    m.addItem (2, "Rimuovi assegnazione CC", cc >= 0);
    if (proc.ccMap.learnParam.load() >= 0) m.addItem (3, "Annulla MIDI Learn");
    m.addSeparator();
    m.addItem (4, "Ripristina mappa Arturia KeyLab");

    auto* p = &proc;
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [p, paramIndex] (int r)
    {
        if (r == 1) p->ccMap.learnParam.store (paramIndex);
        if (r == 2) p->ccMap.clearParam (paramIndex);
        if (r == 3) p->ccMap.learnParam.store (-1);
        if (r == 4) p->ccMap.resetToKeyLabDefaults();
    });
}

juce::Font font (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

// ---------------------------------------------------------------------------------------
void LearnSlider::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showLearnMenu (proc, paramIndex, this);
    else juce::Slider::mouseDown (e);
}

void LearnSlider::paint (juce::Graphics& g)
{
    juce::Slider::paint (g);
    if (learning)
    {
        g.setColour (ui::learn);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 2.0f);
    }
}

void PerfButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) showLearnMenu (proc, paramIndex, this);
    else juce::Button::mouseDown (e);
}

void PerfButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX();
    const bool on = getToggleState();

    // LED
    const float lit = ledOverride >= 0.0f ? ledOverride : (on ? 1.0f : 0.0f);
    juce::Rectangle<float> led (cx - 5.0f, b.getY() + 6.0f, 10.0f, 10.0f);
    if (lit > 0.05f)
    {
        g.setColour (ui::ledOn.withAlpha (0.25f * lit));
        g.fillEllipse (led.expanded (6.0f * lit));
    }
    g.setColour (ui::ledOff.interpolatedWith (ui::ledOn, lit));
    g.fillEllipse (led);
    g.setColour (juce::Colours::black);
    g.drawEllipse (led, 1.0f);

    // cappuccio: "affonda" quando inserito
    const float depth = on || down ? 2.0f : 0.0f;
    juce::Rectangle<float> cap (cx - 16.0f, b.getY() + 34.0f + depth, 32.0f, 32.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (cap.translated (1.5f, 3.0f - depth), 3.0f);
    auto top = on ? ui::creamDim : ui::cream;
    if (highlighted) top = top.brighter (0.08f);
    juce::ColourGradient grad (top.brighter (0.1f), cap.getX(), cap.getY(), top.darker (0.15f), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (cap, 3.0f, 1.0f);

    if (onText.isNotEmpty())
    {
        g.setColour (ui::panel);
        g.setFont (font (9.5f));
        g.drawText (on ? onText : offText, cap, juce::Justification::centred);
    }
    if (learning)
    {
        g.setColour (ui::learn);
        g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 2.0f);
    }
}

// ---------------------------------------------------------------------------------------
PerformerPanel::PerformerPanel (PerformerAudioProcessor& p)
    : proc (p), keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // BRASS SECTION
    sections.push_back ({ "BRASS SECTION", (int) slots.size(), 6, {} });
    addSlider (pid::brassAttack, "ATTACK");
    addSlider (pid::brassDecay, "DECAY");
    addSlider (pid::brassRange, "RANGE");
    addSlider (pid::brassResonance, "RESONANCE");
    addButton (pid::brassOn, "ON");
    addSlider (pid::brassVolume, "VOLUME");
    // STRINGS SECTION
    sections.push_back ({ "STRINGS SECTION", (int) slots.size(), 8, {} });
    addSlider (pid::stringsLow, "LOW");
    addSlider (pid::stringsMid, "MID");
    addSlider (pid::stringsHigh, "HIGH");
    addButton (pid::strings16, "16'");
    addButton (pid::strings8, "8'");
    addSlider (pid::crescendo, "CRESCENDO");
    addSlider (pid::sustain, "SUSTAIN");
    addSlider (pid::stringsVolume, "VOLUME");
    // MODULATION SECTION
    sections.push_back ({ "MODULATION", (int) slots.size(), 4, {} });
    addSlider (pid::modDelay, "DELAY");
    addSlider (pid::modRate, "RATE");
    addSlider (pid::modDepth, "DEPTH");
    addButton (pid::modToVcf, "VCF / FM");
    modLedButton = buttons.back().get();
    modLedButton->onText = "VCF";
    modLedButton->offText = "FM";
    // MASTER
    sections.push_back ({ "MASTER", (int) slots.size(), 1, {} });
    addSlider (pid::masterVolume, "VOLUME");

    // pannello posteriore: PITCH e opzione stereo
    pitchKnob = std::make_unique<LearnSlider> (proc, pid::pitch);
    pitchKnob->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pitchKnob->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    pitchKnob->setDoubleClickReturnValue (true, 0.0);
    pitchKnob->setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (*pitchKnob);
    pitchAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "pitch", *pitchKnob);

    stereoToggle.setButtonText ("STEREO ENSEMBLE");
    stereoToggle.setColour (juce::ToggleButton::textColourId, ui::cream);
    stereoToggle.setColour (juce::ToggleButton::tickColourId, ui::ledOn);
    stereoToggle.setColour (juce::ToggleButton::tickDisabledColourId, ui::creamDim);
    stereoToggle.setTooltip ("Estensione: distribuisce le tre linee di ritardo sui due canali (l'originale è mono)");
    addAndMakeVisible (stereoToggle);
    stereoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, pid::stereoEnsembleId, stereoToggle);

    retrigToggle.setButtonText ("BRASS RETRIG");
    retrigToggle.setColour (juce::ToggleButton::textColourId, ui::cream);
    retrigToggle.setColour (juce::ToggleButton::tickColourId, ui::ledOn);
    retrigToggle.setColour (juce::ToggleButton::tickDisabledColourId, ui::creamDim);
    retrigToggle.setTooltip ("Estensione: riattacco del brass a ogni nota anche suonando legato (l'originale riparte solo dal primo tasto)");
    addAndMakeVisible (retrigToggle);
    retrigAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "brassRetrig", retrigToggle);

    // tastiera 49 tasti Do-Do
    keyboard.setAvailableRange (perf::kLowestNote, perf::kLowestNote + perf::kNumKeys - 1);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xfff6f1e4));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff111111));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff6d675a));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, ui::ledOn.withAlpha (0.35f));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, ui::cream.withAlpha (0.25f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha (0.5f));
    addAndMakeVisible (keyboard);

    setSize (W, H);
}

PerformerPanel::~PerformerPanel() = default;

void PerformerPanel::addSlider (int paramIndex, const juce::String& label)
{
    auto s = std::make_unique<LearnSlider> (proc, paramIndex);
    s->setSliderStyle (juce::Slider::LinearVertical);
    s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s->setPopupDisplayEnabled (true, true, this);
    s->setDoubleClickReturnValue (true, pid::all[(size_t) paramIndex].defaultValue);
    addAndMakeVisible (*s);
    sAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, pid::all[(size_t) paramIndex].id, *s));
    slots.push_back ({ label, s.get(), paramIndex, {} });
    sliders.push_back (std::move (s));
}

void PerformerPanel::addButton (int paramIndex, const juce::String& label)
{
    auto b = std::make_unique<PerfButton> (proc, paramIndex);
    addAndMakeVisible (*b);
    bAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        proc.apvts, pid::all[(size_t) paramIndex].id, *b));
    slots.push_back ({ label, b.get(), paramIndex, {} });
    buttons.push_back (std::move (b));
}

void PerformerPanel::resized()
{
    constexpr int cheek = 34, slotW = 52, gap = 18;
    int x = cheek + 13;
    for (auto& sec : sections)
    {
        const int w = sec.count * slotW + 16;
        sec.box = { x, 86, w, 232 };
        for (int i = 0; i < sec.count; ++i)
        {
            auto& slot = slots[(size_t) (sec.first + i)];
            slot.area = { x + 8 + i * slotW, 94, slotW, 216 };
            if (dynamic_cast<PerfButton*> (slot.comp) != nullptr)
                slot.comp->setBounds (slot.area.getX(), 140, slotW, 80);
            else
                slot.comp->setBounds (slot.area.getX(), 96, slotW, 176);
        }
        x += w + gap;
    }

    pitchKnob->setBounds (944, 7, 44, 44);
    stereoToggle.setBounds (1032, 16, 128, 26);
    retrigToggle.setBounds (744, 16, 124, 26);

    const int kbW = W - 2 * cheek;
    keyboard.setBounds (cheek, 338, kbW, H - 338 - 8);
    keyboard.setKeyWidth ((float) kbW / 29.0f); // 29 tasti bianchi
    keyboard.setBlackNoteLengthProportion (0.62f);
}

void PerformerPanel::drawWood (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::ColourGradient wood (juce::Colour (0xff5a3720), r.getX(), r.getY(),
                               juce::Colour (0xff3b2213), r.getRight(), r.getY(), false);
    wood.addColour (0.5, juce::Colour (0xff6d4428));
    g.setGradientFill (wood);
    g.fillRoundedRectangle (r, 6.0f);
    juce::Random rnd (int (r.getX()) + 7);
    for (int i = 0; i < 14; ++i)
    {
        const float gx = r.getX() + 3.0f + rnd.nextFloat() * (r.getWidth() - 6.0f);
        g.setColour (juce::Colour (0xff2a170b).withAlpha (0.25f + 0.3f * rnd.nextFloat()));
        juce::Path grain;
        grain.startNewSubPath (gx, r.getY());
        grain.cubicTo (gx + 3.0f, r.getY() + r.getHeight() * 0.3f, gx - 3.0f, r.getY() + r.getHeight() * 0.7f,
                       gx + 1.5f, r.getBottom());
        g.strokePath (grain, juce::PathStrokeType (0.8f + rnd.nextFloat()));
    }
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
}

void PerformerPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    // pannello
    juce::Rectangle<float> face (30.0f, 0.0f, (float) W - 60.0f, 336.0f);
    juce::ColourGradient pg (ui::panelEdge, 0.0f, 0.0f, ui::panel, 0.0f, 336.0f, false);
    g.setGradientFill (pg);
    g.fillRect (face);

    // intestazione: marchio originale del plugin
    juce::Rectangle<float> badge (52.0f, 10.0f, 92.0f, 38.0f);
    g.setColour (ui::cream);
    g.drawRoundedRectangle (badge, 5.0f, 2.0f);
    g.setFont (font (25.0f));
    g.drawText ("SB-49", badge, juce::Justification::centred);
    g.setFont (font (13.0f));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("STRING  \xc2\xb7  BRASS  ENSEMBLE")), juce::Rectangle<float> (156.0f, 12.0f, 330.0f, 18.0f),
                juce::Justification::centredLeft);
    g.setColour (ui::creamDim);
    g.setFont (font (10.5f, false));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("divide-down paraphonic  \xc2\xb7  49 keys  \xc2\xb7  tasto destro su un controllo = MIDI Learn")),
                juce::Rectangle<float> (156.0f, 30.0f, 520.0f, 16.0f), juce::Justification::centredLeft);
    g.setColour (ui::cream);
    g.setFont (font (10.0f));
    g.drawText ("PITCH", juce::Rectangle<float> (892.0f, 20.0f, 48.0f, 16.0f), juce::Justification::centredRight);
    g.setColour (ui::creamDim.withAlpha (0.5f));
    g.drawLine (40.0f, 58.0f, (float) W - 40.0f, 58.0f, 1.0f);

    // sezioni
    for (const auto& sec : sections)
    {
        g.setColour (ui::cream);
        g.setFont (font (11.5f));
        g.drawText (sec.title, sec.box.withY (64).withHeight (18), juce::Justification::centred);
        g.setColour (ui::creamDim);
        g.drawRoundedRectangle (sec.box.toFloat(), 9.0f, 1.6f);
        for (int i = 0; i < sec.count; ++i)
        {
            const auto& slot = slots[(size_t) (sec.first + i)];
            g.setColour (ui::cream);
            g.setFont (font (9.0f));
            g.drawFittedText (slot.label, slot.area.withY (278).withHeight (26).reduced (1, 0),
                              juce::Justification::centred, 2, 0.8f);
        }
    }

    // listello sopra la tastiera
    g.setColour (juce::Colour (0xff5e1410));
    g.fillRect (30, 330, W - 60, 8);

    // fianchi in legno
    drawWood (g, { 0.0f, 0.0f, 34.0f, (float) H });
    drawWood (g, { (float) W - 34.0f, 0.0f, 34.0f, (float) H });
}

void PerformerPanel::refresh()
{
    if (modLedButton != nullptr)
    {
        const float v = proc.getLfoMonitor();
        if (std::abs (v - modLedButton->ledOverride) > 0.02f)
        {
            modLedButton->ledOverride = v;
            modLedButton->repaint();
        }
    }

    const int learnIdx = proc.ccMap.learnParam.load();
    const int ver = proc.ccMap.version.load();
    const bool mapChanged = ver != lastMapVersion;
    lastMapVersion = ver;

    auto update = [&] (auto* comp, int idx)
    {
        const bool l = (idx == learnIdx);
        if (comp->learning != l) { comp->learning = l; comp->repaint(); }
        if (mapChanged)
        {
            const int cc = proc.ccMap.ccForParam (idx);
            comp->setTooltip (juce::String (pid::all[(size_t) idx].name)
                              + (cc >= 0 ? "  -  MIDI CC " + juce::String (cc) : "  -  nessun CC (tasto destro per MIDI Learn)"));
        }
    };
    for (auto& slot : slots)
    {
        if (auto* s = dynamic_cast<LearnSlider*> (slot.comp)) update (s, slot.paramIndex);
        else if (auto* b = dynamic_cast<PerfButton*> (slot.comp)) update (b, slot.paramIndex);
    }
    update (pitchKnob.get(), pid::pitch);
}

// ---------------------------------------------------------------------------------------
PerformerEditor::PerformerEditor (PerformerAudioProcessor& p)
    : AudioProcessorEditor (&p), panel (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits (PerformerPanel::W / 2, PerformerPanel::H / 2, PerformerPanel::W * 2, PerformerPanel::H * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) PerformerPanel::W / PerformerPanel::H);
    setSize (PerformerPanel::W, PerformerPanel::H);
    startTimerHz (30);
}

PerformerEditor::~PerformerEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void PerformerEditor::resized()
{
    panel.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) PerformerPanel::W));
}
