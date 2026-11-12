#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    // Correspondance MIDI (kit GM) : note -> instrument
    int noteToInst (int note)
    {
        switch (note)
        {
            case 35: case 36: return tr8::BD;
            case 38: case 40: return tr8::SD;
            case 41: case 43: return tr8::LT;
            case 45: case 47: return tr8::MT;
            case 48: case 50: return tr8::HT;
            case 37:          return tr8::RS;
            case 39:          return tr8::HC;
            case 42: case 44: return tr8::CH;
            case 46:          return tr8::OH;
            case 49: case 57: return tr8::CC;
            case 51: case 59: return tr8::RC;
            default:          return -1;
        }
    }
}

AatiniAudioProcessor::AatiniAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createParameterLayout())
{
    loadDefaultPattern();
}

AudioProcessorValueTreeState::ParameterLayout AatiniAudioProcessor::createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&] (const String& id, const String& name, float lo, float hi, float def, float step = 0.001f)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def));
    };

    addFloat (ParamIDs::volume,  "Volume",  0.0f, 1.0f, 0.8f);
    addFloat (ParamIDs::tempo,   "Tempo",   40.0f, 240.0f, 120.0f, 0.1f);
    addFloat (ParamIDs::shuffle, "Shuffle", 0.0f, 1.0f, 0.0f);
    addFloat (ParamIDs::accent,  "Accent",  0.0f, 1.0f, 0.5f);
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::sync, 1 }, "Sync host", true));

    for (int i = 0; i < tr8::kNumInst; ++i)
    {
        const String n = tr8::kInstNames[i];
        addFloat (instId (i, "level"), n + " Level", 0.0f, 1.0f, 0.8f);
        addFloat (instId (i, "tune"),  n + " Tune",  0.0f, 1.0f, 0.5f);
        addFloat (instId (i, "decay"), n + " Decay", 0.0f, 1.0f, 0.5f);
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { instId (i, "model"), 1 }, n + " 909", false));

        if (i == tr8::BD) { addFloat (instId (i, "attack"), n + " Attack", 0.0f, 1.0f, 0.5f); addFloat (instId (i, "comp"), n + " Comp", 0.0f, 1.0f, 0.0f); }
        if (i == tr8::SD) { addFloat (instId (i, "snappy"), n + " Snappy", 0.0f, 1.0f, 0.5f); addFloat (instId (i, "comp"), n + " Comp", 0.0f, 1.0f, 0.0f); }
    }
    return layout;
}

void AatiniAudioProcessor::loadDefaultPattern()
{
    auto& p = seq.pattern;
    p.clear();
    for (int s : { 0, 4, 8, 12 })           p.set (0, tr8::BD, s);
    for (int s : { 4, 12 })                 p.set (0, tr8::SD, s);
    for (int s : { 2, 6, 10, 14 })          p.set (0, tr8::OH, s);
    for (int s : { 0, 2, 4, 6, 8, 10, 12, 14 }) p.set (0, tr8::CH, s, true, true);
    p.setAccent (0, 0);
}

bool AatiniAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == AudioChannelSet::stereo();
}

void AatiniAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    seq.prepare (sampleRate);
    hostWasPlaying = false;
}

void AatiniAudioProcessor::updateEngineParams()
{
    auto get = [this] (const String& id) { return apvts.getRawParameterValue (id)->load(); };

    for (int i = 0; i < tr8::kNumInst; ++i)
    {
        tr8::InstParams p;
        p.model = get (instId (i, "model")) > 0.5f ? tr8::M909 : tr8::M808;
        p.tune  = (get (instId (i, "tune")) - 0.5f) * 2.0f;
        p.decay = get (instId (i, "decay"));

        if (i == tr8::BD) { p.attack = get (instId (i, "attack")); p.comp = get (instId (i, "comp")); }
        if (i == tr8::SD) { p.snappy = get (instId (i, "snappy")); p.comp = get (instId (i, "comp")); }

        const float lvl = get (instId (i, "level"));
        engine.setParams ((tr8::Inst) i, p);
        engine.setLevel ((tr8::Inst) i, lvl * lvl * 1.25f);
    }
}

void AatiniAudioProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    buffer.clear();

    const int n = buffer.getNumSamples();
    if (n == 0) return;

    updateEngineParams();

    // ---- transport : suit l'hôte (Ableton) ou la lecture interne -------------------
    bool hostPlaying = false, hasPpq = false;
    double hostBpm = 0.0, hostPpq = 0.0;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto b = pos->getBpm())         hostBpm = *b;
            if (auto q = pos->getPpqPosition()) { hostPpq = *q; hasPpq = true; }
        }

    const bool  sync       = apvts.getRawParameterValue (ParamIDs::sync)->load() > 0.5f;
    const float tempoParam = apvts.getRawParameterValue (ParamIDs::tempo)->load();
    const bool  followHost = sync && hostPlaying;
    const bool  shouldRun  = followHost || internalPlay.load();
    const double tempo     = (followHost && hostBpm > 1.0) ? hostBpm : (double) tempoParam;

    seq.setTempo ((float) tempo);
    seq.setShuffle (apvts.getRawParameterValue (ParamIDs::shuffle)->load());
    seq.setAccentLevel (apvts.getRawParameterValue (ParamIDs::accent)->load());
    seq.setVariationMask (varMask.load());

    if (shouldRun && ! seq.isRunning())
    {
        if (followHost && hasPpq)
        {
            const double stepLen   = getSampleRate() * 60.0 / (tempo * 4.0);
            const double stepPos   = hostPpq * 4.0;
            const double nextStep  = std::ceil (stepPos - 1.0e-6);
            seq.startAt ((long long) nextStep, (nextStep - stepPos) * stepLen);
        }
        else
        {
            seq.start();
        }
    }
    else if (! shouldRun && seq.isRunning())
    {
        seq.stop();
    }
    hostWasPlaying = hostPlaying;

    const int fill = fillRequest.exchange (-1);
    if (fill >= 0 && seq.isRunning()) seq.triggerFill (fill);

    // ---- événements de la période : séquenceur, MIDI, boutons de l'interface --------
    struct Ev { int off; int inst; float vel; };
    Ev evs[512];
    int numEv = 0;
    auto push = [&] (int off, int inst, float vel) { if (numEv < 512) evs[numEv++] = { off, inst, vel }; };

    seq.advance (n, [&] (int off, tr8::Inst i, float v) { push (off, (int) i, v); });

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            const int inst = noteToInst (msg.getNoteNumber());
            if (inst >= 0) push (std::min (metadata.samplePosition, n - 1), inst, msg.getFloatVelocity());
        }
    }

    const uint32_t ui = uiTriggers.exchange (0);
    for (int i = 0; i < tr8::kNumInst; ++i)
        if (ui & (1u << i)) push (0, i, 0.9f);

    // ---- rendu -----------------------------------------------------------------------
    const float gain = apvts.getRawParameterValue (ParamIDs::volume)->load();
    float* left  = buffer.getWritePointer (0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int k = 0; k < n; ++k)
    {
        for (int e = 0; e < numEv; ++e)
            if (evs[e].off == k)
                engine.trigger ((tr8::Inst) evs[e].inst, evs[e].vel);

        const float s = std::tanh (engine.process() * gain);
        left[k] = s;
        if (right != nullptr) right[k] = s;
    }

    currentStep.store (seq.isRunning() ? seq.getCurrentStep() : -1);
    currentSlot.store (seq.getCurrentSlot());
}

AudioProcessorEditor* AatiniAudioProcessor::createEditor()
{
    return new AatiniAudioProcessorEditor (*this);
}

void AatiniAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("pattern", String (seq.pattern.toHex()), nullptr);
    state.setProperty ("varMask", (int) varMask.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AatiniAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            const auto tree = ValueTree::fromXml (*xml);
            apvts.replaceState (tree);

            if (tree.hasProperty ("pattern"))
                seq.pattern.fromHex (tree["pattern"].toString().toStdString());

            if (tree.hasProperty ("varMask"))
                varMask.store ((uint8_t) (int) tree["varMask"]);
        }
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AatiniAudioProcessor();
}
