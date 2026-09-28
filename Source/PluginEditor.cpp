#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    juce::Colour kBg1 { 0xff101211 };
    juce::Colour kBg2 { 0xff161a16 };
    juce::Colour kAccent { 0xffcfb991 };
    juce::Colour kAccentDim { 0xffbba783 };
    juce::Colour kText { 0xffefece3 };
    juce::Colour kTextDim { 0xffaaa99e };

    constexpr int kPresetCustomId = 1;
    constexpr int kFirstPresetId = 2;
    constexpr int kMidiModeItemId = 2;

}

MirrorAudioProcessorEditor::MirrorAudioProcessorEditor(MirrorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setLookAndFeel(&theme);
    engineQualityBox.addItemList({ "Original", "Refined" }, 1);
    engineQualityBox.setTooltip("Original preserves the v1.5 pitch interpolation. Refined smooths the transition between interpolation filters. Old sessions load Original automatically.");
    addAndMakeVisible(engineQualityBox);
    engineQualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.apvts, "engineQuality", engineQualityBox);
    engineQualityLabel.setText("PITCH ENGINE", juce::dontSendNotification);
    engineQualityLabel.setColour(juce::Label::textColourId, kTextDim);
    engineQualityLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
    addAndMakeVisible(engineQualityLabel);
    // U+042F is the Cyrillic capital Ya: the actual mirrored-R glyph used in
    // the wordmark, not a fragile font trick. Times New Roman has the glyph on
    // macOS; the fallbacks keep the logo legible if a host substitutes fonts.
    titleLabel.setText(juce::String::fromUTF8(u8"MIЯЯOЯ"), juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setColour(juce::Label::textColourId, kText);
    auto wordmarkFont = juce::Font(juce::FontOptions("Times New Roman", 32.0f, juce::Font::plain));
    juce::StringArray wordmarkFallbacks;
    wordmarkFallbacks.add("Georgia");
    wordmarkFallbacks.add("Arial Unicode MS");
    wordmarkFallbacks.add(juce::Font::getDefaultSerifFontName());
    wordmarkFont.setPreferredFallbackFamilies(wordmarkFallbacks);
    wordmarkFont.setFallbackEnabled(true);
    titleLabel.setFont(wordmarkFont.withExtraKerningFactor(0.13f));
    addAndMakeVisible(titleLabel);

    creditLabel.setText("by Louis Gabriel", juce::dontSendNotification);
    creditLabel.setJustificationType(juce::Justification::centredLeft);
    creditLabel.setColour(juce::Label::textColourId, kTextDim);
    creditLabel.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::italic)));
    addAndMakeVisible(creditLabel);

    mainPageButton.setButtonText("MAIN");
    mainPageButton.setClickingTogglesState(true);
    mainPageButton.setRadioGroupId(1001);
    mainPageButton.setColour(juce::TextButton::buttonOnColourId, kAccent);
    mainPageButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff382d25));
    mainPageButton.onClick = [this] { showPage(0); };
    addAndMakeVisible(mainPageButton);

    harmonyPageButton.setButtonText("HARMONY");
    harmonyPageButton.setClickingTogglesState(true);
    harmonyPageButton.setRadioGroupId(1001);
    harmonyPageButton.setColour(juce::TextButton::buttonOnColourId, kAccent);
    harmonyPageButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff382d25));
    harmonyPageButton.onClick = [this] { showPage(1); };
    addAndMakeVisible(harmonyPageButton);

    modeBox.addItemList({ "Manual", "MIDI" }, 1);
    modeBox.setComponentID("mode");
    addAndMakeVisible(modeBox);
    modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "mode", modeBox);
    modeBox.onChange = [this]
    {
        updateModeDependentControls();
        resized();
    };
    modeLabel.setText("MODE", juce::dontSendNotification);
    modeLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(modeLabel);

    vocalRangeBox.addItemList({ "Auto", "Bass", "Baritone", "Tenor", "Alto", "Soprano" }, 1);
    addAndMakeVisible(vocalRangeBox);
    vocalRangeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "vocalRange", vocalRangeBox);
    vocalRangeBox.onChange = [this] { markPresetAsCustom(); };
    vocalRangeLabel.setText("VOCAL RANGE", juce::dontSendNotification);
    vocalRangeLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(vocalRangeLabel);

    harmonyStyleBox.addItemList({ "Tight", "Natural", "Wide", "Choir" }, 1);
    addAndMakeVisible(harmonyStyleBox);
    harmonyStyleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "harmonyStyle", harmonyStyleBox);
    harmonyStyleBox.onChange = [this] { markPresetAsCustom(); };
    harmonyStyleLabel.setText("STYLE", juce::dontSendNotification);
    harmonyStyleLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(harmonyStyleLabel);

    // This selector is deliberately not an APVTS parameter. It applies a
    // collection of parameters, while a restored DAW session should never
    // pretend to know which name the user last chose.
    presetBox.addItem("SELECT / CUSTOM", kPresetCustomId);
    presetBox.setComponentID("preset");
    presetBox.addSeparator();
    presetBox.addItem("Glass Bloom", kFirstPresetId);
    presetBox.addItem("Fractured Light", kFirstPresetId + 1);
    presetBox.addItem("Night Choir", kFirstPresetId + 2);
    presetBox.setSelectedId(kPresetCustomId, juce::dontSendNotification);
    presetBox.onChange = [this]
    {
        const int selected = presetBox.getSelectedId();
        if (selected >= kFirstPresetId)
            applyPreset(selected - kFirstPresetId + 1);
    };
    addAndMakeVisible(presetBox);
    presetLabel.setText("PRESET", juce::dontSendNotification);
    presetLabel.setJustificationType(juce::Justification::centredRight);
    presetLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(presetLabel);

    // --- INPUT ---
    inputSectionLabel.setText("INPUT", juce::dontSendNotification);
    inputSectionLabel.setColour(juce::Label::textColourId, kAccentDim);
    inputSectionLabel.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    addAndMakeVisible(inputSectionLabel);

    rootBox.addItemList({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    rootBox.setComponentID("rootNote");
    addAndMakeVisible(rootBox);
    rootAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "rootNote", rootBox);
    rootLabel.setText("KEY", juce::dontSendNotification);
    rootLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(rootLabel);

    scaleBox.addItemList({ "Chromatic", "Major", "Minor" }, 1);
    scaleBox.setComponentID("scaleType");
    addAndMakeVisible(scaleBox);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "scaleType", scaleBox);
    scaleLabel.setText("SCALE", juce::dontSendNotification);
    scaleLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(scaleLabel);

    setupKnob(trackingKnob, "tracking", "TRACKING");
    setupKnob(glideKnob, "glide", "TRANSITION");
    lockKnob(trackingKnob, "TRACKING · 100%", "Tracking is fixed at 100% for MIRROR's stable quality path.");
    lockKnob(glideKnob, "TRANSITION · 100%", "Transition is fixed at 100% for MIRROR's stable quality path.");

    freezeButton.setButtonText("FREEZE");
    freezeButton.setColour(juce::ToggleButton::tickColourId, kAccent);
    freezeButton.setColour(juce::ToggleButton::textColourId, kTextDim);
    addAndMakeVisible(freezeButton);
    freezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "freeze", freezeButton);

    // --- MIDI HARMONY (shown only in MIDI mode) ---
    setupKnob(midiVelocityKnob, "midiVelocity", "VELOCITY");
    midiVelocityKnob.slider.textFromValueFunction = [] (double value)
    {
        return juce::String(juce::roundToInt(juce::jlimit(0.0, 1.0, value) * 100.0)) + "%";
    };
    midiVelocityKnob.slider.setTooltip("How much MIDI note velocity changes the harmony level.");

    midiVoicingBox.addItemList({ "Close", "Open", "Wide" }, 1);
    addAndMakeVisible(midiVoicingBox);
    midiVoicingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "midiVoicing", midiVoicingBox);
    midiVoicingBox.onChange = [this] { markPresetAsCustom(); };
    midiVoicingBox.setTooltip("Spacing used when MIRROR expands repeated MIDI chord tones.");
    midiVoicingLabel.setText("VOICING", juce::dontSendNotification);
    midiVoicingLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(midiVoicingLabel);

    midiInversionBox.addItemList({ "Auto", "Root", "1st", "2nd", "3rd" }, 1);
    addAndMakeVisible(midiInversionBox);
    midiInversionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "midiInversion", midiInversionBox);
    midiInversionBox.onChange = [this] { markPresetAsCustom(); };
    midiInversionBox.setTooltip("Choose the MIDI chord inversion, or let Auto minimise voice movement.");
    midiInversionLabel.setText("INVERSION", juce::dontSendNotification);
    midiInversionLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(midiInversionLabel);

    midiTimingBox.addItemList({ "Live", "Aligned" }, 1);
    addAndMakeVisible(midiTimingBox);
    midiTimingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "midiTiming", midiTimingBox);
    midiTimingBox.onChange = [this] { markPresetAsCustom(); };
    midiTimingBox.setTooltip("Live responds at the incoming MIDI sample. Aligned delays chord changes to the processed vocal.");
    midiTimingLabel.setText("MIDI TIMING", juce::dontSendNotification);
    midiTimingLabel.setColour(juce::Label::textColourId, kTextDim);
    addAndMakeVisible(midiTimingLabel);

    // --- DRY VOICE ---
    drySectionLabel.setText("DRY VOICE", juce::dontSendNotification);
    drySectionLabel.setColour(juce::Label::textColourId, kAccentDim);
    drySectionLabel.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    addAndMakeVisible(drySectionLabel);

    setupKnob(dryLevelKnob, "dry", "LEVEL");
    setupKnob(dryPanKnob, "dryPan", "PAN");
    setupKnob(dryFormantKnob, "dryFormant", "FORMANT");
    setupKnob(dryPitchKnob, "dryPitch", "PITCH");
    setupKnob(dryWidthKnob, "dryWidth", "WIDTH");

    // --- HARMONY (fane 2) ---
    harmonySectionLabel.setText("HARMONY", juce::dontSendNotification);
    harmonySectionLabel.setColour(juce::Label::textColourId, kAccentDim);
    harmonySectionLabel.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    addAndMakeVisible(harmonySectionLabel);

    advancedButton.setButtonText("ADVANCED");
    advancedButton.setClickingTogglesState(true);
    advancedButton.setColour(juce::TextButton::buttonOnColourId, kAccent);
    advancedButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff382d25));
    advancedButton.onClick = [this]
    {
        showAdvanced = advancedButton.getToggleState();
        showPage(currentPage);
    };
    addAndMakeVisible(advancedButton);

    for (int i = 0; i < kNumHarmonyVoices; ++i)
        setupVoiceColumn(voiceColumns[(size_t) i], i);

    // --- CHARACTER ---
    characterSectionLabel.setText("CHARACTER", juce::dontSendNotification);
    characterSectionLabel.setColour(juce::Label::textColourId, kAccentDim);
    characterSectionLabel.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    addAndMakeVisible(characterSectionLabel);

    setupKnob(humanizeKnob, "humanize", "HUMANIZE");
    setupKnob(characterKnob, "character", "CHARACTER");
    setupKnob(spreadKnob, "spread", "SPREAD");

    // --- AMBIENCE + MIX ---
    ambienceSectionLabel.setText("AMBIENCE", juce::dontSendNotification);
    ambienceSectionLabel.setColour(juce::Label::textColourId, kAccentDim);
    ambienceSectionLabel.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    addAndMakeVisible(ambienceSectionLabel);
    setupKnob(ambienceKnob, "ambience", "AMBIENCE");

    mixSectionLabel.setText("MIX", juce::dontSendNotification);
    mixSectionLabel.setColour(juce::Label::textColourId, kAccentDim);
    mixSectionLabel.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
    addAndMakeVisible(mixSectionLabel);
    setupKnob(harmonyMixKnob, "harmonyMix", "HARMONY MIX");
    harmonyMixKnob.slider.setSliderStyle(juce::Slider::LinearHorizontal);
    harmonyMixKnob.slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 26);
    harmonyMixKnob.slider.setTooltip("One level for all four harmonies, including their ambience. Lead level stays unchanged. 100% preserves the original balance.");
    setupKnob(globalSaturationKnob, "globalSaturation", "GLUE");
    setupKnob(outputGainKnob, "outputGain", "OUTPUT");
    outputGainKnob.slider.setTooltip("Final output trim after colour, before the safety limiter.");

    statusLabel.setColour(juce::Label::textColourId, kTextDim);
    statusLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(statusLabel);
    pageHintLabel.setColour(juce::Label::textColourId, kTextDim);
    pageHintLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(pageHintLabel);
    mixHintLabel.setText("All four voices. Your lead stays unchanged.", juce::dontSendNotification);
    mixHintLabel.setColour(juce::Label::textColourId, kTextDim);
    mixHintLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    addAndMakeVisible(mixHintLabel);
    helpButton.setTooltip("Quick start and keyboard controls");
    helpButton.onClick = [this]
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "MIRROR · Quick start",
            "MANUAL: choose Key / Scale, then intervals on the Voices page.\n"
            "MIDI: route MIDI notes from your DAW to MIRROR. Key / Scale do not set played MIDI notes.\n\n"
            "HARMONY MIX controls all generated voices together, without turning down the lead. "
            "Use Lead Level for harmonies-only processing.\n\n"
            "ADVANCED reveals each voice's tone, colour, timing and modulation. "
            "Formant is gentle tone shaping, not independent formant resynthesis.\n\n"
            "Double-click a control to reset it. Drag for continuous adjustment or click its value to type. "
            "Tab selects controls; arrow keys adjust sliders. Hover for help.\n\n"
            "Development candidate · 1.6.0 · Not a signed commercial release.",
            "Got it", this);
    };
    addAndMakeVisible(helpButton);
    setSize(840, 640);
    startTimerHz(20);

    mainPageButton.setToggleState(true, juce::dontSendNotification);
    showPage(0);
}

