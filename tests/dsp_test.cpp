// Offline sanity tests for the DSP (no SDK needed):  g++ -std=c++17 -O2 -Isource tests/dsp_test.cpp
#include "ms20_params.h"
#include "ms20_synth.h"
#include "ms20_presets.h"
#include "ms20_scope.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <vector>
using namespace MS20;

static const double FS = 48000.0;

static std::vector<double> spectrum(const std::vector<float>& x, int start, int N) {   // magnitude, Hann, naive DFT via FFT
    std::vector<std::complex<double>> a(N);
    for (int i = 0; i < N; ++i) a[i] = x[start + i] * (0.5 - 0.5 * std::cos(2 * kPi * i / N));
    for (int i = 1, j = 0; i < N; ++i) { int bit = N >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap(a[i], a[j]); }
    for (int len = 2; len <= N; len <<= 1) {
        auto w = std::polar(1.0, -2 * kPi / len);
        for (int i = 0; i < N; i += len) { std::complex<double> wn = 1; for (int k = 0; k < len / 2; ++k) { auto u = a[i + k], v = a[i + k + len / 2] * wn; a[i + k] = u + v; a[i + k + len / 2] = u - v; wn *= w; } }
    }
    std::vector<double> m(N / 2);
    for (int i = 0; i < N / 2; ++i) m[i] = std::abs(a[i]);
    return m;
}

static std::vector<float> render(Synth& s, int n) { std::vector<float> l(n), r(n); s.render(l.data(), r.data(), n); return l; }

