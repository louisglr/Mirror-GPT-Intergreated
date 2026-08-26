#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    juce::Font displayFont(float size, int style = juce::Font::plain)
    {
        return juce::Font("Georgia", size, style);
    }

    juce::String mirrorText(juce::String text)
    {
        return text.replace("R", juce::String::fromUTF8("Я"))
                   .replace("r", juce::String::fromUTF8("Я"));
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
            g.drawLine(centre, dot, 2.4f);
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
    setResizeLimits(980, 620, 1680, 1040);

    configureLabel(titleLabel, "MIЯЯOЯ", 42.0f, false);
    titleLabel.setFont(displayFont(42.0f));
    titleLabel.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(titleLabel);

    configureLabel(creditLabel, "By Lou!s GabЯ!el", 14.0f, false);
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
    mainPageButton.setButtonText("MAIN");
    harmonyPageButton.setButtonText("HaЯmony");
    advancedButton.setButtonText("ADVANCED");
    mainPageButton.setRadioGroupId(8001);
    harmonyPageButton.setRadioGroupId(8001);
    mainPageButton.onClick = [this] { showPage(0); };
    harmonyPageButton.onClick = [this] { showPage(1); };
    advancedButton.onClick = [this]
    {
        showAdvanced = advancedButton.getToggleState();
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
    scaleBox.addItemList({ "ChЯomatic", "MajoЯ", "MinoЯ" }, 1);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "scaleType", scaleBox);
    configureLabel(scaleLabel, "SCALE", 15.0f);
    addAndMakeVisible(scaleLabel);

    configureComboBox(vocalRangeBox, "Vokalområde for pitch-tracking");
    vocalRangeBox.addItemList({ "Auto", "Bass", "BaЯitone", "TenoЯ", "Alto", "SopЯano" }, 1);
    vocalRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "vocalRange", vocalRangeBox);
    configureLabel(vocalRangeLabel, "VOICE ЯANGE", 10.5f);
    addAndMakeVisible(vocalRangeLabel);

    configureComboBox(harmonyStyleBox, "Musikalsk harmoni-stil");
    harmonyStyleBox.addItemList({ "Tight", "NatuЯal", "Wide", "ChoiЯ" }, 1);
    harmonyStyleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "harmonyStyle", harmonyStyleBox);
    configureLabel(harmonyStyleLabel, "STYLE", 10.5f);
    addAndMakeVisible(harmonyStyleLabel);

    configureComboBox(midiVoicingBox, "Fordeling af MIDI-akkorden");
    midiVoicingBox.addItemList({ "Close", "Open", "Wide" }, 1);
    midiVoicingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "midiVoicing", midiVoicingBox);
    configureLabel(midiVoicingLabel, "MIDI VOICING", 10.5f);
    addAndMakeVisible(midiVoicingLabel);

    configureComboBox(midiInversionBox, "MIDI-inversion");
    midiInversionBox.addItemList({ "Auto", "Яoot", "1st", "2nd", "3Яd" }, 1);
    midiInversionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "midiInversion", midiInversionBox);
    configureLabel(midiInversionLabel, "INVEЯSION", 10.5f);
    addAndMakeVisible(midiInversionLabel);

    configureComboBox(presetBox, "PЯeset");
    presetBox.addItem("VocodeЯ Glass", 1);
    presetBox.addItem("FЯactuЯed Stack", 2);
    presetBox.addItem("Stadium ChoiЯ", 3);
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

    modeBox.onChange = [this] { updateControlVisibility(); };
    mainPageButton.setToggleState(true, juce::dontSendNotification);
    setSize(1220, 760);
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
    auto intervals = getIntervalNames();
    for (auto& item : intervals)
        item = mirrorText(item);
    column.intervalBox.addItemList(intervals, 1);
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
    vocalRangeBox.setVisible(true);
    vocalRangeLabel.setVisible(true);
    harmonyStyleBox.setVisible(true);
    harmonyStyleLabel.setVisible(true);
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
                        &ambienceKnob, &harmonyMixKnob, &globalSaturationKnob })
        showKnob(*knob);
    freezeButton.setVisible(main);

    for (auto& column : voiceColumns)
    {
        column.title.setVisible(harmony);
        column.enableButton.setVisible(harmony);
        column.soloButton.setVisible(harmony);
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
    auto area = getLocalBounds().reduced(24);
    const int width = getWidth();

    titleLabel.setBounds(0, 23, juce::jmin(305, width / 3), 58);
    creditLabel.setBounds(4, 82, 280, 24);
    presetLabel.setBounds(5, 118, 85, 18);
    presetBox.setBounds(5, 137, 190, 33);

    const int controlW = 170;
    const int modeX = width / 2 - controlW / 2;
    modeLabel.setBounds(modeX, 20, controlW, 20);
    modeBox.setBounds(modeX, 42, controlW, 34);

    rootLabel.setBounds(width - 314, 20, 116, 20);
    rootBox.setBounds(width - 314, 42, 116, 34);
    scaleLabel.setBounds(width - 184, 20, 150, 20);
    scaleBox.setBounds(width - 184, 42, 150, 34);

    mainPageButton.setBounds(width / 2 - 170, 92, 168, 36);
    harmonyPageButton.setBounds(width / 2 + 2, 92, 168, 36);
    advancedButton.setBounds(width / 2 - 85, 133, 170, 31);

    const int compactY = 142;
    vocalRangeLabel.setBounds(width - 314, 112, 116, 16);
    vocalRangeBox.setBounds(width - 314, 128, 116, 27);
    harmonyStyleLabel.setBounds(width - 184, 112, 150, 16);
    harmonyStyleBox.setBounds(width - 184, 128, 150, 27);
    midiVoicingLabel.setBounds(modeX - 96, 142, 92, 16);
    midiVoicingBox.setBounds(modeX - 96, 158, 92, 27);
    midiInversionLabel.setBounds(modeX + controlW + 4, 142, 92, 16);
    midiInversionBox.setBounds(modeX + controlW + 4, 158, 92, 27);
    juce::ignoreUnused(compactY);

    auto body = area.withTop(180).reduced(0, 4);
    if (currentPage == 0)
        layoutMain(body);
    else
        layoutHarmony(body);
}