MirrorAudioProcessorEditor::~MirrorAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MirrorAudioProcessorEditor::setupKnob(KnobWithLabel& k, const juce::String& paramId, const juce::String& labelText)
{
    k.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
    k.slider.setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    k.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff262b36));
    k.slider.setColour(juce::Slider::thumbColourId, kText);
    addAndMakeVisible(k.slider);

    k.label.setText(labelText, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.label.setColour(juce::Label::textColourId, kTextDim);
    k.label.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::plain)));
    addAndMakeVisible(k.label);

    // SliderAttachment uses Slider::Listener rather than this callback, so
    // this remains a safe way to record a hands-on edit as Custom.
    k.slider.onValueChange = [this] { markPresetAsCustom(); };

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, paramId, k.slider);
    configureValueDisplay(k.slider, paramId);
}

void MirrorAudioProcessorEditor::lockKnob(KnobWithLabel& k, const juce::String& lockedLabel,
                                          const juce::String& helpText)
{
    // The processor deliberately ignores these legacy values and runs each
    // stage at unity. Detaching avoids displaying an old automation value as
    // if it were still actionable, while preserving the APVTS parameter for
    // compatibility with existing sessions.
    k.attachment.reset();
    k.slider.setValue(1.0, juce::dontSendNotification);
    k.slider.textFromValueFunction = [] (double) { return "100%"; };
    k.slider.valueFromTextFunction = [] (const juce::String&) { return 1.0; };
    k.slider.setTextBoxIsEditable(false);
    k.slider.setEnabled(false);
    k.slider.setAlpha(0.72f);
    k.slider.setTooltip(helpText);

    k.label.setText(lockedLabel, juce::dontSendNotification);
    k.label.setAlpha(0.82f);
    k.label.setTooltip(helpText);
}

