#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour kInk { 0xff070709 };
    const juce::Colour kSurface { 0xff111014 };
    const juce::Colour kSurfaceLift { 0xff1b1920 };
    const juce::Colour kGold { 0xffdbc8a2 };
    const juce::Colour kGoldDim { 0xff8d785b };
    const juce::Colour kText { 0xffeee6d8 };
    const juce::Colour kTextDim { 0xffa99e8d };
    const juce::Colour kPurple { 0xffbd72ff };
    const juce::Colour kPurpleDim { 0xff4f276c };

    // Times New Roman is present on supported macOS systems and contains the
    // Cyrillic Я glyph.  Using the Unicode code point explicitly avoids a
    // source-encoding/fallback-font mismatch that previously rendered a
    // normal R on some hosts.
    juce::Font displayFont(float size, int style = juce::Font::plain)
    {
        return juce::Font("Times New Roman", size, style);
    }

    juce::String mirrorText(juce::String text)
    {
        const auto reversedR = juce::String::charToString((juce::juce_wchar) 0x042f);
        return text.replace("R", reversedR).replace("r", reversedR);
    }

    const juce::Image& engravedBackgroundTexture()
    {
        static const auto texture = juce::ImageFileFormat::loadFrom(
            BinaryData::mirror_engraved_background_jpg,
            (size_t) BinaryData::mirror_engraved_background_jpgSize);
        return texture;
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
            return displayFont(juce::jlimit(15.0f, 22.0f, (float) box.getHeight() * 0.49f));
        }

        juce::Font getPopupMenuFont() override
        {
            return displayFont(18.0f);
        }

        void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
        {
            label.setBounds(12, 1, box.getWidth() - 42, box.getHeight() - 2);
            label.setFont(getComboBoxFont(box));
            label.setJustificationType(juce::Justification::centred);
        }

        void drawPopupMenuBackgroundWithOptions(juce::Graphics& g, int width, int height,
                                                const juce::PopupMenu::Options&) override
        {
            auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);
            g.setColour(juce::Colour(0xff0a090d).withAlpha(0.98f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), 7.0f);
            g.setColour(kGoldDim.withAlpha(0.95f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 7.0f, 1.0f);
            g.setColour(kPurple.withAlpha(0.22f));
            g.drawRoundedRectangle(bounds.reduced(4.0f), 5.0f, 0.65f);
        }

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float position, float startAngle, float endAngle,
                              juce::Slider& slider) override
        {
            auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(5.0f);
            const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
            bounds = bounds.withSizeKeepingCentre(diameter, diameter);

            const auto centre = bounds.getCentre();
            const float radius = bounds.getWidth() * 0.5f;
            const bool active = slider.isMouseOverOrDragging();
            const float glow = active ? 0.42f : 0.20f;

            juce::ColourGradient body(kGold.withAlpha(0.92f), bounds.getTopLeft(),
                                      juce::Colour(0xff2c1e18), bounds.getBottomRight(), false);
            g.setGradientFill(body);
            g.fillEllipse(bounds);

            g.setColour(juce::Colours::black.withAlpha(0.8f));
            g.drawEllipse(bounds, 2.0f);
            g.setColour(kGold.withAlpha(0.80f));
            g.drawEllipse(bounds.reduced(2.5f), 1.0f);
            g.setColour(kPurple.withAlpha(glow));
            g.drawEllipse(bounds.expanded(4.0f), active ? 2.0f : 1.0f);

            auto inner = bounds.reduced(radius * 0.20f);
            juce::ColourGradient innerShade(juce::Colour(0xfff0d9ac).withAlpha(0.78f), inner.getTopLeft(),
                                             juce::Colour(0xff6a4930).withAlpha(0.85f), inner.getBottomRight(), false);
            g.setGradientFill(innerShade);
            g.fillEllipse(inner);
            g.setColour(kGold.withAlpha(0.62f));
            g.drawEllipse(inner, 0.8f);

            const float angle = juce::jmap(position, startAngle, endAngle);
            const auto dot = centre + juce::Point<float>(std::cos(angle), std::sin(angle)) * (radius * 0.60f);
            g.setColour(kPurple.withAlpha(0.92f));
            g.drawLine(centre.x, centre.y, dot.x, dot.y, 2.4f);
            g.setColour(kText.withAlpha(0.85f));
            g.fillEllipse(juce::Rectangle<float>(dot.x - 1.8f, dot.y - 1.8f, 3.6f, 3.6f));
        }

        void drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                          int, int, int, int, juce::ComboBox& box) override
        {
            auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(1.0f);
            g.setColour(juce::Colours::black.withAlpha(0.72f));
            g.fillRoundedRectangle(bounds, 7.0f);
            g.setColour((isDown || box.isMouseOver()) ? kGold : kGoldDim);
            g.drawRoundedRectangle(bounds, 7.0f, isDown ? 1.8f : 1.0f);
            g.setColour(kPurple.withAlpha(isDown ? 0.75f : 0.25f));
            g.drawRoundedRectangle(bounds.reduced(3.0f), 5.0f, 0.8f);

            juce::Path arrow;
            const float ax = (float) width - 18.0f;
            const float ay = (float) height * 0.5f - 2.0f;
            arrow.startNewSubPath(ax - 5.0f, ay);
            arrow.lineTo(ax + 5.0f, ay);
            arrow.lineTo(ax, ay + 6.0f);
            arrow.closeSubPath();
            g.setColour(kGold);
            g.fillPath(arrow);
        }

        void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                              bool isHighlighted, bool) override
        {
            auto box = juce::Rectangle<float>(4.0f, 3.0f, 20.0f, 20.0f);
            const bool on = button.getToggleState();
            g.setColour(juce::Colours::black.withAlpha(0.78f));
            g.fillRoundedRectangle(box, 4.0f);
            g.setColour(on ? kPurple : (isHighlighted ? kGold : kGoldDim));
            g.drawRoundedRectangle(box, 4.0f, on ? 1.8f : 1.0f);

            if (on)
            {
                g.setColour(kPurple.withAlpha(0.20f));
                g.fillRoundedRectangle(box.reduced(2.0f), 2.0f);
                juce::Path tick;
                tick.startNewSubPath(8.0f, 13.0f);
                tick.lineTo(12.0f, 17.0f);
                tick.lineTo(20.0f, 8.0f);
                g.setColour(kPurple.brighter(0.25f));
                g.strokePath(tick, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            g.setColour(on ? kText : kTextDim);
            g.setFont(displayFont(14.5f));
            g.drawText(button.getButtonText(), 31, 0, button.getWidth() - 31, button.getHeight(),
                       juce::Justification::centredLeft, true);
        }

        void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                  bool highlighted, bool down) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
            const bool on = button.getToggleState();
            g.setColour(juce::Colours::black.withAlpha(0.73f));
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour((on || highlighted) ? kGold : kGoldDim);
            g.drawRoundedRectangle(bounds, 6.0f, on ? 1.4f : 0.9f);
            if (on)
            {
                g.setColour(kPurple.withAlpha(down ? 0.95f : 0.74f));
                g.fillRoundedRectangle(juce::Rectangle<float>(bounds.getX() + 10.0f, bounds.getBottom() - 3.0f,
                                                               bounds.getWidth() - 20.0f, 1.6f), 1.0f);
            }
        }

        juce::Font getTextButtonFont(juce::TextButton&, int height) override
        {
            return displayFont(juce::jlimit(14.0f, 21.0f, (float) height * 0.52f));
        }
    };
}

