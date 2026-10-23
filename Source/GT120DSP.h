#pragma once
// GT120 Sim - DSP core (British 120 W single-channel head, 4 x EL34, 2 x ECC83).
//
// Public information used: single channel, Volume / Bass / Treble / Presence / Bass Boost and a
// four-position Drive switch placed after the first gain stage; presence circuit; 4 x EL34 Class AB
// push-pull; silicon rectifier. The remaining circuit details are NOT published, so the topology
// below is an informed approximation:
//
//   V1a gain -> bass boost -> Drive (4 steps) -> V1b gain -> tone stack (Bass/Treble, fixed mid dip)
//   -> Volume -> V2a recovery -> phase inverter -> EL34 push-pull with NFB/Presence + supply sag
//   -> output transformer -> optional cabinet voicing
//
// This is a behavioural model, not a schematic-exact copy.

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace gt120
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.283185307179586f;

struct Params
{
    float inputDb   = 0.0f;   // host-side input trim, -24..+24
    int   drive     = 2;      // 0..3  (positions 1..4)
    float bassBoost = 0.0f;   // 0..1
    float volume    = 0.5f;   // 0..1
    float bass      = 0.5f;   // 0..1
    float treble    = 0.6f;   // 0..1
    float presence  = 0.4f;   // 0..1
    float sag       = 0.25f;  // 0..1
    bool  cabOn     = true;
    float outputDb  = -6.0f;  // host-side trim
};

struct OnePole
{
    float a = 0.0f, z = 0.0f;
    void setCutoff (float fc, float fs) { a = 1.0f - std::exp (-kTwoPi * fc / fs); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }
};