void MirrorAudioProcessorEditor::layoutMain(juce::Rectangle<int> body)
{
    const int sideWidth = juce::jlimit(220, 300, body.getWidth() / 4);
    dryPanelBounds = body.removeFromLeft(sideWidth).reduced(4);
    characterPanelBounds = body.removeFromRight(sideWidth).reduced(4);
    auto centre = body.reduced(16, 0);
    const int outputHeight = juce::jmin(205, centre.getHeight() / 3);
    outputPanelBounds = centre.removeFromBottom(outputHeight).reduced(16, 0);
    mirrorBounds = centre.reduced(0, 8);

    auto dry = dryPanelBounds.reduced(21, 67);
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

    auto character = characterPanelBounds.reduced(20, 67);
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
    freezeButton.setBounds(characterPanelBounds.getCentreX() - 48, characterPanelBounds.getBottom() - 31, 96, 26);

    auto output = outputPanelBounds.reduced(38, 60);
    auto knobs = output.removeFromTop(90);
    auto glue = knobs.removeFromLeft(knobs.getWidth() / 2);
    placeKnob(globalSaturationKnob, glue.reduced(25, 0));
    auto mix = output.reduced(10, 0);
    harmonyMixKnob.label.setBounds(mix.removeFromTop(19));
    harmonyMixKnob.slider.setBounds(mix.removeFromTop(36).reduced(5, 5));
}