MirrorAudioProcessorEditor::MirrorAudioProcessorEditor(MirrorAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor)
{
    mirrorLookAndFeel = std::make_unique<MirrorLookAndFeel>();
    setLookAndFeel(mirrorLookAndFeel.get());
    setOpaque(true);
    setResizable(true, true);
    // The visual system is authored at the exact 1723×913 reference canvas,
    // then scales down proportionally for smaller DAW windows.
    setResizeLimits(1280, 680, 1723, 913);

    configureLabel(titleLabel, mirrorText("MIRROR"), 42.0f, false);
    titleLabel.setFont(displayFont(42.0f));
    titleLabel.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(titleLabel);

    configureLabel(creditLabel, mirrorText("By Lou!s Gabriel"), 14.0f, false);
    creditLabel.setFont(displayFont(14.0f));
    creditLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(creditLabel);

    for (auto* button : { &mainPageButton, &harmonyPageButton, &advancedButton })
    {
        button->setClickingTogglesState(true);
        button->setColour(juce::TextButton::textColourOffId, kTextDim);
        button->setColour(juce::TextButton::textColourOnId, kText);
        addAndMakeVisible(button);
    }
    mainPageButton.setButtonText("Main");
    harmonyPageButton.setButtonText(mirrorText("Harmony"));
    advancedButton.setButtonText("Advanced");
    mainPageButton.setRadioGroupId(8001);
    harmonyPageButton.setRadioGroupId(8001);
    mainPageButton.onClick = [this] { showPage(0); };
    harmonyPageButton.onClick = [this] { showPage(1); };
    advancedButton.onClick = [this]
    {
        showAdvanced = advancedButton.getToggleState();
        // This must run before layout: Advanced controls are deliberately
        // hidden on the simple page and were otherwise never revealed.
        updateControlVisibility();
        resized();
        repaint();
    };

    configureComboBox(modeBox, "Manual eller MIDI");
    modeBox.addItemList({ "Manual", "MIDI" }, 1);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "mode", modeBox);
    configureLabel(modeLabel, "MODE", 15.0f);
    addAndMakeVisible(modeLabel);

    configureComboBox(rootBox, "Toneartens grundtone");
    rootBox.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    rootAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "rootNote", rootBox);
    configureLabel(rootLabel, "KEY", 15.0f);
    addAndMakeVisible(rootLabel);

    configureComboBox(scaleBox, "Skala");
    scaleBox.addItemList({ "Chromatic", "Major", "Minor" }, 1);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "scaleType", scaleBox);
    configureLabel(scaleLabel, "SCALE", 15.0f);
    addAndMakeVisible(scaleLabel);

    configureComboBox(vocalRangeBox, "Vokalområde for pitch-tracking");
    vocalRangeBox.addItemList({ "Auto", "Bass", "Baritone", "Tenor", "Alto", "Soprano" }, 1);
    vocalRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "vocalRange", vocalRangeBox);
    configureLabel(vocalRangeLabel, "VOICE ЯANGE", 10.5f);
    addAndMakeVisible(vocalRangeLabel);

    configureComboBox(harmonyStyleBox, "Musikalsk harmoni-stil");
    harmonyStyleBox.addItemList({ "Tight", "Natural", "Wide", "Choir" }, 1);
    harmonyStyleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "harmonyStyle", harmonyStyleBox);
    configureLabel(harmonyStyleLabel, "STYLE", 10.5f);
    addAndMakeVisible(harmonyStyleLabel);

    configureComboBox(midiVoicingBox, "Fordeling af MIDI-akkorden");
    midiVoicingBox.addItemList({ "Close", "Open", "Wide" }, 1);
    midiVoicingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "midiVoicing", midiVoicingBox);
    configureLabel(midiVoicingLabel, "MIDI VOICING", 10.5f);
    addAndMakeVisible(midiVoicingLabel);

    configureComboBox(midiInversionBox, "MIDI-inversion");
    midiInversionBox.addItemList({ "Auto", "Root", "1st", "2nd", "3rd" }, 1);
    midiInversionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "midiInversion", midiInversionBox);
    configureLabel(midiInversionLabel, "INVEЯSION", 10.5f);
    addAndMakeVisible(midiInversionLabel);

    configureComboBox(presetBox, "PЯeset");
    presetBox.addItem("Vocoder Glass", 1);
    presetBox.addItem("Fractured Stack", 2);
    presetBox.addItem("Stadium Choir", 3);
    presetBox.setSelectedId(1, juce::dontSendNotification);
    presetBox.onChange = [this] { applyPreset(presetBox.getSelectedId()); };
    configureLabel(presetLabel, "PЯESET", 11.0f, false);
    addAndMakeVisible(presetLabel);

    setupKnob(trackingKnob, "tracking", "TЯACKING");
    setupKnob(glideKnob, "glide", "TЯANSITION");
    freezeButton.setButtonText("FЯEEZE");
    freezeButton.setTooltip("Holder seneste pitch-mål");
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "freeze", freezeButton);
    addAndMakeVisible(freezeButton);

    setupKnob(dryLevelKnob, "dry", "GAIN");
    setupKnob(dryPanKnob, "dryPan", "PAN");
    setupKnob(dryFormantKnob, "dryFormant", "FoЯmant");
    setupKnob(dryPitchKnob, "dryPitch", "PITCH");
    setupKnob(dryWidthKnob, "dryWidth", "WIDTH");

    for (int i = 0; i < kNumHarmonyVoices; ++i)
        setupVoiceColumn(voiceColumns[(size_t) i], i);

    setupKnob(humanizeKnob, "humanize", "HUMANIZE");
    setupKnob(characterKnob, "character", "COLOЯ");
    setupKnob(spreadKnob, "spread", "SpЯead");
    setupKnob(ambienceKnob, "ambience", "AMBIENCE");
    setupKnob(harmonyMixKnob, "harmony", "MIX");
    harmonyMixKnob.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    harmonyMixKnob.slider.setColour(juce::Slider::trackColourId, kGoldDim);
    harmonyMixKnob.slider.setColour(juce::Slider::thumbColourId, kGold);
    setupKnob(globalSaturationKnob, "globalSaturation", "GLUE");
    setupKnob(outputGainKnob, "outputGain", "GAIN");

    modeBox.onChange = [this] { updateControlVisibility(); };
    mainPageButton.setToggleState(true, juce::dontSendNotification);
    setSize(1723, 913);
    updateControlVisibility();
    startTimerHz(30);
}

