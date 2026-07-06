#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

// ASTRA-TUNE: real-time pitch corrector.
// YIN pitch detection on 2x-decimated audio + a dual-tap crossfading
// delay-line pitch shifter, retuned toward the nearest scale note.
class AstraTuneProcessor : public juce::AudioProcessor
{
public:
    AstraTuneProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return "ASTRA-TUNE"; }
    bool acceptsMidi() const override                   { return false; }
    bool producesMidi() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.0; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts;

    // live readouts for the editor (written on the audio thread)
    std::atomic<float> uiFreq       { 0.0f };
    std::atomic<float> uiMidi       { -1.0f };
    std::atomic<float> uiTargetMidi { -1.0f };
    std::atomic<float> uiClarity    { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void runDetect();
    void updateTarget (int keyRoot, const bool* scaleMask, float humanize);
    float readRb (int ch, double delay) const;

    double fs = 48000.0;

    // pitch shifter
    int win = 1440;
    std::vector<std::vector<float>> rb;
    int rbSize = 0, rbMask = 0, wpos = 0;
    double phase = 0.0;

    // detection (YIN on 2x-decimated signal)
    double decFs = 24000.0;
    int decSize = 1024, decPos = 0;
    bool decToggle = false;
    float decHold = 0.0f;
    std::vector<float> decBuf, lin, dbuf, cbuf;
    int maxLag = 0, minLag = 2;
    int hop = 512, sinceHop = 0;

    // correction
    double shift = 0.0, targetShift = 0.0;
    double curFreq = 0.0, curClarity = 0.0, curMidi = -1.0;
    int targetMidi = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AstraTuneProcessor)
};
