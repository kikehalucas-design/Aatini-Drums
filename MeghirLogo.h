#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>

// ============================================================================
//  Logo Meghir : smiley jaune moutarde, yeux en spirale, casque, petite bouche
//  en "o", dans un médaillon crème, avec un arc de vumètre vert / orange / rouge.
//
//  Réutilisable tel quel dans tous les plugins : il suffit de copier ce fichier
//  à côté des sources, puis d'appeler dans le dessin de l'éditeur :
//
//      meghir::drawLogo (g, juce::Rectangle<float> (x, y, 90.0f, 90.0f));
//
//  Le paramètre "level" (0 à 1) règle le nombre de barres allumées de l'arc.
//  À 1 elles le sont toutes ; on pourra plus tard le relier au niveau du signal.
// ============================================================================

namespace meghir
{
inline void drawLogo (juce::Graphics& g, juce::Rectangle<float> area, float level = 1.0f)
{
    const juce::Colour paper (0xfff2e8d5), ink (0xff1b1b1b), face (0xfff2b705);
    const juce::Colour zones[3] = { juce::Colour (0xff2fbf4a), juce::Colour (0xfff28c28), juce::Colour (0xffe5392b) };

    juce::Graphics::ScopedSaveState outer (g);

    // On dessine dans un carré de 100 x 100 unités, ramené à la zone demandée.
    const float s = std::min (area.getWidth(), area.getHeight()) / 100.0f;
    g.addTransform (juce::AffineTransform::scale (s).translated (area.getCentreX() - 50.0f * s,
                                                                  area.getCentreY() - 50.0f * s));

    // Médaillon crème
    g.setColour (paper);
    g.fillEllipse (1.0f, 1.0f, 98.0f, 98.0f);
    g.setColour (ink.withAlpha (0.55f));
    g.drawEllipse (1.0f, 1.0f, 98.0f, 98.0f, 1.6f);

    // Contenu un peu réduit pour garder une marge dans le médaillon
    g.addTransform (juce::AffineTransform::scale (0.88f, 0.88f, 50.0f, 50.0f));

    // ---- Arc de vumètre : 28 barres du vert au rouge -------------------------
    const auto polar = [] (float r, float deg)
    {
        const float a = juce::degreesToRadians (deg);
        return juce::Point<float> (50.0f + r * std::sin (a), 50.0f - r * std::cos (a));
    };

    for (int i = 0; i < 28; ++i)
    {
        const float t   = (float) i / 27.0f;
        const float len = 3.0f + 11.0f * std::pow (t, 1.3f);
        const float ang = -135.0f + 270.0f * t;
        const auto p = polar (34.0f, ang);
        const auto q = polar (34.0f + len, ang);
        const juce::Colour c = t < 0.62f ? zones[0] : (t < 0.86f ? zones[1] : zones[2]);

        juce::Path bar;
        bar.startNewSubPath (p);
        bar.lineTo (q);

        g.setColour (c.withAlpha (t <= level + 0.0001f ? 1.0f : 0.16f));
        g.strokePath (bar, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ---- Smiley (dessiné dans son propre repère, incliné de 10 degrés) -------
    {
        juce::Graphics::ScopedSaveState state (g);
        g.addTransform (juce::AffineTransform::rotation (juce::degreesToRadians (-10.0f), 50.0f, 56.0f)
                            .translated (-50.0f, -56.0f)
                            .scaled (0.46f)
                            .translated (50.0f, 50.0f));

        // Visage : remplissage légèrement décalé (effet "décalage d'impression"), puis contour
        g.setColour (face);
        g.fillEllipse (53.0f - 32.0f, 58.5f - 32.0f, 64.0f, 64.0f);
        g.setColour (ink);
        g.drawEllipse (50.0f - 32.0f, 56.0f - 32.0f, 64.0f, 64.0f, 5.0f);

        const juce::PathStrokeType stroke (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        // Arceau du casque
        juce::Path band;
        band.addCentredArc (50.0f, 56.0f, 38.0f, 38.0f, 0.0f,
                            -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (band, stroke);

        // Yeux en spirale
        for (float cx : { 40.0f, 60.0f })
        {
            juce::Path spiral;
            for (int i = 0; i <= 40; ++i)
            {
                const float t = (float) i / 40.0f;
                const float a = t * 5.0f * juce::MathConstants<float>::pi;
                const float k = t * 7.5f;
                const float x = cx + k * std::cos (a);
                const float y = 49.0f + k * std::sin (a);

                if (i == 0) spiral.startNewSubPath (x, y);
                else        spiral.lineTo (x, y);
            }
            g.strokePath (spiral, stroke);
        }

        // Petite bouche en "o"
        g.setColour (ink);
        g.fillEllipse (45.0f, 62.5f, 10.0f, 13.0f);
        g.drawEllipse (45.0f, 62.5f, 10.0f, 13.0f, 2.5f);

        // Oreillettes du casque
        g.fillRoundedRectangle (6.0f, 47.0f, 13.0f, 23.0f, 5.0f);
        g.fillRoundedRectangle (81.0f, 47.0f, 13.0f, 23.0f, 5.0f);
    }
}
} // namespace meghir