MirrorAudioProcessorEditor::~MirrorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MirrorAudioProcessorEditor::configureLabel(juce::Label& label, const juce::String& text, float fontSize, bool centred)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(centred ? juce::Justification::centred : juce::Justification::centredLeft);
    label.setFont(displayFont(fontSize));
    label.setColour(juce::Label::textColourId, kTextDim);
}

void MirrorAudioProcessorEditor::configureComboBox(juce::ComboBox& box, const juce::String& tooltip)
{
    box.setJustificationType(juce::Justification::centred);
    box.setColour(juce::ComboBox::textColourId, kText);
    box.setColour(juce::ComboBox::arrowColourId, kGold);
    box.setTooltip(tooltip);
    addAndMakeVisible(box);
}

void MirrorAudioProcessorEditor::setupKnob(KnobWithLabel& knob, const juce::String& parameterID, const juce::String& displayName)
{
    knob.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    knob.slider.setColour(juce::Slider::rotarySliderFillColourId, kPurple);
    knob.slider.setTooltip(displayName);
    addAndMakeVisible(knob.slider);

    configureLabel(knob.label, displayName, 12.5f);
    addAndMakeVisible(knob.label);
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, parameterID, knob.slider);
}

void MirrorAudioProcessorEditor::setupVoiceColumn(VoiceColumn& column, int voiceIndex)
{
    const juce::String index(voiceIndex + 1);
    configureLabel(column.title, "VOICE " + index, 21.0f);
    column.title.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(column.title);

    column.enableButton.setButtonText("ON");
    column.enableButton.setTooltip("Aktiver Voice " + index);
    addAndMakeVisible(column.enableButton);
    column.enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "voiceEnable" + index, column.enableButton);

    column.soloButton.setButtonText("SOLO");
    column.soloButton.setTooltip("Solo Voice " + index);
    addAndMakeVisible(column.soloButton);
    column.soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.apvts, "voiceSolo" + index, column.soloButton);

    configureComboBox(column.intervalBox, "Interval for Voice " + index);
    // Menu values stay conventional and immediately readable (+3rd, -3rd);
    // the mirrored glyph is reserved for the brand and decorative labels.
    column.intervalBox.addItemList(getIntervalNames(), 1);
    column.intervalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "voiceInterval" + index, column.intervalBox);

    auto setupSmall = [this](juce::Slider& slider, juce::Label& label, const juce::String& parameterID, const juce::String& caption)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setTooltip(caption);
        addAndMakeVisible(slider);
        configureLabel(label, caption, 10.5f);
        addAndMakeVisible(label);
    };

    setupSmall(column.levelSlider, column.levelLabel, "voiceLevel" + index, "LEVEL");
    column.levelSlider.setSliderStyle(juce::Slider::LinearVertical);
    column.levelSlider.setColour(juce::Slider::trackColourId, kGoldDim);
    column.levelSlider.setColour(juce::Slider::thumbColourId, kGold);
    setupSmall(column.panSlider, column.panLabel, "voicePan" + index, "PAN");
    setupSmall(column.formantSlider, column.formantLabel, "voiceFormant" + index, "FoЯmant");
    setupSmall(column.fineTuneSlider, column.fineTuneLabel, "voiceFineTune" + index, "FINE");
    setupSmall(column.toneSlider, column.toneLabel, "voiceTone" + index, "TONE");
    setupSmall(column.saturationSlider, column.saturationLabel, "voiceSaturation" + index, "SAT");
    setupSmall(column.microDelaySlider, column.microDelayLabel, "voiceMicroDelay" + index, "DELAY");
    setupSmall(column.vibratoSlider, column.vibratoLabel, "voiceVibrato" + index, "VIBЯATO");
    setupSmall(column.vibratoRateSlider, column.vibratoRateLabel, "voiceVibratoRate" + index, "VIB. ЯATE");

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

    // A preset is self-contained: it resets musical context as well as
    // visible controls, so it recalls the same sound in every session.
    setChoice("mode", 0, 2);
    setChoice("rootNote", 0, 12);
    setChoice("scaleType", 1, 3);
    set("freeze", 0.0f);
    set("dryPan", 0.0f); set("dryFormant", 0.0f); set("dryPitch", 0.0f); set("dryWidth", 0.5f);
    set("midiVelocity", 0.0f); set("globalSaturation", 0.04f);
    setChoice("vocalRange", 0, 6);
    setChoice("harmonyStyle", 1, 4);
    setChoice("midiVoicing", 1, 3);
    setChoice("midiInversion", 0, 5);

    switch (presetIndex)
    {
        case 1:
            setChoice("harmonyStyle", 0, 4);
            set("humanize", 0.10f); set("character", 0.0f); set("spread", 0.5f); set("ambience", 0.16f);
            set("dry", 0.18f); set("harmony", 0.78f); set("tracking", 0.72f); set("glide", 0.48f);
            voice(0, true, 0, 0.70f, -0.22f); voice(1, true, 3, 0.56f, 0.22f); voice(2, true, 7, 0.42f, -0.45f); voice(3, true, 13, 0.30f, 0.45f);
            advanced(0, -0.08f, -4.0f, 0.14f, 0.02f, 0.0f, 0.00f, 0.42f);
            advanced(1, -0.14f, 5.0f, 0.20f, 0.03f, 1.5f, 0.02f, 0.47f);
            advanced(2, -0.20f, -7.0f, 0.30f, 0.03f, 3.0f, 0.01f, 0.39f);
            advanced(3, 0.10f, 8.0f, 0.38f, 0.02f, 5.0f, 0.01f, 0.53f);
            break;
        case 2:
            setChoice("harmonyStyle", 2, 4);
            set("humanize", 0.32f); set("character", 0.08f); set("spread", 0.86f); set("ambience", 0.25f);
            set("dry", 0.32f); set("harmony", 0.72f); set("tracking", 0.64f); set("glide", 0.44f);
            voice(0, true, 3, 0.64f, -0.62f); voice(1, true, 7, 0.58f, 0.58f); voice(2, true, 14, 0.38f, -0.18f); voice(3, true, 4, 0.28f, 0.30f);
            advanced(0, -0.12f, -5.0f, 0.16f, 0.03f, 0.0f, 0.03f, 0.38f);
            advanced(1, -0.18f, 6.0f, 0.24f, 0.04f, 2.0f, 0.02f, 0.46f);
            advanced(2, -0.28f, -9.0f, 0.36f, 0.04f, 4.0f, 0.03f, 0.34f);
            advanced(3, 0.05f, 9.0f, 0.42f, 0.02f, 6.0f, 0.02f, 0.51f);
            break;
        case 3:
            setChoice("harmonyStyle", 3, 4);
            set("humanize", 0.24f); set("character", 0.10f); set("spread", 0.95f); set("ambience", 0.30f);
            set("dry", 0.42f); set("harmony", 0.76f); set("tracking", 0.58f); set("glide", 0.38f);
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
    currentPage = pageIndex;
    mainPageButton.setToggleState(currentPage == 0, juce::dontSendNotification);
    harmonyPageButton.setToggleState(currentPage == 1, juce::dontSendNotification);
    updateControlVisibility();
    resized();
    repaint();
}

