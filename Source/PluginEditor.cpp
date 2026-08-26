#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace
{
    const juce::Colour kInk        { 0xff060609 };
    const juce::Colour kSurface    { 0xff0e0d11 };
    const juce::Colour kLift       { 0xff1a1720 };
    const juce::Colour kGold       { 0xffdec8a0 };
    const juce::Colour kGoldBright { 0xffffe3b5 };
    const juce::Colour kGoldDim    { 0xff846d50 };
    const juce::Colour kText       { 0xffeee7dc };
    const juce::Colour kTextDim    { 0xffaaa092 };
    const juce::Colour kPurple     { 0xffc575ff };
    const juce::Colour kPurpleDim  { 0xff4e2967 };

    juce::Font displayFont(float size, int style = juce::Font::plain)
    {
        // Times New Roman is included with the supported macOS hosts and
        // contains the Cyrillic Я used only in the MIRROR wordmark.
        return juce::Font(juce::FontOptions("Times New Roman", size, style));
    }

    juce::String mirroredBrand()
    {
        const auto ya = juce::String::charToString((juce::juce_wchar) 0x042f);
        return "MI" + ya + ya + "O" + ya;
    }

    juce::String mirroredCredit()
    {
        const auto ya = juce::String::charToString((juce::juce_wchar) 0x042f);
        return "By Lou!s Gab" + ya + "iel";
    }

    class MirrorLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        MirrorLookAndFeel()
        {
            setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
            setColour(juce::ComboBox::outlineColourId, kGoldDim);
            setColour(juce::ComboBox::textColourId, kText);
            setColour(juce::PopupMenu::backgroundColourId, kSurface);
            setColour(juce::PopupMenu::textColourId, kText);
            setColour(juce::PopupMenu::highlightedBackgroundColourId, kPurpleDim);
            setColour(juce::PopupMenu::highlightedTextColourId, kText);
        }

        juce::Font getComboBoxFont(juce::ComboBox& box) override
        {
            return displayFont(juce::jlimit(12.0f, 18.0f, (float) box.getHeight() * 0.52f));
        }

        juce::Font getPopupMenuFont() override
        {
            return displayFont(15.0f);
        }

        void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds(10, 1, box.getWidth() - 32, box.getHeight() - 2);
            label.setFont(getComboBoxFont(box));
            label.setJustificationType(juce::Justification::centred);
        }

        void drawPopupMenuBackgroundWithOptions(juce::Graphics& g, int width, int height,
                                                const juce::PopupMenu::Options&) override
        {
            const auto b = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);
            g.setColour(kInk.withAlpha(0.99f));
            g.fillRoundedRectangle(b.reduced(1.0f), 6.0f);
            g.setColour(kGoldDim);
            g.drawRoundedRectangle(b.reduced(1.5f), 6.0f, 1.0f);
            g.setColour(kPurple.withAlpha(0.20f));
            g.drawRoundedRectangle(b.reduced(4.0f), 4.0f, 0.7f);
        }

        void drawComboBox(juce::Graphics& g, int width, int height, bool,
                          int, int, int, int, juce::ComboBox&) override
        {
            auto b = juce::Rectangle<float>(0.8f, 0.8f, (float) width - 1.6f, (float) height - 1.6f);
            g.setColour(juce::Colours::black.withAlpha(0.78f));
            g.fillRoundedRectangle(b, 5.0f);
            g.setColour(kGoldDim.withAlpha(0.96f));
            g.drawRoundedRectangle(b, 5.0f, 1.0f);
            g.setColour(kGoldBright.withAlpha(0.30f));
            g.drawLine(b.getX() + 4.0f, b.getY() + 2.0f, b.getRight() - 4.0f, b.getY() + 2.0f, 0.65f);

            const auto cx = b.getRight() - 15.0f;
            const auto cy = b.getCentreY();
            juce::Path arrow;
            arrow.startNewSubPath(cx - 4.0f, cy - 2.0f);
            arrow.lineTo(cx + 4.0f, cy - 2.0f);
            arrow.lineTo(cx, cy + 3.0f);
            arrow.closeSubPath();
            g.setColour(kGold.withAlpha(0.88f));
            g.fillPath(arrow);
        }

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float position, float, float, juce::Slider& slider) override
        {
            auto b = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
            const float diameter = juce::jmin(b.getWidth(), b.getHeight()) * 0.71f;
            b = b.withSizeKeepingCentre(diameter, diameter).reduced(1.0f);

            const auto centre = b.getCentre();
            const float radius = b.getWidth() * 0.5f;
            const float start = juce::MathConstants<float>::pi * 0.75f;
            const float zero = juce::MathConstants<float>::pi * 1.5f;
            const float end = juce::MathConstants<float>::pi * 2.25f;
            const float minimum = (float) slider.getMinimum();
            const float maximum = (float) slider.getMaximum();
            const float value = (float) slider.getValue();
            const bool bipolar = minimum < 0.0f && maximum > 0.0f;

            float angle = juce::jmap(position, start, end);
            if (bipolar)
            {
                angle = value <= 0.0f
                    ? juce::jmap(value, minimum, 0.0f, start, zero)
                    : juce::jmap(value, 0.0f, maximum, zero, end);
            }

            for (int tick = 0; tick < 17; ++tick)
            {
                const float phase = juce::jmap((float) tick, 0.0f, 16.0f, start, end);
                const auto dot = centre + juce::Point<float>(std::cos(phase), std::sin(phase)) * (radius * 1.12f);
                g.setColour((tick == 8 && bipolar ? kPurple : kGold).withAlpha(tick == 8 && bipolar ? 0.70f : 0.31f));
                g.fillEllipse(dot.x - 1.0f, dot.y - 1.0f, 2.0f, 2.0f);
            }

            g.setColour(juce::Colours::black.withAlpha(0.88f));
            g.fillEllipse(b.expanded(3.0f));
            g.setColour(kGoldDim.withAlpha(0.95f));
            g.drawEllipse(b.expanded(3.0f), 1.0f);

            juce::ColourGradient brass(juce::Colour(0xfff4dcae), b.getTopLeft(),
                                       juce::Colour(0xff754d30), b.getBottomRight(), false);
            g.setGradientFill(brass);
            g.fillEllipse(b);
            g.setColour(juce::Colour(0xff2c1c15).withAlpha(0.90f));
            g.drawEllipse(b.reduced(1.5f), 1.0f);

            const auto inner = b.reduced(radius * 0.19f);
            juce::ColourGradient brush(juce::Colour(0xffffedc7).withAlpha(0.78f), inner.getTopLeft(),
                                       juce::Colour(0xff6b4228).withAlpha(0.94f), inner.getBottomRight(), false);
            g.setGradientFill(brush);
            g.fillEllipse(inner);
            g.setColour(kGoldBright.withAlpha(0.43f));
            g.drawEllipse(inner, 0.6f);

            const auto begin = centre + juce::Point<float>(std::cos(angle), std::sin(angle)) * (radius * 0.10f);
            const auto finish = centre + juce::Point<float>(std::cos(angle), std::sin(angle)) * (radius * 0.68f);
            g.setColour(juce::Colours::black.withAlpha(0.52f));
            g.drawLine(begin.x + 0.8f, begin.y + 1.2f, finish.x + 0.8f, finish.y + 1.2f, 3.2f);
            g.setColour(kPurple.brighter(slider.isMouseOverOrDragging() ? 0.26f : 0.05f));
            g.drawLine(begin.x, begin.y, finish.x, finish.y, slider.isMouseOverOrDragging() ? 2.45f : 2.0f);
            g.setColour(kText.withAlpha(0.92f));
            g.fillEllipse(finish.x - 1.25f, finish.y - 1.25f, 2.5f, 2.5f);
        }

        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float, float, juce::Slider::SliderStyle style,
                              juce::Slider&) override
        {
            if (style == juce::Slider::LinearHorizontal)
            {
                const auto track = juce::Rectangle<float>((float) x + 7.0f, (float) y + (float) height * 0.5f - 2.0f,
                                                          (float) width - 14.0f, 4.0f);
                g.setColour(juce::Colours::black.withAlpha(0.88f));
                g.fillRoundedRectangle(track.expanded(1.8f, 2.7f), 4.0f);
                g.setColour(kGoldDim.withAlpha(0.96f));
                g.drawRoundedRectangle(track.expanded(1.8f, 2.7f), 4.0f, 0.8f);
                g.setColour(kPurple.withAlpha(0.86f));
                g.fillRoundedRectangle(track.withWidth(juce::jlimit(0.0f, track.getWidth(), sliderPos - track.getX())), 2.0f);

                const auto thumb = juce::Rectangle<float>(sliderPos - 9.0f, track.getCentreY() - 9.0f, 18.0f, 18.0f);
                juce::ColourGradient brass(juce::Colour(0xfff5dba9), thumb.getTopLeft(),
                                           juce::Colour(0xff745033), thumb.getBottomRight(), false);
                g.setGradientFill(brass);
                g.fillEllipse(thumb);
                g.setColour(kGoldBright.withAlpha(0.78f));
                g.drawEllipse(thumb, 0.8f);
                return;
            }

            const auto track = juce::Rectangle<float>((float) x + (float) width * 0.5f - 2.0f, (float) y + 7.0f,
                                                      4.0f, (float) height - 14.0f);
            g.setColour(juce::Colours::black.withAlpha(0.88f));
            g.fillRoundedRectangle(track.expanded(2.5f, 1.6f), 3.0f);
            g.setColour(kGoldDim.withAlpha(0.93f));
            g.drawRoundedRectangle(track.expanded(2.5f, 1.6f), 3.0f, 0.8f);
            g.setColour(kPurple.withAlpha(0.72f));
            g.fillRoundedRectangle(juce::Rectangle<float>(track.getX(), sliderPos, track.getWidth(),
                                                            juce::jmax(0.0f, track.getBottom() - sliderPos)), 2.0f);

            const auto thumb = juce::Rectangle<float>(track.getCentreX() - 10.0f, sliderPos - 10.0f, 20.0f, 20.0f);
            juce::ColourGradient brass(juce::Colour(0xfff2d9aa), thumb.getTopLeft(),
                                       juce::Colour(0xff6c472c), thumb.getBottomRight(), false);
            g.setGradientFill(brass);
            g.fillEllipse(thumb);
            g.setColour(kGoldBright.withAlpha(0.76f));
            g.drawEllipse(thumb, 0.8f);
        }

        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour&, bool hover, bool down) override
        {
            auto b = button.getLocalBounds().toFloat().reduced(0.8f);
            const bool selected = button.getToggleState();
            g.setColour(juce::Colours::black.withAlpha(selected ? 0.70f : 0.82f));
            g.fillRoundedRectangle(b, 5.0f);
            g.setColour((selected ? kGold : kGoldDim).withAlpha(hover ? 1.0f : 0.92f));
            g.drawRoundedRectangle(b, 5.0f, selected ? 1.35f : 0.95f);
            g.setColour(kGoldBright.withAlpha(selected ? 0.42f : 0.22f));
            g.drawLine(b.getX() + 4.0f, b.getY() + 2.0f, b.getRight() - 4.0f, b.getY() + 2.0f, 0.65f);
            if (selected)
            {
                const auto under = juce::Rectangle<float>(b.getX() + 11.0f, b.getBottom() - 3.0f,
                                                          b.getWidth() - 22.0f, 1.4f);
                g.setColour(kPurple.withAlpha(down ? 0.95f : 0.75f));
                g.fillRoundedRectangle(under, 1.0f);
            }
        }

        void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool hover, bool) override
        {
            g.setColour((button.getToggleState() || hover ? kText : kTextDim).withAlpha(button.isEnabled() ? 1.0f : 0.4f));
            g.setFont(displayFont(juce::jlimit(12.0f, 18.0f, (float) button.getHeight() * 0.50f)));
            g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(5, 1), juce::Justification::centred, 1);
        }

        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool hover, bool) override
        {
            const auto bounds = button.getLocalBounds().toFloat();
            const auto box = juce::Rectangle<float>(1.0f, bounds.getCentreY() - 8.0f, 16.0f, 16.0f);
            g.setColour(juce::Colours::black.withAlpha(0.88f));
            g.fillRoundedRectangle(box, 3.0f);
            g.setColour((button.getToggleState() ? kPurple : kGoldDim).withAlpha(hover ? 1.0f : 0.90f));
            g.drawRoundedRectangle(box, 3.0f, button.getToggleState() ? 1.4f : 0.9f);
            if (button.getToggleState())
            {
                juce::Path mark;
                mark.startNewSubPath(box.getX() + 3.1f, box.getCentreY());
                mark.lineTo(box.getCentreX() - 0.7f, box.getBottom() - 3.5f);
                mark.lineTo(box.getRight() - 2.8f, box.getY() + 3.5f);
                g.setColour(kPurple.brighter(0.45f));
                g.strokePath(mark, juce::PathStrokeType(1.9f));
            }
            g.setColour(button.isEnabled() ? kText : kTextDim.withAlpha(0.5f));
            g.setFont(displayFont(12.5f));
            g.drawFittedText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft(23), juce::Justification::centredLeft, 1);
        }
    };
}

