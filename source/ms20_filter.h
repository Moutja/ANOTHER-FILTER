#pragma once
// Header-only DSP for an MS-20-style filter section:
//
//   input -> drive (tanh) -> HP stage -> LP stage -> dry/wet mix -> output trim
//
// Each stage is a 12 dB/oct Sallen-Key-style (Korg-35 topology) filter built from
// zero-delay-feedback (TPT) one-pole filters. The resonance feedback path is soft-clipped
// with tanh, which is what gives the MS-20 its driven, screamy peak and lets it
// self-oscillate at a bounded level.
//
//   LP stage: x -> LP1 -> (+) -> LP2 -> y     feedback: y*K -> HP -> back into (+)
//   HP stage: x -> HP1 -> (+) -> HP2 -> y     feedback: y*K -> LP -> back into (+)
//
// Linear analysis: 1 - K*s/(1+s)^2 = 0, so self-oscillation starts at K = 2.

#include <algorithm>
#include <cmath>

namespace MS20 {

constexpr double kPi = 3.14159265358979323846;

struct Targets
{
    double hpHz = 20.0;
    double hpK = 0.0;
    double lpHz = 20000.0;
    double lpK = 0.0;
    double driveGain = 1.0;
    double mix = 1.0;
    double outGain = 1.0;
};

class Stage
{
public:
    void reset() { z1_ = z2_ = z3_ = 0.0; }

    // G = g / (1 + g) with g = tan(pi * fc / fs). K = resonance feedback gain.
    double lowPass(double x, double G, double K)
    {
        const double Gm = 1.0 - G;

        // LP1
        const double v1 = G * (x - z1_);
        const double y1 = v1 + z1_;
        z1_ = y1 + v1;

        // Solve the delay-free loop for the *linear* output, used to drive the saturator
        const double y2lin = (G * y1 + Gm * z2_ - G * Gm * z3_) / (1.0 - K * G * Gm);
        const double fb = std::tanh(K * y2lin);

        // HP in the feedback path
        const double v3 = G * (fb - z3_);
        const double lp3 = v3 + z3_;
        z3_ = lp3 + v3;
        const double hp = fb - lp3;

        // LP2
        const double u = y1 + hp;
        const double v2 = G * (u - z2_);
        const double y2 = v2 + z2_;
        z2_ = y2 + v2;
        return y2;
    }

    double highPass(double x, double G, double K)
    {
        const double Gm = 1.0 - G;

        // HP1
        const double v1 = G * (x - z1_);
        const double lp1 = v1 + z1_;
        z1_ = lp1 + v1;
        const double y1 = x - lp1;

        const double y2lin = Gm * (y1 + Gm * z3_ - z2_) / (1.0 - K * G * Gm);
        const double fb = std::tanh(K * y2lin);

        // LP in the feedback path
        const double v3 = G * (fb - z3_);
        const double l = v3 + z3_;
        z3_ = l + v3;

        // HP2
        const double u = y1 + l;
        const double v2 = G * (u - z2_);
        const double lp2 = v2 + z2_;
        z2_ = lp2 + v2;
        return u - lp2;
    }

private:
    double z1_ = 0.0, z2_ = 0.0, z3_ = 0.0;
};

class Filter
{
public:
    static constexpr int kMaxChannels = 2;

    void setSampleRate(double sr)
    {
        sr_ = sr;
        smooth_ = 1.0 - std::exp(-1.0 / (0.005 * sr));  // ~5 ms parameter smoothing
    }

    void setTargets(const Targets& t) { t_ = t; }

    void reset()
    {
        cur_ = t_;
        for (int c = 0; c < kMaxChannels; ++c) { hp_[c].reset(); lp_[c].reset(); }
    }

    void process(const float* const* in, float* const* out, int numCh, int numSamples)
    {
        numCh = std::min(numCh, kMaxChannels);
        for (int s = 0; s < numSamples; ++s)
        {
            glide(cur_.hpHz, t_.hpHz);       glide(cur_.hpK, t_.hpK);
            glide(cur_.lpHz, t_.lpHz);       glide(cur_.lpK, t_.lpK);
            glide(cur_.driveGain, t_.driveGain);
            glide(cur_.mix, t_.mix);         glide(cur_.outGain, t_.outGain);

            const double hpG = toG(cur_.hpHz);
            const double lpG = toG(cur_.lpHz);
            const double comp = 1.0 / std::sqrt(cur_.driveGain);

            for (int c = 0; c < numCh; ++c)
            {
                const double dry = in[c][s];
                double x = std::tanh(cur_.driveGain * dry) * comp;
                x = hp_[c].highPass(x, hpG, cur_.hpK);
                x = lp_[c].lowPass(x, lpG, cur_.lpK);
                out[c][s] = static_cast<float>(cur_.outGain * (dry + cur_.mix * (x - dry)));
            }
        }
    }

private:
    void glide(double& cur, double tgt) const { cur += smooth_ * (tgt - cur); }

    double toG(double hz) const
    {
        hz = std::min(hz, 0.45 * sr_);
        const double g = std::tan(kPi * hz / sr_);
        return g / (1.0 + g);
    }

    double sr_ = 44100.0, smooth_ = 0.005;
    Targets t_, cur_;
    Stage hp_[kMaxChannels], lp_[kMaxChannels];
};

} // namespace MS20