void MirrorAudioProcessorEditor::updateControlVisibility()
{
    const bool main = currentPage == 0;
    const bool harmony = currentPage == 1;
    const int mode = (int) *audioProcessor.apvts.getRawParameterValue("mode");

    advancedButton.setVisible(harmony);
    // The visual design keeps the header as sparse as the reference panels.
    // These remain fully automatable host parameters rather than competing
    // with the Key/Scale/MIDI controls in the compact front panel.
    vocalRangeBox.setVisible(false);
    vocalRangeLabel.setVisible(false);
    harmonyStyleBox.setVisible(false);
    harmonyStyleLabel.setVisible(false);
    presetLabel.setVisible(false);
    midiVoicingBox.setVisible(mode == 1);
    midiVoicingLabel.setVisible(mode == 1);
    midiInversionBox.setVisible(mode == 1);
    midiInversionLabel.setVisible(mode == 1);

    auto showKnob = [main](KnobWithLabel& knob)
    {
        knob.slider.setVisible(main);
        knob.label.setVisible(main);
    };
    for (auto* knob : { &trackingKnob, &glideKnob, &dryLevelKnob, &dryPanKnob, &dryFormantKnob,
                        &dryPitchKnob, &dryWidthKnob, &humanizeKnob, &characterKnob, &spreadKnob,
                        &ambienceKnob, &harmonyMixKnob, &globalSaturationKnob, &outputGainKnob })
        showKnob(*knob);
    freezeButton.setVisible(main);

    for (auto& column : voiceColumns)
    {
        column.title.setVisible(harmony);
        column.enableButton.setVisible(harmony);
        column.soloButton.setVisible(false);
        // Interval is a primary musical choice, not a hidden host-only
        // parameter.  It is placed discretely inside every voice panel.
        column.intervalBox.setVisible(harmony);
        for (auto* component : { (juce::Component*) &column.levelSlider, (juce::Component*) &column.levelLabel,
                                 (juce::Component*) &column.panSlider, (juce::Component*) &column.panLabel,
                                 (juce::Component*) &column.formantSlider, (juce::Component*) &column.formantLabel })
            component->setVisible(harmony);
        for (auto* component : { (juce::Component*) &column.fineTuneSlider, (juce::Component*) &column.fineTuneLabel,
                                 (juce::Component*) &column.toneSlider, (juce::Component*) &column.toneLabel,
                                 (juce::Component*) &column.saturationSlider, (juce::Component*) &column.saturationLabel,
                                 (juce::Component*) &column.microDelaySlider, (juce::Component*) &column.microDelayLabel,
                                 (juce::Component*) &column.vibratoSlider, (juce::Component*) &column.vibratoLabel,
                                 (juce::Component*) &column.vibratoRateSlider, (juce::Component*) &column.vibratoRateLabel })
            component->setVisible(harmony && showAdvanced);
    }
}

void MirrorAudioProcessorEditor::placeKnob(KnobWithLabel& knob, juce::Rectangle<int> bounds)
{
    knob.label.setBounds(bounds.removeFromTop(17));
    knob.slider.setBounds(bounds.reduced(2, 0));
}

void MirrorAudioProcessorEditor::placeSmallKnob(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds)
{
    label.setBounds(bounds.removeFromTop(14));
    slider.setBounds(bounds.reduced(1, 0));
}

