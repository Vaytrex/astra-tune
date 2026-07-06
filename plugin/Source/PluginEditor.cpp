#include "PluginEditor.h"

namespace
{
    const juce::Colour kBg      { 0xff0a0e14 };
    const juce::Colour kPanel   { 0xff10161f };
    const juce::Colour kLine    { 0xff223044 };
    const juce::Colour kCyan    { 0xff35e0ff };
    const juce::Colour kText    { 0xffc8d6e5 };
    const juce::Colour kMuted   { 0xff5c6f85 };

    const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    void styleRotary (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        s.setColour (juce::Slider::rotarySliderFillColourId, kCyan);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, kLine);
        s.setColour (juce::Slider::thumbColourId, kCyan);
        s.setColour (juce::Slider::textBoxTextColourId, kText);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    void styleCaption (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, kMuted);
        l.setFont (juce::FontOptions (11.0f));
    }
}

AstraTuneEditor::AstraTuneEditor (AstraTuneProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    for (int i = 0; i < 12; ++i)
        keyBox.addItem (kNoteNames[i], i + 1);
    scaleBox.addItemList ({ "Major", "Natural Minor", "Major Pentatonic", "Minor Pentatonic", "Chromatic" }, 1);

    for (auto* box : { &keyBox, &scaleBox })
    {
        box->setColour (juce::ComboBox::backgroundColourId, kPanel);
        box->setColour (juce::ComboBox::textColourId, kText);
        box->setColour (juce::ComboBox::outlineColourId, kLine);
        box->setColour (juce::ComboBox::arrowColourId, kCyan);
        addAndMakeVisible (*box);
    }

    styleRotary (retuneSlider);
    retuneSlider.setTextValueSuffix (" ms");
    styleRotary (humanizeSlider);
    humanizeSlider.setTextValueSuffix (" %");
    styleRotary (mixSlider);
    mixSlider.setTextValueSuffix (" %");
    addAndMakeVisible (retuneSlider);
    addAndMakeVisible (humanizeSlider);
    addAndMakeVisible (mixSlider);

    bypassButton.setColour (juce::ToggleButton::textColourId, kMuted);
    bypassButton.setColour (juce::ToggleButton::tickColourId, kCyan);
    addAndMakeVisible (bypassButton);

    noteLabel.setText ("--", juce::dontSendNotification);
    noteLabel.setJustificationType (juce::Justification::centred);
    noteLabel.setColour (juce::Label::textColourId, kCyan);
    noteLabel.setFont (juce::FontOptions (52.0f, juce::Font::bold));
    addAndMakeVisible (noteLabel);

    freqLabel.setText ("0.0 Hz", juce::dontSendNotification);
    freqLabel.setJustificationType (juce::Justification::centred);
    freqLabel.setColour (juce::Label::textColourId, kMuted);
    freqLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (freqLabel);

    styleCaption (keyLabel, "KEY");
    styleCaption (scaleLabel, "SCALE");
    styleCaption (retuneLabel, "RETUNE SPEED");
    styleCaption (humanizeLabel, "HUMANIZE");
    styleCaption (mixLabel, "MIX");
    for (auto* l : { &keyLabel, &scaleLabel, &retuneLabel, &humanizeLabel, &mixLabel })
        addAndMakeVisible (*l);

    keyAtt      = std::make_unique<ComboAttachment>  (proc.apvts, "key", keyBox);
    scaleAtt    = std::make_unique<ComboAttachment>  (proc.apvts, "scale", scaleBox);
    retuneAtt   = std::make_unique<SliderAttachment> (proc.apvts, "retune", retuneSlider);
    humanizeAtt = std::make_unique<SliderAttachment> (proc.apvts, "humanize", humanizeSlider);
    mixAtt      = std::make_unique<SliderAttachment> (proc.apvts, "mix", mixSlider);
    bypassAtt   = std::make_unique<ButtonAttachment> (proc.apvts, "bypass", bypassButton);

    setSize (640, 360);
    startTimerHz (30);
}

AstraTuneEditor::~AstraTuneEditor() = default;

void AstraTuneEditor::timerCallback()
{
    const float freq = proc.uiFreq.load();
    const float target = proc.uiTargetMidi.load();

    if (freq > 0.0f && target >= 0.0f)
    {
        const int t = (int) target;
        noteLabel.setText (juce::String (kNoteNames[((t % 12) + 12) % 12]) + juce::String (t / 12 - 1),
                           juce::dontSendNotification);
        freqLabel.setText (juce::String (freq, 1) + " Hz", juce::dontSendNotification);
    }
    else
    {
        noteLabel.setText ("--", juce::dontSendNotification);
        freqLabel.setText ("no pitch", juce::dontSendNotification);
    }
}

void AstraTuneEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kPanel, 0.0f, 0.0f, kBg, 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.setColour (kCyan);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("ASTRA-TUNE", 22, 16, 300, 30, juce::Justification::centredLeft);

    g.setColour (kMuted);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("REAL-TIME PITCH CORRECTOR  -  EVO CLASS", 24, 44, 400, 14, juce::Justification::centredLeft);

    g.setColour (kLine);
    g.drawLine (20.0f, 64.0f, (float) getWidth() - 20.0f, 64.0f);

    // note display well
    g.setColour (juce::Colour (0xff060a10));
    g.fillRoundedRectangle (22.0f, 76.0f, 160.0f, 110.0f, 8.0f);
    g.setColour (kLine);
    g.drawRoundedRectangle (22.0f, 76.0f, 160.0f, 110.0f, 8.0f, 1.0f);
}

void AstraTuneEditor::resized()
{
    noteLabel.setBounds (22, 84, 160, 70);
    freqLabel.setBounds (22, 154, 160, 24);

    keyLabel.setBounds (210, 80, 120, 14);
    keyBox.setBounds (210, 96, 120, 28);
    scaleLabel.setBounds (350, 80, 170, 14);
    scaleBox.setBounds (350, 96, 170, 28);
    bypassButton.setBounds (545, 96, 80, 28);

    const int knobY = 200, knobH = 130, knobW = 130;
    retuneLabel.setBounds (60, knobY - 16, knobW, 14);
    retuneSlider.setBounds (60, knobY, knobW, knobH);
    humanizeLabel.setBounds (255, knobY - 16, knobW, 14);
    humanizeSlider.setBounds (255, knobY, knobW, knobH);
    mixLabel.setBounds (450, knobY - 16, knobW, 14);
    mixSlider.setBounds (450, knobY, knobW, knobH);
}
