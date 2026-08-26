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
        return juce::Font(juce::FontOptions("Times New Roman", size, style));
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

    const juce::Image& referenceMainImage()
    {
        static const auto image = juce::ImageFileFormat::loadFrom(
            BinaryData::mirror_reference_main_png,
            (size_t) BinaryData::mirror_reference_main_pngSize);
        return image;
    }

    const juce::Image& referenceHarmonyImage()
    {
        static const auto image = juce::ImageFileFormat::loadFrom(
            BinaryData::mirror_reference_harmony_png,
            (size_t) BinaryData::mirror_reference_harmony_pngSize);
        return image;
    }

    const juce::Image& referenceAdvancedImage()
    {
        static const auto image = juce::ImageFileFormat::loadFrom(
            BinaryData::mirror_reference_advanced_png,
            (size_t) BinaryData::mirror_reference_advanced_pngSize);
        return image;
    }

    bool isReferenceOverlay(const juce::Component& component)
    {
        return component.getComponentID() == "reference-overlay";
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
            const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height);
            g.setColour(juce::Colour(0xff09080b).withAlpha(0.985f));
            g.fillRoundedRectangle(bounds.reduced(1.0f), 6.0f);
            g.setColour(kGoldDim.withAlpha(0.96f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.0f);
            g.setColour(kPurple.withAlpha(0.18f));
            g.drawRoundedRectangle(bounds.reduced(4.0f), 4.0f, 0.65f);
        }

        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float position, float, float, juce::Slider& slider) override
        {
            auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
            const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.70f;
            bounds = bounds.withSizeKeepingCentre(diameter, diameter).reduced(1.0f);

            const auto centre = bounds.getCentre();
            const float radius = bounds.getWidth() * 0.5f;
            const bool active = slider.isMouseOverOrDragging();
            const float start = juce::MathConstants<float>::pi * 0.75f;
            const float zero = juce::MathConstants<float>::pi * 1.5f;
            const float end = juce::MathConstants<float>::pi * 2.25f;
            const float min = (float) slider.getMinimum();
            const float max = (float) slider.getMaximum();
            const float value = (float) slider.getValue();
            const bool bipolar = min < 0.0f && max > 0.0f;

            float angle = juce::jmap(position, start, end);
            if (bipolar)
            {
                angle = value <= 0.0f
                    ? juce::jmap(value, min, 0.0f, start, zero)
                    : juce::jmap(value, 0.0f, max, zero, end);
            }

            for (int tick = 0; tick < 17; ++tick)
            {
                const float phase = juce::jmap((float) tick, 0.0f, 16.0f, start, end);
                const auto dot = centre + juce::Point<float>(std::cos(phase), std::sin(phase)) * (radius * 1.10f);
                g.setColour(kGold.withAlpha(tick == 8 && bipolar ? 0.70f : 0.32f));
                g.fillEllipse(dot.x - 1.05f, dot.y - 1.05f, 2.1f, 2.1f);
            }

            g.setColour(juce::Colours::black.withAlpha(0.84f));
            g.fillEllipse(bounds.expanded(3.0f));
            g.setColour(kGoldDim.withAlpha(0.94f));
            g.drawEllipse(bounds.expanded(3.0f), 1.05f);

            juce::ColourGradient brass(juce::Colour(0xfff1d9a8), bounds.getX(), bounds.getY(),
                                       juce::Colour(0xff7d5536), bounds.getRight(), bounds.getBottom(), false);
            g.setGradientFill(brass);
            g.fillEllipse(bounds);
            g.setColour(juce::Colour(0xff2c1b14).withAlpha(0.88f));
            g.drawEllipse(bounds.reduced(1.6f), 1.1f);

            const auto inner = bounds.reduced(radius * 0.19f);
            juce::ColourGradient brush(juce::Colour(0xffffedc8).withAlpha(0.72f), inner.getTopLeft(),
                                       juce::Colour(0xff6c4329).withAlpha(0.92f), inner.getBottomRight(), false);
            g.setGradientFill(brush);
            g.fillEllipse(inner);
            g.setColour(kGold.withAlpha(0.55f));
            g.drawEllipse(inner, 0.7f);

            const auto startPoint = centre + juce::Point<float>(std::cos(angle), std::sin(angle)) * (radius * 0.10f);
            const auto endPoint = centre + juce::Point<float>(std::cos(angle), std::sin(angle)) * (radius * 0.67f);
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.drawLine(startPoint.x + 0.8f, startPoint.y + 1.3f, endPoint.x + 0.8f, endPoint.y + 1.3f, 3.2f);
            g.setColour(kPurple.brighter(active ? 0.30f : 0.08f));
            g.drawLine(startPoint.x, startPoint.y, endPoint.x, endPoint.y, active ? 2.5f : 2.0f);
            g.setColour(kText.withAlpha(0.92f));
            g.fillEllipse(endPoint.x - 1.4f, endPoint.y - 1.4f, 2.8f, 2.8f);
        }

        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, float, float,
                              const juce::Slider::SliderStyle style, juce::Slider&) override
        {
            if (style == juce::Slider::LinearHorizontal)
            {
                const auto track = juce::Rectangle<float>((float) x + 6.0f, (float) y + (float) height * 0.50f - 2.2f,
                                                          (float) width - 12.0f, 4.4f);
                g.setColour(juce::Colours::black.withAlpha(0.85f));
                g.fillRoundedRectangle(track.expanded(1.7f, 2.8f), 4.0f);
                g.setColour(kGoldDim.withAlpha(0.94f));
                g.drawRoundedRectangle(track.expanded(1.7f, 2.8f), 4.0f, 0.85f);
                const float fillWidth = juce::jlimit(0.0f, track.getWidth(), sliderPos - track.getX());
                g.setColour(kPurple.withAlpha(0.84f));
                g.fillRoundedRectangle(track.withWidth(fillWidth), 2.2f);
                const auto thumb = juce::Rectangle<float>(sliderPos - 10.0f, track.getCentreY() - 10.0f, 20.0f, 20.0f);
                juce::ColourGradient brass(juce::Colour(0xfff4dbac), thumb.getTopLeft(),
                                           juce::Colour(0xff765033), thumb.getBottomRight(), false);
                g.setGradientFill(brass);
                g.fillEllipse(thumb);
                g.setColour(kGold.withAlpha(0.88f));
                g.drawEllipse(thumb, 1.0f);
                return;
            }

            const auto track = juce::Rectangle<float>((float) x + (float) width * 0.50f - 2.1f, (float) y + 7.0f,
                                                      4.2f, (float) height - 14.0f);
            g.setColour(juce::Colours::black.withAlpha(0.84f));
            g.fillRoundedRectangle(track.expanded(2.6f, 1.7f), 4.0f);
            g.setColour(kGoldDim.withAlpha(0.90f));
            g.drawRoundedRectangle(track.expanded(2.6f, 1.7f), 4.0f, 0.8f);
            const float fillY = juce::jlimit(track.getY(), track.getBottom(), sliderPos);
            g.setColour(kPurple.withAlpha(0.80f));
            g.fillRoundedRectangle(juce::Rectangle<float>(track.getX(), fillY, track.getWidth(), track.getBottom() - fillY), 2.0f);
            const auto thumb = juce::Rectangle<float>((float) x + (float) width * 0.50f - 15.0f, sliderPos - 15.0f, 30.0f, 30.0f);
            juce::ColourGradient brass(juce::Colour(0xfff4dbac), thumb.getTopLeft(),
                                       juce::Colour(0xff745034), thumb.getBottomRight(), false);
            g.setGradientFill(brass);
            g.fillEllipse(thumb);
            g.setColour(kGold.withAlpha(0.92f));
            g.drawEllipse(thumb, 1.0f);
        }

        void drawComboBox(juce::Graphics& g, int width, int height, bool isDown,
                          int, int, int, int, juce::ComboBox& box) override
        {
            // At the reference defaults the supplied artwork is the exact
            // control face. Once a selection differs, draw a live face over it.
            if (isReferenceOverlay(box) && box.getAlpha() < 0.01f)
                return;

            const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(1.0f);
            g.setColour(juce::Colours::black.withAlpha(0.87f));
            g.fillRoundedRectangle(bounds, 5.5f);
            g.setColour(kGoldDim.withAlpha(0.94f));
            g.drawRoundedRectangle(bounds, 5.5f, isDown || box.isMouseOver() ? 1.55f : 1.0f);
            g.setColour(kGold.withAlpha(0.48f));
            g.drawRoundedRectangle(bounds.reduced(3.0f), 3.8f, 0.65f);

            juce::Path arrow;
            const float ax = (float) width - 19.0f;
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
            const auto box = juce::Rectangle<float>(4.0f, 3.0f, 26.0f, 26.0f);
            const bool on = button.getToggleState();
            g.setColour(juce::Colours::black.withAlpha(0.92f));
            g.fillRoundedRectangle(box, 3.0f);
            g.setColour(on ? kPurple : (isHighlighted ? kGold : kGoldDim));
            g.drawRoundedRectangle(box, 3.0f, on ? 1.7f : 1.0f);
            if (on)
            {
                g.setColour(kPurple.withAlpha(0.22f));
                g.fillRoundedRectangle(box.reduced(2.0f), 2.0f);
                juce::Path tick;
                tick.startNewSubPath(8.0f, 15.0f);
                tick.lineTo(13.0f, 20.0f);
                tick.lineTo(23.0f, 9.0f);
                g.setColour(kPurple.brighter(0.28f));
                g.strokePath(tick, juce::PathStrokeType(2.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }

        void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                  bool highlighted, bool down) override
        {
            if (isReferenceOverlay(button))
                return;

            const auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
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

        void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                            bool highlighted, bool down) override
        {
            if (isReferenceOverlay(button))
                return;
            juce::LookAndFeel_V4::drawButtonText(g, button, highlighted, down);
        }

        juce::Font getTextButtonFont(juce::TextButton&, int height) override
        {
            return displayFont(juce::jlimit(14.0f, 21.0f, (float) height * 0.52f));
        }
    };
}

