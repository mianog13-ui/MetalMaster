#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// Parameters
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout MetalMasterAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto addFloat = [&p] (const String& id, const String& name, float lo, float hi, float def,
                          const String& unit, float skew = 1.0f, float step = 0.01f)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step, skew), def,
            AudioParameterFloatAttributes().withLabel (unit)));
    };

    auto addBool = [&p] (const String& id, const String& name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    // ---- Input + low cut + tone ------------------------------------------
    addFloat ("inGain",    "Input Gain",        -12.0f,  12.0f,   0.0f, "dB", 1.0f, 0.1f);
    addFloat ("hpFreq",    "High-Pass Freq",     20.0f,  60.0f,  28.0f, "Hz", 1.0f, 0.5f);
    addFloat ("lowGain",   "Low Shelf (80 Hz)",  -6.0f,   6.0f,   0.0f, "dB", 1.0f, 0.1f);
    addFloat ("lmFreq",    "Low-Mid Freq",      150.0f, 600.0f, 300.0f, "Hz", 0.5f, 1.0f);
    addFloat ("lmGain",    "Low-Mid Gain",       -9.0f,   3.0f,   0.0f, "dB", 1.0f, 0.1f);
    addFloat ("presFreq",  "Presence Freq",    1500.0f, 6000.0f, 3000.0f, "Hz", 0.5f, 10.0f);
    addFloat ("presGain",  "Presence Gain",      -6.0f,   6.0f,   0.0f, "dB", 1.0f, 0.1f);
    addFloat ("airGain",   "Air Shelf (10 kHz)", -6.0f,   6.0f,   0.0f, "dB", 1.0f, 0.1f);

    // ---- Multiband compressor ---------------------------------------------
    addBool  ("mbOn",      "Multiband On", true);
    addFloat ("xo1",       "Crossover 1",        60.0f,  300.0f, 120.0f, "Hz", 0.5f, 1.0f);
    addFloat ("xo2",       "Crossover 2",       300.0f, 2000.0f, 900.0f, "Hz", 0.5f, 1.0f);
    addFloat ("xo3",       "Crossover 3",      2500.0f, 10000.0f, 5000.0f, "Hz", 0.5f, 10.0f);
    addFloat ("mbAtk",     "MB Attack",           1.0f,  100.0f,  20.0f, "ms", 0.5f, 0.1f);
    addFloat ("mbRel",     "MB Release",         20.0f,  500.0f, 150.0f, "ms", 0.5f, 1.0f);

    const char* bandNames[4] = { "Low", "LoMid", "HiMid", "High" };
    for (int i = 0; i < 4; ++i)
    {
        const String n (i + 1);
        addFloat ("mbThr" + n,   String (bandNames[i]) + " Threshold", -40.0f, 0.0f, 0.0f, "dB", 1.0f, 0.1f);
        addFloat ("mbRatio" + n, String (bandNames[i]) + " Ratio",       1.0f, 10.0f, 2.0f, ":1", 0.6f, 0.1f);
        addFloat ("mbGain" + n,  String (bandNames[i]) + " Gain",       -6.0f,  6.0f, 0.0f, "dB", 1.0f, 0.1f);
    }

    // ---- Stereo (mid/side) ----------------------------------------------------
    addFloat ("width",     "Width",               0.0f,  200.0f, 100.0f, "%",  1.0f, 1.0f);
    addFloat ("monoFreq",  "Mono Bass Below",    20.0f,  300.0f,  20.0f, "Hz", 0.5f, 1.0f);

    // ---- Bus compressor ---------------------------------------------------------
    addBool  ("busOn",     "Bus Comp On", true);
    addFloat ("busThresh", "Bus Threshold",     -30.0f,    0.0f,   0.0f, "dB", 1.0f, 0.1f);
    addFloat ("busRatio",  "Bus Ratio",           1.0f,   10.0f,   2.0f, ":1", 0.6f, 0.1f);
    addFloat ("busAtk",    "Bus Attack",          1.0f,  100.0f,  20.0f, "ms", 0.5f, 0.1f);
    addFloat ("busRel",    "Bus Release",        20.0f, 1000.0f, 200.0f, "ms", 0.5f, 1.0f);
    addFloat ("busMakeup", "Bus Makeup",          0.0f,   12.0f,   0.0f, "dB", 1.0f, 0.1f);
    addFloat ("busMix",    "Bus Mix",             0.0f,  100.0f, 100.0f, "%",  1.0f, 1.0f);

    // ---- Saturation / clipper -----------------------------------------------------
    addFloat ("satDrive",  "Saturation Drive",    0.0f,   18.0f,   0.0f, "dB", 1.0f, 0.1f);
    addFloat ("satMix",    "Saturation Mix",      0.0f,  100.0f,  50.0f, "%",  1.0f, 1.0f);
    addBool  ("clipOn",    "Clipper On", true);
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "clipType", 1 }, "Clipper Type",
                                                         StringArray { "Soft", "Hard" }, 0));
    addFloat ("clipThresh", "Clip Threshold",   -12.0f,    0.0f,  -2.0f, "dB", 1.0f, 0.1f);

    // ---- Limiter ----------------------------------------------------------------------
    addFloat ("limDrive",  "Limiter Drive",       0.0f,   15.0f,   0.0f, "dB", 1.0f, 0.1f);
    addFloat ("limCeil",   "Ceiling",            -6.0f,    0.0f,  -1.0f, "dB", 1.0f, 0.1f);
    addFloat ("limRel",    "Release",            10.0f,  500.0f,  80.0f, "ms", 0.4f, 1.0f);
    addBool  ("truePeak",  "True Peak", true);

    // ---- Macros (offsets layered on top of the normal knobs) -------------------
    addFloat ("macWeight", "Macro Weight",        0.0f,  100.0f,   0.0f, "%", 1.0f, 1.0f);
    addFloat ("macPunch",  "Macro Punch",         0.0f,  100.0f,   0.0f, "%", 1.0f, 1.0f);
    addFloat ("macAir",    "Macro Air",           0.0f,  100.0f,   0.0f, "%", 1.0f, 1.0f);
    addFloat ("macWidth",  "Macro Width",         0.0f,  100.0f,   0.0f, "%", 1.0f, 1.0f);
    addFloat ("macLoud",   "Macro Loudness",      0.0f,  100.0f,   0.0f, "%", 1.0f, 1.0f);

    return { p.begin(), p.end() };
}