// RBJ biquads (transposed direct form II)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }

    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        const float r = 1.0f / na0;
        b0 = nb0 * r; b1 = nb1 * r; b2 = nb2 * r; a1 = na1 * r; a2 = na2 * r;
    }

    void lowShelf (float fs, float f0, float dB)
    {
        const float A = std::pow (10.0f, dB / 40.0f), w = kTwoPi * f0 / fs, c = std::cos (w);
        const float al = std::sin (w) * 0.5f * std::sqrt (2.0f), t = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t),
             (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    void highShelf (float fs, float f0, float dB)
    {
        const float A = std::pow (10.0f, dB / 40.0f), w = kTwoPi * f0 / fs, c = std::cos (w);
        const float al = std::sin (w) * 0.5f * std::sqrt (2.0f), t = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
             (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
    void peak (float fs, float f0, float dB, float Q)
    {
        const float A = std::pow (10.0f, dB / 40.0f), w = kTwoPi * f0 / fs, c = std::cos (w);
        const float al = std::sin (w) / (2.0f * Q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void highpass (float fs, float f0, float Q)
    {
        const float w = kTwoPi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0f * Q);
        set ((1 + c) * 0.5f, -(1 + c), (1 + c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
    void lowpass (float fs, float f0, float Q)
    {
        const float w = kTwoPi * f0 / fs, c = std::cos (w), al = std::sin (w) / (2.0f * Q);
        set ((1 - c) * 0.5f, 1 - c, (1 - c) * 0.5f, 1 + al, -2 * c, 1 - al);
    }
};

struct Coeffs
{
    float inGain = 1, driveGain = 1, bassBoost = 0, volGain = 0.25f, sag = 0.25f, fbHF = 0.7f;
    float cabMix = 1, outGain = 0.5f;
};

// Inverting triode gain stage: grid-current limiting on positive swings plus a slightly asymmetric
// cutoff / saturation curve (even harmonics). Small-signal gain is exactly -G.
inline float triodeStage (float x, float G, float Vg)
{
    constexpr float b = 0.14f;
    static const float tb = std::tanh (b);
    static const float norm = 1.0f / (1.0f - tb * tb);
    float u = x / Vg;
    if (u > 0.7f)
        u = 0.7f + 0.5f * std::tanh ((u - 0.7f) / 0.5f);   // grid conduction
    const float s = (std::tanh (u + b) - tb) * norm;
    return -G * Vg * s;
}

inline float pentodeSat (float f)
{
    const float f2 = f * f;
    return f / std::sqrt (std::sqrt (1.0f + f2 * f2));
}

class Channel
{
public:
    void prepare (float fsOS)
    {
        fs = fsOS;
        dcIn.setCutoff (12.0f, fs);
        c1hp.setCutoff (55.0f, fs);
        c2hp.setCutoff (35.0f, fs);
        c3hp.setCutoff (30.0f, fs);
        bbLP.setCutoff (110.0f, fs);
        m1.setCutoff (18000.0f, fs);
        m2.setCutoff (12000.0f, fs);
        m3.setCutoff (14000.0f, fs);
        fbLP.setCutoff (2000.0f, fs);
        envLP.setCutoff (10.0f, fs);
        otHP.setCutoff (35.0f, fs);
        otLP.setCutoff (16000.0f, fs);

        cabHP.highpass (fs, 80.0f, 0.75f);
        cabLow.peak (fs, 115.0f, 3.0f, 1.0f);
        cabDip.peak (fs, 480.0f, -2.5f, 0.9f);
        cabPres.peak (fs, 2900.0f, 5.0f, 1.2f);
        cabLP.lowpass (fs, 5600.0f, 0.75f);
        cabLP2.lowpass (fs, 7800.0f, 0.6f);
        reset();
    }

    void reset()
    {
        for (auto* f : { &dcIn, &c1hp, &c2hp, &c3hp, &bbLP, &m1, &m2, &m3, &fbLP, &envLP, &otHP, &otLP })
            f->reset();
        for (auto* b : { &lowShelf, &highShelf, &midDip, &presShelf, &cabHP, &cabLow, &cabDip, &cabPres, &cabLP, &cabLP2 })
            b->reset();
        yPrev = 0.0f;
    }

    // tone controls (updated once per chunk)
    void setTone (float bass, float treble, float presence)
    {
        lowShelf.lowShelf (fs, 140.0f, -14.0f + 22.0f * bass);
        highShelf.highShelf (fs, 2300.0f, -14.0f + 24.0f * treble);
        const float depth = -(4.0f + 6.0f * (1.0f - 0.5f * (bass + treble)));
        midDip.peak (fs, 650.0f, depth, 0.7f);
        presShelf.highShelf (fs, 3500.0f, -1.5f + 6.5f * presence);
    }

    float process (float x, const Coeffs& c)
    {
        // ---- preamp ----
        float v = dcIn.hp (x) * c.inGain;
        v = triodeStage (v, 22.0f, 1.0f);                  // V1a
        v = c1hp.hp (v);
        v += c.bassBoost * 2.2f * bbLP.lp (v);             // bass boost network (low shelf-ish)
        v = m1.lp (v);
        v *= c.driveGain * 0.55f;                          // Drive switch (4 steps)
        v = triodeStage (v, 32.0f, 1.2f);                  // V1b
        v = c2hp.hp (v);
        v = m2.lp (v);

        // ---- tone stack (passive, ~ -14 dB insertion loss) ----
        v = midDip.process (highShelf.process (lowShelf.process (v)));
        v *= 0.3f * c.volGain;

        v = triodeStage (v, 28.0f, 1.5f);                  // V2a recovery
        v = c3hp.hp (v);
        v = m3.lp (v);

        // ---- phase inverter + EL34 push-pull with negative feedback ----
        v = 40.0f * std::tanh (v * (1.0f / 40.0f));        // PI headroom
        const float xin = v * (1.0f / 20.0f) * ((1.0f + kA * kBeta) / kA);

        const float lo = fbLP.lp (yPrev);
        const float fb = lo + c.fbHF * (yPrev - lo);       // Presence lowers the HF feedback
        const float u = kA * (xin - kBeta * fb);

        auto srelu = [] (float w) { const float t = w - 0.02f; return 0.5f * (t + std::sqrt (t * t + 0.0225f)); };
        float y = pentodeSat (srelu (u)) - pentodeSat (srelu (-u));

        const float env = envLP.lp (y * y);                // supply sag (silicon rectifier: mild)
        y *= 1.0f - 0.35f * c.sag * std::min (1.0f, 2.0f * env);
        yPrev = y;

        y = otLP.lp (otHP.hp (y));                         // output transformer
        y = presShelf.process (y);

        // ---- cabinet voicing ----
        float cab = cabHP.process (y);
        cab = cabLow.process (cab);
        cab = cabDip.process (cab);
        cab = cabPres.process (cab);
        cab = cabLP2.process (cabLP.process (cab)) * 1.5f;

        float out = (y + c.cabMix * (cab - y)) * c.outGain;
        if (! std::isfinite (out))
        {
            reset();
            return 0.0f;
        }
        return std::clamp (out, -4.0f, 4.0f);
    }

private:
    static constexpr float kA = 6.0f, kBeta = 0.1f;
    float fs = 176400.0f, yPrev = 0.0f;
    OnePole dcIn, c1hp, c2hp, c3hp, bbLP, m1, m2, m3, fbLP, envLP, otHP, otLP;
    Biquad lowShelf, highShelf, midDip, presShelf, cabHP, cabLow, cabDip, cabPres, cabLP, cabLP2;
};

class Engine
{
public:
    static constexpr int kChunk = 32;

    void prepare (double sampleRate)
    {
        fsOS = (float) sampleRate * 4.0f;
        os.reset();
        os.initProcessing ((size_t) kChunk);
        for (auto& c : channels)
            c.prepare (fsOS);
        for (auto* s : { &smIn, &smDrive, &smBB, &smVol, &smBass, &smTreble, &smPres, &smSag, &smCab, &smOut })
            s->reset (sampleRate, 0.03);
        snap = true;
    }

    void reset()
    {
        os.reset();
        for (auto& c : channels) c.reset();
        snap = true;
    }

    int getLatencySamples() const { return (int) std::lround (os.getLatencyInSamples()); }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        const int numCh = std::min (buffer.getNumChannels(), 2);
        const int n = buffer.getNumSamples();
        if (numCh == 0 || n == 0) return;

        static const float driveGains[4] = { 0.08f, 0.25f, 0.6f, 1.0f };

        auto set = [this] (juce::SmoothedValue<float>& s, float v)
        {
            if (snap) s.setCurrentAndTargetValue (v); else s.setTargetValue (v);
        };
        set (smIn, p.inputDb);  set (smDrive, driveGains[std::clamp (p.drive, 0, 3)]);
        set (smBB, p.bassBoost); set (smVol, p.volume);  set (smBass, p.bass);
        set (smTreble, p.treble); set (smPres, p.presence); set (smSag, p.sag);
        set (smCab, p.cabOn ? 1.0f : 0.0f); set (smOut, p.outputDb);
        snap = false;

        for (int pos = 0; pos < n; pos += kChunk)
        {
            const int len = std::min (kChunk, n - pos);

            Coeffs c;
            c.inGain = std::pow (10.0f, smIn.skip (len) / 20.0f);
            c.driveGain = smDrive.skip (len);
            c.bassBoost = smBB.skip (len);
            const float vol = smVol.skip (len);
            c.volGain = vol * vol;
            const float bass = smBass.skip (len), treble = smTreble.skip (len), pres = smPres.skip (len);
            c.sag = smSag.skip (len);
            c.fbHF = 1.0f - 0.85f * pres;
            c.cabMix = smCab.skip (len);
            c.outGain = std::pow (10.0f, smOut.skip (len) / 20.0f);

            for (int ch = 0; ch < numCh; ++ch)
                channels[(size_t) ch].setTone (bass, treble, pres);

            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) pos, (size_t) len);
            auto up = os.processSamplesUp (block);
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = up.getChannelPointer ((size_t) ch);
                for (size_t i = 0; i < up.getNumSamples(); ++i)
                    d[i] = channels[(size_t) ch].process (d[i], c);
            }
            os.processSamplesDown (block);
        }
    }

private:
    float fsOS = 176400.0f;
    bool snap = true;
    std::array<Channel, 2> channels;
    juce::dsp::Oversampling<float> os { 2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true };
    juce::SmoothedValue<float> smIn, smDrive, smBB, smVol, smBass, smTreble, smPres, smSag, smCab, smOut;
};
} // namespace gt120