MirrorAudioProcessorEditor::MirrorAudioProcessorEditor(MirrorAudioProcessor& mirrorProcessor)
    : AudioProcessorEditor(&mirrorProcessor), audioProcessor(mirrorProcessor)
{
    mirrorLookAndFeel = std::make_unique<MirrorLookAndFeel>();
    setLookAndFeel(mirrorLookAndFeel.get());
    setOpaque(true);
    setResizable(false, false);

    configureLabel(titleLabel, mirroredBrand(), 37.0f, false);
    titleLabel.setFont(displayFont(37.0f));
    titleLabel.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(titleLabel);

    configureLabel(creditLabel, mirroredCredit(), 13.0f, false);
    creditLabel.setFont(displayFont(13.0f));
    creditLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(creditLabel);

    for (auto* button : { &mainPageButton, &harmonyPageButton, &advancedButton })
    {
        button->setClickingTogglesState(true);
        addAndMakeVisible(button);
    }
    mainPageButton.setButtonText("Main");
    harmonyPageButton.setButtonText("Harmony");
    advancedButton.setButtonText("Advanced");
    mainPageButton.setRadioGroupId(8001);
    harmonyPageButton.setRadioGroupId(8001);
    mainPageButton.onClick = [this] { showPage(0); };
    harmonyPageButton.onClick = [this] { showPage(1); };
    advancedButton.onClick = [this]
    {
        showAdvanced = advancedButton.getToggleState();
        updateControlVisibility();
        resized();
        repaint();
    };

    configureComboBox(modeBox, "Vælg Manual eller MIDI");
    modeBox.addItemList({ "Manual", "MIDI" }, 1);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "mode", modeBox);
    configureLabel(modeLabel, "MODE", 12.0f);
    addAndMakeVisible(modeLabel);

    configureComboBox(rootBox, "Toneartens grundtone");
    rootBox.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    rootAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "rootNote", rootBox);
    configureLabel(rootLabel, "KEY", 12.0f);
    addAndMakeVisible(rootLabel);

    configureComboBox(scaleBox, "Skala");
    scaleBox.addItemList({ "Chromatic", "Major", "Minor" }, 1);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "scaleType", scaleBox);
    configureLabel(scaleLabel, "SCALE", 12.0f);
    addAndMakeVisible(scaleLabel);

    configureComboBox(presetBox, "Vælg et udgangspunkt");
    presetBox.addItem("Hide & Seek", 1);
    presetBox.addItem("715 Creek", 2);
    presetBox.addItem("All the Love", 3);
    presetBox.setTextWhenNothingSelected("Preset");
    presetBox.setSelectedId(0, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        if (const int preset = presetBox.getSelectedId(); preset > 0)
            applyPreset(preset);
    };
    configureLabel(presetLabel, "PRESET", 11.0f, false);
    addAndMakeVisible(presetLabel);

    configureComboBox(vocalRangeBox, "Vokalområde for pitch-tracking");
    vocalRangeBox.addItemList({ "Auto", "Bass", "Baritone", "Tenor", "Alto", "Soprano" }, 1);
    vocalRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "vocalRange", vocalRangeBox);
    configureLabel(vocalRangeLabel, "VOICE RANGE", 10.5f);
    addAndMakeVisible(vocalRangeLabel);

    configureComboBox(harmonyStyleBox, "Musikalsk harmoni-stil");
    harmonyStyleBox.addItemList({ "Tight", "Natural", "Wide", "Choir" }, 1);
    harmonyStyleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "harmonyStyle", harmonyStyleBox);
    configureLabel(harmonyStyleLabel, "STYLE", 10.5f);
    addAndMakeVisible(harmonyStyleLabel);

    configureComboBox(midiVoicingBox, "Fordeling af MIDI-akkorden");
    midiVoicingBox.addItemList({ "Close", "Open", "Wide" }, 1);
    midiVoicingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "midiVoicing", midiVoicingBox);
    configureLabel(midiVoicingLabel, "VOICING", 10.5f);
    addAndMakeVisible(midiVoicingLabel);

    configureComboBox(midiInversionBox, "MIDI-inversion");
    midiInversionBox.addItemList({ "Auto", "Root", "1st", "2nd", "3rd" }, 1);
    midiInversionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "midiInversion", midiInversionBox);
    configureLabel(midiInversionLabel, "INVERSION", 10.5f);
    addAndMakeVisible(midiInversionLabel);

    setupKnob(trackingKnob, "tracking", "Tracking");
    setupKnob(glideKnob, "glide", "Transition");
    freezeButton.setButtonText("Freeze");
    freezeButton.setTooltip("Holder seneste pitch-mål");
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "freeze", freezeButton);
    addAndMakeVisible(freezeButton);

    setupKnob(dryLevelKnob, "dry", "Gain");
    setupKnob(dryPanKnob, "dryPan", "Pan");
    setupKnob(dryFormantKnob, "dryFormant", "Formant");
    setupKnob(dryPitchKnob, "dryPitch", "Pitch");
    setupKnob(dryWidthKnob, "dryWidth", "Width");

    for (int i = 0; i < kNumHarmonyVoices; ++i)
        setupVoiceColumn(voiceColumns[(size_t) i], i);

    setupKnob(humanizeKnob, "humanize", "Humanize");
    setupKnob(characterKnob, "character", "Color");
    setupKnob(spreadKnob, "spread", "Spread");
    setupKnob(ambienceKnob, "ambience", "Ambience");
    setupKnob(harmonyMixKnob, "harmony", "Mix");
    harmonyMixKnob.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    setupKnob(globalSaturationKnob, "globalSaturation", "Glue");
    setupKnob(outputGainKnob, "outputGain", "Gain");

    // These performance stages are deliberately fixed at their full-quality
    // setting.  The controls stay visible as part of the instrument face,
    // but their pointers cannot drift away from 100%.
    for (auto* fixed : { &trackingKnob, &glideKnob, &harmonyMixKnob })
    {
        fixed->attachment.reset();
        fixed->slider.setValue(fixed->slider.getMaximum(), juce::dontSendNotification);
        fixed->slider.setInterceptsMouseClicks(false, false);
        fixed->slider.setTooltip("Fixed at 100%");
    }

    mainPageButton.setToggleState(true, juce::dontSendNotification);
    setSize(1120, 650);
    updateControlVisibility();
    startTimerHz(24);
}

