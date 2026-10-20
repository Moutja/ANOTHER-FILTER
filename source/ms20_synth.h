#pragma once
// Header-only synth engine (no SDK dependency):
//   8 voices, each: OSC1 + OSC2 (SAW/TRI/SQR, shape, octave, pitch, X-Mod, sync/ring)
//                   -> drive (SOFT tanh / FUZZ) -> HP stage -> LP stage (cutoff: filter envelope + LFO)
//                   -> dry/wet -> amp ADSR
//   voices are summed, decimated, then sent through a stereo reverb.
// Everything per-voice runs at 4x the host rate; the summed signal is decimated with a
// linear-phase Kaiser FIR, so aliasing from the oscillators, the saturators and the filter is
// pushed down by ~80 dB.

#include <algorithm>
#include <cmath>
#include <vector>
#include "ms20_filter.h"
#include "ms20_params.h"

namespace MS20 {

constexpr int kOversample = 4;
constexpr int kMaxVoices = 8;
constexpr int kFirTaps = 193;        // odd, linear phase; latency = 96 oversampled samples = 24 host samples
constexpr double kMasterGain = 0.3;  // headroom for chords
constexpr double kReverbWetGain = 1.0;

struct SynthParams
{
    int driveType = 0;  // 0 SOFT (smooth tanh), 1 FUZZ (asymmetric clipping + octave grit + gate, ~6x more gain)
    int wave1 = 0, oct1 = 1, wave2 = 0, oct2 = 1, syncMode = 0;  // wave 0 SAW 1 TRI 2 SQR; oct 0..3 = 16' 8' 4' 2'; sync 0 OFF 1 RING 2 SYNC
    double pitch1 = 0.0, pitch2 = 0.0;                           // semitones
    double shape1 = 0.0, shape2 = 0.0, xmod = 0.0, oscMix = 0.5; // 0..1
    double hpHz = 20.0, hpK = 0.0, lpHz = 20000.0, lpK = 0.0;
    double driveGain = 1.0, dryWet = 1.0, outGain = 1.0;
    double attack = 0.005, decay = 0.3, sustain = 0.7, release = 0.4;      // amp env: seconds, seconds, 0..1, seconds
    double fAttack = 0.005, fDecay = 0.4, fSustain = 0.3, fRelease = 0.4;  // filter env
    double fAmount = 0.0;                                                  // -1..+1 (x6 octaves on the LP cutoff)
    double lfoRate = 5.0, lfoDepth = 0.0;                                  // Hz, 0..1
    int lfoWave = 0, lfoTarget = 0;                                        // wave 0 SIN 1 TRI 2 SQR 3 S&H; target 0 PITCH 1 SHAPE 2 CUTOFF 3 AMP
    double revMix = 0.0, revSize = 0.5, revDamp = 0.5;                     // 0..1
    double modWheel = 0.0;                                                 // 0..1, adds to LFO depth
    double bend = 0.0;                                                     // semitones
};

// Converts the normalized parameter array (what the host stores) into engine values.
inline SynthParams buildSynthParams(const double* norm)
{
    auto plain = [&](int id) { return specToPlain(id, norm[id]); };
    auto index = [&](int id) { return specToIndex(id, norm[id]); };

    SynthParams p;
    p.wave1 = index(kParamOsc1Wave);   p.oct1 = index(kParamOsc1Oct);
    p.pitch1 = plain(kParamOsc1Pitch); p.shape1 = norm[kParamOsc1Shape];
    p.wave2 = index(kParamOsc2Wave);   p.oct2 = index(kParamOsc2Oct);
    p.pitch2 = plain(kParamOsc2Pitch); p.shape2 = norm[kParamOsc2Shape];
    p.xmod = norm[kParamXMod];
    p.syncMode = index(kParamSyncRing);
    p.oscMix = norm[kParamOscMix];

    p.hpHz = plain(kParamHpFreq);      p.hpK = norm[kParamHpPeak] * Limits::kMaxResonance;
    p.lpHz = plain(kParamLpFreq);      p.lpK = norm[kParamLpPeak] * Limits::kMaxResonance;
    p.driveGain = 1.0 + norm[kParamDrive] * (Limits::kMaxDriveGain - 1.0);
    p.driveType = index(kParamDriveType);
    p.dryWet = norm[kParamDryWet];
    p.outGain = std::pow(10.0, plain(kParamOutput) / 20.0);

    p.attack = plain(kParamAmpAttack);   p.decay = plain(kParamAmpDecay);
    p.sustain = norm[kParamAmpSustain];  p.release = plain(kParamAmpRelease);
    p.fAttack = plain(kParamFEnvAttack);   p.fDecay = plain(kParamFEnvDecay);
    p.fSustain = norm[kParamFEnvSustain];  p.fRelease = plain(kParamFEnvRelease);
    p.fAmount = plain(kParamFEnvAmount) / 100.0;

    p.lfoRate = plain(kParamLfoRate);    p.lfoDepth = norm[kParamLfoDepth];
    p.lfoWave = index(kParamLfoWave);    p.lfoTarget = index(kParamLfoTarget);
    p.revMix = norm[kParamRevMix];       p.revSize = norm[kParamRevSize];  p.revDamp = norm[kParamRevDamp];
    p.modWheel = norm[kParamModWheel];
    p.bend = plain(kParamPitchBend);
    return p;
}

inline double polyBlep(double t, double dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

inline double oscWave(int wave, double p, double dt, double shape)
{
    switch (wave)
    {
        case 0:  // SAW; Shape adds a phase-locked octave-up saw
        {
            double s = 2.0 * p - 1.0 - polyBlep(p, dt);
            if (shape > 0.0)
            {
                double p2 = p * 2.0;
                p2 -= std::floor(p2);
                const double s2 = 2.0 * p2 - 1.0 - polyBlep(p2, 2.0 * dt);
                s = (s + shape * s2) / (1.0 + 0.6 * shape);
            }
            return s;
        }
        case 1:  // TRI; Shape = wavefolder
        {
            double tri = 4.0 * std::fabs(p - 0.5) - 1.0;
            if (shape > 0.0)
            {
                const double a = std::min(1.0, shape * 3.0);
                const double g = 1.0 + 3.0 * shape;
                tri = (1.0 - a) * tri + a * std::sin(tri * g * 0.5 * kPi);
            }
            return tri;
        }
        default:  // SQR; Shape = pulse width (50% -> 5%)
        {
            const double pw = 0.5 - 0.45 * shape;
            double v = (p < pw) ? 1.0 : -1.0;
            v += polyBlep(p, dt);
            double q = p + 1.0 - pw;
            q -= std::floor(q);
            v -= polyBlep(q, dt);
            const double m = 2.0 * pw - 1.0;  // remove DC, keep peaks <= 1
            return (v - m) / (1.0 + std::fabs(m));
        }
    }
}

// Linear-phase decimate-by-4 low-pass (Kaiser window, ~80 dB stop band), symmetric-FIR optimised.
class Decimator
{
public:
    Decimator()
    {
        const int c = (kFirTaps - 1) / 2;
        const double fc = 0.48 / kOversample;  // cycles per oversampled sample (-6 dB point at 0.48 * host rate)
        const double beta = 8.0;
        double sum = 0.0;
        for (int n = 0; n < kFirTaps; ++n)
        {
            const double x = n - c;
            const double sinc = (x == 0.0) ? 1.0 : std::sin(2.0 * kPi * fc * x) / (2.0 * kPi * fc * x);
            const double r = x / c;
            const double w = bessel0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / bessel0(beta);
            h_[n] = 2.0 * fc * sinc * w;
            sum += h_[n];
        }
        for (double& v : h_) v /= sum;
        reset();
    }

    void reset() { std::fill(buf_, buf_ + kBuf, 0.0); pos_ = 0; }

    double push(const double* x)
    {
        for (int i = 0; i < kOversample; ++i)
            buf_[pos_++ & kMask] = x[i];
        const unsigned newest = pos_ - 1;
        const int c = (kFirTaps - 1) / 2;
        double y = h_[c] * buf_[(newest - c) & kMask];
        for (int k = 0; k < c; ++k)
            y += h_[k] * (buf_[(newest - k) & kMask] + buf_[(newest - (kFirTaps - 1 - k)) & kMask]);
        return y;
    }

private:
    static double bessel0(double x)
    {
        double s = 1.0, t = 1.0;
        for (int k = 1; k < 60; ++k) { const double q = x / (2.0 * k); t *= q * q; s += t; }
        return s;
    }
    static constexpr int kBuf = 256;
    static constexpr unsigned kMask = kBuf - 1;
    double h_[kFirTaps];
    double buf_[kBuf];
    unsigned pos_ = 0;
};

// Freeverb-style stereo reverb (8 damped combs + 4 allpasses per channel).
class Reverb
{
public:
    void setSampleRate(double sr)
    {
        static const int combTuning[8] = {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
        static const int allTuning[4] = {556, 441, 341, 225};
        const double k = sr / 44100.0;
        for (int i = 0; i < 8; ++i)
        {
            comb_[0][i].resize(static_cast<int>(combTuning[i] * k));
            comb_[1][i].resize(static_cast<int>((combTuning[i] + 23) * k));
        }
        for (int i = 0; i < 4; ++i)
        {
            all_[0][i].resize(static_cast<int>(allTuning[i] * k));
            all_[1][i].resize(static_cast<int>((allTuning[i] + 23) * k));
        }
        clear();
    }

    void clear()
    {
        for (int c = 0; c < 2; ++c)
        {
            for (auto& x : comb_[c]) x.clear();
            for (auto& x : all_[c]) x.clear();
        }
    }

    void setParams(double size, double damp)
    {
        feedback_ = 0.7 + 0.28 * size;
        damp1_ = 0.4 * damp;
        damp2_ = 1.0 - damp1_;
    }

    void process(double in, double& outL, double& outR)
    {
        const double x = in * 0.015;
        double o[2] = {0.0, 0.0};
        for (int c = 0; c < 2; ++c)
        {
            for (auto& cb : comb_[c]) o[c] += cb.process(x, feedback_, damp1_, damp2_);
            for (auto& ap : all_[c]) o[c] = ap.process(o[c]);
        }
        outL = o[0] * 3.0;
        outR = o[1] * 3.0;
    }

private:
    struct Comb
    {
        std::vector<double> buf;
        int idx = 0;
        double store = 0.0;
        void resize(int n) { buf.assign(std::max(1, n), 0.0); idx = 0; store = 0.0; }
        void clear() { std::fill(buf.begin(), buf.end(), 0.0); idx = 0; store = 0.0; }
        double process(double in, double fb, double d1, double d2)
        {
            const double out = buf[idx];
            store = out * d2 + store * d1;
            if (std::fabs(store) < 1e-30) store = 0.0;  // no denormals in the tail
            buf[idx] = in + store * fb;
            if (++idx >= static_cast<int>(buf.size())) idx = 0;
            return out;
        }
    };
    struct Allpass
    {
        std::vector<double> buf;
        int idx = 0;
        void resize(int n) { buf.assign(std::max(1, n), 0.0); idx = 0; }
        void clear() { std::fill(buf.begin(), buf.end(), 0.0); idx = 0; }
        double process(double in)
        {
            const double bo = buf[idx];
            const double out = bo - in;
            buf[idx] = in + bo * 0.5;
            if (std::fabs(buf[idx]) < 1e-30) buf[idx] = 0.0;
            if (++idx >= static_cast<int>(buf.size())) idx = 0;
            return out;
        }
    };
    Comb comb_[2][8];
    Allpass all_[2][4];
    double feedback_ = 0.84, damp1_ = 0.2, damp2_ = 0.8;
};

class Synth
{
public:
    Synth() { reset(); }

    void setSampleRate(double sr)
    {
        fs_ = sr;
        fsOs_ = sr * kOversample;
        smooth_ = 1.0 - std::exp(-1.0 / (0.005 * sr));  // ~5 ms parameter smoothing
        reverb_.setSampleRate(sr);
    }

    void setParams(const SynthParams& p) { target_ = p; }

    void reset()
    {
        for (Voice& v : voices_) v = Voice();
        dec_.reset();
        reverb_.clear();
        cur_ = target_;
        ageCounter_ = 0;
        lfoPhase_ = 0.0;
        lfoHold_ = 0.0;
        rng_ = 12345u;
    }

    int activeVoices() const
    {
        int n = 0;
        for (const Voice& v : voices_) n += v.active ? 1 : 0;
        return n;
    }

    void noteOn(int note, double velocity)
    {
        Voice* v = nullptr;
        for (Voice& x : voices_) if (x.active && x.note == note) { v = &x; break; }  // retrigger
        bool fresh = false;
        if (!v) for (Voice& x : voices_) if (!x.active) { v = &x; fresh = true; break; }
        if (!v)  // steal: quietest releasing voice, else the oldest
        {
            double bestKey = 1e300;
            for (Voice& x : voices_)
            {
                const double key = (x.amp.stage == Env::Release) ? x.amp.level : 10.0 + static_cast<double>(x.age);
                if (key < bestKey) { bestKey = key; v = &x; }
            }
        }
        if (fresh)
        {
            v->p1 = v->p2 = v->last2 = 0.0;
            v->amp = EnvState();
            v->flt = EnvState();
            v->hp.reset();
            v->lp.reset();
            v->dc = 0.0;
        }
        v->active = true;
        v->note = note;
        v->vel = std::min(1.0, std::max(0.0, velocity));
        v->baseHz = 440.0 * std::exp2((note - 69) / 12.0);
        v->age = ++ageCounter_;
        v->amp.stage = Env::Attack;
        v->flt.stage = Env::Attack;
    }

    void noteOff(int note)
    {
        for (Voice& v : voices_)
            if (v.active && v.note == note && v.amp.stage != Env::Release)
            {
                v.amp.stage = Env::Release;
                v.flt.stage = Env::Release;
            }
    }

    void render(float* l, float* r, int n)
    {
        cur_.wave1 = target_.wave1;   cur_.wave2 = target_.wave2;
        cur_.oct1 = target_.oct1;     cur_.oct2 = target_.oct2;
        cur_.syncMode = target_.syncMode;
        cur_.driveType = target_.driveType;
        cur_.lfoWave = target_.lfoWave; cur_.lfoTarget = target_.lfoTarget;
        cur_.lfoRate = target_.lfoRate;
        cur_.attack = target_.attack; cur_.decay = target_.decay; cur_.release = target_.release;
        cur_.fAttack = target_.fAttack; cur_.fDecay = target_.fDecay; cur_.fRelease = target_.fRelease;
        ampC_.atkInc = 1.0 / (std::max(0.0005, cur_.attack) * fs_);
        ampC_.decCoef = std::exp(-6.9 / (std::max(0.001, cur_.decay) * fs_));
        ampC_.relCoef = std::exp(-6.9 / (std::max(0.001, cur_.release) * fs_));
        fltC_.atkInc = 1.0 / (std::max(0.0005, cur_.fAttack) * fs_);
        fltC_.decCoef = std::exp(-6.9 / (std::max(0.001, cur_.fDecay) * fs_));
        fltC_.relCoef = std::exp(-6.9 / (std::max(0.001, cur_.fRelease) * fs_));
        reverb_.setParams(target_.revSize, target_.revDamp);

        for (int i = 0; i < n; ++i)
        {
            glide(cur_.pitch1, target_.pitch1);   glide(cur_.pitch2, target_.pitch2);
            glide(cur_.shape1, target_.shape1);   glide(cur_.shape2, target_.shape2);
            glide(cur_.xmod, target_.xmod);       glide(cur_.oscMix, target_.oscMix);
            glide(cur_.hpHz, target_.hpHz);       glide(cur_.hpK, target_.hpK);
            glide(cur_.lpHz, target_.lpHz);       glide(cur_.lpK, target_.lpK);
            glide(cur_.driveGain, target_.driveGain);
            glide(cur_.dryWet, target_.dryWet);   glide(cur_.outGain, target_.outGain);
            glide(cur_.sustain, target_.sustain); glide(cur_.fSustain, target_.fSustain);
            glide(cur_.fAmount, target_.fAmount);
            glide(cur_.lfoDepth, target_.lfoDepth);
            glide(cur_.modWheel, target_.modWheel);
            glide(cur_.bend, target_.bend);
            glide(cur_.revMix, target_.revMix);
            ampC_.sustain = cur_.sustain;
            fltC_.sustain = cur_.fSustain;

            // LFO (shared by all voices)
            lfoPhase_ += cur_.lfoRate / fs_;
            if (lfoPhase_ >= 1.0)
            {
                lfoPhase_ -= 1.0;
                rng_ = rng_ * 1664525u + 1013904223u;
                lfoHold_ = static_cast<double>(rng_ >> 8) / 8388608.0 - 1.0;  // -1..1
            }
            double lfo = 0.0;
            switch (cur_.lfoWave)
            {
                case 0: lfo = std::sin(2.0 * kPi * lfoPhase_); break;
                case 1: lfo = 4.0 * std::fabs(lfoPhase_ - 0.5) - 1.0; break;
                case 2: lfo = lfoPhase_ < 0.5 ? 1.0 : -1.0; break;
                default: lfo = lfoHold_; break;
            }
            const double depth = std::min(1.0, cur_.lfoDepth + cur_.modWheel);
            double lfoPitch = 0.0, lfoShape = 0.0, lfoCutOct = 0.0, lfoAmp = 1.0;
            switch (cur_.lfoTarget)
            {
                case 0: lfoPitch = lfo * depth * depth * 24.0; break;
                case 1: lfoShape = lfo * depth * 0.5; break;
                case 2: lfoCutOct = lfo * depth * 4.0; break;
                default: lfoAmp = 1.0 - depth * 0.5 * (1.0 - lfo); break;
            }

            const double hpG = toG(cur_.hpHz);
            const bool fuzz = cur_.driveType == 1;
            const double drive01 = (cur_.driveGain - 1.0) / (Limits::kMaxDriveGain - 1.0);
            const double driveG = fuzz ? 1.0 + (cur_.driveGain - 1.0) * 6.0 : cur_.driveGain;
            const double comp = fuzz ? 1.0 : 1.0 / std::sqrt(cur_.driveGain);
            const double fuzzMix = std::min(1.0, drive01 * 4.0);        // FUZZ fades in over the first quarter of the knob
            const double fuzzOut = (1.0 + 0.6 * drive01) / (1.0 + 1.5 * drive01);
            const double gate = 0.04 * drive01;                         // FUZZ: sputtery noise-gate on the pre-gain signal
            const double dcK = 2.0 * kPi * 8.0 / fsOs_;                 // FUZZ: ~8 Hz DC blocker
            const double g1 = std::min(1.0, 2.0 * (1.0 - cur_.oscMix));
            const double g2 = std::min(1.0, 2.0 * cur_.oscMix);
            const double xmSemis = cur_.xmod * cur_.xmod * 24.0;
            const bool useXMod = xmSemis > 0.01;
            const bool sync = cur_.syncMode == 2, ring = cur_.syncMode == 1;
            const double bendMul = std::exp2((cur_.bend + lfoPitch) / 12.0);
            const double mul1 = std::ldexp(1.0, cur_.oct1 - 1) * std::exp2(cur_.pitch1 / 12.0) * bendMul;
            const double mul2 = std::ldexp(1.0, cur_.oct2 - 1) * std::exp2(cur_.pitch2 / 12.0) * bendMul;
            const double sh1 = std::min(1.0, std::max(0.0, cur_.shape1 + lfoShape));
            const double sh2 = std::min(1.0, std::max(0.0, cur_.shape2 + lfoShape));

            double acc[kOversample] = {0.0, 0.0, 0.0, 0.0};
            for (Voice& v : voices_)
            {
                if (!v.active) continue;
                const double ae = stepEnv(v.amp, ampC_);
                const double fe = stepEnv(v.flt, fltC_);
                if (v.amp.stage == Env::Idle) { v.active = false; continue; }
                const double amp = ae * (0.25 + 0.75 * v.vel) * lfoAmp;
                const double lpG = toG(cur_.lpHz * std::exp2(cur_.fAmount * 6.0 * fe + lfoCutOct));
                const double dt1base = v.baseHz * mul1 / fsOs_;
                const double dt2 = std::min(0.45, v.baseHz * mul2 / fsOs_);

                for (int k = 0; k < kOversample; ++k)
                {
                    double dt1 = dt1base;
                    if (useXMod) dt1 *= std::exp2(xmSemis * v.last2 / 12.0);  // OSC2 -> OSC1 pitch
                    dt1 = std::min(0.45, dt1);

                    v.p1 += dt1;
                    bool wrapped = false;
                    if (v.p1 >= 1.0) { v.p1 -= 1.0; wrapped = true; }
                    const double o1 = oscWave(cur_.wave1, v.p1, dt1, sh1);

                    if (sync && wrapped && dt1 > 1e-9) v.p2 = std::min(0.999, v.p1 / dt1 * dt2);  // hard sync
                    else { v.p2 += dt2; if (v.p2 >= 1.0) v.p2 -= 1.0; }
                    double o2 = oscWave(cur_.wave2, v.p2, dt2, sh2);
                    v.last2 = o2;
                    if (ring) o2 *= o1;

                    const double dry = 0.5 * (g1 * o1 + g2 * o2);
                    double x;
                    if (fuzz)
                    {
                        const double u = driveG * dry;
                        // asymmetric clipping: flat top on the positive half, softer bottom on the negative half
                        double y = u >= 0.0 ? std::min(u, 0.5) : -0.9 * std::tanh(-u / 0.9);
                        y += 0.25 * drive01 * std::min(std::fabs(u), 0.5);   // rectified component = octave-up grit
                        y *= fuzzOut;
                        const double a = std::fabs(dry);
                        if (a < gate) y *= a / gate;                    // gate: notes sputter out as they decay
                        v.dc += dcK * (y - v.dc);
                        x = dry + fuzzMix * ((y - v.dc) - dry);
                    }
                    else
                        x = std::tanh(driveG * dry);
                    x *= comp;
                    x = v.hp.highPass(x, hpG, cur_.hpK);
                    x = v.lp.lowPass(x, lpG, cur_.lpK);
                    acc[k] += amp * (dry + cur_.dryWet * (x - dry));
                }
            }

            const double y = dec_.push(acc) * cur_.outGain * kMasterGain;
            double wl, wr;
            reverb_.process(y, wl, wr);
            const double m = cur_.revMix;
            const double outL = y * (1.0 - m) + wl * m * kReverbWetGain;
            const double outR = y * (1.0 - m) + wr * m * kReverbWetGain;
            l[i] = static_cast<float>(std::min(4.0, std::max(-4.0, outL)));
            if (r) r[i] = static_cast<float>(std::min(4.0, std::max(-4.0, outR)));
        }
    }

private:
    enum class Env { Idle, Attack, Decay, Sustain, Release };
    struct EnvState { Env stage = Env::Idle; double level = 0.0; };
    struct EnvCoefs { double atkInc = 0.01, decCoef = 0.999, relCoef = 0.999, sustain = 0.7; };

    struct Voice
    {
        bool active = false;
        int note = -1;
        double vel = 0.0, baseHz = 440.0;
        unsigned long long age = 0;
        double p1 = 0.0, p2 = 0.0, last2 = 0.0;
        EnvState amp, flt;
        Stage hp, lp;
        double dc = 0.0;  // DC blocker state for the FUZZ drive
    };

    static double stepEnv(EnvState& e, const EnvCoefs& c)
    {
        switch (e.stage)
        {
            case Env::Attack:
                e.level += c.atkInc;
                if (e.level >= 1.0) { e.level = 1.0; e.stage = Env::Decay; }
                break;
            case Env::Decay:
                e.level = c.sustain + (e.level - c.sustain) * c.decCoef;
                if (e.level - c.sustain < 1e-4) { e.level = c.sustain; e.stage = Env::Sustain; }
                break;
            case Env::Sustain:
                e.level += (c.sustain - e.level) * 0.01;
                break;
            case Env::Release:
                e.level *= c.relCoef;
                if (e.level < 1e-5) { e.level = 0.0; e.stage = Env::Idle; }
                break;
            default: break;
        }
        return e.level;
    }

    void glide(double& cur, double tgt) const { cur += smooth_ * (tgt - cur); }

    double toG(double hz) const
    {
        hz = std::min(hz, 0.45 * fsOs_);
        const double g = std::tan(kPi * hz / fsOs_);
        return g / (1.0 + g);
    }

    double fs_ = 44100.0, fsOs_ = 44100.0 * kOversample, smooth_ = 0.005;
    EnvCoefs ampC_, fltC_;
    unsigned long long ageCounter_ = 0;
    double lfoPhase_ = 0.0, lfoHold_ = 0.0;
    unsigned rng_ = 12345u;
    SynthParams target_, cur_;
    Voice voices_[kMaxVoices];
    Decimator dec_;
    Reverb reverb_;
};

} // namespace MS20