MirrorAudioProcessorEditor::MirrorAudioProcessorEditor(MirrorAudioProcessor& mirrorProcessor)
    : AudioProcessorEditor(&mirrorProcessor), audioProcessor(mirrorProcessor)
{
    mirrorLookAndFeel = std::make_unique<MirrorLookAndFeel>();
    setLookAndFeel(mirrorLookAndFeel.get());
    setOpaque(true);
    // The supplied visual specification is a fixed studio canvas. Keeping it
    // fixed preserves the 1:1 proportions instead of shrinking the panels
    // into a generic plug-in layout.
    setResizable(false, false);

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
        button->setComponentID("reference-overlay");
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
        const int targetWidth = showAdvanced ? 1701 : 1730;
        const int targetHeight = showAdvanced ? 925 : 909;
        if (getWidth() != targetWidth || getHeight() != targetHeight)
            setSize(targetWidth, targetHeight);
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
    configureLabel(midiVoicingLabel, "VOICING", 15.0f);
    addAndMakeVisible(midiVoicingLabel);

    configureComboBox(midiInversionBox, "MIDI-inversion");
    midiInversionBox.addItemList({ "Auto", "Root", "1st", "2nd", "3rd" }, 1);
    midiInversionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "midiInversion", midiInversionBox);
    configureLabel(midiInversionLabel, "INVERSION", 15.0f);
    addAndMakeVisible(midiInversionLabel);

    configureComboBox(presetBox, "PЯeset");
    presetBox.addItem("Vocoder Glass", 1);
    presetBox.addItem("Fractured Stack", 2);
    presetBox.addItem("Stadium Choir", 3);
    // The supplied canvas already contains the precise PЯeset caption.
    // An empty initial selection keeps the startup appearance 1:1, while
    // a chosen preset is still shown normally afterwards.
    presetBox.setTextWhenNothingSelected(juce::String());
    presetBox.setSelectedId(0, juce::dontSendNotification);
    presetBox.onChange = nullptr;
    configureLabel(presetLabel, "PЯESET", 11.0f, false);
    addAndMakeVisible(presetLabel);

    // Exact artwork handles the default chrome and typography. Controls stay
    // transparent at their reference values, then become live faces as soon
    // as the user chooses a different value.
    for (auto* box : { &modeBox, &rootBox, &scaleBox, &presetBox })
        box->setComponentID("reference-overlay");

    const auto syncReferenceCombo = [](juce::ComboBox& box, const juce::String& referenceText)
    {
        box.setAlpha(box.getText() == referenceText ? 0.0f : 1.0f);
    };
    modeBox.onChange = [this, syncReferenceCombo]
    {
        syncReferenceCombo(modeBox, "Manual");
        updateControlVisibility();
        resized();
        repaint();
    };
    rootBox.onChange = [this, syncReferenceCombo] { syncReferenceCombo(rootBox, "C"); };
    scaleBox.onChange = [this, syncReferenceCombo] { syncReferenceCombo(scaleBox, "Major"); };
    presetBox.onChange = [this, syncReferenceCombo]
    {
        syncReferenceCombo(presetBox, juce::String());
        applyPreset(presetBox.getSelectedId());
    };
    syncReferenceCombo(modeBox, "Manual");
    syncReferenceCombo(rootBox, "C");
    syncReferenceCombo(scaleBox, "Major");
    syncReferenceCombo(presetBox, juce::String());

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

    mainPageButton.setToggleState(true, juce::dontSendNotification);
    setSize(1723, 913);
    updateControlVisibility();
    // The reference canvas is static; 10 Hz is only for host automation of
    // the Mode parameter and avoids a needless 30 fps GUI redraw loop.
    startTimerHz(10);
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

    column.enableButton.setButtonText("");
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

    auto setupSmall = [this](juce::Slider& slider, juce::Label& label, const juce::String& /*parameterID*/, const juce::String& caption)
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

    const int targetWidth = currentPage == 0 ? 1723 : (showAdvanced ? 1701 : 1730);
    const int targetHeight = currentPage == 0 ? 913 : (showAdvanced ? 925 : 909);
    if (getWidth() != targetWidth || getHeight() != targetHeight)
        setSize(targetWidth, targetHeight);

    updateControlVisibility();
    resized();
    repaint();
}