void MirrorAudioProcessorEditor::markPresetAsCustom()
{
    if (isApplyingPreset || presetBox.getSelectedId() == kPresetCustomId)
        return;

    presetBox.setSelectedId(kPresetCustomId, juce::dontSendNotification);
}

void MirrorAudioProcessorEditor::updateModeDependentControls()
{
    const bool showMain = currentPage == 0;
    const bool isMidi = modeBox.getSelectedId() == kMidiModeItemId;
    const bool showManualInput = !isMidi;
    const bool showMidiInput = showMain && isMidi;

    // Key/Scale are deliberately preserved when a preset changes, but MIDI
    // voices are driven by played notes rather than that Manual-only context.
    rootBox.setVisible(showManualInput);
    rootLabel.setVisible(showManualInput);
    scaleBox.setVisible(showManualInput);
    scaleLabel.setVisible(showManualInput);
    trackingKnob.slider.setVisible(false);
    trackingKnob.label.setVisible(false);
    glideKnob.slider.setVisible(false);
    glideKnob.label.setVisible(false);
    freezeButton.setVisible(showMain && !isMidi);
    freezeButton.setEnabled(showManualInput);

    midiVelocityKnob.slider.setVisible(showMidiInput);
    midiVelocityKnob.label.setVisible(showMidiInput);
    midiVoicingBox.setVisible(showMidiInput);
    midiVoicingLabel.setVisible(showMidiInput);
    midiInversionBox.setVisible(showMidiInput);
    midiInversionLabel.setVisible(showMidiInput);
    midiTimingBox.setVisible(showMidiInput);
    midiTimingLabel.setVisible(showMidiInput);


    inputSectionLabel.setVisible(false);
    for (auto& voice : voiceColumns)
    {
        voice.intervalBox.setEnabled(!isMidi);
        voice.intervalBox.setTooltip(isMidi ? "In MIDI mode, played notes set the harmonies." : "Harmony interval relative to the lead and selected scale.");
    }
    pageHintLabel.setText(currentPage == 0
        ? "Shape the lead, build the ensemble, then blend."
        : (isMidi ? "MIDI notes set the harmony. Shape each voice below." : "Choose intervals, balance the voices, then explore Advanced."),
        juce::dontSendNotification);
}