//==============================================================================
// Presets (anything not listed is reset to its default)
//==============================================================================
namespace
{
struct PV { const char* id; float v; };
struct Preset { const char* name; std::vector<PV> values; };

const std::vector<Preset>& presetList()
{
    static const std::vector<Preset> list = {
        { "Init (Neutral)", {} },

        { "Architects - Polished & Wide", {
            { "hpFreq", 28.f }, { "lowGain", 1.0f }, { "lmFreq", 320.f }, { "lmGain", -2.0f },
            { "presFreq", 3200.f }, { "presGain", 1.5f }, { "airGain", 2.5f },
            { "xo1", 120.f }, { "xo2", 900.f }, { "xo3", 5000.f }, { "mbAtk", 20.f }, { "mbRel", 150.f },
            { "mbThr1", -22.f }, { "mbThr2", -20.f }, { "mbThr3", -20.f }, { "mbThr4", -22.f },
            { "mbRatio1", 2.5f }, { "mbRatio2", 2.0f }, { "mbRatio3", 2.0f }, { "mbRatio4", 1.8f },
            { "mbGain4", 0.5f },
            { "busThresh", -14.f }, { "busRatio", 2.0f }, { "busAtk", 25.f }, { "busRel", 180.f }, { "busMakeup", 1.0f },
            { "width", 115.f }, { "monoFreq", 110.f },
            { "satDrive", 5.f }, { "satMix", 40.f }, { "clipType", 1.f }, { "clipThresh", -4.f },
            { "limDrive", 5.f }, { "limCeil", -1.f }, { "limRel", 60.f } } },

        { "Make Them Suffer - Atmospheric", {
            { "hpFreq", 28.f }, { "lowGain", 0.5f }, { "lmFreq", 300.f }, { "lmGain", -1.5f },
            { "presFreq", 3000.f }, { "presGain", 1.0f }, { "airGain", 3.0f },
            { "mbAtk", 30.f }, { "mbRel", 200.f },
            { "mbThr1", -24.f }, { "mbThr2", -22.f }, { "mbThr3", -22.f }, { "mbThr4", -24.f },
            { "mbRatio1", 2.0f }, { "mbRatio2", 1.8f }, { "mbRatio3", 1.8f }, { "mbRatio4", 1.6f },
            { "mbGain4", 0.5f },
            { "busThresh", -16.f }, { "busRatio", 1.8f }, { "busAtk", 30.f }, { "busRel", 250.f }, { "busMakeup", 1.0f },
            { "width", 125.f }, { "monoFreq", 100.f },
            { "satDrive", 4.f }, { "satMix", 35.f }, { "clipType", 0.f }, { "clipThresh", -3.5f },
            { "limDrive", 4.f }, { "limCeil", -1.f }, { "limRel", 90.f } } },

        { "Currents - Tight & Punchy", {
            { "hpFreq", 30.f }, { "lmFreq", 300.f }, { "lmGain", -2.5f },
            { "presFreq", 3200.f }, { "presGain", 2.0f }, { "airGain", 2.0f },
            { "mbAtk", 10.f }, { "mbRel", 100.f },
            { "mbThr1", -22.f }, { "mbThr2", -20.f }, { "mbThr3", -20.f }, { "mbThr4", -22.f },
            { "mbRatio1", 3.0f }, { "mbRatio2", 2.5f }, { "mbRatio3", 2.0f }, { "mbRatio4", 2.0f },
            { "busThresh", -12.f }, { "busRatio", 2.5f }, { "busAtk", 15.f }, { "busRel", 120.f }, { "busMakeup", 1.5f },
            { "width", 105.f }, { "monoFreq", 130.f },
            { "satDrive", 7.f }, { "satMix", 45.f }, { "clipType", 1.f }, { "clipThresh", -5.f },
            { "limDrive", 6.f }, { "limCeil", -1.f }, { "limRel", 45.f } } },

        { "Loud & Aggressive", {
            { "hpFreq", 30.f }, { "lowGain", 0.5f }, { "lmFreq", 300.f }, { "lmGain", -2.5f },
            { "presFreq", 3200.f }, { "presGain", 2.0f }, { "airGain", 2.0f },
            { "mbAtk", 10.f }, { "mbRel", 100.f },
            { "mbThr1", -24.f }, { "mbThr2", -22.f }, { "mbThr3", -22.f }, { "mbThr4", -24.f },
            { "mbRatio1", 3.0f }, { "mbRatio2", 2.5f }, { "mbRatio3", 2.5f }, { "mbRatio4", 2.0f },
            { "busThresh", -10.f }, { "busRatio", 3.0f }, { "busAtk", 12.f }, { "busRel", 100.f }, { "busMakeup", 2.0f },
            { "width", 105.f }, { "monoFreq", 130.f },
            { "satDrive", 8.f }, { "satMix", 50.f }, { "clipType", 1.f }, { "clipThresh", -6.f },
            { "limDrive", 8.f }, { "limCeil", -1.f }, { "limRel", 40.f } } },
    };
    return list;
}
} // namespace

