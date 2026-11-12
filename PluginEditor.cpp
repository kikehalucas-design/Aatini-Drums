#include "PluginEditor.h"
#include "MeghirLogo.h"

#include <algorithm>
#include <cmath>

// ============================================================================
//  Interface d'Aatini Drums. Tout est dessiné dans un espace de 1200 x 680 unités,
//  mis à l'échelle selon la taille de la fenêtre.
// ============================================================================

namespace
{
constexpr float kW = 1200.0f;
constexpr float kH = 680.0f;

const juce::Colour kText    { 0xffe6e6ea };
const juce::Colour kTextDim { 0xff8e8e96 };
const juce::Colour kRed     { 0xffe5392b };
const juce::Colour kOrange  { 0xfff28c28 };
const juce::Colour kYellow  { 0xffffd52e };
const juce::Colour kWhite   { 0xfff2f2f2 };
const juce::Colour kGreen   { 0xff2fbf4a };
const juce::Colour kCyan    { 0xff2ec8ff };

void drawLabel (juce::Graphics& g, const juce::String& text, float cx, float cy, float size,
                bool bold = false, juce::Colour colour = kTextDim)
{
    g.setColour (colour);
    g.setFont (bold ? juce::FontOptions (size).withStyle ("Bold") : juce::FontOptions (size));
    g.drawText (text, juce::Rectangle<float> (cx - 80.0f, cy - size, 160.0f, size * 2.0f), juce::Justification::centred, false);
}
} // namespace

//==============================================================================
// Aspect des potards et faders
//==============================================================================
void AatiniLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff34343a));
    g.strokePath (track, juce::PathStrokeType (2.2f));

    juce::Path value;
    value.addCentredArc (c.x, c.y, r, r, 0.0f, startAngle, angle, true);
    g.setColour (kOrange);
    g.strokePath (value, juce::PathStrokeType (2.8f));

    const float br = r - 5.0f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4a52), c.x - br * 0.4f, c.y - br * 0.6f,
                                             juce::Colour (0xff17171a), c.x + br * 0.5f, c.y + br * 0.8f, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
    g.setColour (juce::Colour (0xff0a0a0c));
    g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 1.2f);

    const auto p1 = c.getPointOnCircumference (br * 0.2f, angle);
    const auto p2 = c.getPointOnCircumference (br - 2.0f, angle);
    g.setColour (kWhite);
    g.drawLine (p1.x, p1.y, p2.x, p2.y, 2.4f);
}

void AatiniLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cx = (float) x + (float) width * 0.5f;

    g.setColour (juce::Colour (0xff070708));
    g.fillRoundedRectangle (cx - 3.0f, (float) y, 6.0f, (float) height, 3.0f);

    g.setColour (juce::Colour (0xff5a5a62));
    for (int k = 0; k <= 10; ++k)
    {
        const float ty = (float) y + (float) height * (float) k / 10.0f;
        g.drawLine (cx - 15.0f, ty, cx - 9.0f, ty, 1.0f);
        g.drawLine (cx + 9.0f, ty, cx + 15.0f, ty, 1.0f);
    }

    const juce::Rectangle<float> cap (cx - 14.0f, sliderPos - 10.0f, 28.0f, 20.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (1.5f, 2.0f), 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d8dc), 0.0f, cap.getY(),
                                             juce::Colour (0xff85858b), 0.0f, cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colour (0xff202024));
    g.drawLine (cap.getX() + 3.0f, cap.getCentreY(), cap.getRight() - 3.0f, cap.getCentreY(), 2.0f);
}

//==============================================================================
// Bouton à voyant
//==============================================================================
void Pad::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (2.0f);
    const int st = state ? state() : 0;

    g.setColour (juce::Colour (0xff27272b));
    g.fillRoundedRectangle (r, 5.0f);

    if (st > 0)
    {
        g.setColour (colour.withAlpha (st == 2 ? 0.95f : 0.38f));
        g.fillRoundedRectangle (r.reduced (3.0f), 4.0f);
    }

    g.setColour (juce::Colour (0xff4c4c53));
    g.drawRoundedRectangle (r, 5.0f, 1.2f);

    if (marker && marker())
    {
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 2.2f);
    }

    const juce::String t = label ? label() : text;
    if (t.isNotEmpty())
    {
        g.setColour (st == 2 ? juce::Colour (0xff101010) : juce::Colour (0xffd6d6da));
        g.setFont (compact ? 12.0f : 15.0f);
        g.drawText (t, getLocalBounds(), juce::Justification::centred, false);
    }
}

