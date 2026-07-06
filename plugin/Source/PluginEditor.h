#pragma once

#include "PluginProcessor.h"

class AstraTuneEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit AstraTuneEditor (AstraTuneProcessor&);
    ~AstraTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    AstraTuneProcessor& proc;

    juce::ComboBox keyBox, scaleBox;
    juce::Slider retuneSlider, humanizeSlider, mixSlider;
    juce::ToggleButton bypassButton { "BYPASS" };
    juce::Label noteLabel, freqLabel;
    juce::Label keyLabel, scaleLabel, retuneLabel, humanizeLabel, mixLabel;

    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<ComboAttachment> keyAtt, scaleAtt;
    std::unique_ptr<SliderAttachment> retuneAtt, humanizeAtt, mixAtt;
    std::unique_ptr<ButtonAttachment> bypassAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AstraTuneEditor)
};
