#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr double kLn2Over12 = 0.057762265046662109;

    const bool kScaleMasks[5][12] = {
        { 1,0,1,0,1,1,0,1,0,1,0,1 },   // Major
        { 1,0,1,1,0,1,0,1,1,0,1,0 },   // Natural Minor
        { 1,0,1,0,1,0,0,1,0,1,0,0 },   // Major Pentatonic
        { 1,0,0,1,0,1,0,1,0,0,1,0 },   // Minor Pentatonic
        { 1,1,1,1,1,1,1,1,1,1,1,1 },   // Chromatic
    };
}

AstraTuneProcessor::AstraTuneProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout AstraTuneProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "key", 1 }, "Key",
        StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "scale", 1 }, "Scale",
        StringArray { "Major", "Natural Minor", "Major Pentatonic", "Minor Pentatonic", "Chromatic" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "retune", 1 }, "Retune Speed",
        NormalisableRange<float> (0.0f, 400.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "humanize", 1 }, "Humanize",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f));
    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

juce::AudioProcessorParameter* AstraTuneProcessor::getBypassParameter() const
{
    return apvts.getParameter ("bypass");
}

bool AstraTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out)
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void AstraTuneProcessor::prepareToPlay (double sampleRate, int)
{
    fs  = sampleRate;
    win = (int) std::round (0.03 * fs);

    rbSize = 1;
    while (rbSize < 4 * win)
        rbSize <<= 1;
    rbMask = rbSize - 1;
    rb.assign (2, std::vector<float> ((size_t) rbSize, 0.0f));
    wpos  = 0;
    phase = 0.0;

    decFs = fs / 2.0;
    decSize = 1024;
    decBuf.assign ((size_t) decSize, 0.0f);
    lin.assign ((size_t) decSize, 0.0f);
    decPos = 0;
    decToggle = false;
    decHold = 0.0f;
    maxLag = juce::jmin ((int) (decFs / 60.0), decSize / 2 - 1);
    minLag = juce::jmax (2, (int) (decFs / 1000.0));
    dbuf.assign ((size_t) maxLag + 1, 0.0f);
    cbuf.assign ((size_t) maxLag + 1, 0.0f);
    hop = 512;
    sinceHop = 0;

    shift = targetShift = 0.0;
    curFreq = curClarity = 0.0;
    curMidi = -1.0;
    targetMidi = -1;

    setLatencySamples (win / 2 + 2);
}

float AstraTuneProcessor::readRb (int ch, double delay) const
{
    const double pos = (double) wpos - delay;
    const int i0 = (int) std::floor (pos);
    const float frac = (float) (pos - i0);
    const auto& b = rb[(size_t) ch];
    const float s0 = b[(size_t) (i0 & rbMask)];
    const float s1 = b[(size_t) ((i0 + 1) & rbMask)];
    return s0 + (s1 - s0) * frac;
}

void AstraTuneProcessor::runDetect()
{
    const int n = decSize;
    for (int i = 0; i < n; ++i)
        lin[(size_t) i] = decBuf[(size_t) ((decPos + i) % n)];

    double rms = 0.0;
    for (int i = 0; i < n; ++i)
        rms += lin[(size_t) i] * lin[(size_t) i];
    rms = std::sqrt (rms / n);
    if (rms < 0.004)
    {
        curFreq = 0.0;
        curClarity = 0.0;
        return;
    }

    const int half = n / 2;
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        float s = 0.0f;
        const float* b = lin.data();
        for (int i = 0; i < half; ++i)
        {
            const float df = b[i] - b[i + lag];
            s += df * df;
        }
        dbuf[(size_t) lag] = s;
    }

    // cumulative-mean-normalized difference (YIN)
    cbuf[0] = 1.0f;
    double run = 0.0;
    for (int lag = 1; lag <= maxLag; ++lag)
    {
        run += dbuf[(size_t) lag];
        cbuf[(size_t) lag] = (float) (dbuf[(size_t) lag] * lag / (run > 0.0 ? run : 1e-12));
    }

    int tau = -1;
    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        if (cbuf[(size_t) lag] < 0.15f)
        {
            while (lag + 1 <= maxLag && cbuf[(size_t) (lag + 1)] < cbuf[(size_t) lag])
                ++lag;
            tau = lag;
            break;
        }
    }
    if (tau < 0)
    {
        curFreq = 0.0;
        curClarity = 0.0;
        return;
    }

    double t = tau;
    if (tau > 1 && tau < maxLag)
    {
        const double a = cbuf[(size_t) (tau - 1)];
        const double b = cbuf[(size_t) tau];
        const double g = cbuf[(size_t) (tau + 1)];
        const double denom = a - 2.0 * b + g;
        if (std::abs (denom) > 1e-12)
            t = tau + 0.5 * (a - g) / denom;
    }

    curFreq = decFs / t;
    curClarity = juce::jlimit (0.0, 1.0, 1.0 - cbuf[(size_t) tau]);
}