void MirrorAudioProcessorEditor::layoutHarmony(juce::Rectangle<int> body)
{
    auto area = body.reduced(18, 10);
    const int columnWidth = area.getWidth() / kNumHarmonyVoices;
    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        auto columnBounds = area.withX(area.getX() + columnWidth * i).withWidth(columnWidth).reduced(8, 0);
        auto& column = voiceColumns[(size_t) i];

        column.enableButton.setBounds(columnBounds.getX() + 2, columnBounds.getY() + 2, 65, 28);
        column.soloButton.setBounds(columnBounds.getRight() - 65, columnBounds.getY() + 2, 63, 28);
        column.title.setBounds(columnBounds.getX(), columnBounds.getY() + 28, columnBounds.getWidth(), 31);
        column.intervalBox.setBounds(columnBounds.getX() + 44, columnBounds.getY() + 61, columnBounds.getWidth() - 88, 30);

        voiceLevelBounds[(size_t) i] = { columnBounds.getX(), columnBounds.getY() + 105, 30, columnBounds.getHeight() - 120 };
        column.levelLabel.setBounds(voiceLevelBounds[(size_t) i].getX() - 7, voiceLevelBounds[(size_t) i].getY() - 24, 44, 18);
        column.levelSlider.setBounds(voiceLevelBounds[(size_t) i]);

        voicePanelBounds[(size_t) i] = { columnBounds.getX() + 42, columnBounds.getY() + 100,
                                          columnBounds.getWidth() - 46, columnBounds.getHeight() - 110 };
        auto inner = voicePanelBounds[(size_t) i].reduced(16, 58);

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
            auto top = inner.removeFromTop(100);
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
    juce::ColourGradient gradient(juce::Colour(0xff171319), bounds.getCentreX(), 0.0f,
                                  juce::Colour(0xff030304), bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillAll();

    juce::Random random(177013);
    g.setColour(kGoldDim.withAlpha(0.08f));
    for (int i = 0; i < 78; ++i)
    {
        const float x = random.nextFloat() * bounds.getWidth();
        const float y = random.nextFloat() * bounds.getHeight();
        const float size = 14.0f + random.nextFloat() * 52.0f;
        g.drawEllipse(x, y, size, size, 0.45f);
        if ((i % 3) == 0)
            g.drawLine(x - size * 0.6f, y, x + size * 0.6f, y, 0.35f);
    }

    g.setColour(juce::Colours::black.withAlpha(0.24f));
    for (int i = 0; i < 10; ++i)
        g.fillRect(0.0f, (float) i * bounds.getHeight() / 10.0f, bounds.getWidth(), 1.0f);

    g.setColour(kGoldDim.withAlpha(0.58f));
    g.drawRoundedRectangle(bounds.reduced(3.0f), 5.0f, 1.0f);
    g.setColour(kPurple.withAlpha(0.12f));
    g.drawRoundedRectangle(bounds.reduced(7.0f), 3.0f, 0.7f);
}

void MirrorAudioProcessorEditor::drawHeader(juce::Graphics& g) const
{
    g.setColour(kGoldDim.withAlpha(0.38f));
    g.drawLine(24.0f, 177.0f, (float) getWidth() - 24.0f, 177.0f, 0.75f);
    g.setColour(kPurple.withAlpha(0.30f));
    const float activeX = currentPage == 0 ? (float) getWidth() / 2.0f - 165.0f : (float) getWidth() / 2.0f + 7.0f;
    g.fillRoundedRectangle(activeX, 126.0f, 158.0f, 1.4f, 1.0f);
}

void MirrorAudioProcessorEditor::drawSectionTitle(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> bounds) const
{
    g.setFont(displayFont(28.0f));
    g.setColour(kText.withAlpha(0.90f));
    g.drawText(text, bounds, juce::Justification::centred);
    g.setFont(displayFont(24.0f));
    g.setColour(kText.withAlpha(0.075f));
    g.drawText(text, bounds.translated(0, 20), juce::Justification::centred);
}

void MirrorAudioProcessorEditor::drawMirrorPanel(juce::Graphics& g, juce::Rectangle<float> bounds, bool enabled, bool ornate) const
{
    auto outer = bounds.reduced(2.0f);
    const float corner = 24.0f;
    g.setColour(juce::Colours::black.withAlpha(enabled ? 0.72f : 0.84f));
    g.fillRoundedRectangle(outer, corner);
    g.setColour(kGoldDim.withAlpha(enabled ? 0.88f : 0.42f));
    g.drawRoundedRectangle(outer, corner, 2.0f);
    g.setColour(kGold.withAlpha(enabled ? 0.64f : 0.24f));
    g.drawRoundedRectangle(outer.reduced(5.0f), corner - 5.0f, 0.8f);
    g.setColour(kPurple.withAlpha(enabled ? 0.11f : 0.02f));
    g.fillRoundedRectangle(outer.reduced(8.0f), corner - 8.0f);

    if (ornate)
    {
        const auto top = outer.getCentreX();
        const auto y = outer.getY() + 18.0f;
        g.setColour(kGold.withAlpha(enabled ? 0.52f : 0.22f));
        g.drawEllipse(top - 8.0f, y - 8.0f, 16.0f, 16.0f, 0.9f);
        g.drawLine(top - 14.0f, y, top + 14.0f, y, 0.7f);
        g.drawLine(top, y - 14.0f, top, y + 14.0f, 0.7f);
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
        g.drawArc(x - 9.0f, top - 5.0f, 18.0f, 18.0f, 0.2f, 2.45f, 1.0f);
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
