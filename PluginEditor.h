#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "PluginProcessor.h"

// Aspect des potards et des faders.
class AatiniLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

// Bouton à voyant : pas du séquenceur, variation, fill, lecture, sélection d'instrument...
class Pad : public juce::Component
{
public:
    Pad() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    juce::String text;
    juce::Colour colour { 0xffe5392b };
    bool compact = false;

    std::function<int()> state;                                // 0 éteint, 1 faible, 2 allumé
    std::function<bool()> marker;                              // tête de lecture
    std::function<juce::String()> label;                       // texte dynamique (facultatif)
    std::function<void (const juce::MouseEvent&)> onClick;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override;
};

class AatiniAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit AatiniAudioProcessorEditor (AatiniAudioProcessor&);
    ~AatiniAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Label  { juce::String text; float cx, cy, size; bool bold; };
    struct Placed { juce::Component* comp; float x, y, w, h; };

    void timerCallback() override;

    juce::Slider* addKnob  (const juce::String& paramId, float x, float y, float size);
    juce::Slider* addFader (const juce::String& paramId, float x, float y, float w, float h);
    Pad*          addPad   (float x, float y, float w, float h);

    bool getBool (const char* id) const;
    void toggleBool (const juce::String& id);
    void setBool (const juce::String& id, bool v);

    AatiniAudioProcessor& proc;
    AatiniLookAndFeel laf;

    std::vector<std::unique_ptr<juce::Slider>> sliders;
    std::vector<std::unique_ptr<Pad>> pads;
    std::vector<Placed> placed;
    std::vector<Label> labels;

    // Les attachements doivent être détruits avant les potards : on les déclare en dernier.
    std::vector<std::unique_ptr<SliderAttachment>> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AatiniAudioProcessorEditor)
};
