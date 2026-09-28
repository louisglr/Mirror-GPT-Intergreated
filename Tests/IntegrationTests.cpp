#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

namespace
{
int failures = 0;
void check(bool ok, const char* message)
{
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void set(MirrorAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.apvts.getParameter(id);
    check(parameter != nullptr, "missing parameter");
    if (parameter) parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
float get(MirrorAudioProcessor& p, const juce::String& id)
{ return p.apvts.getRawParameterValue(id)->load(); }

void prepare(MirrorAudioProcessor& p, int block = 128, double rate = 48000)
{
    p.setPlayConfigDetails(2, 2, rate, block);
    p.prepareToPlay(rate, block);
}

void stateTests()
{
    MirrorAudioProcessor p;
    set(p, "harmonyMix", 0.37f);
    set(p, "engineQuality", 1);
    set(p, "rootNote", 8);
    juce::MemoryBlock data;
    p.getStateInformation(data);
    MirrorAudioProcessor restored;
    restored.setStateInformation(data.getData(), (int)data.getSize());
    check(std::abs(get(restored, "harmonyMix") - 0.37f) < 0.001f, "mix state round-trip");
    check(get(restored, "engineQuality") == 1, "engine state round-trip");
    check(get(restored, "rootNote") == 8, "key state round-trip");
    auto old = p.apvts.copyState();
    old.removeChild(old.getChildWithProperty("id", "harmonyMix"), nullptr);
    old.removeChild(old.getChildWithProperty("id", "engineQuality"), nullptr);
    auto xml = old.createXml();
    juce::AudioProcessor::copyXmlToBinary(*xml, data);
    restored.setStateInformation(data.getData(), (int)data.getSize());
    check(get(restored, "harmonyMix") == 1, "old session must restore unity mix");
    check(get(restored, "engineQuality") == 0, "old session must restore Original engine");
    const char junk[] = "not plugin state";
    restored.setStateInformation(junk, (int)sizeof(junk));
    check(get(restored, "harmonyMix") == 1, "malformed state changed mix");
}

void mixTests()
{
    MirrorAudioProcessor full, half, silent;
    for (auto* p : { &full, &half, &silent })
    {
        set(*p, "dry", 0); set(*p, "humanize", 0); set(*p, "globalSaturation", 0);
        set(*p, "ambience", 0); prepare(*p);
    }
    set(half, "harmonyMix", 0.5f); set(silent, "harmonyMix", 0);
    juce::AudioBuffer<float> a(2, 128), b(2, 128), c(2, 128);
    double energy = 0, error = 0, muted = 0;
    for (int block = 0; block < 600; ++block)
    {
        for (int n = 0; n < 128; ++n)
        {
            const float x = 0.08f * std::sin((float)(block*128+n)*juce::MathConstants<float>::twoPi*200/48000);
            for (auto* buffer : { &a, &b, &c })
                for (int ch = 0; ch < 2; ++ch) buffer->setSample(ch, n, x);
        }
        juce::MidiBuffer ma, mb, mc;
        full.processBlock(a, ma); half.processBlock(b, mb); silent.processBlock(c, mc);
        if (block < 300) continue; // Settled smoothing and detector.
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 128; ++n)
            {
                const float x = a.getSample(ch,n), y = b.getSample(ch,n), z = c.getSample(ch,n);
                check(std::isfinite(x) && std::isfinite(y) && std::isfinite(z), "nonfinite mixed output");
                energy += (double)x*x;
                error += std::abs(y - 0.5f*x);
                muted += std::abs(z);
            }
    }
    check(energy > 0.001, "mix test has no audible harmony");
    check(error < 0.01, "50% harmony mix is not half linear level below limiter");
    check(muted < 1.0e-6, "zero mix leaks harmonies or ambience");

    MirrorAudioProcessor lead;
    set(lead, "harmonyMix", 0); set(lead, "dry", 1); set(lead, "globalSaturation", 0);
    prepare(lead);
    double leadEnergy = 0;
    for (int block = 0; block < 50; ++block)
    {
        a.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < 128; ++n) a.setSample(ch, n, 0.05f);
        juce::MidiBuffer midi; lead.processBlock(a, midi);
        if (block > 30) leadEnergy += a.getRMSLevel(0,0,128);
    }
    check(leadEnergy > 0.1, "zero harmony mix also muted the lead");
}

void midiTests(int timing)
{
    MirrorAudioProcessor p;
    set(p, "mode", 1); set(p, "midiTiming", (float)timing);
    prepare(p, 64);
    juce::AudioBuffer<float> audio(2,64);
    auto send = [&](const juce::MidiMessage& message)
    {
        juce::MidiBuffer midi; midi.addEvent(message, 17);
        audio.clear(); p.processBlock(audio, midi);
        for (int i = 0; i < 40; ++i)
        { juce::MidiBuffer empty; audio.clear(); p.processBlock(audio, empty); }
    };
    send(juce::MidiMessage::noteOn(1, 60, 0.5f));
    send(juce::MidiMessage::noteOn(2, 60, 0.8f));
    send(juce::MidiMessage::noteOff(1, 60));
    check(p.currentHeldNoteCount.load() == 1, "Live/Aligned channel ownership regression");
    send(juce::MidiMessage::controllerEvent(2, 64, 127));
    send(juce::MidiMessage::noteOff(2, 60));
    check(p.currentHeldNoteCount.load() == 1, "sustain did not hold note");
    send(juce::MidiMessage::controllerEvent(2, 121, 0));
    check(p.currentHeldNoteCount.load() == 0, "reset controllers left sustained note");
}

void bypassLatencyTest()
{
    MirrorAudioProcessor p;
    prepare(p, 64);
    juce::AudioBuffer<float> block(2,64);
    for (int i = 0; i < 100; ++i)
    { block.clear(); juce::MidiBuffer midi; p.processBlockBypassed(block, midi); }
    int peakIndex = -1;
    float peak = 0;
    for (int offset = 0; offset < p.getLatencySamples()+128; offset += 64)
    {
        block.clear();
        if (offset == 0) { block.setSample(0,0,0.25f); block.setSample(1,0,0.25f); }
        juce::MidiBuffer midi; p.processBlockBypassed(block, midi);
        for (int n = 0; n < 64; ++n)
            if (std::abs(block.getSample(0,n)) > peak)
            { peak = std::abs(block.getSample(0,n)); peakIndex = offset+n; }
    }
    check(peakIndex == p.getLatencySamples(), "host bypass delay does not equal reported PDC");
    check(std::abs(peak-0.25f) < 1.0e-5f, "host bypass changed impulse level");
}

void automationStressTest(double rate)
{
    MirrorAudioProcessor p;
    prepare(p, 512, rate);
    const int sizes[] { 1, 32, 64, 127, 256, 512 };
    juce::AudioBuffer<float> storage(2,512);
    int sample = 0, blockIndex = 0;
    while (sample < (int)(rate*3))
    {
        const int size = sizes[blockIndex%6];
        juce::AudioBuffer<float> audio(storage.getArrayOfWritePointers(), 2, size);
        for (int n = 0; n < size; ++n)
        {
            const float x = 0.15f*std::sin((float)(sample+n)*juce::MathConstants<float>::twoPi*180/(float)rate);
            audio.setSample(0,n,x); audio.setSample(1,n,x*0.9f);
        }
        if (blockIndex%31 == 0)
        {
            set(p,"harmonyMix", (float)(blockIndex%5)/4);
            set(p,"engineQuality", (float)(blockIndex%2));
            set(p,"voiceSaturation1", (float)(blockIndex%7)/6);
            set(p,"voiceFormant2", (float)(blockIndex%3)-1);
        }
        if (blockIndex == 99) audio.setSample(0,0,std::numeric_limits<float>::quiet_NaN());
        juce::MidiBuffer midi;
        p.processBlock(audio,midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < size; ++n)
                check(std::isfinite(audio.getSample(ch,n)) && std::abs(audio.getSample(ch,n)) <= 1.0f,
                      "automation/sample-rate stress emitted invalid output");
        sample += size; ++blockIndex;
    }
}

juce::Component* component(juce::Component& parent, const juce::String& id)
{
    for (auto* c : parent.getChildren())
        if (c->getComponentID() == id) return c;
    return nullptr;
}
void click(juce::Component& editor, const juce::String& text, bool toggle = false)
{
    for (auto* c : editor.getChildren())
        if (auto* b = dynamic_cast<juce::TextButton*>(c); b && b->getButtonText() == text)
        {
            if (toggle) b->setToggleState(!b->getToggleState(), juce::dontSendNotification);
            if (b->onClick) b->onClick();
            return;
        }
    check(false, "missing navigation button");
}
void uiTests(const juce::File& snapshots)
{
    MirrorAudioProcessor p;
    prepare(p);
    std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
    auto capture = [&](const char* name)
    {
        for (auto* c : editor->getChildren())
            if (c->isVisible() && (dynamic_cast<juce::Slider*>(c) || dynamic_cast<juce::ComboBox*>(c) || dynamic_cast<juce::Button*>(c)))
            {
                check(editor->getLocalBounds().contains(c->getBounds()), "control outside editor");
                check(c->getWidth() >= 30 && c->getHeight() >= 20, "control has unusable bounds");
            }
        if (snapshots.getFullPathName().isEmpty()) return;
        check(snapshots.createDirectory().wasOk(), "snapshot directory failed");
        auto stream = snapshots.getChildFile(name).createOutputStream();
        check(stream != nullptr, "snapshot file failed");
        if (stream) { juce::PNGImageFormat png; check(png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f), *stream), "PNG snapshot failed"); }
    };
    capture("main.png");
    auto* mix = dynamic_cast<juce::Slider*>(component(*editor, "harmonyMix"));
    check(mix && mix->isEnabled() && mix->isVisible(), "mix unavailable on Main");
    if (mix) mix->setValue(0.42, juce::sendNotificationSync);
    check(std::abs(get(p,"harmonyMix")-0.42f) < 0.001f, "mix attachment disconnected");
    click(*editor, "HARMONY");
    check(mix && mix->isVisible(), "mix disappeared on Voices");
    auto* fine = component(*editor, "voiceFineTune1");
    check(fine && !fine->isVisible(), "Advanced leaked into basic page");
    capture("voices.png");
    click(*editor, "ADVANCED", true);
    check(fine && fine->isVisible() && fine->getHeight() >= 50, "Advanced does not reveal usable controls");
    capture("advanced.png");
    set(p, "mode", 1);
    auto* interval = component(*editor, "voiceInterval1");
    check(interval && !interval->isEnabled(), "Manual interval still enabled in MIDI");
    click(*editor, "MAIN"); capture("midi.png");
    set(p, "rootNote", 9); set(p, "scaleType", 2);
    auto* preset = dynamic_cast<juce::ComboBox*>(component(*editor, "preset"));
    check(preset != nullptr, "preset selector missing");
    if (preset) preset->setSelectedId(2, juce::sendNotificationSync);
    check(get(p,"rootNote") == 9 && get(p,"scaleType") == 2, "preset overwrote musical context");
    check(std::abs(get(p,"harmonyMix")-0.42f) < 0.001f, "preset overwrote master mix");
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    stateTests(); mixTests(); midiTests(0); midiTests(1); bypassLatencyTest();
    automationStressTest(44100); automationStressTest(48000); automationStressTest(96000);
    uiTests(argc > 1 ? juce::File(juce::String::fromUTF8(argv[1])) : juce::File {});
    std::cout << (failures ? "Integration tests FAILED\n" : "Integration tests passed\n");
    return failures ? 1 : 0;
}
