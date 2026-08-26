#pragma once

#include <array>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// The editor is drawn entirely from JUCE components and vector paths.  The
// reference art is a visual brief only; it is not composited into the plug-in.
class MirrorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit MirrorAudioProcessorEditor(MirrorAudioProcessor&);
    ~MirrorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct KnobWithLabel
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct VoiceColumn
    {
        juce::Label title;
        juce::ToggleButton enableButton, soloButton;
        juce::ComboBox intervalBox;
        juce::Slider levelSlider, panSlider, formantSlider;
        juce::Slider fineTuneSlider, toneSlider, saturationSlider, microDelaySlider, vibratoSlider, vibratoRateSlider;
        juce::Label levelLabel, panLabel, formantLabel;
        juce::Label fineTuneLabel, toneLabel, saturationLabel, microDelayLabel, vibratoLabel, vibratoRateLabel;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAttachment, soloAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> intervalAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment, panAttachment, formantAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fineTuneAttachment, toneAttachment, saturationAttachment, microDelayAttachment, vibratoAttachment, vibratoRateAttachment;
    };

    void timerCallback() override;
    void setupKnob(KnobWithLabel&, const juce::String& parameterID, const juce::String& caption);
    void setupVoiceColumn(VoiceColumn&, int voiceIndex);
    void configureLabel(juce::Label&, const juce::String& text, float fontSize, bool centred = true);
    void configureComboBox(juce::ComboBox&, const juce::String& tooltip);
    void applyPreset(int presetIndex);
    void showPage(int pageIndex);
    void updateControlVisibility();

    void layoutHeader();
    void layoutMain();
    void layoutHarmony();
    void placeKnob(KnobWithLabel&, juce::Rectangle<int> bounds);
    void placeSmallKnob(juce::Slider&, juce::Label&, juce::Rectangle<int> bounds);

    void drawBackground(juce::Graphics&) const;
    void drawHeader(juce::Graphics&) const;
    void drawPanel(juce::Graphics&, juce::Rectangle<float> bounds, bool enabled, bool ornate = true) const;
    void drawSectionTitle(juce::Graphics&, const juce::String&, juce::Rectangle<int> bounds) const;
    void drawMirrorVisualizer(juce::Graphics&) const;
    void drawVoiceGlyph(juce::Graphics&, int voiceIndex, float x, float groundY, float energy, float alpha) const;
    void drawMainPanels(juce::Graphics&) const;
    void drawHarmonyPanels(juce::Graphics&) const;

    MirrorAudioProcessor& audioProcessor;
    std::unique_ptr<juce::LookAndFeel_V4> mirrorLookAndFeel;

    juce::Label titleLabel, creditLabel;
    juce::TextButton mainPageButton, harmonyPageButton, advancedButton;
    int currentPage = 0;
    bool showAdvanced = false;
    int lastMode = -1;

    juce::ComboBox modeBox, rootBox, scaleBox, presetBox;
    juce::ComboBox vocalRangeBox, harmonyStyleBox, midiVoicingBox, midiInversionBox;
    juce::Label modeLabel, rootLabel, scaleLabel, presetLabel;
    juce::Label vocalRangeLabel, harmonyStyleLabel, midiVoicingLabel, midiInversionLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment, rootAttachment, scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> vocalRangeAttachment, harmonyStyleAttachment, midiVoicingAttachment, midiInversionAttachment;

    KnobWithLabel trackingKnob, glideKnob;
    juce::ToggleButton freezeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    KnobWithLabel dryLevelKnob, dryPanKnob, dryFormantKnob, dryPitchKnob, dryWidthKnob;
    std::array<VoiceColumn, kNumHarmonyVoices> voiceColumns;
    KnobWithLabel humanizeKnob, characterKnob, spreadKnob;
    KnobWithLabel ambienceKnob, harmonyMixKnob, globalSaturationKnob, outputGainKnob;

    std::array<juce::Rectangle<int>, kNumHarmonyVoices> voicePanelBounds;
    juce::Rectangle<int> dryPanelBounds, characterPanelBounds, outputPanelBounds, mirrorBounds;

    std::array<float, kNumHarmonyVoices> visualPan {};
    std::array<float, kNumHarmonyVoices> visualEnergy {};
    std::array<float, kNumHarmonyVoices> visualPresence {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MirrorAudioProcessorEditor)
};