void MirrorAudioProcessorEditor::updateControlVisibility()
{
    const bool main = currentPage == 0;
    const bool harmony = currentPage == 1;

    // Titles, panel labels and navigation are deliberately baked into the
    // three supplied reference canvases. Only functional controls are
    // overlaid, which prevents duplicate or mismatched typography.
    for (auto* label : { &titleLabel, &creditLabel, &modeLabel, &rootLabel, &scaleLabel,
                         &presetLabel, &vocalRangeLabel, &harmonyStyleLabel,
                         &midiVoicingLabel, &midiInversionLabel })
        label->setVisible(false);

    mainPageButton.setVisible(true);
    harmonyPageButton.setVisible(true);
    advancedButton.setVisible(harmony);

    modeBox.setVisible(true);
    rootBox.setVisible(true);
    scaleBox.setVisible(true);
    presetBox.setVisible(true);

    vocalRangeBox.setVisible(false);
    harmonyStyleBox.setVisible(false);
    midiVoicingBox.setVisible(false);
    midiInversionBox.setVisible(false);
    freezeButton.setVisible(false);

    auto showKnob = [main](KnobWithLabel& knob)
    {
        knob.slider.setVisible(main);
        knob.label.setVisible(false);
    };
    for (auto* knob : { &trackingKnob, &glideKnob, &dryLevelKnob, &dryPanKnob, &dryFormantKnob,
                        &dryPitchKnob, &dryWidthKnob, &humanizeKnob, &characterKnob, &spreadKnob,
                        &ambienceKnob, &harmonyMixKnob, &globalSaturationKnob, &outputGainKnob })
        showKnob(*knob);

    for (auto& column : voiceColumns)
    {
        column.title.setVisible(false);
        column.enableButton.setVisible(harmony && !showAdvanced);
        column.soloButton.setVisible(false);

        // The screenshots are the visual canvas; the selected musical
        // interval remains a compact functional control inside each panel.
        column.intervalBox.setVisible(harmony);

        column.levelSlider.setVisible(harmony);
        column.levelLabel.setVisible(false);
        column.panSlider.setVisible(harmony);
        column.panLabel.setVisible(false);
        column.formantSlider.setVisible(harmony);
        column.formantLabel.setVisible(false);

        for (auto* component : { (juce::Component*) &column.fineTuneSlider, (juce::Component*) &column.toneSlider,
                                 (juce::Component*) &column.saturationSlider, (juce::Component*) &column.microDelaySlider,
                                 (juce::Component*) &column.vibratoSlider, (juce::Component*) &column.vibratoRateSlider })
            component->setVisible(harmony && showAdvanced);

        for (auto* label : { &column.fineTuneLabel, &column.toneLabel, &column.saturationLabel,
                             &column.microDelayLabel, &column.vibratoLabel, &column.vibratoRateLabel })
            label->setVisible(false);
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
    const bool harmony = currentPage == 1;
    const float referenceWidth = harmony ? (showAdvanced ? 1701.0f : 1730.0f) : 1723.0f;
    const float referenceHeight = harmony ? (showAdvanced ? 925.0f : 909.0f) : 913.0f;
    const float sx = (float) getWidth() / referenceWidth;
    const float sy = (float) getHeight() / referenceHeight;
    const auto rect = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };

    // Exact hit areas over each native reference canvas. The three sources
    // are intentionally not stretched into a single generic window size.
    if (!harmony)
    {
        presetBox.setBounds(rect(50, 183, 230, 47));
        modeBox.setBounds(rect(774, 67, 220, 49));
        rootBox.setBounds(rect(1252, 67, 210, 49));
        scaleBox.setBounds(rect(1493, 67, 210, 49));
        mainPageButton.setBounds(rect(624, 134, 240, 47));
        harmonyPageButton.setBounds(rect(868, 134, 242, 47));
        advancedButton.setBounds(rect(774, 191, 180, 39));
    }
    else if (!showAdvanced)
    {
        presetBox.setBounds(rect(48, 179, 229, 46));
        modeBox.setBounds(rect(761, 75, 210, 47));
        rootBox.setBounds(rect(1232, 77, 210, 46));
        scaleBox.setBounds(rect(1470, 77, 210, 46));
        mainPageButton.setBounds(rect(619, 134, 238, 47));
        harmonyPageButton.setBounds(rect(861, 134, 230, 47));
        advancedButton.setBounds(rect(764, 197, 203, 39));
    }
    else
    {
        presetBox.setBounds(rect(47, 175, 230, 48));
        modeBox.setBounds(rect(742, 71, 214, 47));
        rootBox.setBounds(rect(1220, 70, 208, 47));
        scaleBox.setBounds(rect(1455, 70, 200, 47));
        mainPageButton.setBounds(rect(609, 134, 232, 47));
        harmonyPageButton.setBounds(rect(845, 134, 234, 47));
        advancedButton.setBounds(rect(742, 190, 213, 42));
    }

    vocalRangeBox.setBounds(-100, -100, 1, 1);
    harmonyStyleBox.setBounds(-100, -100, 1, 1);
    midiVoicingBox.setBounds(-100, -100, 1, 1);
    midiInversionBox.setBounds(-100, -100, 1, 1);
    freezeButton.setBounds(-100, -100, 1, 1);

    if (currentPage == 0)
        layoutMain(getLocalBounds());
    else
        layoutHarmony(getLocalBounds());
}