void MirrorAudioProcessorEditor::resized()
{
    constexpr float referenceWidth = 1723.0f;
    constexpr float referenceHeight = 913.0f;
    const float sx = (float) getWidth() / referenceWidth;
    const float sy = (float) getHeight() / referenceHeight;
    const auto rect = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(
            juce::roundToInt(x * sx), juce::roundToInt(y * sy),
            juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };

    const bool midiMode = (int) *audioProcessor.apvts.getRawParameterValue("mode") == 1;
    titleLabel.setBounds(rect(50, 30, 370, 84));
    creditLabel.setBounds(rect(50, 118, 310, 30));
    presetLabel.setBounds(0, -30, 1, 1);
    presetBox.setBounds(rect(50, 183, 230, 47));

    modeLabel.setBounds(rect(775, 26, 220, 32));
    modeBox.setBounds(rect(774, 67, 220, 49));
    rootLabel.setBounds(rect(1248, 26, 210, 32));
    rootBox.setBounds(rect(1252, 67, 210, 49));
    scaleLabel.setBounds(rect(1490, 26, 210, 32));
    scaleBox.setBounds(rect(1493, 67, 210, 49));

    mainPageButton.setBounds(rect(624, 134, 240, 47));
    harmonyPageButton.setBounds(rect(868, 134, 242, 47));

    // MIDI options are only revealed after MIDI is explicitly selected, then
    // sit beneath the navigation without colliding with the reference layout.
    midiVoicingLabel.setBounds(rect(608, 191, 190, 22));
    midiVoicingBox.setBounds(rect(608, 214, 190, 34));
    midiInversionLabel.setBounds(rect(924, 191, 190, 22));
    midiInversionBox.setBounds(rect(924, 214, 190, 34));
    advancedButton.setBounds(rect(774, midiMode ? 260 : 191, 180, 39));

    vocalRangeLabel.setBounds(0, -30, 1, 1);
    vocalRangeBox.setBounds(0, -30, 1, 1);
    harmonyStyleLabel.setBounds(0, -30, 1, 1);
    harmonyStyleBox.setBounds(0, -30, 1, 1);

    const auto body = rect(0, midiMode ? 305 : 232, referenceWidth, referenceHeight - (midiMode ? 305 : 232));
    if (currentPage == 0)
        layoutMain(body);
    else
        layoutHarmony(body);
}
void MirrorAudioProcessorEditor::layoutMain(juce::Rectangle<int>)
{
    constexpr float referenceWidth = 1723.0f;
    constexpr float referenceHeight = 913.0f;
    const float sx = (float) getWidth() / referenceWidth;
    const float sy = (float) getHeight() / referenceHeight;
    const auto rect = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(
            juce::roundToInt(x * sx), juce::roundToInt(y * sy),
            juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };

    // Directly trace the three supplied Main reference panels.
    dryPanelBounds = rect(58, 273, 420, 580);
    characterPanelBounds = rect(1264, 273, 406, 580);
    mirrorBounds = rect(487, 212, 770, 257);
    outputPanelBounds = rect(632, 495, 530, 350);

    auto dry = dryPanelBounds.reduced(45, 116);
    auto top = dry.removeFromTop(dry.getHeight() / 3);
    auto leftTop = top.removeFromLeft(top.getWidth() / 2);
    placeKnob(dryLevelKnob, leftTop);
    placeKnob(dryPitchKnob, top);
    auto middle = dry.removeFromTop(dry.getHeight() / 2);
    placeKnob(dryPanKnob, middle.reduced(middle.getWidth() / 4, 0));
    auto bottom = dry;
    auto leftBottom = bottom.removeFromLeft(bottom.getWidth() / 2);
    placeKnob(dryWidthKnob, leftBottom);
    placeKnob(dryFormantKnob, bottom);

    auto character = characterPanelBounds.reduced(44, 116);
    const int rowH = character.getHeight() / 3;
    auto row1 = character.removeFromTop(rowH);
    auto row2 = character.removeFromTop(rowH);
    auto row3 = character;
    auto placePair = [this](KnobWithLabel& a, KnobWithLabel& b, juce::Rectangle<int> row)
    {
        auto left = row.removeFromLeft(row.getWidth() / 2);
        placeKnob(a, left);
        placeKnob(b, row);
    };
    placePair(trackingKnob, glideKnob, row1);
    placePair(humanizeKnob, characterKnob, row2);
    placePair(ambienceKnob, spreadKnob, row3);
    freezeButton.setBounds(rect(1370, 802, 182, 28));

    auto output = outputPanelBounds.reduced(62, 86);
    auto knobs = output.removeFromTop(112);
    auto glue = knobs.removeFromLeft(knobs.getWidth() / 2);
    placeKnob(globalSaturationKnob, glue.reduced(20, 0));
    placeKnob(outputGainKnob, knobs.reduced(20, 0));
    auto mix = output.reduced(8, 0);
    harmonyMixKnob.label.setBounds(mix.removeFromTop(24));
    harmonyMixKnob.slider.setBounds(mix.removeFromTop(42).reduced(6, 6));
}
void MirrorAudioProcessorEditor::layoutHarmony(juce::Rectangle<int>)
{
    constexpr float referenceWidth = 1730.0f;
    constexpr float referenceHeight = 909.0f;
    const float sx = (float) getWidth() / referenceWidth;
    const float sy = (float) getHeight() / referenceHeight;
    const auto rect = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(
            juce::roundToInt(x * sx), juce::roundToInt(y * sy),
            juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };

    const float panelY = showAdvanced ? 338.0f : 367.0f;
    const float panelH = showAdvanced ? 525.0f : 500.0f;

    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const float offset = (float) i * 410.0f;
        auto& column = voiceColumns[(size_t) i];

        column.enableButton.setBounds(rect(215 + offset, 257, 82, 30));
        column.title.setBounds(rect(138 + offset, 300, 270, 42));
        voiceLevelBounds[(size_t) i] = rect(62 + offset, panelY + 4, 30, panelH - 10);
        column.levelLabel.setBounds(rect(51 + offset, panelY - 22, 58, 18));
        column.levelSlider.setBounds(voiceLevelBounds[(size_t) i]);

        voicePanelBounds[(size_t) i] = rect(140 + offset, panelY, 264, panelH);
        column.soloButton.setBounds(-200, -200, 1, 1);

        // The interval is restored as a compact, readable musical selector.
        // In Advanced it sits above the rows; in the simple view it uses the
        // deliberately open space in the lower panel.
        const float intervalY = showAdvanced ? panelY + 42.0f : panelY + 235.0f;
        column.intervalBox.setBounds(rect(166 + offset, intervalY, 212, 32));

        auto inner = voicePanelBounds[(size_t) i].reduced(18, showAdvanced ? 82 : 58);
        auto placePair = [this](juce::Slider& a, juce::Label& aLabel, juce::Slider& b, juce::Label& bLabel,
                                juce::Rectangle<int> row)
        {
            auto left = row.removeFromLeft(row.getWidth() / 2);
            placeSmallKnob(a, aLabel, left);
            placeSmallKnob(b, bLabel, row);
        };

        if (showAdvanced)
        {
            const int rowHeight = inner.getHeight() / 4;
            auto row1 = inner.removeFromTop(rowHeight);
            auto row2 = inner.removeFromTop(rowHeight);
            auto row3 = inner.removeFromTop(rowHeight);
            auto row4 = inner;
            placePair(column.panSlider, column.panLabel, column.formantSlider, column.formantLabel, row1);
            placePair(column.fineTuneSlider, column.fineTuneLabel, column.toneSlider, column.toneLabel, row2);
            placePair(column.saturationSlider, column.saturationLabel, column.microDelaySlider, column.microDelayLabel, row3);
            placePair(column.vibratoSlider, column.vibratoLabel, column.vibratoRateSlider, column.vibratoRateLabel, row4);
        }
        else
        {
            auto top = inner.removeFromTop(130);
            placePair(column.panSlider, column.panLabel, column.formantSlider, column.formantLabel, top);
        }
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

    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const auto index = (size_t) i;
        const float pan = *audioProcessor.apvts.getRawParameterValue("voicePan" + juce::String(i + 1));
        const bool enabled = *audioProcessor.apvts.getRawParameterValue("voiceEnable" + juce::String(i + 1)) > 0.5f;
        const float targetX = 0.50f + juce::jlimit(-1.0f, 1.0f, pan) * 0.34f;
        const float targetEnergy = enabled ? audioProcessor.currentVoiceVisualLevels[index].load() : 0.0f;
        const float targetPresence = enabled ? 1.0f : 0.0f;
        visualPan[index] += (targetX - visualPan[index]) * 0.11f;
        visualEnergy[index] += (targetEnergy - visualEnergy[index]) * 0.18f;
        visualPresence[index] += (targetPresence - visualPresence[index]) * (enabled ? 0.16f : 0.055f);
    }
    repaint();
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

    const auto& texture = engravedBackgroundTexture();
    if (texture.isValid())
    {
        g.setOpacity(0.68f);
        g.drawImageWithin(texture, 0, 0, getWidth(), getHeight(),
                          juce::RectanglePlacement::stretchToFit, false);
        g.setOpacity(1.0f);
    }

    // A restrained vignette preserves the engraved depth while keeping
    // foreground controls legible at every supported UI scale.
    juce::ColourGradient vignette(juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getCentreY(),
                                  juce::Colours::black.withAlpha(0.58f), bounds.getRight(), bounds.getBottom(), true);
    g.setGradientFill(vignette);
    g.fillRect(bounds);

    g.setColour(kGoldDim.withAlpha(0.58f));
    g.drawRoundedRectangle(bounds.reduced(4.0f), 5.0f, 1.0f);
    g.setColour(kGold.withAlpha(0.20f));
    g.drawRoundedRectangle(bounds.reduced(8.0f), 3.0f, 0.65f);
}
void MirrorAudioProcessorEditor::drawHeader(juce::Graphics& g) const
{
    constexpr float referenceWidth = 1723.0f;
    constexpr float referenceHeight = 913.0f;
    const float sx = (float) getWidth() / referenceWidth;
    const float sy = (float) getHeight() / referenceHeight;
    const bool midiMode = (int) *audioProcessor.apvts.getRawParameterValue("mode") == 1;
    const float activeX = currentPage == 0 ? 635.0f : 879.0f;
    const float headerY = midiMode ? 298.0f : 228.0f;

    g.setColour(kGoldDim.withAlpha(0.36f));
    g.drawLine(28.0f * sx, headerY * sy, (referenceWidth - 28.0f) * sx, headerY * sy, 0.75f * sx);
    g.setColour(kPurple.withAlpha(0.78f));
    g.fillRoundedRectangle(activeX * sx, 176.0f * sy, 218.0f * sx, 1.7f * sy, 1.0f);
}
void MirrorAudioProcessorEditor::drawSectionTitle(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> bounds) const
{
    const float scale = (float) getWidth() / 1723.0f;
    g.setFont(displayFont(37.0f * scale));
    g.setColour(kText.withAlpha(0.86f));
    g.drawText(text, bounds.withHeight(72), juce::Justification::centred);
    g.setFont(displayFont(31.0f * scale));
    g.setColour(kText.withAlpha(0.09f));
    g.drawText(text, bounds.withHeight(72).translated(0, (int) (28.0f * scale)),
               juce::Justification::centred);
}
void MirrorAudioProcessorEditor::drawMirrorPanel(juce::Graphics& g, juce::Rectangle<float> bounds, bool enabled, bool ornate) const
{
    auto outer = bounds.reduced(2.0f);
    const float left = outer.getX(), right = outer.getRight();
    const float top = outer.getY(), bottom = outer.getBottom();
    const float height = outer.getHeight();

    // The silhouette deliberately has the shoulder and right-hand notch of
    // the reference "mirror-glass" frames rather than a generic rounded box.
    juce::Path frame;
    frame.startNewSubPath(left + 31.0f, top);
    frame.lineTo(right - 35.0f, top);
    frame.cubicTo(right - 17.0f, top, right - 11.0f, top + 14.0f, right - 11.0f, top + 28.0f);
    frame.lineTo(right - 11.0f, top + height * 0.35f);
    frame.cubicTo(right + 5.0f, top + height * 0.40f, right + 5.0f, top + height * 0.46f,
                  right - 4.0f, top + height * 0.50f);
    frame.cubicTo(right + 5.0f, top + height * 0.54f, right + 5.0f, top + height * 0.60f,
                  right - 11.0f, top + height * 0.65f);
    frame.lineTo(right - 11.0f, bottom - 28.0f);
    frame.cubicTo(right - 11.0f, bottom - 14.0f, right - 17.0f, bottom, right - 35.0f, bottom);
    frame.lineTo(left + 31.0f, bottom);
    frame.cubicTo(left + 13.0f, bottom, left + 7.0f, bottom - 14.0f, left + 7.0f, bottom - 31.0f);
    frame.lineTo(left + 7.0f, top + 31.0f);
    frame.cubicTo(left + 7.0f, top + 14.0f, left + 13.0f, top, left + 31.0f, top);
    frame.closeSubPath();

    g.setColour(juce::Colours::black.withAlpha(enabled ? 0.76f : 0.87f));
    g.fillPath(frame);
    g.setColour(kGoldDim.withAlpha(enabled ? 0.90f : 0.42f));
    g.strokePath(frame, juce::PathStrokeType(2.0f));
    g.setColour(kGold.withAlpha(enabled ? 0.60f : 0.22f));
    g.strokePath(frame, juce::PathStrokeType(0.8f));

    auto glass = outer.reduced(9.0f);
    g.setColour(kPurple.withAlpha(enabled ? 0.085f : 0.018f));
    g.fillRoundedRectangle(glass, 17.0f);
    g.setColour(kGoldDim.withAlpha(enabled ? 0.28f : 0.11f));
    g.drawRoundedRectangle(glass, 17.0f, 0.7f);

    if (ornate)
    {
        const auto centre = outer.getCentreX();
        const auto crestY = top + 17.0f;
        g.setColour(kGold.withAlpha(enabled ? 0.58f : 0.22f));
        g.drawEllipse(centre - 9.0f, crestY - 9.0f, 18.0f, 18.0f, 0.9f);
        g.drawLine(centre - 16.0f, crestY, centre + 16.0f, crestY, 0.7f);
        g.drawLine(centre, crestY - 16.0f, centre, crestY + 16.0f, 0.7f);
        for (auto point : { outer.getTopLeft(), outer.getTopRight(), outer.getBottomLeft(), outer.getBottomRight() })
            g.fillEllipse(point.x - 1.8f, point.y - 1.8f, 3.6f, 3.6f);
    }
}

