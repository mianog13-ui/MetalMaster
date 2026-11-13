#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

// Small, dependency-free DSP building blocks for MetalMaster.
namespace mm
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kButterQ = 0.70710678118654752;

static inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
static inline float linToDb (float lin) noexcept { return 20.0f * std::log10 (std::max (lin, 1.0e-6f)); }

//==============================================================================
// Stereo biquad (RBJ cookbook), double precision, no allocations.
class Biquad
{
public:
    void reset() noexcept
    {
        for (auto& v : z1) v = 0.0;
        for (auto& v : z2) v = 0.0;
    }

    void setCoefficients (double nb0, double nb1, double nb2, double na1, double na2) noexcept
    {
        b0 = nb0; b1 = nb1; b2 = nb2; a1 = na1; a2 = na2;
    }

    void makeLowPass (double fs, double f, double q) noexcept
    {
        f = clampFreq (fs, f);
        const double w0 = 2.0 * kPi * f / fs, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        normalise ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + alpha, -2 * c, 1 - alpha);
    }

    void makeHighPass (double fs, double f, double q) noexcept
    {
        f = clampFreq (fs, f);
        const double w0 = 2.0 * kPi * f / fs, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        normalise ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + alpha, -2 * c, 1 - alpha);
    }

    void makeAllPass (double fs, double f, double q) noexcept
    {
        f = clampFreq (fs, f);
        const double w0 = 2.0 * kPi * f / fs, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        normalise (1 - alpha, -2 * c, 1 + alpha, 1 + alpha, -2 * c, 1 - alpha);
    }

    void makePeak (double fs, double f, double q, double gainDb) noexcept
    {
        f = clampFreq (fs, f);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * f / fs, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        normalise (1 + alpha * A, -2 * c, 1 - alpha * A, 1 + alpha / A, -2 * c, 1 - alpha / A);
    }

    void makeLowShelf (double fs, double f, double q, double gainDb) noexcept
    {
        f = clampFreq (fs, f);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * f / fs, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double t = 2.0 * std::sqrt (A) * alpha;
        normalise (A * ((A + 1) - (A - 1) * c + t),
                   2 * A * ((A - 1) - (A + 1) * c),
                   A * ((A + 1) - (A - 1) * c - t),
                   (A + 1) + (A - 1) * c + t,
                   -2 * ((A - 1) + (A + 1) * c),
                   (A + 1) + (A - 1) * c - t);
    }

    void makeHighShelf (double fs, double f, double q, double gainDb) noexcept
    {
        f = clampFreq (fs, f);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * kPi * f / fs, c = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double t = 2.0 * std::sqrt (A) * alpha;
        normalise (A * ((A + 1) + (A - 1) * c + t),
                   -2 * A * ((A - 1) + (A + 1) * c),
                   A * ((A + 1) + (A - 1) * c - t),
                   (A + 1) - (A - 1) * c + t,
                   2 * ((A - 1) - (A + 1) * c),
                   (A + 1) - (A - 1) * c - t);
    }

    inline double process (int ch, double x) noexcept
    {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

private:
    static double clampFreq (double fs, double f) noexcept { return std::clamp (f, 5.0, 0.45 * fs); }

    void normalise (double nb0, double nb1, double nb2, double a0, double na1, double na2) noexcept
    {
        setCoefficients (nb0 / a0, nb1 / a0, nb2 / a0, na1 / a0, na2 / a0);
    }

    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] { 0, 0 }, z2[2] { 0, 0 };
};

