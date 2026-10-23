// Offline sanity test for the GT120 Sim DSP core (no GUI needed).
#include "GT120DSP.h"
#include <cstdio>

static double goertzel (const std::vector<float>& x, double f, double fs)
{
    const double w = 2.0 * M_PI * f / fs;
    double s1 = 0, s2 = 0, c = 2.0 * std::cos (w);
    for (float v : x) { double s0 = v + c * s1 - s2; s2 = s1; s1 = s0; }
    const double p = s1 * s1 + s2 * s2 - c * s1 * s2;
    return 2.0 * std::sqrt (std::max (0.0, p)) / (double) x.size();
}

struct Result { double rms, peak, dc, fund, thd; bool finite; };

static Result run (gt120::Params p, double amp, double f0, double fs = 48000.0)
{
    gt120::Engine eng;
    eng.prepare (fs);
    const int N = (int) fs, block = 512;
    juce::AudioBuffer<float> buf (2, block);
    std::vector<float> out;
    for (int pos = 0; pos < N; pos += block)
    {
        for (int i = 0; i < block; ++i)
        {
            float v = (float) (amp * std::sin (2.0 * M_PI * f0 * (pos + i) / fs));
            buf.setSample (0, i, v);
            buf.setSample (1, i, v);
        }
        eng.process (buf, p);
        for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
    }
    std::vector<float> tail (out.begin() + N / 2, out.end());
    Result r { 0, 0, 0, 0, 0, true };
    for (float v : tail)
    {
        if (! std::isfinite (v)) r.finite = false;
        r.rms += v * v; r.peak = std::max (r.peak, (double) std::fabs (v)); r.dc += v;
    }
    r.rms = std::sqrt (r.rms / tail.size()); r.dc /= tail.size();
    r.fund = goertzel (tail, f0, fs);
    double h = 0;
    for (int k = 2; k <= 7; ++k)
        if (k * f0 < fs * 0.45) { const double a = goertzel (tail, k * f0, fs); h += a * a; }
    r.thd = r.fund > 1e-9 ? std::sqrt (h) / r.fund : 0;
    return r;
}

static void show (const char* name, const Result& r)
{
    std::printf ("%-38s rms=%.3f peak=%.3f dc=%+.4f fund=%.3f thd=%5.1f%% %s\n", name, r.rms, r.peak, r.dc, r.fund,
                 r.thd * 100.0, r.finite ? "ok" : "NaN/Inf!");
}

int main()
{
    int failures = 0;
    auto check = [&failures] (bool c, const char* m) { if (! c) { std::printf ("FAIL: %s\n", m); ++failures; } };

    gt120::Params p;

    // --- levels and stability across input level
    for (double a : { 0.01, 0.05, 0.1, 0.3, 1.0 })
    {
        char n[64]; std::snprintf (n, sizeof n, "default, 220 Hz, in=%.2f", a);
        auto r = run (p, a, 220.0); show (n, r);
        check (r.finite && r.peak < 2.5 && std::fabs (r.dc) < 0.02, "default stable and DC free");
    }

    // --- drive switch: more drive = more distortion
    double thd[4];
    for (int d = 0; d < 4; ++d)
    {
        gt120::Params q = p; q.drive = d;
        char n[64]; std::snprintf (n, sizeof n, "drive position %d, in=0.1", d + 1);
        auto r = run (q, 0.1, 220.0); show (n, r); thd[d] = r.thd;
    }
    check (thd[3] > thd[0] * 2.0, "highest drive position distorts clearly more than the lowest");

    // --- volume: no master, so volume up = more power-amp saturation and level
    gt120::Params vl = p, vh = p; vl.volume = 0.2f; vh.volume = 0.9f;
    auto rvl = run (vl, 0.1, 220.0), rvh = run (vh, 0.1, 220.0);
    show ("volume 0.2", rvl); show ("volume 0.9", rvh);
    check (rvh.rms > rvl.rms * 2.0, "volume raises level");
    check (rvh.thd > rvl.thd, "volume raises distortion (no master volume)");

    // --- bass boost adds low end
    gt120::Params b0 = p, b1 = p; b1.bassBoost = 1.0f;
    auto rb0 = run (b0, 0.05, 90.0), rb1 = run (b1, 0.05, 90.0);
    show ("bass boost 0 @ 90 Hz", rb0); show ("bass boost 1 @ 90 Hz", rb1);
    check (rb1.rms > rb0.rms * 1.2, "bass boost raises low end");

    // --- tone controls (clean-ish settings to keep it linear)
    gt120::Params t = p; t.drive = 0; t.volume = 0.3f;
    t.bass = 0.0f; auto bLo = run (t, 0.02, 100.0);
    t.bass = 1.0f; auto bHi = run (t, 0.02, 100.0);
    show ("bass 0 @ 100 Hz", bLo); show ("bass 1 @ 100 Hz", bHi);
    check (bHi.fund > bLo.fund * 2.0, "bass knob works");
    t.bass = 0.5f;
    t.treble = 0.0f; auto tLo = run (t, 0.02, 4000.0);
    t.treble = 1.0f; auto tHi = run (t, 0.02, 4000.0);
    show ("treble 0 @ 4 kHz", tLo); show ("treble 1 @ 4 kHz", tHi);
    check (tHi.fund > tLo.fund * 2.0, "treble knob works");

    // --- presence
    gt120::Params pr = t; pr.treble = 0.5f; pr.cabOn = false;
    pr.presence = 0.0f; auto pLo = run (pr, 0.05, 4000.0);
    pr.presence = 1.0f; auto pHi = run (pr, 0.05, 4000.0);
    show ("presence 0 @ 4 kHz", pLo); show ("presence 1 @ 4 kHz", pHi);
    check (pHi.fund > pLo.fund * 1.3, "presence raises highs");

    // --- cab
    gt120::Params co = p, cn = p; cn.cabOn = false;
    auto rco = run (co, 0.1, 6500.0), rcn = run (cn, 0.1, 6500.0);
    show ("cab on  @ 6.5 kHz", rco); show ("cab off @ 6.5 kHz", rcn);
    check (rco.fund < rcn.fund, "cab rolls off the top end");

    // --- extremes
    gt120::Params mx = p; mx.volume = 1.0f; mx.drive = 3; mx.bassBoost = 1.0f; mx.presence = 1.0f;
    mx.bass = 1.0f; mx.treble = 1.0f; mx.sag = 1.0f; mx.inputDb = 24.0f;
    for (double f : { 82.0, 440.0, 3000.0 })
    {
        char n[64]; std::snprintf (n, sizeof n, "max everything @ %.0f Hz, in=1.0", f);
        auto r = run (mx, 1.0, f); show (n, r);
        check (r.finite && r.peak < 4.01 && std::fabs (r.dc) < 0.1, "max settings stable");
    }

    for (double fs : { 44100.0, 96000.0 })
    {
        auto r = run (p, 0.2, 220.0, fs);
        char n[64]; std::snprintf (n, sizeof n, "default @ %.0f Hz", fs);
        show (n, r);
        check (r.finite, "finite at other sample rates");
    }

    std::printf (failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures;
}
