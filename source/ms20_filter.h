#pragma once
// One MS-20-style filter stage: 12 dB/oct Sallen-Key-like (Korg-35 topology) built from
// zero-delay-feedback (TPT) one-pole filters, with a tanh-saturated resonance feedback path.
//
//   LP stage: x -> LP1 -> (+) -> LP2 -> y     feedback: y*K -> HP -> back into (+)
//   HP stage: x -> HP1 -> (+) -> HP2 -> y     feedback: y*K -> LP -> back into (+)
//
// Linear analysis: 1 - K*s/(1+s)^2 = 0, so self-oscillation starts at K = 2.

#include <cmath>

namespace MS20 {

constexpr double kPi = 3.14159265358979323846;

class Stage
{
public:
    void reset() { z1_ = z2_ = z3_ = 0.0; }

    // G = g / (1 + g) with g = tan(pi * fc / fs). K = resonance feedback gain.
    double lowPass(double x, double G, double K)
    {
        const double Gm = 1.0 - G;

        const double v1 = G * (x - z1_);            // LP1
        const double y1 = v1 + z1_;
        z1_ = y1 + v1;

        // Delay-free loop solved for the *linear* output; that value drives the saturator
        const double y2lin = (G * y1 + Gm * z2_ - G * Gm * z3_) / (1.0 - K * G * Gm);
        const double fb = std::tanh(K * y2lin);

        const double v3 = G * (fb - z3_);           // HP in the feedback path
        const double lp3 = v3 + z3_;
        z3_ = lp3 + v3;
        const double hp = fb - lp3;

        const double u = y1 + hp;                   // LP2
        const double v2 = G * (u - z2_);
        const double y2 = v2 + z2_;
        z2_ = y2 + v2;
        return y2;
    }

    double highPass(double x, double G, double K)
    {
        const double Gm = 1.0 - G;

        const double v1 = G * (x - z1_);            // HP1
        const double lp1 = v1 + z1_;
        z1_ = lp1 + v1;
        const double y1 = x - lp1;

        const double y2lin = Gm * (y1 + Gm * z3_ - z2_) / (1.0 - K * G * Gm);
        const double fb = std::tanh(K * y2lin);

        const double v3 = G * (fb - z3_);           // LP in the feedback path
        const double l = v3 + z3_;
        z3_ = l + v3;

        const double u = y1 + l;                    // HP2
        const double v2 = G * (u - z2_);
        const double lp2 = v2 + z2_;
        z2_ = lp2 + v2;
        return u - lp2;
    }

private:
    double z1_ = 0.0, z2_ = 0.0, z3_ = 0.0;
};

} // namespace MS20