juce::StringArray MetalMasterAudioProcessor::getPresetNames()
{
    juce::StringArray names;
    for (const auto& p : presetList())
        names.add (p.name);
    return names;
}

void MetalMasterAudioProcessor::applyPreset (int index)
{
    const auto& list = presetList();
    if (index < 0 || index >= (int) list.size())
        return;

    for (auto* prm : getParameters())
        prm->setValueNotifyingHost (prm->getDefaultValue());

    for (const auto& pv : list[(size_t) index].values)
        if (auto* prm = apvts.getParameter (pv.id))
            prm->setValueNotifyingHost (prm->convertTo0to1 (pv.v));

    apvts.state.setProperty ("preset", index, nullptr);
}

int MetalMasterAudioProcessor::getPresetIndex() const
{
    return (int) apvts.state.getProperty ("preset", 0);
}

//==============================================================================
MetalMasterAudioProcessor::MetalMasterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };

    pIn = get ("inGain");       pHp = get ("hpFreq");
    pLow = get ("lowGain");     pLmFreq = get ("lmFreq");   pLmGain = get ("lmGain");
    pPresFreq = get ("presFreq"); pPresGain = get ("presGain"); pAir = get ("airGain");

    pMbOn = get ("mbOn");  pXo1 = get ("xo1");  pXo2 = get ("xo2");  pXo3 = get ("xo3");
    pMbAtk = get ("mbAtk"); pMbRel = get ("mbRel");
    for (int i = 0; i < 4; ++i)
    {
        const juce::String n (i + 1);
        pMbThr[i]   = get ("mbThr" + n);
        pMbRatio[i] = get ("mbRatio" + n);
        pMbGain[i]  = get ("mbGain" + n);
        mbGrDb[i].store (0.0f);
    }

    pBusOn = get ("busOn");  pBusThresh = get ("busThresh");  pBusRatio = get ("busRatio");
    pBusAtk = get ("busAtk"); pBusRel = get ("busRel");  pBusMakeup = get ("busMakeup");  pBusMix = get ("busMix");

    pWidth = get ("width");  pMonoFreq = get ("monoFreq");

    pSatDrive = get ("satDrive"); pSatMix = get ("satMix");
    pClipOn = get ("clipOn");  pClipType = get ("clipType");  pClipThresh = get ("clipThresh");
    pLimDrive = get ("limDrive"); pLimCeil = get ("limCeil"); pLimRel = get ("limRel"); pTruePeak = get ("truePeak");

    pMacWeight = get ("macWeight"); pMacPunch = get ("macPunch"); pMacAir = get ("macAir");
    pMacWidth = get ("macWidth");   pMacLoud = get ("macLoud");
}