void Pad::mouseDown (const juce::MouseEvent& e)
{
    if (onClick) onClick (e);
    repaint();
}

//==============================================================================
// Éditeur
//==============================================================================
juce::Slider* AatiniAudioProcessorEditor::addKnob (const juce::String& id, float x, float y, float size)
{
    auto s = std::make_unique<juce::Slider> (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox);
    s->setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    s->setMouseDragSensitivity (240);
    s->setSliderSnapsToMousePosition (false);
    addAndMakeVisible (*s);

    attachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, id, *s));
    placed.push_back ({ s.get(), x, y, size, size });
    sliders.push_back (std::move (s));
    return sliders.back().get();
}

juce::Slider* AatiniAudioProcessorEditor::addFader (const juce::String& id, float x, float y, float w, float h)
{
    auto s = std::make_unique<juce::Slider> (juce::Slider::LinearVertical, juce::Slider::NoTextBox);
    s->setSliderSnapsToMousePosition (false);
    addAndMakeVisible (*s);

    attachments.push_back (std::make_unique<SliderAttachment> (proc.apvts, id, *s));
    placed.push_back ({ s.get(), x, y, w, h });
    sliders.push_back (std::move (s));
    return sliders.back().get();
}

Pad* AatiniAudioProcessorEditor::addPad (float x, float y, float w, float h)
{
    auto p = std::make_unique<Pad>();
    addAndMakeVisible (*p);
    placed.push_back ({ p.get(), x, y, w, h });
    pads.push_back (std::move (p));
    return pads.back().get();
}

bool AatiniAudioProcessorEditor::getBool (const char* id) const
{
    return proc.apvts.getRawParameterValue (id)->load() > 0.5f;
}

void AatiniAudioProcessorEditor::setBool (const juce::String& id, bool v)
{
    if (auto* b = dynamic_cast<juce::AudioParameterBool*> (proc.apvts.getParameter (id)))
    {
        b->beginChangeGesture();
        *b = v;
        b->endChangeGesture();
    }
}

void AatiniAudioProcessorEditor::toggleBool (const juce::String& id)
{
    if (auto* v = proc.apvts.getRawParameterValue (id))
        setBool (id, v->load() <= 0.5f);
}