MirrorAudioProcessorEditor::~MirrorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MirrorAudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& text,
                                                float fontSize, bool centred)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(displayFont(fontSize));
    label.setColour(juce::Label::textColourId, kTextDim);
    label.setJustificationType(centred ? juce::Justification::centred : juce::Justification::centredLeft);
    label.setInterceptsMouseClicks(false, false);
}

void MirrorAudioProcessorEditor::configureComboBox(juce::ComboBox& box, const juce::String& tooltip)
{
    box.setTooltip(tooltip);
    box.setJustificationType(juce::Justification::centred);
    box.setColour(juce::ComboBox::textColourId, kText);
    addAndMakeVisible(box);
}

void MirrorAudioProcessorEditor::setupKnob(KnobWithLabel& knob, const juce::String& parameterID,
                                           const juce::String& caption)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.slider.setTooltip(caption);
    knob.slider.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(knob.slider);

    configureLabel(knob.label, caption, 11.0f);
    addAndMakeVisible(knob.label);
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, parameterID, knob.slider);
}

void MirrorAudioProcessorEditor::setupVoiceColumn(VoiceColumn& column, int voiceIndex)
{
    const juce::String index(voiceIndex + 1);
    configureLabel(column.title, "Voice " + index, 20.0f);
    column.title.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(column.title);

    column.enableButton.setButtonText("On");
    column.enableButton.setTooltip("Aktiver Voice " + index);
    addAndMakeVisible(column.enableButton);
    column.enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "voiceEnable" + index, column.enableButton);

    column.soloButton.setButtonText("Solo");
    column.soloButton.setTooltip("Solo Voice " + index);
    addAndMakeVisible(column.soloButton);
    column.soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "voiceSolo" + index, column.soloButton);

    configureComboBox(column.intervalBox, "Interval for Voice " + index);
    column.intervalBox.addItemList(getIntervalNames(), 1);
    column.intervalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "voiceInterval" + index, column.intervalBox);

    const auto setupSmall = [this](juce::Slider& slider, juce::Label& label, const juce::String& caption)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setDoubleClickReturnValue(true, 0.0);
        slider.setTooltip(caption);
        addAndMakeVisible(slider);
        configureLabel(label, caption, 10.0f);
        addAndMakeVisible(label);
    };

    setupSmall(column.levelSlider, column.levelLabel, "Level");
    column.levelSlider.setSliderStyle(juce::Slider::LinearVertical);
    setupSmall(column.panSlider, column.panLabel, "Pan");
    setupSmall(column.formantSlider, column.formantLabel, "Formant");
    setupSmall(column.fineTuneSlider, column.fineTuneLabel, "Fine");
    setupSmall(column.toneSlider, column.toneLabel, "Tone");
    setupSmall(column.saturationSlider, column.saturationLabel, "Sat");
    setupSmall(column.microDelaySlider, column.microDelayLabel, "Delay");
    setupSmall(column.vibratoSlider, column.vibratoLabel, "Vibrato");
    setupSmall(column.vibratoRateSlider, column.vibratoRateLabel, "Vib. Rate");

    column.levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "voiceLevel" + index, column.levelSlider);
    column.panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "voicePan" + index, column.panSlider);
    column.formantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "voiceFormant" + index, column.formantSlider);
    column.fineTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "voiceFineTune" + index, column.fineTuneSlider);
    column.toneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "voiceTone" + index, column.toneSlider);
    column.saturationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "voiceSaturation" + index, column.saturationSlider);
    column.microDelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "voiceMicroDelay" + index, column.microDelaySlider);
    column.vibratoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "voiceVibrato" + index, column.vibratoSlider);
    column.vibratoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "voiceVibratoRate" + index, column.vibratoRateSlider);
}