void MirrorAudioProcessorEditor::drawMirrorVisualizer(juce::Graphics& g) const
{
    if (mirrorBounds.isEmpty())
        return;

    auto bounds = mirrorBounds.toFloat().reduced(4.0f);
    juce::Path mirror;
    mirror.startNewSubPath(bounds.getX() + bounds.getWidth() * 0.05f, bounds.getCentreY());
    mirror.cubicTo(bounds.getX() + bounds.getWidth() * 0.13f, bounds.getY() + 4.0f,
                   bounds.getX() + bounds.getWidth() * 0.27f, bounds.getY() + 12.0f,
                   bounds.getCentreX(), bounds.getY() + 5.0f);
    mirror.cubicTo(bounds.getX() + bounds.getWidth() * 0.70f, bounds.getY() - 2.0f,
                   bounds.getX() + bounds.getWidth() * 0.85f, bounds.getY() + 30.0f,
                   bounds.getRight() - bounds.getWidth() * 0.05f, bounds.getCentreY());
    mirror.cubicTo(bounds.getX() + bounds.getWidth() * 0.87f, bounds.getBottom() - 5.0f,
                   bounds.getX() + bounds.getWidth() * 0.70f, bounds.getBottom() - 13.0f,
                   bounds.getCentreX(), bounds.getBottom() - 7.0f);
    mirror.cubicTo(bounds.getX() + bounds.getWidth() * 0.28f, bounds.getBottom() + 3.0f,
                   bounds.getX() + bounds.getWidth() * 0.10f, bounds.getBottom() - 27.0f,
                   bounds.getX() + bounds.getWidth() * 0.05f, bounds.getCentreY());
    mirror.closeSubPath();

    juce::ColourGradient glass(juce::Colour(0xff1d1929), bounds.getCentreX(), bounds.getY(),
                                juce::Colour(0xff07070a), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(glass);
    g.fillPath(mirror);
    g.setColour(kGold.withAlpha(0.78f));
    g.strokePath(mirror, juce::PathStrokeType(2.0f));
    g.setColour(kPurple.withAlpha(0.28f));
    g.strokePath(mirror, juce::PathStrokeType(5.0f));

    g.saveState();
    g.reduceClipRegion(mirror);
    const float midY = bounds.getCentreY() + 28.0f;
    for (int line = 0; line < 3; ++line)
    {
        juce::Path wave;
        const float offset = (float) line * 13.0f;
        wave.startNewSubPath(bounds.getX(), midY + offset);
        for (int x = 0; x <= (int) bounds.getWidth(); x += 8)
        {
            const float pulse = 5.0f + 8.0f * (visualEnergy[0] + visualEnergy[1] + visualEnergy[2] + visualEnergy[3]) * 0.25f;
            const float y = midY + offset + std::sin((float) x * 0.045f + offset) * pulse;
            wave.lineTo(bounds.getX() + (float) x, y);
        }
        g.setColour(kPurple.withAlpha(0.08f + 0.035f * (float) line));
        g.strokePath(wave, juce::PathStrokeType(1.0f));
    }

    const float ground = bounds.getBottom() - 25.0f;
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const float x = bounds.getX() + bounds.getWidth() * visualPan[(size_t) i];
        drawVoiceGlyph(g, i, x, ground, visualEnergy[(size_t) i], visualPresence[(size_t) i]);
    }
    g.restoreState();
}