void MirrorAudioProcessorEditor::setupVoiceColumn(VoiceColumn& c, int voiceIndex)
{
    juce::String idx(voiceIndex + 1);

    c.title.setText("VOICE " + idx, juce::dontSendNotification);
    c.title.setJustificationType(juce::Justification::centred);
    c.title.setColour(juce::Label::textColourId, kText);
    c.title.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    addAndMakeVisible(c.title);

    c.enableButton.setButtonText("ON");
    c.enableButton.setColour(juce::ToggleButton::tickColourId, kAccent);
    c.enableButton.setColour(juce::ToggleButton::textColourId, kTextDim);
    addAndMakeVisible(c.enableButton);
    c.enableAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "voiceEnable" + idx, c.enableButton);
    c.enableButton.onClick = [this] { markPresetAsCustom(); };

    c.soloButton.setButtonText("SOLO");
    c.soloButton.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xffffd479));
    c.soloButton.setColour(juce::ToggleButton::textColourId, kTextDim);
    addAndMakeVisible(c.soloButton);
    c.soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "voiceSolo" + idx, c.soloButton);
    c.soloButton.onClick = [this] { markPresetAsCustom(); };

    c.intervalBox.addItemList(getIntervalNames(), 1);
    c.intervalBox.setComponentID("voiceInterval" + idx);
    addAndMakeVisible(c.intervalBox);
    c.intervalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "voiceInterval" + idx, c.intervalBox);
    c.intervalBox.onChange = [this] { markPresetAsCustom(); };

    auto setupSmallKnob = [this](juce::Slider& s, juce::Label& lbl, const juce::String& text, juce::Colour colour)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 12);
        s.setColour(juce::Slider::rotarySliderFillColourId, colour);
        s.setColour(juce::Slider::thumbColourId, kText);
        addAndMakeVisible(s);

        lbl.setText(text, juce::dontSendNotification);
        lbl.setJustificationType(juce::Justification::centred);
        lbl.setColour(juce::Label::textColourId, kTextDim);
        lbl.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::plain)));
        addAndMakeVisible(lbl);

        s.onValueChange = [this] { markPresetAsCustom(); };

    };

    setupSmallKnob(c.levelSlider, c.levelLabel, "LEVEL", kAccent);
    setupSmallKnob(c.panSlider, c.panLabel, "PAN", kAccent);
    setupSmallKnob(c.formantSlider, c.formantLabel, "FORMANT", kAccent);
    setupSmallKnob(c.fineTuneSlider, c.fineTuneLabel, "FINE", kAccentDim);
    setupSmallKnob(c.toneSlider, c.toneLabel, "TONE", kAccentDim);
    setupSmallKnob(c.saturationSlider, c.saturationLabel, "SAT", kAccentDim);
    setupSmallKnob(c.microDelaySlider, c.microDelayLabel, "DELAY", kAccentDim);
    setupSmallKnob(c.vibratoSlider, c.vibratoLabel, "VIBRATO", kAccentDim);
    setupSmallKnob(c.vibratoRateSlider, c.vibratoRateLabel, "VIB RATE", kAccentDim);

    c.levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "voiceLevel" + idx, c.levelSlider);
    c.panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "voicePan" + idx, c.panSlider);
    c.formantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "voiceFormant" + idx, c.formantSlider);
    c.fineTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "voiceFineTune" + idx, c.fineTuneSlider);
    c.toneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "voiceTone" + idx, c.toneSlider);
    c.saturationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "voiceSaturation" + idx, c.saturationSlider);
    c.microDelayAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "voiceMicroDelay" + idx, c.microDelaySlider);
    c.vibratoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "voiceVibrato" + idx, c.vibratoSlider);
    c.vibratoRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "voiceVibratoRate" + idx, c.vibratoRateSlider);
    configureValueDisplay(c.levelSlider, "voiceLevel" + idx);
    configureValueDisplay(c.panSlider, "voicePan" + idx);
    configureValueDisplay(c.formantSlider, "voiceFormant" + idx);
    configureValueDisplay(c.fineTuneSlider, "voiceFineTune" + idx);
    configureValueDisplay(c.toneSlider, "voiceTone" + idx);
    configureValueDisplay(c.saturationSlider, "voiceSaturation" + idx);
    configureValueDisplay(c.microDelaySlider, "voiceMicroDelay" + idx);
    configureValueDisplay(c.vibratoSlider, "voiceVibrato" + idx);
    configureValueDisplay(c.vibratoRateSlider, "voiceVibratoRate" + idx);
}