void MirrorAudioProcessorEditor::applyPreset(int presetIndex)
{
    auto& apvts = audioProcessor.apvts;

    auto set = [&](const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    auto setChoice = [&](const juce::String& id, int choiceIndex, int numChoices)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost((float) choiceIndex / (float) juce::jmax(1, numChoices - 1));
    };
    auto voice = [&](int i, bool enable, int intervalIdx, float level, float pan)
    {
        juce::String idx(i + 1);
        static constexpr float tone[kNumHarmonyVoices] = { 0.06f, 0.10f, 0.14f, 0.18f };
        static constexpr float saturation[kNumHarmonyVoices] = { 0.02f, 0.03f, 0.04f, 0.05f };
        static constexpr float microDelayMs[kNumHarmonyVoices] = { 0.0f, 2.0f, 4.0f, 6.0f };
        set("voiceEnable" + idx, enable ? 1.0f : 0.0f);
        set("voiceSolo" + idx, 0.0f);
        setChoice("voiceInterval" + idx, intervalIdx, kNumMusicalIntervals);
        set("voiceLevel" + idx, level);
        set("voicePan" + idx, pan);
        set("voiceFormant" + idx, 0.0f);
        set("voiceFineTune" + idx, 0.0f);
        set("voiceTone" + idx, tone[i]);
        set("voiceSaturation" + idx, saturation[i]);
        set("voiceMicroDelay" + idx, microDelayMs[i]);
        set("voiceVibrato" + idx, 0.0f);
        set("voiceVibratoRate" + idx, 0.45f);
    };
    auto advanced = [&](int i, float formant, float fineTune, float tone, float saturation,
                        float microDelayMs, float vibrato, float vibratoRate)
    {
        juce::String idx(i + 1);
        set("voiceFormant" + idx, formant);
        set("voiceFineTune" + idx, fineTune);
        set("voiceTone" + idx, tone);
        set("voiceSaturation" + idx, saturation);
        set("voiceMicroDelay" + idx, microDelayMs);
        set("voiceVibrato" + idx, vibrato);
        set("voiceVibratoRate" + idx, vibratoRate);
    };

    // A preset changes the vocal texture, but deliberately preserves the
    // user-selected Key and Scale so it remains musical in the current song.
    setChoice("mode", 0, 2);
    set("harmony", 1.0f); set("tracking", 1.0f); set("glide", 1.0f);
    set("freeze", 0.0f);
    set("dryPan", 0.0f); set("dryFormant", 0.0f); set("dryPitch", 0.0f); set("dryWidth", 0.5f);
    set("midiVelocity", 0.0f); set("globalSaturation", 0.04f); set("outputGain", 0.0f);
    setChoice("vocalRange", 0, 6);
    setChoice("harmonyStyle", 1, 4);
    setChoice("midiVoicing", 1, 3);
    setChoice("midiInversion", 0, 5);

    switch (presetIndex)
    {
        case 1:
            setChoice("harmonyStyle", 0, 4);
            set("humanize", 0.10f); set("character", 0.0f); set("spread", 0.5f); set("ambience", 0.16f);
            set("dry", 0.18f); set("harmony", 1.0f); set("tracking", 1.0f); set("glide", 1.0f);
            voice(0, true, 0, 0.70f, -0.22f); voice(1, true, 3, 0.56f, 0.22f); voice(2, true, 7, 0.42f, -0.45f); voice(3, true, 13, 0.30f, 0.45f);
            advanced(0, -0.08f, -4.0f, 0.14f, 0.02f, 0.0f, 0.00f, 0.42f);
            advanced(1, -0.14f, 5.0f, 0.20f, 0.03f, 1.5f, 0.02f, 0.47f);
            advanced(2, -0.20f, -7.0f, 0.30f, 0.03f, 3.0f, 0.01f, 0.39f);
            advanced(3, 0.10f, 8.0f, 0.38f, 0.02f, 5.0f, 0.01f, 0.53f);
            break;
        case 2:
            setChoice("harmonyStyle", 2, 4);
            set("humanize", 0.32f); set("character", 0.08f); set("spread", 0.86f); set("ambience", 0.25f);
            set("dry", 0.32f); set("harmony", 1.0f); set("tracking", 1.0f); set("glide", 1.0f);
            voice(0, true, 3, 0.64f, -0.62f); voice(1, true, 7, 0.58f, 0.58f); voice(2, true, 14, 0.38f, -0.18f); voice(3, true, 4, 0.28f, 0.30f);
            advanced(0, -0.12f, -5.0f, 0.16f, 0.03f, 0.0f, 0.03f, 0.38f);
            advanced(1, -0.18f, 6.0f, 0.24f, 0.04f, 2.0f, 0.02f, 0.46f);
            advanced(2, -0.28f, -9.0f, 0.36f, 0.04f, 4.0f, 0.03f, 0.34f);
            advanced(3, 0.05f, 9.0f, 0.42f, 0.02f, 6.0f, 0.02f, 0.51f);
            break;
        case 3:
            setChoice("harmonyStyle", 3, 4);
            set("humanize", 0.24f); set("character", 0.10f); set("spread", 0.95f); set("ambience", 0.30f);
            set("dry", 0.42f); set("harmony", 1.0f); set("tracking", 1.0f); set("glide", 1.0f);
            voice(0, true, 3, 0.68f, -0.72f); voice(1, true, 7, 0.64f, 0.72f); voice(2, true, 13, 0.42f, -0.25f); voice(3, true, 14, 0.34f, 0.28f);
            advanced(0, -0.04f, -3.0f, 0.10f, 0.04f, 0.0f, 0.04f, 0.43f);
            advanced(1, -0.14f, 4.0f, 0.18f, 0.05f, 2.5f, 0.04f, 0.49f);
            advanced(2, -0.22f, -8.0f, 0.30f, 0.05f, 5.0f, 0.03f, 0.37f);
            advanced(3, 0.12f, 8.0f, 0.40f, 0.03f, 7.0f, 0.03f, 0.56f);
            break;
        default:
            // Unknown preset ids fall back to the first, deterministic preset.
            applyPreset(1);
            break;
    }
}