void MirrorAudioProcessorEditor::drawVoiceGlyph(juce::Graphics& g, int voiceIndex, float x, float groundY,
                                                float energy, float alpha) const
{
    if (alpha < 0.015f)
        return;

    const float breath = std::sin((float) juce::Time::getMillisecondCounterHiRes() * 0.0018f + (float) voiceIndex) * (1.5f + energy * 2.5f);
    const float height = 58.0f + (float) voiceIndex * 4.0f + energy * 14.0f;
    const float top = groundY - height + breath;
    const float glow = juce::jlimit(0.0f, 1.0f, alpha * (0.34f + energy * 0.66f));

    for (int ring = 0; ring < 2; ++ring)
    {
        const float scale = 1.0f + energy * (0.33f + 0.18f * (float) ring);
        auto ringBounds = juce::Rectangle<float>(x - 23.0f * scale, groundY - 5.0f * scale,
                                                  46.0f * scale, 10.0f * scale);
        g.setColour(kPurple.withAlpha(glow * (0.20f - ring * 0.06f)));
        g.drawEllipse(ringBounds, 1.1f);
    }

    g.setColour(kPurple.withAlpha(glow * 0.34f));
    g.fillEllipse(x - 19.0f - energy * 6.0f, top + 10.0f, 38.0f + energy * 12.0f, height * 0.82f);

    juce::Path glyph;
    const float bodyTop = top + 15.0f;
    const float shoulder = 10.0f + (float) voiceIndex * 1.6f;
    glyph.startNewSubPath(x, top + 8.0f);
    glyph.addEllipse(x - 6.0f, top, 12.0f, 13.0f);

    if (voiceIndex == 0)
    {
        glyph.startNewSubPath(x - shoulder, bodyTop + 10.0f);
        glyph.cubicTo(x - 7.0f, bodyTop - 3.0f, x + 7.0f, bodyTop - 3.0f, x + shoulder, bodyTop + 10.0f);
        glyph.cubicTo(x + 8.0f, groundY - 12.0f, x + 5.0f, groundY - 7.0f, x, groundY);
        glyph.cubicTo(x - 5.0f, groundY - 7.0f, x - 8.0f, groundY - 12.0f, x - shoulder, bodyTop + 10.0f);
    }
    else if (voiceIndex == 1)
    {
        glyph.startNewSubPath(x - shoulder - 4.0f, bodyTop + 13.0f);
        glyph.cubicTo(x - 12.0f, bodyTop - 3.0f, x - 2.0f, bodyTop + 2.0f, x, bodyTop + 12.0f);
        glyph.cubicTo(x + 2.0f, bodyTop + 2.0f, x + 12.0f, bodyTop - 3.0f, x + shoulder + 4.0f, bodyTop + 13.0f);
        glyph.lineTo(x + 7.0f, groundY - 8.0f);
        glyph.lineTo(x, groundY);
        glyph.lineTo(x - 7.0f, groundY - 8.0f);
        glyph.closeSubPath();
    }
    else if (voiceIndex == 2)
    {
        glyph.startNewSubPath(x - shoulder, bodyTop + 10.0f);
        glyph.lineTo(x - 8.0f, groundY - 8.0f);
        glyph.lineTo(x, groundY);
        glyph.lineTo(x + 8.0f, groundY - 8.0f);
        glyph.lineTo(x + shoulder, bodyTop + 10.0f);
        glyph.lineTo(x + 5.0f, bodyTop - 2.0f);
        glyph.lineTo(x - 5.0f, bodyTop - 2.0f);
        glyph.closeSubPath();
        g.setColour(kGold.withAlpha(alpha * 0.75f));
        juce::Path crescent;
        crescent.addArc(x - 9.0f, top - 5.0f, 18.0f, 18.0f, 0.2f, 2.45f, true);
        g.strokePath(crescent, juce::PathStrokeType(1.0f));
    }
    else
    {
        glyph.startNewSubPath(x - shoulder + 2.0f, bodyTop + 12.0f);
        glyph.cubicTo(x - 5.0f, bodyTop - 4.0f, x + 5.0f, bodyTop - 4.0f, x + shoulder - 2.0f, bodyTop + 12.0f);
        glyph.cubicTo(x + 7.0f, groundY - 10.0f, x + 5.0f, groundY - 4.0f, x, groundY);
        glyph.cubicTo(x - 5.0f, groundY - 4.0f, x - 7.0f, groundY - 10.0f, x - shoulder + 2.0f, bodyTop + 12.0f);
        for (int outline = 1; outline <= 2; ++outline)
        {
            g.setColour(kPurple.withAlpha(alpha * 0.16f / (float) outline));
            g.drawEllipse(x - 12.0f - outline * 4.0f, top + 7.0f - outline * 2.0f,
                          24.0f + outline * 8.0f, height * 0.76f + outline * 4.0f, 0.8f);
        }
    }

    g.setColour(kGold.withAlpha(alpha * (0.55f + energy * 0.40f)));
    g.strokePath(glyph, juce::PathStrokeType(1.55f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(kText.withAlpha(alpha * 0.46f));
    g.fillEllipse(x - 2.0f, top + 4.0f, 4.0f, 4.0f);
}

void MirrorAudioProcessorEditor::drawMainPanels(juce::Graphics& g) const
{
    drawMirrorPanel(g, dryPanelBounds.toFloat(), true);
    drawMirrorPanel(g, characterPanelBounds.toFloat(), true);
    drawMirrorPanel(g, outputPanelBounds.toFloat(), true);
    drawSectionTitle(g, "DЯY", dryPanelBounds.withHeight(58));
    drawSectionTitle(g, "CHAЯACTEЯ", characterPanelBounds.withHeight(58));
    drawSectionTitle(g, "OUTPUT", outputPanelBounds.withHeight(58));
    drawMirrorVisualizer(g);
}

void MirrorAudioProcessorEditor::drawHarmonyPanels(juce::Graphics& g) const
{
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const bool enabled = *audioProcessor.apvts.getRawParameterValue("voiceEnable" + juce::String(i + 1)) > 0.5f;
        drawMirrorPanel(g, voicePanelBounds[(size_t) i].toFloat(), enabled);
        g.setColour(kGoldDim.withAlpha(enabled ? 0.72f : 0.26f));
        auto levelTrack = voiceLevelBounds[(size_t) i].toFloat().reduced(12.0f, 2.0f);
        g.drawRoundedRectangle(levelTrack, 3.0f, 0.8f);
        if (enabled)
        {
            g.setColour(kPurple.withAlpha(0.24f));
            g.fillRoundedRectangle(voicePanelBounds[(size_t) i].toFloat().reduced(8.0f), 18.0f);
        }
    }
}