//==============================================================================
// Linkwitz-Riley 4th order low/high pair (two cascaded Butterworth sections each).
struct LR4
{
    void set (double fs, double f) noexcept
    {
        lp1.makeLowPass (fs, f, kButterQ);  lp2.makeLowPass (fs, f, kButterQ);
        hp1.makeHighPass (fs, f, kButterQ); hp2.makeHighPass (fs, f, kButterQ);
    }
    void reset() noexcept { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    inline double lp (int ch, double x) noexcept { return lp2.process (ch, lp1.process (ch, x)); }
    inline double hp (int ch, double x) noexcept { return hp2.process (ch, hp1.process (ch, x)); }

    Biquad lp1, lp2, hp1, hp2;
};

//==============================================================================
// Approximate 4x true-peak detector (12-tap windowed-sinc interpolation, 3 inter-sample points).
// push() returns the peak around the sample that arrived kDelay samples ago.
class TruePeakDetector
{
public:
    static constexpr int kTaps = 12;
    static constexpr int kDelay = 6;

    TruePeakDetector()
    {
        for (int p = 0; p < 3; ++p)
        {
            const double f = (p + 1) * 0.25;
            double tmp[kTaps];
            double sum = 0.0;

            for (int j = 0; j < kTaps; ++j)
            {
                const double k = (double) (j - 5);
                const double t = f - k;
                const double sinc = std::abs (t) < 1.0e-9 ? 1.0 : std::sin (kPi * t) / (kPi * t);
                const double w = 0.5 * (1.0 + std::cos (kPi * t / 6.0));
                tmp[j] = sinc * w;
                sum += tmp[j];
            }
            for (int j = 0; j < kTaps; ++j)
                coef[p][j] = (float) (tmp[j] / sum);
        }
        reset();
    }

    void reset() noexcept { hist.fill (0.0f); pos = 0; }

    float push (float x, bool interpolate) noexcept
    {
        hist[(size_t) pos] = x;
        const int oldest = (pos + 1 >= kTaps) ? 0 : pos + 1;

        float mx = std::abs (hist[(size_t) ((oldest + 5) % kTaps)]);

        if (interpolate)
        {
            for (int p = 0; p < 3; ++p)
            {
                float acc = 0.0f;
                int idx = oldest;
                for (int j = 0; j < kTaps; ++j)
                {
                    acc += coef[p][j] * hist[(size_t) idx];
                    if (++idx == kTaps) idx = 0;
                }
                mx = std::max (mx, std::abs (acc));
            }
        }

        pos = oldest;
        return mx;
    }

private:
    float coef[3][kTaps] {};
    std::array<float, kTaps> hist {};
    int pos = 0;
};

//==============================================================================
// Feed-forward compressor gain computer (peak detector, soft knee, smoothing in dB).
class Compressor
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; reset(); }
    void reset() noexcept { grDb = 0.0f; }

    void setParams (float thrDb, float ratio, float attackMs, float releaseMs, float kneeDb) noexcept
    {
        thr = thrDb;
        slope = 1.0f / std::max (1.0f, ratio) - 1.0f;
        knee = std::max (0.5f, kneeDb);
        atk = (float) std::exp (-1.0 / (std::max (0.05f, attackMs) * 0.001 * fs));
        rel = (float) std::exp (-1.0 / (std::max (1.0f, releaseMs) * 0.001 * fs));
    }

    // Returns the linear gain to apply (without make-up).
    inline float getGain (float level) noexcept
    {
        const float over = linToDb (level) - thr;
        float target = 0.0f;

        if (2.0f * over < -knee)
            target = 0.0f;
        else if (2.0f * std::abs (over) <= knee)
        {
            const float t = over + knee * 0.5f;
            target = slope * t * t / (2.0f * knee);
        }
        else
            target = slope * over;

        const float coef = (target < grDb) ? atk : rel;
        grDb = coef * grDb + (1.0f - coef) * target;
        return dbToLin (grDb);
    }

    float getReductionDb() const noexcept { return -grDb; }

private:
    double fs = 44100.0;
    float thr = 0.0f, slope = 0.0f, knee = 6.0f, atk = 0.99f, rel = 0.999f, grDb = 0.0f;
};