void MirrorAudioProcessorEditor::showPage(int pageIndex)
{
    currentPage = pageIndex == 0 ? 0 : 1;
    mainPageButton.setToggleState(currentPage == 0, juce::dontSendNotification);
    harmonyPageButton.setToggleState(currentPage == 1, juce::dontSendNotification);
    if (currentPage == 0)
    {
        showAdvanced = false;
        advancedButton.setToggleState(false, juce::dontSendNotification);
    }
    updateControlVisibility();
    resized();
    repaint();
}

void MirrorAudioProcessorEditor::updateControlVisibility()
{
    const bool main = currentPage == 0;
    const bool harmony = !main;
    const bool midiMode = (int) *audioProcessor.apvts.getRawParameterValue("mode") == 1;

    for (auto* label : { &titleLabel, &creditLabel, &modeLabel, &rootLabel, &scaleLabel, &presetLabel })
        label->setVisible(true);
    for (auto* box : { &modeBox, &rootBox, &scaleBox, &presetBox })
        box->setVisible(true);

    mainPageButton.setVisible(true);
    harmonyPageButton.setVisible(true);
    advancedButton.setVisible(harmony);

    vocalRangeBox.setVisible(harmony);
    harmonyStyleBox.setVisible(harmony);
    vocalRangeLabel.setVisible(harmony);
    harmonyStyleLabel.setVisible(harmony);
    midiVoicingBox.setVisible(harmony && midiMode);
    midiInversionBox.setVisible(harmony && midiMode);
    midiVoicingLabel.setVisible(harmony && midiMode);
    midiInversionLabel.setVisible(harmony && midiMode);

    freezeButton.setVisible(main);

    const auto setKnobVisible = [main](KnobWithLabel& knob)
    {
        knob.slider.setVisible(main);
        knob.label.setVisible(main);
    };
    for (auto* knob : { &trackingKnob, &glideKnob, &dryLevelKnob, &dryPanKnob, &dryFormantKnob,
                        &dryPitchKnob, &dryWidthKnob, &humanizeKnob, &characterKnob, &spreadKnob,
                        &ambienceKnob, &harmonyMixKnob, &globalSaturationKnob, &outputGainKnob })
        setKnobVisible(*knob);

    for (auto& voice : voiceColumns)
    {
        voice.title.setVisible(harmony);
        voice.enableButton.setVisible(harmony);
        voice.soloButton.setVisible(harmony && showAdvanced);
        voice.intervalBox.setVisible(harmony);

        voice.levelSlider.setVisible(harmony);
        voice.levelLabel.setVisible(harmony);
        voice.panSlider.setVisible(harmony);
        voice.panLabel.setVisible(harmony);
        voice.formantSlider.setVisible(harmony);
        voice.formantLabel.setVisible(harmony);

        for (auto* slider : { &voice.fineTuneSlider, &voice.toneSlider, &voice.saturationSlider,
                              &voice.microDelaySlider, &voice.vibratoSlider, &voice.vibratoRateSlider })
            slider->setVisible(harmony && showAdvanced);
        for (auto* label : { &voice.fineTuneLabel, &voice.toneLabel, &voice.saturationLabel,
                             &voice.microDelayLabel, &voice.vibratoLabel, &voice.vibratoRateLabel })
            label->setVisible(harmony && showAdvanced);
    }
}