void MirrorAudioProcessorEditor::showPage(int pageIndex)
{
    currentPage = pageIndex;
    bool showMain = (pageIndex == 0);
    bool showHarmony = (pageIndex == 1);

    for (juce::Component* c : { (juce::Component*) &modeBox, (juce::Component*) &vocalRangeBox,
                     (juce::Component*) &harmonyStyleBox, (juce::Component*) &rootBox,
                     (juce::Component*) &scaleBox, (juce::Component*) &trackingKnob.slider,
                     (juce::Component*) &glideKnob.slider, (juce::Component*) &dryLevelKnob.slider,
                     (juce::Component*) &dryPanKnob.slider, (juce::Component*) &dryFormantKnob.slider,
                     (juce::Component*) &dryPitchKnob.slider, (juce::Component*) &dryWidthKnob.slider,
                     (juce::Component*) &humanizeKnob.slider, (juce::Component*) &characterKnob.slider,
                     (juce::Component*) &spreadKnob.slider, (juce::Component*) &ambienceKnob.slider,
                     (juce::Component*) &harmonyMixKnob.slider, (juce::Component*) &globalSaturationKnob.slider,
                     (juce::Component*) &outputGainKnob.slider, (juce::Component*) &midiVelocityKnob.slider,
                     (juce::Component*) &midiVoicingBox, (juce::Component*) &midiInversionBox,
                     (juce::Component*) &midiTimingBox })
        c->setVisible(showMain);

    for (auto* l : { &modeLabel, &vocalRangeLabel, &harmonyStyleLabel, &inputSectionLabel, &rootLabel, &scaleLabel,
                     &trackingKnob.label, &glideKnob.label, &drySectionLabel, &dryLevelKnob.label,
                     &dryPanKnob.label, &dryFormantKnob.label, &dryPitchKnob.label, &dryWidthKnob.label,
                     &characterSectionLabel, &humanizeKnob.label, &characterKnob.label, &spreadKnob.label,
                     &ambienceSectionLabel, &ambienceKnob.label,
                     &mixSectionLabel, &harmonyMixKnob.label, &globalSaturationKnob.label, &outputGainKnob.label,
                     &midiVelocityKnob.label, &midiVoicingLabel, &midiInversionLabel, &midiTimingLabel })
        l->setVisible(showMain);
    freezeButton.setVisible(showMain);

    harmonySectionLabel.setVisible(showHarmony);
    advancedButton.setVisible(showHarmony);
    for (auto& c : voiceColumns)
    {
        c.title.setVisible(showHarmony);
        c.enableButton.setVisible(showHarmony);
        c.soloButton.setVisible(showHarmony);
        c.intervalBox.setVisible(showHarmony);
        c.levelSlider.setVisible(showHarmony); c.levelLabel.setVisible(showHarmony);
        c.panSlider.setVisible(showHarmony); c.panLabel.setVisible(showHarmony);
        c.formantSlider.setVisible(showHarmony); c.formantLabel.setVisible(showHarmony);
        c.fineTuneSlider.setVisible(showHarmony && showAdvanced); c.fineTuneLabel.setVisible(showHarmony && showAdvanced);
        c.toneSlider.setVisible(showHarmony && showAdvanced); c.toneLabel.setVisible(showHarmony && showAdvanced);
        c.saturationSlider.setVisible(showHarmony && showAdvanced); c.saturationLabel.setVisible(showHarmony && showAdvanced);
        c.microDelaySlider.setVisible(showHarmony && showAdvanced); c.microDelayLabel.setVisible(showHarmony && showAdvanced);
        c.vibratoSlider.setVisible(showHarmony && showAdvanced); c.vibratoLabel.setVisible(showHarmony && showAdvanced);
        c.vibratoRateSlider.setVisible(showHarmony && showAdvanced); c.vibratoRateLabel.setVisible(showHarmony && showAdvanced);
    }


    // Navigation, musical context and master balance never disappear.
    modeBox.setVisible(true); modeLabel.setVisible(true);
    harmonyMixKnob.slider.setVisible(true); harmonyMixKnob.label.setVisible(true);
    outputGainKnob.slider.setVisible(true); outputGainKnob.label.setVisible(true);
    mixSectionLabel.setVisible(false);
    engineQualityBox.setVisible(showMain);
    engineQualityLabel.setVisible(showMain);
    mainPageButton.setToggleState(showMain, juce::dontSendNotification);
    harmonyPageButton.setToggleState(showHarmony, juce::dontSendNotification);
    updateModeDependentControls();
    resized();
    repaint();
}