bool MetalMasterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MetalMasterAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    maxBlock = std::max (1, samplesPerBlock);

    oversampler.initProcessing ((size_t) maxBlock);
    oversampler.reset();

    limiter.prepare (sr, 2.0);
    meter.prepare (sr);
    mb.prepare (sr);
    busComp.prepare (sr);

    for (auto* b : { &hp1, &hp2, &lowShelf, &lowMid, &presence, &air, &scHp, &msHp1, &msHp2 })
        b->reset();

    inGain.reset (sr, 0.03);
    limDrive.reset (sr, 0.03);
    inGain.setCurrentAndTargetValue (mm::dbToLin (pIn->load()));
    limDrive.setCurrentAndTargetValue (mm::dbToLin (pLimDrive->load()));

    setLatencySamples (limiter.getLatency() + (int) std::round (oversampler.getLatencyInSamples()));
}

//==============================================================================
void MetalMasterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    if (buffer.getNumChannels() < 2)
        return;

    juce::dsp::AudioBlock<float> block (buffer);
    const size_t total = block.getNumSamples();

    // Some hosts send blocks larger than the size announced in prepareToPlay: chunk them.
    for (size_t start = 0; start < total; start += (size_t) maxBlock)
    {
        const size_t len = std::min ((size_t) maxBlock, total - start);
        processChunk (block.getSubBlock (start, len));
    }
}

