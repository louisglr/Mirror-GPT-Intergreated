#pragma once

#include <array>
#include <memory>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

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
    void setupKnob(KnobWithLabel& knob, const juce::String& parameterID, const juce::String& displayName);
    void setupVoiceColumn(VoiceColumn& column, int voiceIndex);
    void configureLabel(juce::Label& label, const juce::String& text, float fontSize, bool centred = true);
    void configureComboBox(juce::ComboBox& box, const juce::String& tooltip);
    void applyPreset(int presetIndex);
    void showPage(int pageIndex);
    void updateControlVisibility();
    void layoutMain(juce::Rectangle<int> body);
    void layoutHarmony(juce::Rectangle<int> body);
    void placeKnob(KnobWithLabel& knob, juce::Rectangle<int> bounds);
    void placeSmallKnob(juce::Slider& slider, juce::Label& label, juce::Rectangle<int> bounds);

    void drawBackground(juce::Graphics&) const;
    void drawHeader(juce::Graphics&) const;
    void drawMainPanels(juce::Graphics&) const;
    void drawHarmonyPanels(juce::Graphics&) const;
    void drawMirrorVisualizer(juce::Graphics&) const;
    void drawMirrorPanel(juce::Graphics&, juce::Rectangle<float> bounds, bool enabled, bool ornate = true) const;
    void drawSectionTitle(juce::Graphics&, const juce::String& text, juce::Rectangle<int> bounds) const;
    void drawVoiceGlyph(juce::Graphics&, int voiceIndex, float x, float groundY, float energy, float alpha) const;

    MirrorAudioProcessor& audioProcessor;
    std::unique_ptr<juce::LookAndFeel_V4> mirrorLookAndFeel;

    juce::Label titleLabel, creditLabel;
    juce::TextButton mainPageButton, harmonyPageButton, advancedButton;
    int currentPage = 0;
    bool showAdvanced = false;
    int lastMode = -1;

    juce::ComboBox modeBox;
    juce::ComboBox vocalRangeBox, harmonyStyleBox, midiVoicingBox, midiInversionBox;
    juce::Label modeLabel, vocalRangeLabel, harmonyStyleLabel, midiVoicingLabel, midiInversionLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment, vocalRangeAttachment, harmonyStyleAttachment, midiVoicingAttachment, midiInversionAttachment;

    juce::ComboBox rootBox, scaleBox, presetBox;
    juce::Label rootLabel, scaleLabel, presetLabel, inputSectionLabel;
    KnobWithLabel trackingKnob, glideKnob;
    juce::ToggleButton freezeButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttachment, scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeAttachment;

    KnobWithLabel dryLevelKnob, dryPanKnob, dryFormantKnob, dryPitchKnob, dryWidthKnob;
    std::array<VoiceColumn, kNumHarmonyVoices> voiceColumns;
    KnobWithLabel humanizeKnob, characterKnob, spreadKnob;
    KnobWithLabel ambienceKnob, harmonyMixKnob, globalSaturationKnob;

    std::array<juce::Rectangle<int>, kNumHarmonyVoices> voicePanelBounds;
    std::array<juce::Rectangle<int>, kNumHarmonyVoices> voiceLevelBounds;
    juce::Rectangle<int> dryPanelBounds, characterPanelBounds, outputPanelBounds, mirrorBounds;

    std::array<float, kNumHarmonyVoices> visualPan {};
    std::array<float, kNumHarmonyVoices> visualEnergy {};
    std::array<float, kNumHarmonyVoices> visualPresence {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MirrorAudioProcessorEditor)
};