void MirrorAudioProcessorEditor::applyPreset(int presetIndex)
{
    const juce::ScopedValueSetter<bool> applyingPreset(isApplyingPreset, true);
    auto& apvts = audioProcessor.apvts;

    auto set = [&](const juce::String& id, float value)
    {
        if (auto* p = apvts.getParameter(id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(value));
            p->endChangeGesture();
        }
    };
    auto setChoice = [&](const juce::String& id, int choiceIndex, int numChoices)
    {
        if (auto* p = apvts.getParameter(id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost((float) choiceIndex / (float) juce::jmax(1, numChoices - 1));
            p->endChangeGesture();
        }
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

    // A preset should be deterministic: reset the controls outside a voice
    // column as well as the visible voice parameters. Key, Scale, Mode, Freeze
    // and the MIDI routing choices deliberately remain untouched: they are live
    // musical context, not a hidden part of a named texture.
    set("dryPan", 0.0f); set("dryFormant", 0.0f); set("dryPitch", 0.0f); set("dryWidth", 0.5f);
    set("midiVelocity", 0.0f); set("globalSaturation", 0.04f); // Preserve master Output / Harmony Mix.
    // These three legacy controls are deliberately fixed at unity in the DSP
    // for a coherent, tight default. Keep preset/UI state honest as well.
    set("harmony", 1.0f); set("tracking", 1.0f); set("glide", 1.0f);
    setChoice("vocalRange", 0, 6);
    setChoice("harmonyStyle", 1, 4);

    switch (presetIndex)
    {
        case 1:
            setChoice("harmonyStyle", 0, 4);
            set("humanize", 0.10f); set("character", 0.0f); set("spread", 0.5f); set("ambience", 0.16f);
            set("dry", 0.18f);
            voice(0, true, 0, 0.70f, -0.22f); voice(1, true, 3, 0.56f, 0.22f); voice(2, true, 7, 0.42f, -0.45f); voice(3, true, 13, 0.30f, 0.45f);
            advanced(0, -0.08f, -4.0f, 0.14f, 0.02f, 0.0f, 0.00f, 0.42f);
            advanced(1, -0.14f, 5.0f, 0.20f, 0.03f, 1.5f, 0.02f, 0.47f);
            advanced(2, -0.20f, -7.0f, 0.30f, 0.03f, 3.0f, 0.01f, 0.39f);
            advanced(3, 0.10f, 8.0f, 0.38f, 0.02f, 5.0f, 0.01f, 0.53f);
            break;
        case 2:
            setChoice("harmonyStyle", 2, 4);
            set("humanize", 0.32f); set("character", 0.08f); set("spread", 0.86f); set("ambience", 0.25f);
            set("dry", 0.32f);
            voice(0, true, 3, 0.64f, -0.62f); voice(1, true, 7, 0.58f, 0.58f); voice(2, true, 14, 0.38f, -0.18f); voice(3, true, 4, 0.28f, 0.30f);
            advanced(0, -0.12f, -5.0f, 0.16f, 0.03f, 0.0f, 0.03f, 0.38f);
            advanced(1, -0.18f, 6.0f, 0.24f, 0.04f, 2.0f, 0.02f, 0.46f);
            advanced(2, -0.28f, -9.0f, 0.36f, 0.04f, 4.0f, 0.03f, 0.34f);
            advanced(3, 0.05f, 9.0f, 0.42f, 0.02f, 6.0f, 0.02f, 0.51f);
            break;
        case 3:
            setChoice("harmonyStyle", 3, 4);
            set("humanize", 0.24f); set("character", 0.10f); set("spread", 0.95f); set("ambience", 0.30f);
            set("dry", 0.42f);
            voice(0, true, 3, 0.68f, -0.72f); voice(1, true, 7, 0.64f, 0.72f); voice(2, true, 13, 0.42f, -0.25f); voice(3, true, 14, 0.34f, 0.28f);
            advanced(0, -0.04f, -3.0f, 0.10f, 0.04f, 0.0f, 0.04f, 0.43f);
            advanced(1, -0.14f, 4.0f, 0.18f, 0.05f, 2.5f, 0.04f, 0.49f);
            advanced(2, -0.22f, -8.0f, 0.30f, 0.05f, 5.0f, 0.03f, 0.37f);
            advanced(3, 0.12f, 8.0f, 0.40f, 0.03f, 7.0f, 0.03f, 0.56f);
            break;
        default:
            // A selector that is not state-backed must be honest about an
            // unknown choice rather than silently claiming a different patch.
            presetBox.setSelectedId(kPresetCustomId, juce::dontSendNotification);
            return;
    }
}

void MirrorAudioProcessorEditor::configureValueDisplay(juce::Slider& slider, const juce::String& id)
{
    slider.setComponentID(id);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 17);
    slider.setWantsKeyboardFocus(true);
    slider.setScrollWheelEnabled(false); // Trackpad navigation must not change a mix.
    if (auto* parameter = audioProcessor.apvts.getParameter(id))
    {
        slider.setTitle(parameter->getName(100));
        slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    }
    const bool level = id == "dry" || id.startsWith("voiceLevel");
    const bool pan = id == "dryPan" || id.startsWith("voicePan");
    const bool cents = id.startsWith("voiceFineTune");
    const bool pitch = id == "dryPitch";
    const bool delay = id.startsWith("voiceMicroDelay");
    const bool rate = id.startsWith("voiceVibratoRate");
    const bool width = id == "dryWidth";
    const bool db = id == "outputGain";
    const bool bipolar = slider.getMinimum() < 0 && !pan && !pitch && !cents && !db;
    const int voiceIndex = rate ? juce::jmax(0, id.getTrailingIntValue()-1) : 0;
    if (width) slider.getProperties().set("centre", 0.5f);
    slider.textFromValueFunction = [=](double value) -> juce::String
    {
        if (level) return value <= 0 ? "Off" : juce::String(juce::Decibels::gainToDecibels(value), 1) + " dB";
        if (db) return juce::String(value, 1) + " dB";
        if (pan) return std::abs(value) < 0.005 ? "Centre" : juce::String(value < 0 ? "L " : "R ") + juce::String(juce::roundToInt(std::abs(value)*100));
        if (cents) return juce::String(value > 0 ? "+" : "") + juce::String(value, 1) + " ct";
        if (pitch) return juce::String(value > 0 ? "+" : "") + juce::String(value, 1) + " st";
        if (delay) return juce::String(value, 1) + " ms";
        if (rate) return juce::String(3.0 + 4.2*value + voiceIndex*0.12, 1) + " Hz";
        return juce::String(bipolar && value > 0 ? "+" : "") + juce::String(juce::roundToInt(value*(width ? 200 : 100))) + "%";
    };
    slider.valueFromTextFunction = [=](const juce::String& text) -> double
    {
        const auto cleaned = text.trim();
        const double number = cleaned.retainCharacters("0123456789.-+").getDoubleValue();
        if (level) return cleaned.equalsIgnoreCase("Off") ? 0.0 : juce::Decibels::decibelsToGain(number);
        if (pan) return cleaned.equalsIgnoreCase("Centre") ? 0.0 : number*(cleaned.startsWithIgnoreCase("L") ? -0.01 : 0.01);
        if (rate) return (number - 3.0 - voiceIndex*0.12) / 4.2;
        if (db || cents || pitch || delay) return number;
        return number / (width ? 200.0 : 100.0);
    };
    juce::String help = "Drag to adjust. Double-click to restore the default. Click the value to type.";
    if (id.containsIgnoreCase("formant")) help = "Gentle vocal body / brightness shaping. Not independent formant resynthesis. " + help;
    if (id.containsIgnoreCase("tone")) help = "Higher values darken and filter the harmony so the lead stays clear. " + help;
    if (delay) help = "Fixed micro-delay for this voice, in milliseconds. " + help;
    if (id == "globalSaturation") help = "Subtle saturation on the complete output, including the lead. " + help;
    slider.setTooltip(help);
    slider.updateText();
}

void MirrorAudioProcessorEditor::timerCallback()
{
    const bool midi = modeBox.getSelectedId() == kMidiModeItemId;
    const float confidence = audioProcessor.currentConfidence.load(std::memory_order_relaxed);
    const int notes = audioProcessor.currentHeldNoteCount.load(std::memory_order_relaxed);
    statusLabel.setText(midi ? ("MIDI · " + juce::String(notes) + (notes == 1 ? " note" : " notes"))
                            : (confidence > 0.45f ? "MANUAL · Vocal detected" : "MANUAL · Ready for a vocal"),
                        juce::dontSendNotification);
    for (size_t i = 0; i < displayLevels.size(); ++i)
        displayLevels[i] = juce::jmax(displayLevels[i] * 0.78f,
            audioProcessor.currentVoiceVisualLevels[i].load(std::memory_order_relaxed));
    repaint();
}

void MirrorAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(kBg1, 0, 0, kBg2, 840, 640, false));
    g.fillAll();
    g.setColour(kAccent.withAlpha(0.16f));
    g.drawHorizontalLine(80, 24, 816);
    auto card = [&](juce::Rectangle<float> r)
    {
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillRoundedRectangle(r.translated(0, 3), 12);
        g.setColour(theme.panel);
        g.fillRoundedRectangle(r, 12);
        g.setColour(kAccent.withAlpha(0.14f));
        g.drawRoundedRectangle(r.reduced(0.5f), 12, 1);
    };
    if (currentPage == 0)
    {
        card({24, 174, 244, 340});
        card({280, 174, 292, 340});
        card({584, 174, 232, 340});
        // Four restrained level strips, never a decorative circle obscuring controls.
        for (int i = 0; i < kNumHarmonyVoices; ++i)
        {
            const float x = 301.0f + (float)i*64;
            g.setColour(kAccent.withAlpha(0.10f));
            g.fillRoundedRectangle(x, 474, 52, 5, 2);
            g.setColour(kAccent.withAlpha(0.85f));
            g.fillRoundedRectangle(x, 474, 52*juce::jlimit(0.0f, 1.0f, displayLevels[(size_t)i]), 5, 2);
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            g.setColour(kTextDim);
            g.drawText("VOICE " + juce::String(i+1), (int)x, 484, 54, 16, juce::Justification::centred);
        }
    }
    else
    {
        for (int i = 0; i < kNumHarmonyVoices; ++i)
        {
            card({24.0f + (float)i*201, 174, 189, 340});
            g.setColour(kAccent.withAlpha(0.14f));
            g.fillRoundedRectangle(40.0f + (float)i*201, 502, 157, 3, 1);
            g.setColour(kAccent);
            g.fillRoundedRectangle(40.0f + (float)i*201, 502, 157*displayLevels[(size_t)i], 3, 1);
        }
    }
    card({24, 528, 792, 88});
}

