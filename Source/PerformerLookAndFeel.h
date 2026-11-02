#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
inline const juce::Colour panel     { 0xff17171a };
inline const juce::Colour panelEdge { 0xff2a2a2e };
inline const juce::Colour cream     { 0xffece4cf };
inline const juce::Colour creamDim  { 0xffb9b19c };
inline const juce::Colour ledOn     { 0xffff3b2a };
inline const juce::Colour ledOff    { 0xff4a1612 };
inline const juce::Colour learn     { 0xff38c7ff };
inline const juce::Colour stripeOrange { 0xffe85d26 };
inline const juce::Colour stripeAmber  { 0xfff4a330 };
inline const juce::Colour stripeRed    { 0xffc4302b };
inline const juce::Colour boxRed       { 0xff8e3a2e }; // filetti rossi spenti dei riquadri
inline const juce::Colour face         { 0xff1b1b1d }; // nero opaco del pannello
}

class PerformerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PerformerLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, ui::panelEdge);
        setColour (juce::PopupMenu::textColourId, ui::cream);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::cream);
        setColour (juce::PopupMenu::highlightedTextColourId, ui::panel);
        setColour (juce::TooltipWindow::backgroundColourId, ui::cream);
        setColour (juce::TooltipWindow::textColourId, ui::panel);
        setColour (juce::TooltipWindow::outlineColourId, ui::panel);
    }

    int getSliderThumbRadius (juce::Slider&) override { return 9; }

    // spunte dell'intestazione: stesso carattere delle etichette del pannello
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool) override
    {
        const auto r = b.getLocalBounds().toFloat();
        const float box = 13.0f;
        juce::Rectangle<float> tick (r.getX() + 2.0f, r.getCentreY() - box * 0.5f, box, box);
        g.setColour (highlighted ? ui::cream : ui::creamDim);
        g.drawRoundedRectangle (tick, 2.5f, 1.2f);
        if (b.getToggleState())
        {
            g.setColour (ui::ledOn);
            g.fillRoundedRectangle (tick.reduced (3.0f), 1.5f);
        }
        g.setColour (ui::cream);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText (b.getButtonText(), r.withTrimmedLeft (box + 8.0f), juce::Justification::centredLeft, false);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos,
                           float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        if (style != juce::Slider::LinearVertical)
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, minPos, maxPos, style, s);
            return;
        }
        const float cx = (float) x + (float) w * 0.5f;
        // per slider a valore singolo min/maxPos non sono gli estremi della corsa: si usano y e h
        minPos = (float) (y + h);
        maxPos = (float) y;
        const bool eq = (bool) s.getProperties().getWithDefault ("eq", false);

        // scala serigrafata come sul pannello originale: 0..8 (EQ: + / 0 / -), tacche su entrambi i lati
        g.setFont (juce::Font (juce::FontOptions (8.0f, juce::Font::bold)));
        for (int i = 0; i <= 8; ++i)
        {
            const float ty = minPos + (maxPos - minPos) * (float) i / 8.0f;
            const bool major = eq ? (i % 4) == 0 : true;
            g.setColour (major ? ui::cream : ui::creamDim.withAlpha (0.75f));
            const float len = major ? 6.0f : 4.0f;
            g.drawLine (cx - 7.0f - len, ty, cx - 7.0f, ty, 0.9f);
            g.drawLine (cx + 7.0f, ty, cx + 7.0f + len, ty, 0.9f);
            juce::String t;
            if (! eq) t = juce::String (i);
            else if (i == 8) t = "+";
            else if (i == 4) t = "0";
            else if (i == 0) t = juce::String::charToString ((juce::juce_wchar) 0x2212);
            if (t.isNotEmpty())
            {
                if (eq) g.setFont (juce::Font (juce::FontOptions (i == 4 ? 8.0f : 11.0f, juce::Font::bold)));
                g.drawText (t, juce::Rectangle<float> (cx - 25.0f, ty - 6.0f, 10.0f, 12.0f), juce::Justification::centredRight, false);
            }
        }

        // fessura
        juce::Rectangle<float> slot (cx - 2.0f, maxPos - 4.0f, 4.0f, minPos - maxPos + 8.0f);
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (slot, 2.0f);
        g.setColour (ui::panelEdge.brighter (0.25f));
        g.drawRoundedRectangle (slot, 2.0f, 0.7f);

        // cappuccio nero con filetto metallico inclinato
        juce::Rectangle<float> cap (cx - 9.0f, sliderPos - 8.0f, 18.0f, 16.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (cap.translated (1.5f, 2.0f), 1.5f);
        juce::ColourGradient grad (juce::Colour (0xff3a3a3d), cap.getX(), cap.getY(),
                                   juce::Colour (0xff0b0b0c), cap.getX(), cap.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (cap, 1.5f);
        {
            juce::Path stripe;
            stripe.startNewSubPath (cap.getX() + 1.5f, sliderPos + 1.6f);
            stripe.lineTo (cap.getRight() - 1.5f, sliderPos - 1.6f);
            juce::ColourGradient metal (juce::Colour (0xffe8e8ea), cap.getX(), 0.0f,
                                        juce::Colour (0xff8d8d92), cap.getRight(), 0.0f, false);
            g.setGradientFill (metal);
            g.strokePath (stripe, juce::PathStrokeType (1.8f));
        }
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawLine (cap.getX() + 1.0f, cap.getY() + 0.8f, cap.getRight() - 1.0f, cap.getY() + 0.8f, 0.8f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (cap, 1.5f, 0.8f);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (d, d);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (r.translated (1.5f, 2.0f));
        juce::ColourGradient grad (juce::Colour (0xff3a3a3f), r.getX(), r.getY(),
                                   juce::Colour (0xff111113), r.getRight(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillEllipse (r);
        g.setColour (ui::creamDim);
        g.drawEllipse (r, 1.0f);
        const float a = startAngle + pos * (endAngle - startAngle);
        const auto c = r.getCentre();
        juce::Path p;
        p.addRoundedRectangle (-1.5f, -d * 0.46f, 3.0f, d * 0.3f, 1.5f);
        g.setColour (ui::cream);
        g.fillPath (p, juce::AffineTransform::rotation (a).translated (c));
    }
};