AatiniAudioProcessorEditor::AatiniAudioProcessorEditor (AatiniAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    // ---- section générale ----------------------------------------------------------
    addKnob (ParamIDs::volume,  470.0f, 20.0f, 56.0f);
    addKnob (ParamIDs::tempo,   550.0f, 20.0f, 56.0f)->setPopupDisplayEnabled (true, true, this);
    addKnob (ParamIDs::shuffle, 630.0f, 20.0f, 56.0f);
    addKnob (ParamIDs::accent,  710.0f, 20.0f, 56.0f);
    labels.push_back ({ "VOLUME",  498.0f, 94.0f, 12.0f, false });
    labels.push_back ({ "TEMPO",   578.0f, 94.0f, 12.0f, false });
    labels.push_back ({ "SHUFFLE", 658.0f, 94.0f, 12.0f, false });
    labels.push_back ({ "ACCENT",  738.0f, 94.0f, 12.0f, false });

    {
        auto* play = addPad (810.0f, 26.0f, 82.0f, 40.0f);
        play->text = "PLAY"; play->colour = kGreen;
        play->state = [this] { return proc.currentStep.load() >= 0 ? 2 : 0; };
        play->onClick = [this] (const juce::MouseEvent&) { proc.internalPlay.store (! proc.internalPlay.load()); };

        auto* sync = addPad (900.0f, 26.0f, 82.0f, 40.0f);
        sync->text = "SYNC"; sync->colour = kCyan;
        sync->state = [this] { return getBool (ParamIDs::sync) ? 2 : 0; };
        sync->onClick = [this] (const juce::MouseEvent&) { toggleBool (ParamIDs::sync); };

        for (int f = 0; f < 2; ++f)
        {
            auto* fill = addPad (990.0f + (float) f * 90.0f, 26.0f, 82.0f, 40.0f);
            fill->text = "FILL " + juce::String (f + 1); fill->colour = kYellow;
            fill->onClick = [this, f] (const juce::MouseEvent&) { proc.fillRequest.store (f); };
        }
    }

    // ---- variations A..H ---------------------------------------------------------------
    labels.push_back ({ "VARIATION", 392.0f, 143.0f, 13.0f, true });
    for (int v = 0; v < 8; ++v)
    {
        auto* pad = addPad (470.0f + (float) v * 72.0f, 122.0f, 66.0f, 38.0f);
        pad->text = juce::String::charToString ((juce::juce_wchar) ('A' + v));
        pad->colour = kOrange;
        pad->state = [this, v]
        {
            if ((proc.varMask.load() >> v) & 1) return 2;
            return proc.editVar.load() == v ? 1 : 0;
        };
        pad->onClick = [this, v] (const juce::MouseEvent& e)
        {
            uint8_t m = proc.varMask.load();
            if (e.mods.isShiftDown())
            {
                m = (uint8_t) (m ^ (1u << v));
                if (m == 0) m = (uint8_t) (1u << v);
            }
            else
            {
                m = (uint8_t) (1u << v);
            }
            proc.varMask.store (m);
            proc.editVar.store (v);
        };
    }
    labels.push_back ({ "clic : choisir  |  Maj + clic : enchainer plusieurs variations", 1062.0f, 143.0f, 11.0f, false });

    // ---- les 11 instruments -----------------------------------------------------------------
    auto knobsFor = [] (int inst) -> std::vector<std::pair<juce::String, juce::String>>
    {
        switch (inst)
        {
            case tr8::BD: return { { "attack", "ATTACK" }, { "tune", "TUNE" }, { "comp", "COMP" }, { "decay", "DECAY" } };
            case tr8::SD: return { { "tune", "TUNE" }, { "snappy", "SNAPPY" }, { "comp", "COMP" }, { "decay", "DECAY" } };
            case tr8::RS: return { { "tune", "TUNE" } };
            default:      return { { "tune", "TUNE" }, { "decay", "DECAY" } };
        }
    };

    for (int i = 0; i < tr8::kNumInst; ++i)
    {
        const float x0 = 40.0f + (float) i * 100.0f, cx = x0 + 50.0f;

        labels.push_back ({ tr8::kInstNames[i], cx, 208.0f, 22.0f, true });

        auto* model = addPad (cx - 28.0f, 224.0f, 56.0f, 24.0f);
        model->compact = true;
        const juce::String modelId = instId (i, "model");
        model->colour = kCyan;
        model->label = [this, modelId] { return getBool (modelId.toRawUTF8()) ? juce::String ("909") : juce::String ("808"); };
        model->state = [this, modelId] { return getBool (modelId.toRawUTF8()) ? 2 : 0; };
        model->onClick = [this, modelId] (const juce::MouseEvent&) { toggleBool (modelId); };

        const auto knobs = knobsFor (i);
        for (size_t k = 0; k < knobs.size(); ++k)
        {
            const float kx = x0 + 6.0f + (float) (k % 2) * 46.0f;
            const float ky = 256.0f + (float) (k / 2) * 68.0f;
            addKnob (instId (i, knobs[k].first.toRawUTF8()), kx, ky, 40.0f);
            labels.push_back ({ knobs[k].second, kx + 20.0f, ky + 52.0f, 10.0f, false });
        }

        addFader (instId (i, "level"), cx - 20.0f, 406.0f, 40.0f, 116.0f);

        auto* sel = addPad (x0 + 14.0f, 530.0f, 72.0f, 38.0f);
        sel->text = tr8::kInstNames[i];
        sel->colour = kWhite;
        sel->state = [this, i] { return proc.editInst.load() == i ? 2 : 0; };
        sel->onClick = [this, i] (const juce::MouseEvent&)
        {
            proc.editInst.store (i);
            proc.uiTriggers.fetch_or (1u << i);
        };
    }

    // ---- les 16 pas ------------------------------------------------------------------------------
    {
        auto* acc = addPad (40.0f, 596.0f, 74.0f, 46.0f);
        acc->text = "ACCENT"; acc->compact = true; acc->colour = kWhite;
        acc->state = [this] { return proc.editInst.load() == tr8::kNumInst ? 2 : 0; };
        acc->onClick = [this] (const juce::MouseEvent&) { proc.editInst.store (tr8::kNumInst); };
    }

    for (int s = 0; s < 16; ++s)
    {
        auto* pad = addPad (130.0f + (float) s * 65.5f, 596.0f, 60.0f, 46.0f);
        const juce::Colour groupColours[4] = { kRed, kOrange, kYellow, kWhite };
        pad->colour = groupColours[s / 4];
        pad->text = juce::String (s + 1);
        pad->compact = true;

        pad->state = [this, s]
        {
            const int slot = proc.editVar.load(), inst = proc.editInst.load();
            const auto& pat = proc.getPattern();
            if (inst >= tr8::kNumInst) return ((pat.accent[slot] >> s) & 1) ? 2 : 0;
            if (! ((pat.step[slot][inst] >> s) & 1)) return 0;
            return ((pat.weak[slot][inst] >> s) & 1) ? 1 : 2;
        };

        pad->marker = [this, s] { return proc.currentStep.load() == s && proc.currentSlot.load() == proc.editVar.load(); };

        pad->onClick = [this, s] (const juce::MouseEvent& e)
        {
            auto& pat = proc.getPattern();
            const int slot = proc.editVar.load(), inst = proc.editInst.load();

            if (inst >= tr8::kNumInst)
            {
                pat.setAccent (slot, s, ((pat.accent[slot] >> s) & 1) == 0);
                return;
            }

            const bool on   = (pat.step[slot][inst] >> s) & 1;
            const bool weak = (pat.weak[slot][inst] >> s) & 1;
            const bool alt  = e.mods.isRightButtonDown() || e.mods.isAltDown();

            if (alt) pat.set (slot, (tr8::Inst) inst, s, true, ! (on && weak));   // note faible <-> note normale
            else     pat.set (slot, (tr8::Inst) inst, s, ! on, false);
        };
    }
    labels.push_back ({ "clic : activer / couper un pas  |  Alt + clic : note faible", 1060.0f, 664.0f, 11.0f, false });

    setResizable (true, true);
    setResizeLimits (600, 340, 1800, 1020);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (1080, 612);

    startTimerHz (30);
}