int main() {
    int fails = 0;
    auto check = [&](bool ok, const char* msg) { printf("%s  %s\n", ok ? "PASS" : "FAIL", msg); if (!ok) ++fails; };

    // 1) pitch accuracy: A4, osc1 only, saw
    {
        Synth s; s.setSampleRate(FS); SynthParams p; p.oscMix = 0.0; s.setParams(p); s.reset();
        s.noteOn(69, 1.0);
        auto x = render(s, 48000 * 2);
        auto m = spectrum(x, 48000, 1 << 15);
        int pk = std::max_element(m.begin(), m.end()) - m.begin();
        double f = pk * FS / (1 << 15);
        printf("      A4 peak at %.1f Hz\n", f);
        check(std::fabs(f - 440.0) < 2.0, "A4 saw peaks at 440 Hz");
    }
    // 2) octave switch + pitch knob: 4' osc1 (+1 oct) +12 st -> 4*440 = 1760 Hz from A4... (oct idx2 = x2, +12 st = x2)
    {
        Synth s; s.setSampleRate(FS); SynthParams p; p.oscMix = 0.0; p.oct1 = 2; p.pitch1 = 12.0; s.setParams(p); s.reset();
        s.noteOn(69, 1.0);
        auto x = render(s, 48000 * 2);
        auto m = spectrum(x, 48000, 1 << 15);
        int pk = std::max_element(m.begin(), m.end()) - m.begin();
        double f = pk * FS / (1 << 15);
        printf("      4' +12st peak at %.1f Hz\n", f);
        check(std::fabs(f - 1760.0) < 3.0, "octave + pitch knobs transpose correctly");
    }
    // 3) aliasing: C7 saw/square, energy away from harmonics vs total
    for (int wave = 0; wave < 3; ++wave) {
        Synth s; s.setSampleRate(FS); SynthParams p; p.oscMix = 0.0; p.wave1 = wave; s.setParams(p); s.reset();
        const int note = 96; s.noteOn(note, 1.0);
        auto x = render(s, 48000 * 2);
        const int N = 1 << 15; auto m = spectrum(x, 48000, N);
        const double f0 = 440.0 * std::pow(2.0, (note - 69) / 12.0);
        double harm = 0, other = 0;
        for (int b = 3; b < N / 2; ++b) {
            double f = b * FS / N, h = std::round(f / f0), d = std::fabs(f - h * f0);
            double e = m[b] * m[b];
            if (d < 25.0) harm += e; else other += e;
        }
        printf("      wave %d: non-harmonic energy %.1f dB below harmonics\n", wave, 10 * std::log10(harm / std::max(other, 1e-30)));
        check(10 * std::log10(harm / std::max(other, 1e-30)) > 55.0, "oscillator aliasing > 55 dB down");
    }
    // 4) decimator stop band: 0.6*fs tone at 4x rate must vanish, 0.4*fs tone must pass
    {
        Decimator d; auto run = [&](double fnorm) { double mx = 0; double ph = 0; for (int i = 0; i < 4000; ++i) { double blk[4]; for (int k = 0; k < 4; ++k) { blk[k] = std::sin(ph); ph += 2 * kPi * fnorm / kOversample; } double y = d.push(blk); if (i > 500) mx = std::max(mx, std::fabs(y)); } return mx; };
        double pass = run(0.40), stop = run(0.60);
        printf("      0.40fs gain %.3f, 0.60fs gain %.2e (%.0f dB)\n", pass, stop, 20 * std::log10(stop + 1e-30));
        check(pass > 0.93 && stop < 1e-3, "decimator passes 0.4 fs and rejects 0.6 fs");
    }
    // 5) envelope: sustain level, release to silence, voice freed
    {
        Synth s; s.setSampleRate(FS); SynthParams p; p.attack = 0.01; p.decay = 0.05; p.sustain = 0.5; p.release = 0.1; p.oscMix = 0.0; s.setParams(p); s.reset();
        s.noteOn(60, 1.0);
        auto a = render(s, 48000);
        float peakSus = 0; for (int i = 40000; i < 48000; ++i) peakSus = std::max(peakSus, std::fabs(a[i]));
        s.noteOff(60);
        auto b = render(s, 48000);
        float tail = 0; for (int i = 40000; i < 48000; ++i) tail = std::max(tail, std::fabs(b[i]));
        printf("      sustain peak %.3f, tail %.2e, active voices %d\n", peakSus, tail, s.activeVoices());
        check(peakSus > 0.05 && tail < 1e-4 && s.activeVoices() == 0, "ADSR sustains, releases to silence and frees the voice");
    }
    // 6) filter works: LP at 500 Hz kills the 10th harmonic of A3 (2200 Hz)
    {
        auto level = [&](double lpHz) { Synth s; s.setSampleRate(FS); SynthParams p; p.oscMix = 0.0; p.lpHz = lpHz; s.setParams(p); s.reset(); s.noteOn(57, 1.0); auto x = render(s, 48000 * 2); auto m = spectrum(x, 48000, 1 << 15); int b = (int)std::round(2200.0 * (1 << 15) / FS); double e = 0; for (int i = b - 3; i <= b + 3; ++i) e = std::max(e, m[i]); return 20 * std::log10(e); };
        double open = level(20000), closed = level(500);
        printf("      2.2 kHz harmonic: open %.1f dB, LP500 %.1f dB\n", open, closed);
        check(open - closed > 20.0, "LP filter attenuates high harmonics");
    }
    // 7) sync / ring / x-mod / shape produce finite, bounded audio; extreme stress with 8+ voices
    {
        Synth s; s.setSampleRate(FS); SynthParams p;
        p.wave1 = 2; p.wave2 = 1; p.shape1 = 1; p.shape2 = 1; p.xmod = 1; p.syncMode = 2; p.oscMix = 0.5;
        p.hpHz = 800; p.hpK = 2.1; p.lpHz = 300; p.lpK = 2.1; p.driveGain = 8; p.outGain = 4;
        p.pitch2 = 7.3; p.oct2 = 3; s.setParams(p); s.reset();
        for (int n = 40; n < 52; ++n) s.noteOn(n, 0.9);      // 12 notes -> voice stealing
        bool ok = true; float mx = 0;
        for (int blk = 0; blk < 200; ++blk) {
            if (blk == 60) { p.syncMode = 1; s.setParams(p); }
            if (blk == 120) { p.syncMode = 0; p.xmod = 0.3; s.setParams(p); }
            auto x = render(s, 480);
            for (float v : x) { if (!std::isfinite(v)) ok = false; mx = std::max(mx, std::fabs(v)); }
        }
        printf("      stress max |y| = %.2f, active voices %d\n", mx, s.activeVoices());
        check(ok && mx <= 4.0f, "extreme settings stay finite and bounded");
    }
    // 8) CPU: 8 voices, default-ish patch with both oscillators and both filters
    {
        Synth s; s.setSampleRate(FS); SynthParams p; p.lpHz = 3000; p.lpK = 1.0; p.hpHz = 100; p.driveGain = 3; p.xmod = 0.3; s.setParams(p); s.reset();
        for (int n = 48; n < 56; ++n) s.noteOn(n, 0.8);
        std::vector<float> l(512), r(512);
        auto t0 = std::chrono::steady_clock::now();
        const int blocks = 48000 * 10 / 512;
        for (int i = 0; i < blocks; ++i) s.render(l.data(), r.data(), 512);
        double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("      10 s of audio, 8 voices, 4x oversampled: %.2f s  (%.1f%% of one core)\n", secs, secs / 10.0 * 100);
        check(secs < 5.0, "8-voice load is well under real time");
    }
    // 9) parameter formatting / parsing round trip
    {
        bool ok = true;
        for (int id = 0; id < kParamCount; ++id) {
            std::string txt = formatValue(id, defaultNormalized(id)); double n = -1;
            if (!parseValue(id, txt.c_str(), n)) ok = false;
            else if (std::fabs(specToNormalized(id, specToPlain(id, n)) - n) > 1e-9 && kSpecs[id].fmt != Fmt::List) ok = false;
            if (id == kParamLpFreq || id == kParamAmpAttack || id == kParamOsc1Oct || id == kParamOsc1Pitch || id == kParamDryWet) printf("      %-12s default -> \"%s\"\n", kSpecs[id].title, txt.c_str());
        }
        check(ok, "every parameter's default text parses back");
    }
    // 10) drive types: near-identical at Drive 0; FUZZ is far harsher and asymmetric (even harmonics), levels stay comparable
    {
        auto run = [&](int type, double drive) { Synth s; s.setSampleRate(FS); SynthParams p; p.oscMix = 0.0; p.wave1 = 1; p.driveGain = drive; p.driveType = type; p.lpHz = 20000; s.setParams(p); s.reset(); s.noteOn(45, 1.0); return render(s, 96000); };
        auto bandRatio = [&](const std::vector<float>& x, double fLo, double fHi, double gLo, double gHi) { auto m = spectrum(x, 20000, 1 << 15); double a = 0, b = 0; for (size_t i = 1; i < m.size(); ++i) { double f = i * FS / (1 << 15); if (f >= fLo && f < fHi) a += m[i] * m[i]; if (f >= gLo && f < gHi) b += m[i] * m[i]; } return 10 * std::log10(a / b); };
        auto rms = [&](const std::vector<float>& x) { double e = 0; for (int i = 20000; i < 40000; ++i) e += x[i] * x[i]; return std::sqrt(e / 20000); };
        auto a0 = run(0, 1.0), b0 = run(1, 1.0);
        double maxDiff = 0; for (size_t i = 0; i < a0.size(); ++i) maxDiff = std::max(maxDiff, (double)std::fabs(a0[i] - b0[i]));
        auto soft = run(0, 8.0), fuzz = run(1, 8.0);
        // A2 = 110 Hz triangle: harmonics 2f (220 Hz) vs fundamental, and everything above 2 kHz vs everything below 600 Hz
        double even_soft = bandRatio(soft, 200, 240, 100, 120), even_fuzz = bandRatio(fuzz, 200, 240, 100, 120);
        double hf_soft = bandRatio(soft, 2000, 20000, 20, 600), hf_fuzz = bandRatio(fuzz, 2000, 20000, 20, 600);
        double rs = rms(soft), rf = rms(fuzz), r0 = rms(a0);
        float mx = 0; for (float v : fuzz) mx = std::max(mx, std::fabs(v));
        printf("      drive 0 max diff %.2e | 2nd harmonic: soft %.1f dB, fuzz %.1f dB | HF/LF: soft %.1f dB, fuzz %.1f dB | RMS clean %.3f soft %.3f fuzz %.3f\n", maxDiff, even_soft, even_fuzz, hf_soft, hf_fuzz, r0, rs, rf);
        check(maxDiff < 0.06, "drive types are close at Drive 0");
        check(hf_fuzz > hf_soft + 15.0 && even_fuzz > even_soft + 10.0 && mx < 2.0f, "FUZZ is much harsher and has strong even harmonics");
        check(rf > r0 * 0.4 && rf < r0 * 2.5 && rs > r0 * 0.4 && rs < r0 * 2.5, "drive types keep a comparable level");
    }
    // 11) filter envelope: +100% amount opens the LP cutoff on note-on, so early audio is brighter than sustained audio
    {
        Synth s; s.setSampleRate(FS); SynthParams p; p.oscMix = 0.0; p.lpHz = 300; p.fAmount = 1.0; p.fAttack = 0.001; p.fDecay = 0.3; p.fSustain = 0.0; p.sustain = 1.0; s.setParams(p); s.reset();
        s.noteOn(48, 1.0);
        auto x = render(s, 48000 * 2);
        auto hfRatio = [&](int start, int N) { auto m = spectrum(x, start, N); double lo = 0, hi = 0; for (size_t b = 1; b < m.size(); ++b) { double f = b * FS / N; (f < 600 ? lo : hi) += m[b] * m[b]; } return 10 * std::log10(hi / lo); };
        double early = hfRatio(0, 2048), late = hfRatio(48000, 8192);
        printf("      HF/LF early %.1f dB, late %.1f dB\n", early, late);
        check(early > late + 15.0, "filter envelope sweeps the cutoff");
    }
    // 12) LFO -> AMP (tremolo) and mod wheel adding depth
    {
        auto swing = [&](double depth, double wheel) {
            Synth s; s.setSampleRate(FS); SynthParams p; p.oscMix = 0.0; p.lfoTarget = 3; p.lfoRate = 5.0; p.lfoDepth = depth; p.modWheel = wheel; p.sustain = 1.0; s.setParams(p); s.reset(); s.noteOn(57, 1.0);
            auto x = render(s, 48000 * 2); double lo = 1e9, hi = 0;
            for (int b = 24000; b < 96000 - 480; b += 480) { double e = 0; for (int i = 0; i < 480; ++i) e += x[b + i] * x[b + i]; e = std::sqrt(e / 480); lo = std::min(lo, e); hi = std::max(hi, e); }
            return lo / hi; };
        double none = swing(0.0, 0.0), full = swing(1.0, 0.0), wheel = swing(0.0, 1.0);
        printf("      tremolo min/max RMS: off %.2f, depth 100%% %.2f, mod wheel only %.2f\n", none, full, wheel);
        check(none > 0.8 && full < 0.3 && wheel < 0.3, "LFO tremolo works and the mod wheel adds depth");
    }
    // 13) LFO -> PITCH vibrato widens the spectral line; pitch bend +2 st shifts it exactly
    {
        auto peak = [&](double bend, double depth) {
            Synth s; s.setSampleRate(FS); SynthParams p; p.oscMix = 0.0; p.bend = bend; p.lfoDepth = depth; p.lfoTarget = 0; p.lfoRate = 6.0; s.setParams(p); s.reset(); s.noteOn(69, 1.0);
            auto x = render(s, 48000 * 2); auto m = spectrum(x, 48000, 1 << 15);
            int pk = std::max_element(m.begin(), m.end()) - m.begin(); double tot = 0, near = 0; for (int b = 0; b < (int)m.size(); ++b) { double f = b * FS / (1 << 15); if (f > 300 && f < 700) { tot += m[b] * m[b]; if (std::abs(b - pk) < 4) near += m[b] * m[b]; } }
            return std::make_pair(pk * FS / (1 << 15), near / tot); };
        auto bent = peak(2.0, 0.0), flat = peak(0.0, 0.0), vib = peak(0.0, 0.4);
        printf("      A4 +2 st -> %.1f Hz (expect %.1f); line concentration: steady %.2f, vibrato %.2f\n", bent.first, 440 * std::pow(2.0, 2.0 / 12), flat.second, vib.second);
        check(std::fabs(bent.first - 440 * std::pow(2.0, 2.0 / 12)) < 3.0 && vib.second < flat.second * 0.8, "pitch bend shifts pitch; LFO vibrato smears the line");
    }
    // 14) reverb: tail rings after the note, decays, silent when mix is 0, level roughly matches the dry signal
    {
        auto run = [&](double mix, double size, double* wetRms, double* dryRms) {
            Synth s; s.setSampleRate(FS); SynthParams p; p.oscMix = 0.0; p.revMix = mix; p.revSize = size; p.sustain = 1.0; p.release = 0.01; s.setParams(p); s.reset(); s.noteOn(57, 1.0);
            auto a = render(s, 48000); s.noteOff(57); auto b = render(s, 48000 * 4);
            double ea = 0; for (int i = 24000; i < 48000; ++i) ea += a[i] * a[i];
            double t1 = 0, t2 = 0; for (int i = 4800; i < 4800 + 9600; ++i) t1 += b[i] * b[i]; for (int i = 48000 * 3; i < 48000 * 3 + 9600; ++i) t2 += b[i] * b[i];
            if (wetRms) *wetRms = std::sqrt(ea / 24000); if (dryRms) *dryRms = std::sqrt(t1 / 9600);
            return std::sqrt(t2 / 9600); };
        double steadyWet = 0, tail1 = 0;
        double lateOn = run(1.0, 0.8, &steadyWet, &tail1);
        double dryRef = 0, d2 = 0; run(0.0, 0.8, &dryRef, &d2);
        double lateOff = run(0.0, 0.8, nullptr, nullptr);
        printf("      steady level wet %.3f vs dry %.3f; tail @0.1s %.4f, @3s %.5f (mix0 tail %.1e)\n", steadyWet, dryRef, tail1, lateOn, lateOff);
        check(tail1 > 1e-3 && lateOn < tail1 * 0.5 && lateOff < 1e-6 && steadyWet > dryRef * 0.4 && steadyWet < dryRef * 2.5, "reverb has a decaying tail, mix 0 is clean, level is comparable to dry");
    }
    // 15) presets: names match the parameter list, every preset renders finite audio at sane levels
    {
        const auto& list = presets();
        bool names = (int)list.size() == kFactoryPresetCount && kSpecs[kParamPreset].listCount == kPresetCount;
        for (size_t i = 0; names && i < list.size(); ++i) names = std::string(list[i].name) == kSpecs[kParamPreset].items[i];
        check(names, "preset names match the Preset parameter list");
        bool ok = true;
        for (size_t i = 0; i < list.size(); ++i) {
            double norm[kParamCount]; presetToNormalized((int)i, norm);
            Synth s; s.setSampleRate(FS); s.setParams(buildSynthParams(norm)); s.reset();
            s.noteOn(48, 0.8); s.noteOn(55, 0.8); s.noteOn(60, 0.8);
            float mx = 0; double e = 0; bool fin = true; int cnt = 0;
            for (int b = 0; b < 400; ++b) { auto x = render(s, 240); for (float v : x) { if (!std::isfinite(v)) fin = false; mx = std::max(mx, std::fabs(v)); if (b > 100) { e += v * v; ++cnt; } } }
            double rms = std::sqrt(e / std::max(1, cnt));
            printf("      %-13s peak %.2f  rms %.3f\n", list[i].name, mx, rms);
            if (!fin || mx > 2.0f || mx < 0.02f) ok = false;
        }
        check(ok, "all presets are finite and neither silent nor clipping");
    }

    // 16) scope: ring buffer returns the newest samples in order; trigger lands on a rising zero crossing and holds still
    {
        auto buf = std::make_shared<ScopeBuffer>();
        std::vector<float> a(5000), b(3000);
        for (int i = 0; i < 5000; ++i) a[i] = (float)i;
        for (int i = 0; i < 3000; ++i) b[i] = (float)(5000 + i);
        buf->push(a.data(), nullptr, 5000); buf->push(b.data(), nullptr, 3000);   // wraps the 8192 ring
        float out[100]; buf->latest(out, 100);
        bool order = out[0] == 7900.0f && out[99] == 7999.0f;
        // trigger on a 440 Hz sine rendered in two different blocks: the displayed slice must start at the same phase
        auto sineSlice = [&](int offset) { std::vector<float> in(2304); for (int i = 0; i < 2304; ++i) in[i] = std::sin(2 * kPi * 440.0 * (i + offset) / FS);
            std::vector<float> w(1024); scopeWindow(in.data(), 2304, w.data(), 1024); return w; };
        auto w1 = sineSlice(0), w2 = sineSlice(37);
        double diff = 0; for (int i = 0; i < 1024; ++i) diff = std::max(diff, (double)std::fabs(w1[i] - w2[i]));
        printf("      ring order ok=%d, trigger phase mismatch between two offsets %.3f\n", order, diff);
        check(order && diff < 0.2 && std::fabs(w1[0]) < 0.15f, "scope buffer is ordered and the trigger holds the trace steady");
        std::int64_t id = 0; auto sp = ScopeRegistry::create(id); auto found = ScopeRegistry::find(id); sp.reset(); found.reset();
        check(ScopeRegistry::find(id) == nullptr && id > 0, "scope registry lets a buffer go when both sides release it");
    }
    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails;
}