void MetalMasterAudioProcessor::processChunk (juce::dsp::AudioBlock<float> block)
{
    const int n = (int) block.getNumSamples();
    float* L = block.getChannelPointer (0);
    float* R = block.getChannelPointer (1);

    // ---- Macros: offsets layered on top of the regular knobs ---------------------
    const float mW  = pMacWeight->load() * 0.01f;
    const float mP  = pMacPunch->load()  * 0.01f;
    const float mA  = pMacAir->load()    * 0.01f;
    const float mWi = pMacWidth->load()  * 0.01f;
    const float mL  = pMacLoud->load()   * 0.01f;

    auto cl = [] (float v, float lo, float hi) { return std::clamp (v, lo, hi); };

    const float effLow      = cl (pLow->load()       + 2.5f * mW, -8.0f, 8.0f);
    const float effLowMid   = cl (pLmGain->load()    - 2.0f * mW, -12.0f, 6.0f);
    const float effPres     = cl (pPresGain->load()  + 2.0f * mP, -8.0f, 8.0f);
    const float effAir      = cl (pAir->load()       + 3.0f * mA, -8.0f, 8.0f);
    const float effSat      = cl (pSatDrive->load()  + 4.0f * mP + 2.0f * mL, 0.0f, 24.0f);
    const float effClip     = cl (pClipThresh->load() - 2.5f * mP - 3.0f * mL, -18.0f, 0.0f);
    const float effLimDrive = cl (pLimDrive->load()  + 9.0f * mL, 0.0f, 24.0f);
    const float effWidth    = cl (pWidth->load()     + 40.0f * mWi, 0.0f, 250.0f) * 0.01f;
    const float effBusAtk   = pBusAtk->load() * (1.0f + 1.5f * mP);

    // ---- 1. Input gain + low cut + EQ (base rate) ------------------------------
    hp1.makeHighPass (sampleRate, pHp->load(), 0.5412);   // 4th-order Butterworth
    hp2.makeHighPass (sampleRate, pHp->load(), 1.3066);
    lowShelf.makeLowShelf (sampleRate, 80.0, 0.7071, effLow);
    lowMid.makePeak (sampleRate, pLmFreq->load(), 1.0, effLowMid);
    presence.makePeak (sampleRate, pPresFreq->load(), 0.9, effPres);
    air.makeHighShelf (sampleRate, 10000.0, 0.7071, effAir);

    inGain.setTargetValue (mm::dbToLin (pIn->load()));

    auto eq = [this] (int ch, double x)
    {
        x = hp1.process (ch, x);
        x = hp2.process (ch, x);
        x = lowShelf.process (ch, x);
        x = lowMid.process (ch, x);
        x = presence.process (ch, x);
        return air.process (ch, x);
    };

    for (int i = 0; i < n; ++i)
    {
        const float g = inGain.getNextValue();
        L[i] = (float) eq (0, (double) L[i] * g);
        R[i] = (float) eq (1, (double) R[i] * g);
    }

    // ---- 2. Multiband compressor ---------------------------------------------------
    float mbGr[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (pMbOn->load() > 0.5f)
    {
        mb.setCrossovers (pXo1->load(), pXo2->load(), pXo3->load());

        for (int b = 0; b < 4; ++b)
        {
            float gain = pMbGain[b]->load();
            if (b == 0) gain += 1.0f * mW;   // Weight macro also lifts the low band
            if (b == 3) gain += 1.0f * mA;   // Air macro also lifts the high band
            mb.setBand (b, pMbThr[b]->load(), pMbRatio[b]->load(), gain, pMbAtk->load(), pMbRel->load());
        }
        mb.process (L, R, n, mbGr);
    }
    for (int b = 0; b < 4; ++b)
        mbGrDb[b].store (mbGr[b]);

    // ---- 3. Mid/side: mono bass + width ------------------------------------------
    const float monoF = pMonoFreq->load();
    const bool monoOn = monoF > 20.5f;
    if (monoOn)
    {
        msHp1.makeHighPass (sampleRate, monoF, mm::kButterQ);
        msHp2.makeHighPass (sampleRate, monoF, mm::kButterQ);
    }

    if (monoOn || std::abs (effWidth - 1.0f) > 1.0e-4f)
    {
        for (int i = 0; i < n; ++i)
        {
            const double m = 0.5 * ((double) L[i] + (double) R[i]);
            double s = 0.5 * ((double) L[i] - (double) R[i]);

            if (monoOn)
                s = msHp2.process (0, msHp1.process (0, s));   // removes side content below the cutoff

            s *= (double) effWidth;
            L[i] = (float) (m + s);
            R[i] = (float) (m - s);
        }
    }

    // ---- 4. Bus ("glue") compressor ----------------------------------------------
    float busGr = 0.0f;
    if (pBusOn->load() > 0.5f)
    {
        busComp.setParams (pBusThresh->load(), pBusRatio->load(), effBusAtk, pBusRel->load(), 4.0f);
        scHp.makeHighPass (sampleRate, 90.0, mm::kButterQ);   // keep bass from driving the detector

        const float makeup = mm::dbToLin (pBusMakeup->load());
        const float mix = pBusMix->load() * 0.01f;

        for (int i = 0; i < n; ++i)
        {
            const float l0 = L[i], r0 = R[i];
            const float dl = (float) scHp.process (0, (double) l0);
            const float dr = (float) scHp.process (1, (double) r0);
            const float g = busComp.getGain (std::max (std::abs (dl), std::abs (dr))) * makeup;

            L[i] = l0 + mix * (l0 * g - l0);
            R[i] = r0 + mix * (r0 * g - r0);
            busGr = std::max (busGr, busComp.getReductionDb());
        }
    }
    busGrDb.store (busGr);

    // ---- 5. Saturation + clipper (4x oversampled) -------------------------------------
    const float satK     = mm::dbToLin (effSat) - 1.0f;   // 0 at 0 dB -> identity
    const bool  satOn    = satK > 1.0e-3f && pSatMix->load() > 0.0f;
    const float satNorm  = satOn ? 1.0f / std::tanh (satK) : 1.0f;
    const float satMix   = pSatMix->load() * 0.01f;

    const bool  clipOn   = pClipOn->load() > 0.5f;
    const bool  hardClip = pClipType->load() > 0.5f;
    const float thr      = mm::dbToLin (effClip);

    if (satOn || clipOn)
    {
        auto up = oversampler.processSamplesUp (block);

        for (size_t c = 0; c < 2; ++c)
        {
            float* d = up.getChannelPointer (c);

            for (size_t i = 0; i < up.getNumSamples(); ++i)
            {
                float x = d[i];

                if (satOn)
                {
                    // Peaks at full scale are preserved, the body gets denser.
                    const float wet = std::tanh (x * satK) * satNorm;
                    x += satMix * (wet - x);
                }

                if (clipOn)
                    x = hardClip ? std::clamp (x, -thr, thr)
                                 : thr * std::tanh (x / thr);

                d[i] = x;
            }
        }

        oversampler.processSamplesDown (block);
    }
    else
    {
        // Keep latency constant whether or not the stage is active: run it anyway.
        auto up = oversampler.processSamplesUp (block);
        juce::ignoreUnused (up);
        oversampler.processSamplesDown (block);
    }

    // ---- 6. Limiter (true-peak) -------------------------------------------------------
    limDrive.setTargetValue (mm::dbToLin (effLimDrive));
    for (int i = 0; i < n; ++i)
    {
        const float g = limDrive.getNextValue();
        L[i] *= g;
        R[i] *= g;
    }

    limiter.setCeilingDb (pLimCeil->load());
    limiter.setReleaseMs (pLimRel->load());
    limiter.setTruePeak (pTruePeak->load() > 0.5f);
    grDb.store (limiter.processBlock (L, R, n));

    // ---- 7. Metering ----------------------------------------------------------------------
    meter.process (L, R, n);
}

//==============================================================================
juce::AudioProcessorEditor* MetalMasterAudioProcessor::createEditor()
{
    return new MetalMasterAudioProcessorEditor (*this);
}

void MetalMasterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MetalMasterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MetalMasterAudioProcessor();
}