void MirrorAudioProcessorEditor::timerCallback()
{
    const int mode = (int) *audioProcessor.apvts.getRawParameterValue("mode");
    if (mode != lastMode)
    {
        lastMode = mode;
        updateControlVisibility();
        resized();
    }

    bool changed = false;
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const float target = juce::jlimit(0.0f, 1.0f,
                                          audioProcessor.currentVoiceVisualLevels[(size_t) i].load(std::memory_order_relaxed));
        const float next = visualEnergy[(size_t) i] + (target - visualEnergy[(size_t) i]) * 0.20f;
        if (std::abs(next - visualEnergy[(size_t) i]) > 0.002f)
        {
            visualEnergy[(size_t) i] = next;
            changed = true;
        }
        const float panTarget = (float) *audioProcessor.apvts.getRawParameterValue("voicePan" + juce::String(i + 1));
        visualPan[(size_t) i] += (panTarget - visualPan[(size_t) i]) * 0.10f;
        visualPresence[(size_t) i] = (float) *audioProcessor.apvts.getRawParameterValue("voiceEnable" + juce::String(i + 1));
    }

    if (currentPage == 0 || changed)
        repaint(mirrorBounds.expanded(8));
}

void MirrorAudioProcessorEditor::placeKnob(KnobWithLabel& knob, juce::Rectangle<int> bounds)
{
    const auto knobBounds = bounds.removeFromTop(65);
    knob.slider.setBounds(knobBounds);
    knob.label.setBounds(bounds.withTrimmedTop(0));
}

void MirrorAudioProcessorEditor::placeSmallKnob(juce::Slider& slider, juce::Label& label,
                                                juce::Rectangle<int> bounds)
{
    const auto knobBounds = bounds.removeFromTop(56);
    slider.setBounds(knobBounds);
    label.setBounds(bounds);
}

void MirrorAudioProcessorEditor::layoutHeader()
{
    titleLabel.setBounds(25, 13, 260, 43);
    creditLabel.setBounds(29, 56, 235, 20);

    presetLabel.setBounds(28, 81, 190, 14);
    presetBox.setBounds(28, 96, 194, 28);

    modeLabel.setBounds(482, 12, 150, 18);
    modeBox.setBounds(480, 29, 156, 29);
    rootLabel.setBounds(808, 12, 112, 18);
    rootBox.setBounds(807, 29, 118, 29);
    scaleLabel.setBounds(951, 12, 140, 18);
    scaleBox.setBounds(950, 29, 142, 29);

    mainPageButton.setBounds(424, 79, 135, 30);
    harmonyPageButton.setBounds(563, 79, 135, 30);
    advancedButton.setBounds(492, 115, 138, 25);

    vocalRangeLabel.setBounds(704, 82, 92, 14);
    vocalRangeBox.setBounds(700, 97, 102, 26);
    harmonyStyleLabel.setBounds(808, 82, 72, 14);
    harmonyStyleBox.setBounds(805, 97, 82, 26);
    midiVoicingLabel.setBounds(893, 82, 86, 14);
    midiVoicingBox.setBounds(890, 97, 91, 26);
    midiInversionLabel.setBounds(986, 82, 100, 14);
    midiInversionBox.setBounds(982, 97, 104, 26);
}

void MirrorAudioProcessorEditor::layoutMain()
{
    dryPanelBounds = { 25, 163, 230, 462 };
    mirrorBounds = { 277, 159, 566, 264 };
    characterPanelBounds = { 865, 163, 230, 462 };
    outputPanelBounds = { 394, 443, 332, 182 };

    placeKnob(dryLevelKnob,   { 42, 270, 88, 85 });
    placeKnob(dryPitchKnob,   { 149, 270, 88, 85 });
    placeKnob(dryPanKnob,     { 96, 360, 88, 85 });
    placeKnob(dryWidthKnob,   { 42, 454, 88, 85 });
    placeKnob(dryFormantKnob, { 149, 454, 88, 85 });

    placeKnob(trackingKnob, { 880, 270, 88, 85 });
    placeKnob(glideKnob,    { 980, 270, 88, 85 });
    placeKnob(humanizeKnob, { 880, 363, 88, 85 });
    placeKnob(ambienceKnob, { 980, 363, 88, 85 });
    placeKnob(characterKnob,{ 880, 456, 88, 85 });
    placeKnob(spreadKnob,   { 980, 456, 88, 85 });
    freezeButton.setBounds(925, 555, 108, 28);

    placeKnob(globalSaturationKnob, { 430, 487, 88, 83 });
    placeKnob(outputGainKnob,       { 601, 487, 88, 83 });
    harmonyMixKnob.slider.setBounds(431, 577, 258, 27);
    harmonyMixKnob.label.setBounds(510, 555, 100, 18);
}