void MirrorAudioProcessorEditor::layoutMain(juce::Rectangle<int>)
{
    constexpr float referenceWidth = 1723.0f;
    constexpr float referenceHeight = 913.0f;
    const float sx = (float) getWidth() / referenceWidth;
    const float sy = (float) getHeight() / referenceHeight;
    const auto rect = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };

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

    auto output = outputPanelBounds.reduced(62, 86);
    auto knobs = output.removeFromTop(112);
    auto glue = knobs.removeFromLeft(knobs.getWidth() / 2);
    placeKnob(globalSaturationKnob, glue.reduced(20, 0));
    placeKnob(outputGainKnob, knobs.reduced(20, 0));
    auto mix = output.reduced(8, 0);
    harmonyMixKnob.label.setBounds(-100, -100, 1, 1);
    harmonyMixKnob.slider.setBounds(mix.removeFromTop(56).reduced(6, 7));
}

void MirrorAudioProcessorEditor::layoutHarmony(juce::Rectangle<int>)
{
    const float referenceWidth = showAdvanced ? 1701.0f : 1730.0f;
    const float referenceHeight = showAdvanced ? 925.0f : 909.0f;
    const float sx = (float) getWidth() / referenceWidth;
    const float sy = (float) getHeight() / referenceHeight;
    const auto rect = [sx, sy](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(juce::roundToInt(x * sx), juce::roundToInt(y * sy),
                                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };

    const float panelY = showAdvanced ? 337.0f : 367.0f;
    const float panelH = showAdvanced ? 530.0f : 500.0f;

    for (int i = 0; i < kNumHarmonyVoices; ++i)
    {
        const float offset = (float) i * 410.0f;
        auto& column = voiceColumns[(size_t) i];

        // Checkbox only: its “On” caption is preserved exactly in the artwork.
        column.enableButton.setBounds(rect(215 + offset, 257, 38, 30));
        column.title.setBounds(-100, -100, 1, 1);
        voiceLevelBounds[(size_t) i] = rect(62 + offset, panelY + 4, 30, panelH - 10);
        column.levelLabel.setBounds(-100, -100, 1, 1);
        column.levelSlider.setBounds(voiceLevelBounds[(size_t) i]);
        voicePanelBounds[(size_t) i] = rect(140 + offset, panelY, 264, panelH);
        column.soloButton.setBounds(-100, -100, 1, 1);

        // The harmony interval is a required functional addition. It occupies
        // open glass space without moving any reference artwork.
        const float intervalY = showAdvanced ? panelY + 18.0f : panelY + 218.0f;
        column.intervalBox.setBounds(rect(166 + offset, intervalY, 212, 30));

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
    // Keep host automation in sync with the static default artwork without
    // polling meters or repainting the full 6 MB reference canvas every frame.
    const auto syncReferenceCombo = [](juce::ComboBox& box, const juce::String& referenceText)
    {
        const float targetAlpha = box.getText() == referenceText ? 0.0f : 1.0f;
        if (std::abs(box.getAlpha() - targetAlpha) > 0.001f)
            box.setAlpha(targetAlpha);
    };
    syncReferenceCombo(modeBox, "Manual");
    syncReferenceCombo(rootBox, "C");
    syncReferenceCombo(scaleBox, "Major");
    syncReferenceCombo(presetBox, juce::String());

    const int mode = (int) *audioProcessor.apvts.getRawParameterValue("mode");
    if (mode != lastMode)
    {
        lastMode = mode;
        updateControlVisibility();
        resized();
        repaint();
    }
}

void MirrorAudioProcessorEditor::paint(juce::Graphics& g)
{
    drawBackground(g);
}

void MirrorAudioProcessorEditor::drawBackground(juce::Graphics& g) const
{
    g.fillAll(kInk);
    const juce::Image& reference = currentPage == 0
        ? referenceMainImage()
        : (showAdvanced ? referenceAdvancedImage() : referenceHarmonyImage());

    if (reference.isValid())
    {
        g.drawImageWithin(reference, 0, 0, getWidth(), getHeight(),
                          juce::RectanglePlacement::stretchToFit, false);
        return;
    }

    // A build without the binary resource remains usable, but release builds
    // always use the supplied visual specification above.
    const auto& fallback = engravedBackgroundTexture();
    if (fallback.isValid())
        g.drawImageWithin(fallback, 0, 0, getWidth(), getHeight(),
                          juce::RectanglePlacement::stretchToFit, false);
}

void MirrorAudioProcessorEditor::drawHeader(juce::Graphics& g) const
{
    constexpr float referenceWidth = 1723.0f;
    constexpr float referenceHeight = 913.0f;
    const float sx = (float) getWidth() / referenceWidth;
    const float sy = (float) getHeight() / referenceHeight;
    const float activeX = currentPage == 0 ? 635.0f : 879.0f;
    const float headerY = 228.0f;

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
        g.setColour(kPurple.withAlpha(glow * (0.20f - (float) ring * 0.06f)));
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
            const float outlineScale = (float) outline;
            g.drawEllipse(x - 12.0f - outlineScale * 4.0f, top + 7.0f - outlineScale * 2.0f,
                          24.0f + outlineScale * 8.0f, height * 0.76f + outlineScale * 4.0f, 0.8f);
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