//==============================================================================
// 4-band multiband compressor. Phase-coherent Linkwitz-Riley split with all-pass compensation:
//   band0 = LP1 -> AP2 -> AP3,  band1 = LP2(HP1) -> AP3,  band2 = LP3(HP2(HP1)),  band3 = HP3(HP2(HP1)).
class MultibandCompressor
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& c : comp) c.prepare (fs);
        reset();
    }

    void reset() noexcept
    {
        lr1.reset(); lr2.reset(); lr3.reset();
        apLow2.reset(); apLow3.reset(); apMid3.reset();
        for (auto& c : comp) c.reset();
    }

    void setCrossovers (double f1, double f2, double f3) noexcept
    {
        f1 = std::clamp (f1, 30.0, 0.30 * fs);
        f2 = std::clamp (std::max (f2, f1 * 1.4), 30.0, 0.35 * fs);
        f3 = std::clamp (std::max (f3, f2 * 1.4), 30.0, 0.42 * fs);

        lr1.set (fs, f1); lr2.set (fs, f2); lr3.set (fs, f3);
        apLow2.makeAllPass (fs, f2, kButterQ);
        apLow3.makeAllPass (fs, f3, kButterQ);
        apMid3.makeAllPass (fs, f3, kButterQ);
    }

    void setBand (int i, float thrDb, float ratio, float gainDb, float attackMs, float releaseMs) noexcept
    {
        const float timeScale = (i == 0) ? 2.0f : 1.0f;   // low band reacts more slowly
        comp[(size_t) i].setParams (thrDb, ratio, attackMs * timeScale, releaseMs * timeScale, 6.0f);
        gainLin[i] = dbToLin (gainDb);
    }

    // maxGrDb: array of 4, receives the maximum gain reduction (dB) seen in this block.
    void process (float* l, float* r, int n, float* maxGrDb) noexcept
    {
        for (int b = 0; b < 4; ++b) maxGrDb[b] = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            double band[4][2];

            for (int c = 0; c < 2; ++c)
            {
                const double x = (c == 0) ? (double) l[i] : (double) r[i];

                const double lo1 = lr1.lp (c, x),   hi1 = lr1.hp (c, x);
                const double lo2 = lr2.lp (c, hi1), hi2 = lr2.hp (c, hi1);
                const double lo3 = lr3.lp (c, hi2), hi3 = lr3.hp (c, hi2);

                band[0][c] = apLow3.process (c, apLow2.process (c, lo1));
                band[1][c] = apMid3.process (c, lo2);
                band[2][c] = lo3;
                band[3][c] = hi3;
            }

            double outL = 0.0, outR = 0.0;
            for (int b = 0; b < 4; ++b)
            {
                const float bl = (float) band[b][0], br = (float) band[b][1];
                const float level = std::max (std::abs (bl), std::abs (br));
                const float g = comp[(size_t) b].getGain (level) * gainLin[b];
                outL += (double) (bl * g);
                outR += (double) (br * g);
                maxGrDb[b] = std::max (maxGrDb[b], comp[(size_t) b].getReductionDb());
            }

            l[i] = (float) outL;
            r[i] = (float) outR;
        }
    }

private:
    double fs = 44100.0;
    LR4 lr1, lr2, lr3;
    Biquad apLow2, apLow3, apMid3;
    std::array<Compressor, 4> comp;
    float gainLin[4] { 1.0f, 1.0f, 1.0f, 1.0f };
};

//==============================================================================
// Look-ahead true-peak limiter (stereo-linked).
// True-peak estimate -> gain computer -> sliding minimum over look-ahead window
//   -> instant-attack / exponential-release envelope -> moving average -> applied to delayed audio.
class LookaheadLimiter
{
public:
    void prepare (double sampleRate, double lookaheadMs)
    {
        fs = sampleRate;
        lookahead = std::max (1, (int) std::ceil (lookaheadMs * 0.001 * fs));
        size = lookahead + 1;
        aSize = lookahead + TruePeakDetector::kDelay + 1;
        reset();
    }

