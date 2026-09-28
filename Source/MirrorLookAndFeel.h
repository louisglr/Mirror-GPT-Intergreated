#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

// All controls are live vector components, not a picture with hit targets.
// A single palette/geometry keeps popups, keyboard focus and sliders coherent.
class MirrorLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    const juce::Colour background { 0xff101211 }, panel { 0xff191c1a };
    const juce::Colour gold { 0xffcfb991 }, ink { 0xffefece3 }, muted { 0xffaaa99e };

    MirrorLookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId, background);
        setColour(juce::ComboBox::textColourId, ink);
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff45483e));
        setColour(juce::PopupMenu::backgroundColourId, panel);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff393c32));
        setColour(juce::PopupMenu::highlightedTextColourId, ink);
        setColour(juce::Slider::textBoxTextColourId, ink);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::TextButton::textColourOffId, muted);
        setColour(juce::TextButton::textColourOnId, ink);
        setColour(juce::TooltipWindow::backgroundColourId, panel);
        setColour(juce::TooltipWindow::textColourId, ink);
        setColour(juce::TooltipWindow::outlineColourId, gold.withAlpha(0.4f));
    }

    juce::Font getComboBoxFont(juce::ComboBox&) override { return font(13.0f); }
    juce::Font getPopupMenuFont() override { return font(13.0f); }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return font(12.0f); }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* label = juce::LookAndFeel_V4::createSliderTextBox(slider);
        label->setFont(font(11.0f));
        return label;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float position, float start, float end, juce::Slider& slider) override
    {
        const auto area = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(5.0f);
        const float r = juce::jmax(1.0f, juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 2.0f);
        const auto centre = area.getCentre();
        float anchor = 0.0f;
        if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
        {
            const float zero = (float) slider.valueToProportionOfLength(0.0);
            // Unequal ranges (e.g. Output -18..+12 dB) still put zero upright.
            position = position <= zero ? 0.5f * position / juce::jmax(0.0001f, zero)
                : 0.5f + 0.5f * (position-zero) / juce::jmax(0.0001f, 1.0f-zero);
            anchor = 0.5f;
        }
        if (slider.getProperties().contains("centre"))
            anchor = (float) slider.getProperties()["centre"];
        const float angle = start + position * (end - start);
        const float anchorAngle = start + anchor * (end - start);
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, r, r, 0.0f, start, end, true);
        g.setColour(juce::Colour(0xff393e36));
        g.strokePath(track, juce::PathStrokeType(2.0f));
        juce::Path active;
        active.addCentredArc(centre.x, centre.y, r, r, 0.0f,
                            juce::jmin(anchorAngle, angle), juce::jmax(anchorAngle, angle), true);
        g.setColour(gold.withAlpha(slider.isEnabled() ? 0.95f : 0.3f));
        g.strokePath(active, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const auto body = juce::Rectangle<float>(centre.x - r + 5, centre.y - r + 5, 2*r - 10, 2*r - 10);
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillEllipse(body.translated(0, 2));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff777365), body.getX(), body.getY(),
                                              juce::Colour(0xff262a25), body.getRight(), body.getBottom(), false));
        g.fillEllipse(body);
        g.setColour(gold.withAlpha(slider.isMouseOverOrDragging() ? 0.8f : 0.35f));
        g.drawEllipse(body, 1.0f);
        const float inner = r * 0.25f, outer = juce::jmax(inner, r - 9.0f);
        g.setColour(ink);
        g.drawLine(centre.x + std::sin(angle)*inner, centre.y - std::cos(angle)*inner,
                   centre.x + std::sin(angle)*outer, centre.y - std::cos(angle)*outer, 2.0f);
        if (slider.hasKeyboardFocus(true))
        {
            g.setColour(gold);
            g.drawEllipse(area.reduced(0.5f), 1.0f);
        }
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float position, float, float, juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        const float cy = (float)y + (float)height * 0.5f;
        const float left = (float)x, right = (float)(x+width);
        g.setColour(juce::Colour(0xff353b32));
        g.fillRoundedRectangle(left, cy-3, right-left, 6, 3);
        g.setColour(gold);
        g.fillRoundedRectangle(left, cy-3, juce::jmax(0.0f, position-left), 6, 3);
        g.setColour(background);
        g.fillEllipse(position-9, cy-9, 18, 18);
        g.setColour(slider.isMouseOverOrDragging() || slider.hasKeyboardFocus(true) ? ink : gold);
        g.drawEllipse(position-8, cy-8, 16, 16, 2);
        g.fillEllipse(position-3, cy-3, 6, 6);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) override
    {
        const auto r = button.getLocalBounds().toFloat().reduced(0.5f);
        g.setColour(button.getToggleState() ? juce::Colour(0xff343a30) : background);
        g.fillRoundedRectangle(r, 6);
        g.setColour(gold.withAlpha(over || down || button.hasKeyboardFocus(true) ? 0.9f : 0.25f));
        g.drawRoundedRectangle(r, 6, 1);
        if (button.getToggleState())
        {
            g.setColour(gold);
            g.fillRoundedRectangle(r.getX()+12, r.getBottom()-3, r.getWidth()-24, 2, 1);
        }
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        g.setColour(background);
        g.fillRoundedRectangle(0.5f, 0.5f, (float)width-1, (float)height-1, 5);
        g.setColour(gold.withAlpha(box.hasKeyboardFocus(true) ? 0.85f : 0.25f));
        g.drawRoundedRectangle(0.5f, 0.5f, (float)width-1, (float)height-1, 5, 1);
        juce::Path arrow;
        arrow.startNewSubPath((float)width-18, (float)height*0.5f-2);
        arrow.lineTo((float)width-14, (float)height*0.5f+2);
        arrow.lineTo((float)width-10, (float)height*0.5f-2);
        g.setColour(box.isEnabled() ? gold : muted.withAlpha(0.3f));
        g.strokePath(arrow, juce::PathStrokeType(1.5f));
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool over, bool) override
    {
        const float y = ((float)button.getHeight()-14)*0.5f;
        g.setColour(button.getToggleState() ? gold : juce::Colour(0xff3c4238));
        g.fillRoundedRectangle(2, y, 14, 14, 3);
        if (button.getToggleState())
        {
            juce::Path tick;
            tick.startNewSubPath(5, y+7); tick.lineTo(8, y+10); tick.lineTo(13, y+4);
            g.setColour(background);
            g.strokePath(tick, juce::PathStrokeType(1.8f));
        }
        g.setColour(over || button.getToggleState() ? ink : muted);
        g.setFont(font(11));
        g.drawText(button.getButtonText(), 22, 0, button.getWidth()-22, button.getHeight(), juce::Justification::centredLeft);
        if (button.hasKeyboardFocus(true))
        {
            g.setColour(gold);
            g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(0.5f), 3, 1);
        }
    }

private:
    static juce::Font font(float size) { return juce::Font(juce::FontOptions(size)); }
};
