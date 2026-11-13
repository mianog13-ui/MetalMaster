#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"

class MetalMasterAudioProcessor : public juce::AudioProcessor
{
public:
    MetalMasterAudioProcessor();
    ~MetalMasterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MetalMaster"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Presets are handled by the plugin's own menu (not host programs), so loading a
    // saved project never gets overridden by a preset.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Presets
    static juce::StringArray getPresetNames();
    void applyPreset (int index);
    int getPresetIndex() const;

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor
    mm::LoudnessMeter meter;
    std::atomic<float> grDb { 0.0f };      // limiter gain reduction
    std::atomic<float> busGrDb { 0.0f };   // bus compressor gain reduction
    std::atomic<float> mbGrDb[4];          // multiband gain reduction per band

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void processChunk (juce::dsp::AudioBlock<float> block);

    double sampleRate = 44100.0;
    int maxBlock = 512;

    // Parameter pointers (raw atomics from the APVTS)
    std::atomic<float> *pIn, *pHp, *pLow, *pLmFreq, *pLmGain, *pPresFreq, *pPresGain, *pAir,
                       *pMbOn, *pXo1, *pXo2, *pXo3, *pMbAtk, *pMbRel,
                       *pBusOn, *pBusThresh, *pBusRatio, *pBusAtk, *pBusRel, *pBusMakeup, *pBusMix,
                       *pWidth, *pMonoFreq,
                       *pSatDrive, *pSatMix, *pClipOn, *pClipType, *pClipThresh,
                       *pLimDrive, *pLimCeil, *pLimRel, *pTruePeak,
                       *pMacWeight, *pMacPunch, *pMacAir, *pMacWidth, *pMacLoud;
    std::atomic<float> *pMbThr[4], *pMbRatio[4], *pMbGain[4];

    // EQ
    mm::Biquad hp1, hp2, lowShelf, lowMid, presence, air;

    // Multiband compressor
    mm::MultibandCompressor mb;

    // Bus compressor (with side-chain high-pass)
    mm::Compressor busComp;
    mm::Biquad scHp;

    // Mono-bass high-pass on the side channel (4th order)
    mm::Biquad msHp1, msHp2;

    juce::SmoothedValue<float> inGain, limDrive;

    // 4x oversampling for the saturation + clipper (linear-phase FIR, integer latency)
    juce::dsp::Oversampling<float> oversampler { 2, 2,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };

    mm::LookaheadLimiter limiter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetalMasterAudioProcessor)
};