    void reset()
    {
        delayL.assign ((size_t) aSize, 0.0f);
        delayR.assign ((size_t) aSize, 0.0f);
        minBuf.assign ((size_t) size, 1.0f);
        avgBuf.assign ((size_t) size, 1.0);
        pos = 0;
        aPos = 0;
        env = 1.0;
        sum = (double) size;
        tpL.reset();
        tpR.reset();
    }

    int getLatency() const noexcept { return lookahead + TruePeakDetector::kDelay; }

    void setCeilingDb (float db) noexcept { ceiling = dbToLin (db); }
    void setTruePeak (bool on) noexcept { truePeak = on; }

    void setReleaseMs (float ms) noexcept
    {
        relCoef = std::exp (-1.0 / (std::max (1.0f, ms) * 0.001 * fs));
    }

    // Returns the maximum gain reduction (dB, positive) seen in this block.
    float processBlock (float* l, float* r, int n) noexcept
    {
        float minGain = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            const float inL = l[i], inR = r[i];
            const float peak = std::max (tpL.push (inL, truePeak), tpR.push (inR, truePeak));
            const float g = peak > ceiling ? ceiling / peak : 1.0f;

            delayL[(size_t) aPos] = inL;
            delayR[(size_t) aPos] = inR;
            minBuf[(size_t) pos] = g;

            float m = 1.0f;
            for (float v : minBuf)
                m = std::min (m, v);

            env = (m < env) ? (double) m : (double) m + (env - (double) m) * relCoef;

            sum += env - avgBuf[(size_t) pos];
            avgBuf[(size_t) pos] = env;
            const float gain = (float) (sum / (double) size);

            const int rd = (aPos + 1 >= aSize) ? 0 : aPos + 1;   // oldest sample
            l[i] = std::clamp (delayL[(size_t) rd] * gain, -ceiling, ceiling);
            r[i] = std::clamp (delayR[(size_t) rd] * gain, -ceiling, ceiling);

            minGain = std::min (minGain, gain);
            aPos = rd;
            pos = (pos + 1 >= size) ? 0 : pos + 1;
        }

        return minGain < 1.0f ? -linToDb (minGain) : 0.0f;
    }

private:
    double fs = 44100.0;
    int lookahead = 88, size = 89, aSize = 95, pos = 0, aPos = 0;
    float ceiling = 0.89f;
    bool truePeak = true;
    double relCoef = 0.999, env = 1.0, sum = 0.0;
    std::vector<float> delayL, delayR, minBuf;
    std::vector<double> avgBuf;
    TruePeakDetector tpL, tpR;
};

//==============================================================================
// ITU-R BS.1770-4 loudness: momentary (400 ms), short-term (3 s), integrated (gated),
// plus true-peak hold. process() runs on the audio thread; getters are thread-safe.
class LoudnessMeter
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        subBlockLen = (int) std::round (0.1 * fs);

        {   // Stage 1: high-shelf "pre-filter"
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.setCoefficients ((Vh + Vb * K / Q + K * K) / a0,
                                   2.0 * (K * K - Vh) / a0,
                                   (Vh - Vb * K / Q + K * K) / a0,
                                   2.0 * (K * K - 1.0) / a0,
                                   (1.0 - K / Q + K * K) / a0);
        }
        {   // Stage 2: RLB high-pass
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            hp.setCoefficients (1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0);
        }
        doReset();
    }

    void requestReset() noexcept { resetRequested.store (true); }

    void process (const float* l, const float* r, int n) noexcept
    {
        if (resetRequested.exchange (false))
            doReset();

        float mxL = 0.0f, mxR = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            mxL = std::max (mxL, tpL.push (l[i], true));
            mxR = std::max (mxR, tpR.push (r[i], true));

            const double yl = hp.process (0, shelf.process (0, (double) l[i]));
            const double yr = hp.process (1, shelf.process (1, (double) r[i]));
            acc += yl * yl + yr * yr;

            if (++count >= subBlockLen)
                endSubBlock();
        }

        if (mxL > peakL.load()) peakL.store (mxL);
        if (mxR > peakR.load()) peakR.store (mxR);
    }

    float getMomentary()  const noexcept { return momentary.load(); }
    float getShortTerm()  const noexcept { return shortTerm.load(); }
    float getIntegrated() const noexcept { return integrated.load(); }
    float getPeakL()      const noexcept { return peakL.load(); }
    float getPeakR()      const noexcept { return peakR.load(); }