void MirrorAudioProcessorEditor::resized()
{
    titleLabel.setBounds(24, 15, 230, 42);
    creditLabel.setBounds(27, 55, 210, 16);
    presetLabel.setBounds(297, 17, 60, 15);
    presetBox.setBounds(300, 37, 238, 30);
    modeLabel.setBounds(562, 17, 90, 15);
    modeBox.setBounds(564, 37, 192, 30);
    helpButton.setBounds(784, 37, 32, 30);
    mainPageButton.setBounds(24, 99, 112, 32);
    harmonyPageButton.setBounds(142, 99, 126, 32);
    rootLabel.setBounds(301, 86, 60, 14);
    rootBox.setBounds(300, 104, 90, 28);
    scaleLabel.setBounds(405, 86, 80, 14);
    scaleBox.setBounds(404, 104, 134, 28);
    statusLabel.setBounds(558, 104, 256, 26);
    pageHintLabel.setBounds(25, 142, currentPage == 0 ? 760 : 594, 20);
    advancedButton.setBounds(674, 140, 142, 26);
    harmonySectionLabel.setBounds(0, 0, 0, 0);
    harmonyMixKnob.label.setBounds(42, 541, 158, 16);
    harmonyMixKnob.label.setJustificationType(juce::Justification::centredLeft);
    mixHintLabel.setBounds(42, 590, 534, 16);
    harmonyMixKnob.slider.setBounds(40, 558, 624, 30);
    outputGainKnob.label.setBounds(712, 540, 82, 16);
    outputGainKnob.slider.setBounds(710, 555, 86, 57);
    auto knob = [](KnobWithLabel& k, juce::Rectangle<int> r)
    {
        k.label.setBounds(r.removeFromTop(16));
        k.slider.setBounds(r);
    };
    auto small = [](juce::Slider& slider, juce::Label& label, juce::Rectangle<int> r)
    {
        label.setBounds(r.removeFromTop(14));
        slider.setBounds(r);
    };
    if (currentPage == 0)
    {
        drySectionLabel.setText("01  /  LEAD", juce::dontSendNotification);
        drySectionLabel.setBounds(40, 190, 210, 22);
        knob(dryLevelKnob, {43, 228, 92, 103});
        knob(dryPanKnob, {155, 228, 92, 103});
        knob(dryPitchKnob, {36, 358, 70, 91});
        knob(dryFormantKnob, {110, 358, 78, 91});
        knob(dryWidthKnob, {192, 358, 66, 91});
        freezeButton.setBounds(44, 470, 182, 24);
        characterSectionLabel.setText("02  /  ENSEMBLE", juce::dontSendNotification);
        characterSectionLabel.setBounds(298, 190, 250, 22);
        knob(humanizeKnob, {296, 228, 83, 103});
        knob(characterKnob, {383, 228, 83, 103});
        knob(spreadKnob, {470, 228, 83, 103});
        vocalRangeLabel.setBounds(300, 356, 112, 16);
        vocalRangeBox.setBounds(300, 378, 112, 28);
        harmonyStyleLabel.setBounds(430, 356, 120, 16);
        harmonyStyleBox.setBounds(430, 378, 120, 28);
        engineQualityLabel.setBounds(300, 417, 110, 24);
        engineQualityBox.setBounds(430, 416, 120, 28);
        ambienceSectionLabel.setText("03  /  SPACE & COLOUR", juce::dontSendNotification);
        ambienceSectionLabel.setBounds(602, 190, 198, 22);
        knob(ambienceKnob, {606, 228, 90, 103});
        knob(globalSaturationKnob, {707, 228, 90, 103});
        const bool midi = modeBox.getSelectedId() == kMidiModeItemId;
        if (midi)
        {
            midiVoicingLabel.setBounds(602, 346, 90, 16);
            midiVoicingBox.setBounds(602, 366, 92, 28);
            midiInversionLabel.setBounds(705, 346, 94, 16);
            midiInversionBox.setBounds(705, 366, 94, 28);
            midiTimingLabel.setBounds(602, 418, 98, 16);
            midiTimingBox.setBounds(602, 440, 98, 28);
            knob(midiVelocityKnob, {716, 411, 78, 88});
        }
    }
    else
    {
        for (int i = 0; i < kNumHarmonyVoices; ++i)
        {
            const int x = 24 + i*201;
            auto& c = voiceColumns[(size_t)i];
            c.title.setBounds(x+12, 184, 165, 20);
            c.enableButton.setBounds(x+16, 208, 73, 22);
            c.soloButton.setBounds(x+102, 208, 75, 22);
            c.intervalBox.setBounds(x+16, 238, 157, 28);
            small(c.levelSlider, c.levelLabel, {x+7, 280, 58, 76});
            small(c.panSlider, c.panLabel, {x+65, 280, 58, 76});
            small(c.formantSlider, c.formantLabel, {x+123, 280, 60, 76});
            // All three Advanced rows have fixed non-zero bounds, regardless
            // of page visibility; toggling cannot consume a layout rectangle.
            small(c.fineTuneSlider, c.fineTuneLabel, {x+7, 358, 58, 70});
            small(c.toneSlider, c.toneLabel, {x+65, 358, 58, 70});
            small(c.saturationSlider, c.saturationLabel, {x+123, 358, 60, 70});
            small(c.microDelaySlider, c.microDelayLabel, {x+7, 430, 58, 70});
            small(c.vibratoSlider, c.vibratoLabel, {x+65, 430, 58, 70});
            small(c.vibratoRateSlider, c.vibratoRateLabel, {x+123, 430, 60, 70});
        }
    }
}
