#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// ============================================================================
//  TR8Voices : les 11 instruments de la TR-8S (BD SD LT MT HT RS HC CH OH CC RC),
//  en deux modèles de son : 808 et 909. Aucune dépendance à JUCE.
//
//  Ce sont des modèles comportementaux construits à partir des recettes connues
//  des circuits d'origine. Tous les chiffres marqués CALIBRATION sont à ajuster
//  à l'oreille.
// ============================================================================

namespace tr8
{
enum Inst { BD = 0, SD, LT, MT, HT, RS, HC, CH, OH, CC, RC, kNumInst };
enum Model { M808 = 0, M909 = 1 };

static const char* const kInstNames[kNumInst] = { "BD", "SD", "LT", "MT", "HT", "RS", "HC", "CH", "OH", "CC", "RC" };

// Réglages d'un instrument (tout en 0..1, sauf tune en -1..+1).
struct InstParams
{
    Model model  = M808;
    float tune   = 0.0f;   // -1..+1
    float decay  = 0.5f;   // 0..1
    float attack = 0.5f;   // BD uniquement
    float snappy = 0.5f;   // SD uniquement
    float comp   = 0.0f;   // BD et SD uniquement
};

namespace dsp
{
constexpr float kPi = 3.14159265358979f;

inline float expMap (float v01, float lo, float hi) { return lo * std::pow (hi / lo, std::clamp (v01, 0.0f, 1.0f)); }

struct Noise
{
    uint32_t s = 22222u;
    float next()
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float) (int32_t) s * (1.0f / 2147483648.0f);
    }
};

struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowpass (double fs, float f, float q)
    {
        f = std::min (f, (float) fs * 0.45f);
        const float w = 2 * kPi * f / (float) fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = (1 - c) * 0.5f / a0; b1 = (1 - c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void setHighpass (double fs, float f, float q)
    {
        f = std::min (f, (float) fs * 0.45f);
        const float w = 2 * kPi * f / (float) fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = (1 + c) * 0.5f / a0; b1 = -(1 + c) / a0; b2 = b0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    void setBandpass (double fs, float f, float q)
    {
        f = std::min (f, (float) fs * 0.45f);
        const float w = 2 * kPi * f / (float) fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
};

// Enveloppe exponentielle : amplitude * exp(-t / tau)
struct Env
{
    float v = 0, k = 0;
    void trig (float amp, float tau, double fs) { v = amp; k = std::exp (-1.0f / (std::max (tau, 1.0e-5f) * (float) fs)); }
    float next() { const float o = v; v *= k; return o; }
};

inline float squareWave (double ph) { return ph < 0.5 ? 1.0f : -1.0f; }
inline float sat (float x, float g) { return std::tanh (g * x) / std::tanh (g); }
} // namespace dsp

// ----------------------------------------------------------------------------
class BassDrum
{
public:
    void prepare (double sr) { fs = sr; }

    void trigger (float vel, const InstParams& p)
    {
        using namespace dsp;
        prm = p; v = vel; phase = 0.0;
        const float tune = std::pow (2.0f, p.tune * 0.75f);

        if (p.model == M808)
        {   // CALIBRATION
            fEnd = 50.0f * tune; fStart = fEnd * 2.6f; tauP = 0.020f;
            tauA = expMap (p.decay, 0.10f, 1.0f); drive = 1.3f; clickAmt = 0.05f + p.attack * 0.45f; clickTau = 0.003f; clickLp = 3500.0f;
        }
        else
        {
            fEnd = 55.0f * tune; fStart = fEnd * 5.0f; tauP = 0.012f;
            tauA = expMap (p.decay, 0.06f, 0.6f); drive = 3.0f; clickAmt = 0.15f + p.attack * 0.7f; clickTau = 0.002f; clickLp = 7000.0f;
        }
        amp.trig (1.0f, tauA, fs); pit.trig (1.0f, tauP, fs); click.trig (clickAmt, clickTau, fs);
        lp.setLowpass (fs, clickLp, 0.7f); lp.reset();
        active = true;
    }

    float process()
    {
        using namespace dsp;
        if (! active) return 0.0f;

        const float f = fEnd + (fStart - fEnd) * pit.next();
        phase += f / fs; phase -= std::floor (phase);

        float x = std::sin (2.0f * kPi * (float) phase) * amp.next();
        x += lp.process (noise.next()) * click.next();
        x = sat (x * 0.9f, drive * (1.0f + prm.comp * 2.5f));

        active = amp.v > 1.0e-4f;
        return x * v;
    }

private:
    double fs = 44100.0, phase = 0.0;
    InstParams prm;
    float v = 1, fEnd = 50, fStart = 130, tauP = 0.02f, tauA = 0.3f, drive = 1.3f, clickAmt = 0.2f, clickTau = 0.003f, clickLp = 3500;
    dsp::Env amp, pit, click;
    dsp::Biquad lp;
    dsp::Noise noise;
    bool active = false;
};

// ----------------------------------------------------------------------------
class SnareDrum
{
public:
    void prepare (double sr) { fs = sr; }

    void trigger (float vel, const InstParams& p)
    {
        using namespace dsp;
        prm = p; v = vel; ph1 = ph2 = 0.0;
        const float tune = std::pow (2.0f, p.tune * 0.75f);

        if (p.model == M808)
        {   // CALIBRATION
            f1 = 180.0f * tune; f2 = 330.0f * tune; boost = 0.25f; tauP = 0.012f;
            tauBody = expMap (p.decay, 0.04f, 0.28f); tauNoise = expMap (p.decay, 0.08f, 0.45f);
            noiseLevel = 0.15f + 1.1f * p.snappy; bodyLevel = 0.9f;
            hp.setHighpass (fs, 1800.0f, 0.7f); bp.setBandpass (fs, 4200.0f, 0.4f); drive = 1.2f;
        }
        else
        {
            f1 = 190.0f * tune; f2 = 330.0f * tune; boost = 0.6f; tauP = 0.008f;
            tauBody = expMap (p.decay, 0.05f, 0.3f); tauNoise = expMap (p.decay, 0.10f, 0.5f);
            noiseLevel = 0.2f + 1.3f * p.snappy; bodyLevel = 1.0f;
            hp.setHighpass (fs, 1500.0f, 0.7f); bp.setBandpass (fs, 3800.0f, 0.5f); drive = 2.0f;
        }
        hp.reset(); bp.reset();
        body.trig (1.0f, tauBody, fs); body2.trig (0.7f, tauBody * 0.8f, fs); nz.trig (1.0f, tauNoise, fs); pit.trig (1.0f, tauP, fs);
        active = true;
    }

    float process()
    {
        using namespace dsp;
        if (! active) return 0.0f;

        const float pe = 1.0f + boost * pit.next();
        ph1 += f1 * pe / fs; ph1 -= std::floor (ph1);
        ph2 += f2 * pe / fs; ph2 -= std::floor (ph2);

        float tone = std::sin (2 * kPi * (float) ph1) * body.next() + std::sin (2 * kPi * (float) ph2) * body2.next();
        float n = bp.process (hp.process (noise.next())) * nz.next() * noiseLevel;

        float x = sat ((tone * bodyLevel + n) * 0.7f, drive * (1.0f + prm.comp * 2.0f));
        active = body.v > 1e-4f || nz.v > 1e-4f;
        return x * v;
    }

private:
    double fs = 44100.0, ph1 = 0, ph2 = 0;
    InstParams prm;
    float v = 1, f1 = 180, f2 = 330, boost = 0.3f, tauP = 0.01f, tauBody = 0.1f, tauNoise = 0.2f, noiseLevel = 0.6f, bodyLevel = 1, drive = 1.2f;
    dsp::Env body, body2, nz, pit;
    dsp::Biquad hp, bp;
    dsp::Noise noise;
    bool active = false;
};

// ----------------------------------------------------------------------------
class Tom
{
public:
    void prepare (double sr) { fs = sr; }

    // which : 0 = LT, 1 = MT, 2 = HT
    void trigger (float vel, const InstParams& p, int which)
    {
        using namespace dsp;
        v = vel; phase = 0.0;
        static const float base808[3] = { 90.0f, 130.0f, 190.0f };   // CALIBRATION
        static const float base909[3] = { 105.0f, 150.0f, 220.0f };
        const float tune = std::pow (2.0f, p.tune * 0.75f);

        if (p.model == M808)
        {
            f0 = base808[which] * tune; boost = 0.7f; tauP = 0.025f;
            tauA = expMap (p.decay, 0.08f, 0.5f); clickLevel = 0.08f; sw = 0.0f;
        }
        else
        {
            f0 = base909[which] * tune; boost = 1.2f; tauP = 0.014f;
            tauA = expMap (p.decay, 0.06f, 0.4f); clickLevel = 0.25f; sw = 0.2f;
        }
        amp.trig (1.0f, tauA, fs); pit.trig (1.0f, tauP, fs); click.trig (clickLevel, 0.004f, fs);
        lp.setLowpass (fs, 5000.0f, 0.7f); lp.reset();
        active = true;
    }

    float process()
    {
        using namespace dsp;
        if (! active) return 0.0f;

        const float f = f0 * (1.0f + boost * pit.next());
        phase += f / fs; phase -= std::floor (phase);

        float s = std::sin (2 * kPi * (float) phase);
        s = s * (1.0f - sw) + sw * (2.0f * std::fabs (2.0f * (float) phase - 1.0f) - 1.0f);   // un peu de triangle pour le 909
        float x = s * amp.next() + lp.process (noise.next()) * click.next();

        active = amp.v > 1e-4f;
        return sat (x * 0.9f, 1.2f) * v;
    }

private:
    double fs = 44100.0, phase = 0.0;
    float v = 1, f0 = 100, boost = 0.7f, tauP = 0.02f, tauA = 0.3f, clickLevel = 0.1f, sw = 0.0f;
    dsp::Env amp, pit, click;
    dsp::Biquad lp;
    dsp::Noise noise;
    bool active = false;
};

// ----------------------------------------------------------------------------
class RimShot
{
public:
    void prepare (double sr) { fs = sr; }

    void trigger (float vel, const InstParams& p)
    {
        using namespace dsp;
        v = vel; t = 0.0;
        const float tune = std::pow (2.0f, p.tune * 0.75f);
        if (p.model == M808) { fa = 455.0f * tune; fb = 1650.0f * tune; tauA = 0.012f; tauB = 0.006f; }
        else                 { fa = 500.0f * tune; fb = 1800.0f * tune; tauA = 0.010f; tauB = 0.005f; }
        const float k = expMap (p.decay, 0.6f, 2.5f);
        tauA *= k; tauB *= k;
        click.trig (0.5f, 0.0015f, fs);
        hp.setHighpass (fs, 2500.0f, 0.7f); hp.reset();
        active = true;
    }

    float process()
    {
        using namespace dsp;
        if (! active) return 0.0f;
        const float tt = (float) t; t += 1.0 / fs;

        float x = 0.8f * std::sin (2 * kPi * fa * tt) * std::exp (-tt / tauA)
                + 0.7f * std::sin (2 * kPi * fb * tt) * std::exp (-tt / tauB)
                + hp.process (noise.next()) * click.next();

        active = tt < 0.25f;
        return sat (x * 0.9f, 1.5f) * v;
    }

private:
    double fs = 44100.0, t = 0.0;
    float v = 1, fa = 455, fb = 1650, tauA = 0.012f, tauB = 0.006f;
    dsp::Env click;
    dsp::Biquad hp;
    dsp::Noise noise;
    bool active = false;
};

// ----------------------------------------------------------------------------
class HandClap
{
public:
    void prepare (double sr) { fs = sr; }

    void trigger (float vel, const InstParams& p)
    {
        using namespace dsp;
        v = vel; t = 0.0;
        const float tune = std::pow (2.0f, p.tune * 0.75f);
        bp.setBandpass (fs, 1100.0f * tune, p.model == M808 ? 1.6f : 1.3f); bp.reset();
        hp.setHighpass (fs, 500.0f, 0.7f); hp.reset();
        tail = expMap (p.decay, 0.05f, 0.3f);
        gap = p.model == M808 ? 0.0105f : 0.0095f;
        active = true;
    }

    float process()
    {
        using namespace dsp;
        if (! active) return 0.0f;
        const float tt = (float) t; t += 1.0 / fs;

        // 4 petites rafales rapprochées, puis une queue qui s'éteint
        float e = 0.0f;
        for (int i = 0; i < 4; ++i)
            if (tt >= i * gap) e = std::max (e, std::exp (-(tt - i * gap) / 0.0035f));
        if (tt >= 3 * gap) e = std::max (e, 0.65f * std::exp (-(tt - 3 * gap) / tail));

        float x = bp.process (hp.process (noise.next())) * e * 2.2f;
        active = tt < 3 * gap + tail * 8.0f;
        return sat (x, 1.5f) * v;
    }

private:
    double fs = 44100.0, t = 0.0;
    float v = 1, tail = 0.12f, gap = 0.0105f;
    dsp::Biquad bp, hp;
    dsp::Noise noise;
    bool active = false;
};

// ----------------------------------------------------------------------------
// Charleston : six oscillateurs carrés aux fréquences "métalliques" des circuits d'origine.
class HiHat
{
public:
    void prepare (double sr) { fs = sr; }

    void trigger (float vel, const InstParams& p, bool open)
    {
        using namespace dsp;
        v = vel; isOpen = open;
        const float tune = std::pow (2.0f, p.tune * 0.75f) * (p.model == M909 ? 1.25f : 1.0f);   // CALIBRATION
        static const float ratios[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        for (int i = 0; i < 6; ++i) { inc[i] = ratios[i] * tune / fs; ph[i] = 0.0; }

        const float tau = open ? expMap (p.decay, 0.12f, 0.7f) : expMap (p.decay, 0.025f, 0.10f);
        amp.trig (1.0f, tau, fs);
        hp1.setHighpass (fs, 5500.0f, 0.8f); hp2.setHighpass (fs, 5500.0f, 0.8f); bp.setBandpass (fs, 7400.0f, 0.5f);
        hp1.reset(); hp2.reset(); bp.reset();
        active = true;
    }

    void choke() { amp.k = std::min (amp.k, std::exp (-1.0f / (0.008f * (float) fs))); }

    float process()
    {
        using namespace dsp;
        if (! active) return 0.0f;

        float s = 0.0f;
        for (int i = 0; i < 6; ++i)
        {
            ph[i] += inc[i]; ph[i] -= std::floor (ph[i]);
            s += squareWave (ph[i]);
        }
        s *= (1.0f / 6.0f);

        float x = bp.process (hp2.process (hp1.process (s))) * 2.1f * amp.next();
        active = amp.v > 1e-4f;
        return x * v;
    }

private:
    double fs = 44100.0, ph[6] = {}, inc[6] = {};
    float v = 1;
    bool isOpen = false;
    dsp::Env amp;
    dsp::Biquad hp1, hp2, bp;
    bool active = false;
};

// ----------------------------------------------------------------------------
class Cymbal
{
public:
    void prepare (double sr) { fs = sr; }

    // ride = false : crash, true : ride
    void trigger (float vel, const InstParams& p, bool ride)
    {
        using namespace dsp;
        v = vel; isRide = ride; t = 0.0;
        const float tune = std::pow (2.0f, p.tune * 0.75f) * (ride ? 1.0f : 1.7f);
        static const float ratios[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
        for (int i = 0; i < 6; ++i) { inc[i] = ratios[i] * tune / fs * (ride ? 1.6f : 1.0f); ph[i] = 0.0; }

        tau = ride ? expMap (p.decay, 0.35f, 1.8f) : expMap (p.decay, 0.5f, 2.5f);
        amp.trig (1.0f, tau, fs);
        hp.setHighpass (fs, ride ? 5500.0f : 3500.0f, 0.7f); hp.reset();
        bp.setBandpass (fs, ride ? 9000.0f : 7000.0f, 0.5f); bp.reset();
        active = true;
    }

    float process()
    {
        using namespace dsp;
        if (! active) return 0.0f;
        const float tt = (float) t; t += 1.0 / fs;

        float s = 0.0f;
        for (int i = 0; i < 6; ++i) { ph[i] += inc[i]; ph[i] -= std::floor (ph[i]); s += squareWave (ph[i]); }
        s *= (1.0f / 6.0f);

        float x = 0.6f * s + 0.5f * noise.next();
        x = bp.process (hp.process (x));
        if (isRide)
            x += 0.25f * (std::sin (2 * kPi * 620.0f * tt) + 0.6f * std::sin (2 * kPi * 1230.0f * tt)) * std::exp (-tt / 0.25f);

        const float attack = std::min (1.0f, tt / 0.003f);
        x *= amp.next() * attack * (isRide ? 1.0f : 1.2f);

        active = amp.v > 1e-4f;
        return x * v;
    }

private:
    double fs = 44100.0, t = 0.0, ph[6] = {}, inc[6] = {};
    float v = 1, tau = 1.0f;
    bool isRide = false;
    dsp::Env amp;
    dsp::Biquad hp, bp;
    dsp::Noise noise;
    bool active = false;
};

// ----------------------------------------------------------------------------
// Les 11 instruments ensemble.
class Engine
{
public:
    void prepare (double sr)
    {
        bd.prepare (sr); sd.prepare (sr);
        for (auto& t : toms) t.prepare (sr);
        rs.prepare (sr); hc.prepare (sr); ch.prepare (sr); oh.prepare (sr); cc.prepare (sr); rc.prepare (sr);
        for (int i = 0; i < kNumInst; ++i) { level[i] = 0.8f; }
    }

    void setParams (Inst i, const InstParams& p) { params[i] = p; }
    void setLevel (Inst i, float l) { level[i] = l; }
    const InstParams& getParams (Inst i) const { return params[i]; }

    void trigger (Inst i, float vel)
    {
        const InstParams& p = params[i];
        switch (i)
        {
            case BD: bd.trigger (vel, p); break;
            case SD: sd.trigger (vel, p); break;
            case LT: toms[0].trigger (vel, p, 0); break;
            case MT: toms[1].trigger (vel, p, 1); break;
            case HT: toms[2].trigger (vel, p, 2); break;
            case RS: rs.trigger (vel, p); break;
            case HC: hc.trigger (vel, p); break;
            case CH: oh.choke(); ch.trigger (vel, p, false); break;   // le charleston fermé coupe l'ouvert
            case OH: oh.trigger (vel, p, true); break;
            case CC: cc.trigger (vel, p, false); break;
            case RC: rc.trigger (vel, p, true); break;
            default: break;
        }
    }

    float process()
    {
        float x = 0.0f;
        x += bd.process() * level[BD];
        x += sd.process() * level[SD];
        x += toms[0].process() * level[LT];
        x += toms[1].process() * level[MT];
        x += toms[2].process() * level[HT];
        x += rs.process() * level[RS];
        x += hc.process() * level[HC];
        x += ch.process() * level[CH];
        x += oh.process() * level[OH];
        x += cc.process() * level[CC];
        x += rc.process() * level[RC];
        return x;
    }

    // Sortie individuelle d'un instrument (utile pour les tests)
    float processSolo (Inst i)
    {
        switch (i)
        {
            case BD: return bd.process();
            case SD: return sd.process();
            case LT: return toms[0].process();
            case MT: return toms[1].process();
            case HT: return toms[2].process();
            case RS: return rs.process();
            case HC: return hc.process();
            case CH: return ch.process();
            case OH: return oh.process();
            case CC: return cc.process();
            case RC: return rc.process();
            default: return 0.0f;
        }
    }

private:
    BassDrum bd; SnareDrum sd; Tom toms[3]; RimShot rs; HandClap hc; HiHat ch, oh; Cymbal cc, rc;
    InstParams params[kNumInst];
    float level[kNumInst] = {};
};
} // namespace tr8