private:
    static constexpr int kRing = 30;      // 30 x 100 ms = 3 s
    static constexpr int kBins = 900;     // -70 .. +20 LUFS in 0.1 LU steps

    static float toLufs (double meanSquare) noexcept
    {
        return meanSquare <= 1.0e-12 ? -120.0f : (float) (-0.691 + 10.0 * std::log10 (meanSquare));
    }

    void doReset() noexcept
    {
        shelf.reset();
        hp.reset();
        tpL.reset();
        tpR.reset();
        acc = 0.0;
        count = 0;
        ring.fill (0.0);
        ringPos = 0;
        ringFilled = 0;
        histSum.fill (0.0);
        histCount.fill (0.0);
        momentary.store (-100.0f);
        shortTerm.store (-100.0f);
        integrated.store (-100.0f);
        peakL.store (0.0f);
        peakR.store (0.0f);
    }

    void endSubBlock() noexcept
    {
        const double ms = acc / (double) subBlockLen;
        acc = 0.0;
        count = 0;

        ring[(size_t) ringPos] = ms;
        ringPos = (ringPos + 1) % kRing;
        ringFilled = std::min (ringFilled + 1, kRing);

        if (ringFilled >= 4)
        {
            double s = 0.0;
            for (int k = 0; k < 4; ++k)
                s += ring[(size_t) ((ringPos - 1 - k + kRing) % kRing)];
            const double e = s / 4.0;
            const float lk = toLufs (e);
            momentary.store (lk);

            if (lk > -70.0f)
            {
                const int bin = std::clamp ((int) std::floor ((lk + 70.0f) * 10.0f), 0, kBins - 1);
                histSum[(size_t) bin] += e;
                histCount[(size_t) bin] += 1.0;
            }
        }

        double s = 0.0;
        for (int k = 0; k < ringFilled; ++k)
            s += ring[(size_t) ((ringPos - 1 - k + kRing) % kRing)];
        shortTerm.store (toLufs (s / (double) ringFilled));

        integrated.store (computeIntegrated());
    }

    float computeIntegrated() const noexcept
    {
        double sumE = 0.0, cnt = 0.0;
        for (int b = 0; b < kBins; ++b) { sumE += histSum[(size_t) b]; cnt += histCount[(size_t) b]; }
        if (cnt <= 0.0) return -100.0f;

        const float relThresh = toLufs (sumE / cnt) - 10.0f;
        const int startBin = std::clamp ((int) std::ceil ((relThresh + 70.0f) * 10.0f), 0, kBins - 1);

        double sumE2 = 0.0, cnt2 = 0.0;
        for (int b = startBin; b < kBins; ++b) { sumE2 += histSum[(size_t) b]; cnt2 += histCount[(size_t) b]; }
        return cnt2 <= 0.0 ? -100.0f : toLufs (sumE2 / cnt2);
    }

    double fs = 44100.0;
    int subBlockLen = 4410, count = 0, ringPos = 0, ringFilled = 0;
    double acc = 0.0;
    Biquad shelf, hp;
    TruePeakDetector tpL, tpR;
    std::array<double, kRing> ring {};
    std::array<double, kBins> histSum {}, histCount {};

    std::atomic<bool>  resetRequested { false };
    std::atomic<float> momentary { -100.0f }, shortTerm { -100.0f }, integrated { -100.0f };
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };
};

} // namespace mm