void AstraTuneProcessor::updateTarget (int keyRoot, const bool* scaleMask, float humanize)
{
    if (curFreq > 0.0 && curClarity > 0.5)
    {
        const double midi = 69.0 + 12.0 * std::log2 (curFreq / 440.0);
        const int center = (int) std::round (midi);
        int best = -1;
        double bestDist = 1.0e9;
        for (int nn = center - 7; nn <= center + 7; ++nn)
        {
            const int pc = ((nn - keyRoot) % 12 + 12) % 12;
            if (! scaleMask[pc])
                continue;
            const double dist = std::abs (midi - nn);
            if (dist < bestDist)
            {
                bestDist = dist;
                best = nn;
            }
        }
        curMidi = midi;
        targetMidi = best;
        targetShift = best < 0 ? 0.0 : (best - midi) * (1.0 - humanize);
    }
    else
    {
        curMidi = -1.0;
        targetMidi = -1;
        targetShift = 0.0;
    }

    uiFreq.store ((float) curFreq);
    uiMidi.store ((float) curMidi);
    uiTargetMidi.store ((float) targetMidi);
    uiClarity.store ((float) curClarity);
}

void AstraTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (nCh == 0 || n == 0)
        return;

    const int keyRoot = (int) apvts.getRawParameterValue ("key")->load();
    const int scaleIdx = juce::jlimit (0, 4, (int) apvts.getRawParameterValue ("scale")->load());
    const bool* mask = kScaleMasks[scaleIdx];
    const float retuneMs = apvts.getRawParameterValue ("retune")->load();
    const float humanize = apvts.getRawParameterValue ("humanize")->load() / 100.0f;
    const float mixv = apvts.getRawParameterValue ("mix")->load() / 100.0f;
    const bool bypassed = apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    const double retuneSec = juce::jmax (retuneMs, 0.5f) / 1000.0;
    const double alpha = 1.0 - std::exp (-1.0 / (fs * retuneSec));
    const double pi = juce::MathConstants<double>::pi;

    float* data[2] = { buffer.getWritePointer (0),
                       nCh > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int i = 0; i < n; ++i)
    {
        float mono = data[0][i];
        if (nCh > 1)
            mono = (mono + data[1][i]) * 0.5f;

        // feed decimated detection buffer (2-sample average = cheap AA)
        if (decToggle)
        {
            decBuf[(size_t) decPos] = (mono + decHold) * 0.5f;
            decPos = (decPos + 1) % decSize;
        }
        else
        {
            decHold = mono;
        }
        decToggle = ! decToggle;

        if (++sinceHop >= hop)
        {
            sinceHop = 0;
            runDetect();
            updateTarget (keyRoot, mask, humanize);
        }

        shift += (targetShift - shift) * alpha;
        const double ratio = std::exp (shift * kLn2Over12);

        phase += (1.0 - ratio) / win;
        phase -= std::floor (phase);
        double p2 = phase + 0.5;
        if (p2 >= 1.0)
            p2 -= 1.0;

        const float g1 = (float) std::sin (pi * phase);
        const float g2 = (float) std::sin (pi * p2);
        const double d1 = phase * win + 2.0;
        const double d2 = p2 * win + 2.0;

        for (int ch = 0; ch < nCh; ++ch)
        {
            const float x = data[ch][i];
            rb[(size_t) ch][(size_t) wpos] = x;
            const float y = g1 * readRb (ch, d1) + g2 * readRb (ch, d2);
            data[ch][i] = bypassed ? x : mixv * y + (1.0f - mixv) * x;
        }
        wpos = (wpos + 1) & rbMask;
    }
}

void AstraTuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AstraTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* AstraTuneProcessor::createEditor()
{
    return new AstraTuneEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AstraTuneProcessor();
}