void MirrorAudioProcessorEditor::layoutHarmony()
{
    constexpr int panelY = 214;
    constexpr int panelW = 253;
    constexpr int panelH = 411;
    constexpr int spacing = 14;

    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const int x = 18 + i * (panelW + spacing);
        auto& voice = voiceColumns[(size_t) i];
        voicePanelBounds[(size_t) i] = { x, panelY, panelW, panelH };

        voice.title.setBounds(x, 154, panelW, 30);
        voice.enableButton.setBounds(x + 16, 187, 58, 25);
        voice.soloButton.setBounds(x + 172, 187, 63, 25);
        voice.intervalBox.setBounds(x + 75, 187, 94, 25);
        voice.levelLabel.setBounds(x + 10, 230, 42, 16);
        voice.levelSlider.setBounds(x + 11, 249, 42, 338);

        if (showAdvanced)
        {
            placeSmallKnob(voice.panSlider, voice.panLabel, { x + 71, 257, 76, 68 });
            placeSmallKnob(voice.formantSlider, voice.formantLabel, { x + 153, 257, 76, 68 });
            placeSmallKnob(voice.fineTuneSlider, voice.fineTuneLabel, { x + 71, 331, 76, 68 });
            placeSmallKnob(voice.toneSlider, voice.toneLabel, { x + 153, 331, 76, 68 });
            placeSmallKnob(voice.saturationSlider, voice.saturationLabel, { x + 71, 405, 76, 68 });
            placeSmallKnob(voice.microDelaySlider, voice.microDelayLabel, { x + 153, 405, 76, 68 });
            placeSmallKnob(voice.vibratoSlider, voice.vibratoLabel, { x + 71, 479, 76, 68 });
            placeSmallKnob(voice.vibratoRateSlider, voice.vibratoRateLabel, { x + 153, 479, 76, 68 });
        }
        else
        {
            placeSmallKnob(voice.panSlider, voice.panLabel, { x + 69, 310, 78, 74 });
            placeSmallKnob(voice.formantSlider, voice.formantLabel, { x + 154, 310, 78, 74 });
        }
    }
}

void MirrorAudioProcessorEditor::resized()
{
    layoutHeader();
    if (currentPage == 0)
        layoutMain();
    else
        layoutHarmony();
}

void MirrorAudioProcessorEditor::paint(juce::Graphics& g)
{
    drawBackground(g);
    drawHeader(g);
    if (currentPage == 0)
        drawMainPanels(g);
    else
        drawHarmonyPanels(g);
}

void MirrorAudioProcessorEditor::drawBackground(juce::Graphics& g) const
{
    const auto bounds = getLocalBounds().toFloat();
    g.fillAll(kInk);

    juce::ColourGradient glow(kPurple.withAlpha(0.11f), bounds.getCentreX(), bounds.getCentreY() * 0.76f,
                              juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getBottom(), true);
    g.setGradientFill(glow);
    g.fillRect(bounds);

    // Fine procedural sigils make the surface feel engraved without making
    // the background a bitmap or competing with the controls.
    g.setColour(kGoldDim.withAlpha(0.075f));
    for (int y = 17, row = 0; y < getHeight(); y += 43, ++row)
    {
        for (int x = 10 + (row % 2) * 19; x < getWidth(); x += 53)
        {
            const float r = ((x / 53 + row) % 3 == 0) ? 10.0f : 5.5f;
            g.drawEllipse((float) x - r, (float) y - r, r * 2.0f, r * 2.0f, 0.45f);
            if ((x + row) % 4 == 0)
            {
                g.drawLine((float) x - r, (float) y, (float) x + r, (float) y, 0.38f);
                g.drawLine((float) x, (float) y - r, (float) x, (float) y + r, 0.38f);
            }
        }
    }

    g.setColour(kGoldDim.withAlpha(0.78f));
    g.drawRoundedRectangle(bounds.reduced(4.0f), 3.0f, 1.0f);
    g.setColour(kGoldBright.withAlpha(0.15f));
    g.drawRoundedRectangle(bounds.reduced(7.0f), 2.0f, 0.55f);
}

void MirrorAudioProcessorEditor::drawHeader(juce::Graphics& g) const
{
    g.setColour(kGoldDim.withAlpha(0.38f));
    g.drawLine(22.0f, 139.0f, (float) getWidth() - 22.0f, 139.0f, 0.75f);

    const auto active = currentPage == 0 ? mainPageButton.getBounds() : harmonyPageButton.getBounds();
    g.setColour(kPurple.withAlpha(0.90f));
    g.fillRoundedRectangle((float) active.getX() + 18.0f, (float) active.getBottom() - 3.0f,
                           (float) active.getWidth() - 36.0f, 1.5f, 1.0f);
}

void MirrorAudioProcessorEditor::drawPanel(juce::Graphics& g, juce::Rectangle<float> b,
                                           bool enabled, bool ornate) const
{
    const float alpha = enabled ? 1.0f : 0.47f;
    g.setColour(juce::Colours::black.withAlpha(0.76f * alpha));
    g.fillRoundedRectangle(b, 24.0f);
    g.setColour(kGoldDim.withAlpha(0.90f * alpha));
    g.drawRoundedRectangle(b, 24.0f, 1.8f);
    g.setColour(kGoldBright.withAlpha(0.62f * alpha));
    g.drawRoundedRectangle(b.reduced(4.0f), 20.0f, 0.8f);
    g.setColour(kLift.withAlpha(0.84f * alpha));
    g.fillRoundedRectangle(b.reduced(7.0f), 17.0f);
    g.setColour(kGoldDim.withAlpha(0.43f * alpha));
    g.drawRoundedRectangle(b.reduced(11.0f), 13.0f, 0.55f);

    const auto crest = juce::Point<float>(b.getCentreX(), b.getY() + 17.0f);
    g.setColour(kGold.withAlpha(0.63f * alpha));
    g.drawEllipse(crest.x - 11.0f, crest.y - 11.0f, 22.0f, 22.0f, 0.8f);
    g.drawEllipse(crest.x - 6.0f, crest.y - 6.0f, 12.0f, 12.0f, 0.5f);
    for (int ray = 0; ray < 8; ++ray)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) ray / 8.0f;
        g.drawLine(crest.x + std::cos(a) * 7.0f, crest.y + std::sin(a) * 7.0f,
                   crest.x + std::cos(a) * 10.0f, crest.y + std::sin(a) * 10.0f, 0.65f);
    }

    if (ornate)
    {
        g.setColour(kGoldDim.withAlpha(0.27f * alpha));
        for (const auto& corner : { b.getTopLeft(), b.getTopRight(), b.getBottomLeft(), b.getBottomRight() })
        {
            g.drawEllipse(corner.x - 4.0f, corner.y - 4.0f, 8.0f, 8.0f, 0.6f);
            g.drawLine(corner.x - 9.0f, corner.y, corner.x + 9.0f, corner.y, 0.4f);
            g.drawLine(corner.x, corner.y - 9.0f, corner.x, corner.y + 9.0f, 0.4f);
        }
    }
}