AatiniAudioProcessorEditor::~AatiniAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AatiniAudioProcessorEditor::timerCallback()
{
    for (auto& p : pads)
        p->repaint();
}

void AatiniAudioProcessorEditor::resized()
{
    if (getWidth() <= 0) return;

    const float s = (float) getWidth() / kW;
    for (const auto& p : placed)
        p.comp->setBounds (juce::Rectangle<int> ((int) std::lround (p.x * s), (int) std::lround (p.y * s),
                                                 (int) std::lround (p.w * s), (int) std::lround (p.h * s)));
}

void AatiniAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e1e21), 0.0f, 0.0f,
                                             juce::Colour (0xff0d0d0f), 0.0f, (float) getHeight(), false));
    g.fillAll();

    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kW));

    // Logo et nom du plugin
    meghir::drawLogo (g, juce::Rectangle<float> (14.0f, 8.0f, 100.0f, 100.0f));

    g.setColour (kWhite);
    g.setFont (juce::FontOptions ("Brush Script MT", 56.0f, juce::Font::plain));
    g.drawFittedText ("Aatini Drums", 124, 28, 320, 70, juce::Justification::centredLeft, 1, 0.6f);

    // Cadres
    g.setColour (juce::Colour (0xff3a3a40));
    g.drawLine (24.0f, 172.0f, 1176.0f, 172.0f, 1.2f);
    g.drawRoundedRectangle (24.0f, 184.0f, 1152.0f, 394.0f, 14.0f, 1.4f);
    g.drawRoundedRectangle (24.0f, 586.0f, 1152.0f, 66.0f, 14.0f, 1.4f);

    for (const auto& l : labels)
        drawLabel (g, l.text, l.cx, l.cy, l.size, l.bold, l.bold ? kText : kTextDim);
}