void MirrorAudioProcessorEditor::drawSectionTitle(juce::Graphics& g, const juce::String& title,
                                                   juce::Rectangle<int> bounds) const
{
    g.setColour(kText.withAlpha(0.92f));
    g.setFont(displayFont(24.0f));
    g.drawFittedText(title, bounds, juce::Justification::centred, 1);
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.drawFittedText(title, bounds.translated(0, 2), juce::Justification::centred, 1);
}

void MirrorAudioProcessorEditor::drawVoiceGlyph(juce::Graphics& g, int voiceIndex, float x, float groundY,
                                                float energy, float alpha) const
{
    const float height = 67.0f + energy * 35.0f;
    const float head = 9.0f + energy * 3.0f;
    const float baseWidth = 12.0f + (float) voiceIndex * 1.2f;

    for (int ring = 0; ring < 3; ++ring)
    {
        const float width = 42.0f + static_cast<float>(ring) * 19.0f + energy * 24.0f;
        const float h = 8.0f + static_cast<float>(ring) * 4.0f;
        g.setColour((ring == 0 ? kGold : kPurple).withAlpha(alpha * (0.36f - static_cast<float>(ring) * 0.07f)));
        g.drawEllipse(x - width * 0.5f, groundY - h * 0.5f, width, h, 0.8f);
    }

    const auto headCentre = juce::Point<float>(x, groundY - height);
    g.setColour(kGoldBright.withAlpha(alpha * 0.94f));
    g.drawEllipse(headCentre.x - head, headCentre.y - head, head * 2.0f, head * 2.0f, 1.5f);

    juce::Path body;
    body.startNewSubPath(x, headCentre.y + head);
    body.lineTo(x - baseWidth * 0.55f, groundY - 20.0f);
    body.lineTo(x - baseWidth, groundY - 2.0f);
    body.lineTo(x + baseWidth, groundY - 2.0f);
    body.lineTo(x + baseWidth * 0.55f, groundY - 20.0f);
    body.closeSubPath();

    g.setColour(kGold.withAlpha(alpha * 0.86f));
    g.strokePath(body, juce::PathStrokeType(1.8f));
    g.setColour(kPurple.withAlpha(alpha * (0.20f + energy * 0.35f)));
    g.fillPath(body);
}

void MirrorAudioProcessorEditor::drawMirrorVisualizer(juce::Graphics& g) const
{
    auto b = mirrorBounds.toFloat();
    juce::Path silhouette;
    const float cx = b.getCentreX();
    silhouette.startNewSubPath(b.getX() + 18.0f, b.getCentreY());
    silhouette.cubicTo(b.getX() + 52.0f, b.getY() + 18.0f, cx - 145.0f, b.getY() + 17.0f,
                       cx, b.getY() + 8.0f);
    silhouette.cubicTo(cx + 145.0f, b.getY() + 17.0f, b.getRight() - 52.0f, b.getY() + 18.0f,
                       b.getRight() - 18.0f, b.getCentreY());
    silhouette.cubicTo(b.getRight() - 52.0f, b.getBottom() - 18.0f, cx + 145.0f, b.getBottom() - 17.0f,
                       cx, b.getBottom() - 8.0f);
    silhouette.cubicTo(cx - 145.0f, b.getBottom() - 17.0f, b.getX() + 52.0f, b.getBottom() - 18.0f,
                       b.getX() + 18.0f, b.getCentreY());
    silhouette.closeSubPath();

    g.setColour(juce::Colours::black.withAlpha(0.83f));
    g.fillPath(silhouette);
    g.setColour(kGoldDim.withAlpha(0.95f));
    g.strokePath(silhouette, juce::PathStrokeType(2.0f));
    g.setColour(kGoldBright.withAlpha(0.58f));
    g.strokePath(silhouette, juce::PathStrokeType(0.65f), juce::AffineTransform::translation(0.0f, 2.0f));

    auto inner = b.reduced(28.0f, 35.0f);
    juce::ColourGradient aura(kPurple.withAlpha(0.28f), inner.getCentreX(), inner.getBottom(),
                              juce::Colours::transparentBlack, inner.getCentreX(), inner.getY(), false);
    g.setGradientFill(aura);
    g.fillEllipse(inner);

    const float ground = b.getBottom() - 57.0f;
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const float baseline = juce::jmap((float) i, 0.0f, 3.0f, b.getX() + 130.0f, b.getRight() - 130.0f);
        const float x = baseline + visualPan[(size_t) i] * 24.0f;
        drawVoiceGlyph(g, i, x, ground, visualEnergy[(size_t) i], 0.34f + visualPresence[(size_t) i] * 0.60f);
    }
}

void MirrorAudioProcessorEditor::drawMainPanels(juce::Graphics& g) const
{
    drawPanel(g, dryPanelBounds.toFloat(), true);
    drawPanel(g, characterPanelBounds.toFloat(), true);
    drawPanel(g, outputPanelBounds.toFloat(), true);

    drawSectionTitle(g, "DRY", dryPanelBounds.withTrimmedTop(32).withHeight(36));
    drawSectionTitle(g, "CHARACTER", characterPanelBounds.withTrimmedTop(32).withHeight(36));
    drawSectionTitle(g, "OUTPUT", outputPanelBounds.withTrimmedTop(32).withHeight(34));

    drawMirrorVisualizer(g);

    g.setColour(kTextDim);
    g.setFont(displayFont(12.0f));
    g.drawFittedText("Mix", outputPanelBounds.withTrimmedTop(108).withHeight(18), juce::Justification::centred, 1);
}

void MirrorAudioProcessorEditor::drawHarmonyPanels(juce::Graphics& g) const
{
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const auto& voice = voiceColumns[(size_t) i];
        const bool enabled = voice.enableButton.getToggleState();
        drawPanel(g, voicePanelBounds[(size_t) i].toFloat(), enabled);
        g.setColour(kGoldDim.withAlpha(0.55f));
        const float dividerX = (float) voicePanelBounds[(size_t) i].getX() + 58.0f;
        g.drawLine(dividerX, (float) voicePanelBounds[(size_t) i].getY() + 38.0f,
                   dividerX, (float) voicePanelBounds[(size_t) i].getBottom() - 24.0f, 0.65f);
    }

    if (showAdvanced)
    {
        g.setColour(kPurple.withAlpha(0.54f));
        g.fillRoundedRectangle(492.0f, 138.0f, 138.0f, 1.5f, 1.0f);
    }
}
