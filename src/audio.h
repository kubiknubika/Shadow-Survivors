#pragma once
#include <stdint.h>
#include <math.h>
#include <vector>
#include <string.h>
#include <algorithm>
#include <atomic>
#include "music_composer.h"
#include "boss_score.h"
#include "external_music.h"
#include "voice.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

enum SfxId {
    SFX_CLICK = 0,
    SFX_BLADE,
    SFX_FIREBALL,
    SFX_LIGHTNING,
    SFX_GEM,
    SFX_LEVELUP,
    SFX_CHEST,
    SFX_HIT,
    SFX_BOSS,
    SFX_DASH,
    SFX_ULTIMATE,
    SFX_EVOLVE,
    SFX_BOSS_DEATH,
    SFX_PLAYER_DEATH,
    SFX_MAGNET,
    SFX_REVIVE,
    SFX_FREEZE,
    SFX_TIER_UP,
    SFX_BOSS_TELEGRAPH,
    SFX_BOSS_IMPACT,
    SFX_BOSS_PHASE,
    SFX_PROJECTILE_FLIGHT,
    SFX_PROJECTILE_IMPACT,
    SFX_COUNT
};

struct ActiveVoice {
    bool active = false;
    const int16_t* data = nullptr;
    int length = 0;
    float pos = 0.0f;
    float speed = 1.0f;
    float volume = 0.5f;
    SfxId id = SFX_CLICK;
    uint8_t priority = 0;
};

struct SoundEngine {
    std::atomic<bool> enabled{true};       // SFX toggle [M]
    std::atomic<bool> musicEnabled{true};  // Music toggle [N]
    // Modes 0–4 select exploration, horde, boss, recovery, and victory; 5 is menu, 6 upgrade selection, 7 game over. Variant selects one of ten battle scores.
    std::atomic<int> musicMode{0};
    std::atomic<int> musicVariant{0};
    std::atomic<int> musicBossType{0};
    // Test-arena switches may request a near-immediate mode/track change; normal gameplay remains bar-quantized.
    std::atomic<int> immediateMusicRequest{-1};
    std::atomic<float> sfxVolume{0.65f};
    std::atomic<float> musicVolume{0.42f};
    // 0 = normal scene mapping, 1 = Soul Forge shop (it reuses the menu mode otherwise).
    std::atomic<int> musicSceneHint{0};
    // Выбор файла внутри сцены: номер боя для кампании, случайный шаффл для эндлеса.
    std::atomic<int> musicVariantSeed{-1};
    std::atomic<bool> musicShuffle{false};
    // Scene-level trim for the music bus: 0.20 while a level-up card is open (80 % quieter).
    std::atomic<float> musicSceneGain{1.0f};

    // Player-supplied tracks from audio/. When a scene has a file it plays instead of
    // the synthesised score below; otherwise the built-in score keeps the scene alive.
    ExternalMusic externalMusic;
    ExternalMusicCue externalCues[2]; // 0 = level-up sting, 1 = upgrade-picked sting
    VoiceBank voice;                  // recorded announcer phrases from audio/voice/
    float proceduralMusicGain = 0.0f;

    std::vector<int16_t> sfxSamples[SFX_COUNT];
    float cooldowns[SFX_COUNT] = {0};
    int gemComboStep = 0;
    float gemComboResetTimer = 0.0f;

    static constexpr int MAX_VOICES = 16;
    ActiveVoice voices[MAX_VOICES];

#ifdef _WIN32
    HWAVEOUT hWaveOut = NULL;
    static constexpr int NUM_BUF = 4;
    static constexpr int BUF_SAMPLES = 1470; // ~33ms at 44100Hz
    WAVEHDR headers[NUM_BUF];
    int16_t buffers[NUM_BUF][BUF_SAMPLES];
    HANDLE hThread = NULL;
    std::atomic<bool> running{false};
    CRITICAL_SECTION cs;
#endif

    double musicTime = 0.0;
    float musicNoiseLow = 0.0f;
    float musicMuteGain = 1.0f;
    float sfxMuteGain = 1.0f;
    float musicDuckGain = 1.0f;
    float musicVolumeSmooth = 0.42f;
    float musicSceneGainSmooth = 1.0f;
    float sfxVolumeSmooth = 0.65f;
    float bossMusicBlend = 0.0f; // retained for compatibility with the procedural score state
    uint32_t noiseRng = 987654321u;

    float nextNoise() {
        noiseRng = noiseRng * 1664525u + 1013904223u;
        return ((int)(noiseRng >> 16) - 32768) / 32768.0f;
    }

    static uint8_t sfxPriority(SfxId id) {
        switch (id) {
            case SFX_GEM: return 0;
            case SFX_BLADE: case SFX_FIREBALL: case SFX_LIGHTNING: case SFX_HIT: return 1;
            case SFX_CLICK: case SFX_MAGNET: return 2;
            case SFX_DASH: case SFX_FREEZE: return 3;
            case SFX_BOSS: return 4;
            case SFX_LEVELUP: case SFX_CHEST: case SFX_ULTIMATE: case SFX_EVOLVE:
            case SFX_BOSS_DEATH: case SFX_PLAYER_DEATH: case SFX_REVIVE:
            case SFX_TIER_UP: case SFX_BOSS_PHASE: return 5;
            case SFX_BOSS_TELEGRAPH: case SFX_BOSS_IMPACT: return 4;
            case SFX_PROJECTILE_FLIGHT: return 2;
            case SFX_PROJECTILE_IMPACT: return 3;
            default: return 1;
        }
    }

    static int sfxPolyphony(SfxId id) {
        switch (id) {
            case SFX_CLICK: return 3;
            case SFX_BLADE: case SFX_HIT: return 2;
            case SFX_FIREBALL: case SFX_LIGHTNING: case SFX_BOSS_DEATH: return 2;
            case SFX_TIER_UP: case SFX_BOSS_TELEGRAPH: case SFX_BOSS_IMPACT: case SFX_BOSS_PHASE: return 1;
            case SFX_PROJECTILE_FLIGHT: return 2;
            case SFX_PROJECTILE_IMPACT: return 3;
            default: return 1;
        }
    }

    int activeVoiceCount(SfxId id) const {
        int count = 0;
        for (const ActiveVoice& voice : voices) if (voice.active && voice.id == id) ++count;
        return count;
    }

    void generateSfxBank() {
        const int sr = 44100;

        // 0: SFX_CLICK (soft warm UI tick)
        {
            int n = sr * 35 / 1000;
            sfxSamples[SFX_CLICK].resize(n);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr;
                float env = powf(1.0f - (float)i / n, 2.2f);
                sfxSamples[SFX_CLICK][i] = (int16_t)(sinf(2.0f * 3.14159f * 520.0f * t) * 4500.0f * env);
            }
        }
        // 1: SFX_BLADE (soft airy whoosh)
        {
            int n = sr * 70 / 1000;
            sfxSamples[SFX_BLADE].resize(n);
            float lp = 0.0f;
            for (int i = 0; i < n; ++i) {
                float u = (float)i / n;
                float env = sinf(u * 3.14159f) * (1.0f - u * 0.5f);
                float freq = 480.0f - 290.0f * u;
                lp = lp * 0.82f + nextNoise() * 0.18f;
                float tone = sinf(2.0f * 3.14159f * freq * ((float)i / sr));
                sfxSamples[SFX_BLADE][i] = (int16_t)((tone * 0.45f + lp * 0.55f) * 3800.0f * env);
            }
        }
        // 2: SFX_FIREBALL (deep warm bass explosion)
        {
            int n = sr * 180 / 1000;
            sfxSamples[SFX_FIREBALL].resize(n);
            float lp = 0.0f;
            for (int i = 0; i < n; ++i) {
                float u = (float)i / n;
                float env = powf(1.0f - u, 1.5f);
                lp = lp * 0.90f + nextNoise() * 0.10f;
                float tone = sinf(2.0f * 3.14159f * (135.0f - 85.0f * u) * ((float)i / sr));
                sfxSamples[SFX_FIREBALL][i] = (int16_t)((tone * 0.65f + lp * 0.35f) * 6800.0f * env);
            }
        }
        // 3: SFX_LIGHTNING (crisp thunder snap)
        {
            int n = sr * 140 / 1000;
            sfxSamples[SFX_LIGHTNING].resize(n);
            float lp = 0.0f;
            for (int i = 0; i < n; ++i) {
                float u = (float)i / n;
                float env = powf(1.0f - u, 1.6f);
                lp = lp * 0.70f + nextNoise() * 0.30f;
                float tone = sinf(2.0f * 3.14159f * (620.0f - 380.0f * u) * ((float)i / sr));
                sfxSamples[SFX_LIGHTNING][i] = (int16_t)((tone * 0.5f + lp * 0.5f) * 5200.0f * env);
            }
        }
        // 4: SFX_GEM (Gentle, warm, low-volume marimba/harp pluck — NEVER harsh!)
        {
            int n = sr * 85 / 1000;
            sfxSamples[SFX_GEM].resize(n);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr;
                float u = (float)i / n;
                // Smooth attack & warm exponential decay
                float attack = std::min(1.0f, (float)i / (sr * 0.004f));
                float decay = powf(1.0f - u, 2.6f);
                // Warm fundamental at 293.66 Hz (D4) + soft 2nd harmonic
                float w = sinf(2.0f * 3.14159f * 293.66f * t) * 0.80f
                        + sinf(2.0f * 3.14159f * 587.33f * t) * 0.20f * (1.0f - u);
                sfxSamples[SFX_GEM][i] = (int16_t)(w * 2600.0f * attack * decay);
            }
        }
        // 5: SFX_LEVELUP (Warm gothic chord arpeggio)
        {
            int n = sr * 420 / 1000;
            sfxSamples[SFX_LEVELUP].resize(n);
            float notes[4] = {293.66f, 349.23f, 440.0f, 587.33f}; // Dm chord
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr;
                int step = std::min(3, (i * 4) / n);
                float env = powf(1.0f - (float)i / n, 0.8f);
                float w = sinf(2.0f * 3.14159f * notes[step] * t) * 0.7f
                        + sinf(2.0f * 3.14159f * notes[step] * 2.0f * t) * 0.3f;
                sfxSamples[SFX_LEVELUP][i] = (int16_t)(w * 6200.0f * env);
            }
        }
        // 6: SFX_CHEST (Rich treasure fanfare)
        {
            int n = sr * 520 / 1000;
            sfxSamples[SFX_CHEST].resize(n);
            float notes[5] = {293.66f, 369.99f, 440.0f, 587.33f, 739.99f};
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr;
                int step = std::min(4, (i * 5) / n);
                float env = powf(1.0f - (float)i / n, 0.7f);
                sfxSamples[SFX_CHEST][i] = (int16_t)(sinf(2.0f * 3.14159f * notes[step] * t) * 6800.0f * env);
            }
        }
        // 7: SFX_HIT (Deep thud)
        {
            int n = sr * 100 / 1000;
            sfxSamples[SFX_HIT].resize(n);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr;
                float u = (float)i / n;
                float env = powf(1.0f - u, 1.4f);
                sfxSamples[SFX_HIT][i] = (int16_t)(sinf(2.0f * 3.14159f * (125.0f - 65.0f * u) * t) * 7500.0f * env);
            }
        }
        // 8: SFX_BOSS (Deep ominous warhorn)
        {
            int n = sr * 580 / 1000;
            sfxSamples[SFX_BOSS].resize(n);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr;
                float u = (float)i / n;
                float env = sinf(u * 3.14159f);
                float w = sinf(2.0f * 3.14159f * 82.41f * t) * 0.6f
                        + sinf(2.0f * 3.14159f * 123.47f * t) * 0.4f;
                sfxSamples[SFX_BOSS][i] = (int16_t)(w * 7800.0f * env);
            }
        }
        // 9: SFX_DASH (Kinetic wind-burst)
        {
            int n = sr * 110 / 1000;
            sfxSamples[SFX_DASH].resize(n);
            float lp = 0.0f;
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr;
                float u = (float)i / n;
                float env = sinf(u * 3.14159f);
                lp = lp * 0.85f + nextNoise() * 0.15f;
                float sweep = sinf(2.0f * 3.14159f * (220.0f + 260.0f * u) * t);
                sfxSamples[SFX_DASH][i] = (int16_t)((sweep * 0.55f + lp * 0.45f) * 5800.0f * env);
            }
        }
        // 10: SFX_ULTIMATE (Epic screen-shaking power surge)
        {
            int n = sr * 650 / 1000;
            sfxSamples[SFX_ULTIMATE].resize(n);
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr;
                float u = (float)i / n;
                float env = sinf(u * 3.14159f);
                float w = sinf(2.0f * 3.14159f * (110.0f + 220.0f * u) * t) * 0.5f
                        + sinf(2.0f * 3.14159f * (220.0f + 440.0f * u) * t) * 0.5f;
                sfxSamples[SFX_ULTIMATE][i] = (int16_t)(w * 8000.0f * env);
            }
        }
        // 11: SFX_EVOLVE (Symbiosis evolution fanfare)
        {
            int n = sr * 700 / 1000;
            sfxSamples[SFX_EVOLVE].resize(n);
            float notes[6] = {293.66f, 349.23f, 440.0f, 587.33f, 698.46f, 880.0f};
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr;
                int step = std::min(5, (i * 6) / n);
                float env = powf(1.0f - (float)i / n, 0.6f);
                float w = sinf(2.0f * 3.14159f * notes[step] * t) * 0.7f
                        + sinf(2.0f * 3.14159f * notes[step] * 1.5f * t) * 0.3f;
                sfxSamples[SFX_EVOLVE][i] = (int16_t)(w * 7600.0f * env);
            }
        }
        // 10: SFX_BOSS_DEATH — low impact, brass fall, and shimmering tail.
        {
            int n = sr * 1100 / 1000;
            sfxSamples[SFX_BOSS_DEATH].resize(n);
            float lp = 0.0f;
            for (int i = 0; i < n; ++i) {
                float t = (float)i / sr, u = (float)i / n;
                float env = powf(1.0f - u, 1.15f);
                lp = lp * 0.94f + nextNoise() * 0.06f;
                float bass = sinf(2.0f * 3.14159f * (72.0f - 34.0f * u) * t);
                float brass = sinf(2.0f * 3.14159f * (146.83f - 55.0f * u) * t);
                float shimmer = sinf(2.0f * 3.14159f * 587.33f * t) * (1.0f - u) * 0.16f;
                sfxSamples[SFX_BOSS_DEATH][i] = (int16_t)((bass * 0.52f + brass * 0.25f + lp * 0.18f + shimmer) * 9400.0f * env);
            }
        }
        // 11: SFX_PLAYER_DEATH — restrained descending minor chord.
        {
            int n = sr * 1350 / 1000;
            sfxSamples[SFX_PLAYER_DEATH].resize(n);
            const float notes[3] = {293.66f, 277.18f, 220.0f};
            for (int i = 0; i < n; ++i) {
                float u = (float)i / n, t = (float)i / sr;
                float env = powf(1.0f - u, 1.25f);
                int step = std::min(2, (i * 3) / n);
                float tone = sinf(2.0f * 3.14159f * notes[step] * t) * 0.72f
                           + sinf(2.0f * 3.14159f * notes[step] * 0.5f * t) * 0.28f;
                sfxSamples[SFX_PLAYER_DEATH][i] = (int16_t)(tone * 7200.0f * env);
            }
        }
        // SFX_MAGNET: four gentle ascending "woo" pulses, with a light gliding top.
        {
            int n = sr * 760 / 1000;
            sfxSamples[SFX_MAGNET].resize(n);
            const float notes[4] = {330.0f, 392.0f, 494.0f, 587.0f};
            for (int i = 0; i < n; ++i) {
                float u = (float)i / n;
                int step = std::min(3, (i * 4) / n);
                float local = u * 4.0f - step;
                float t = (float)i / sr;
                float freq = notes[step] * (1.0f + 0.08f * local);
                float env = powf(std::max(0.0f, sinf(local * 3.14159f)), 0.8f) * (0.82f + 0.18f * u);
                float tone = sinf(2.0f * 3.14159f * freq * t);
                float overtone = sinf(2.0f * 3.14159f * freq * 2.01f * t) * 0.16f;
                sfxSamples[SFX_MAGNET][i] = (int16_t)((tone * 0.72f + overtone) * 5200.0f * env);
            }
        }
        // Phoenix revival: breathy, filtered ash-noise, a warm low impact, and a rising bell-like chord.
        {
            int n = sr * 1850 / 1000;
            sfxSamples[SFX_REVIVE].resize(n);
            float lowNoise = 0.0f;
            static const float notes[4] = {196.0f, 293.66f, 392.0f, 587.33f};
            for (int i = 0; i < n; ++i) {
                const float u = (float)i / n;
                const float t = (float)i / sr;
                const float rawNoise = nextNoise();
                lowNoise += (rawNoise - lowNoise) * 0.055f;
                const int step = std::min(3, (int)(u * 4.0f));
                const float bellEnv = powf(std::max(0.0f, 1.0f - u * 0.86f), 1.8f);
                const float bell = sinf(6.2831853f * notes[step] * t)
                    + 0.22f * sinf(6.2831853f * notes[step] * 2.0f * t + 0.3f);
                const float rising = sinf(6.2831853f * (112.0f + 90.0f * u) * t);
                const float impact = expf(-u * 42.0f) * (rawNoise * 0.7f + rising * 0.3f);
                const float ash = lowNoise * (0.55f - 0.30f * u) + (rawNoise - lowNoise) * 0.09f;
                const float sample = bell * bellEnv * 0.30f + rising * (1.0f - u) * 0.16f + ash + impact * 0.75f;
                sfxSamples[SFX_REVIVE][i] = (int16_t)(sample * 8600.0f);
            }
        }
        // Frost Nova: a soft descending air-sweep with a glassy rising shimmer and filtered frost noise.
        {
            const int n = sr * 620 / 1000;
            sfxSamples[SFX_FREEZE].resize(n);
            float lowNoise = 0.0f;
            double sweepPhase = 0.0;
            for (int i = 0; i < n; ++i) {
                const float u = (float)i / (float)n;
                const float t = (float)i / (float)sr;
                const float rawNoise = nextNoise();
                lowNoise += (rawNoise - lowNoise) * 0.065f;
                sweepPhase += (245.0 - 86.0 * u) / (double)sr;
                const float sweep = sinf(6.2831853f * (float)sweepPhase);
                const float shimmer = sinf(6.2831853f * (610.0f + 510.0f * u) * t + 0.35f);
                const float shimmer2 = sinf(6.2831853f * (923.0f + 280.0f * u) * t + 1.1f);
                const float attack = std::min(1.0f, u / 0.07f);
                const float tail = powf(std::max(0.0f, 1.0f - u), 1.25f);
                const float air = (rawNoise - lowNoise) * expf(-u * 9.0f);
                const float crystal = shimmer * 0.20f + shimmer2 * 0.085f;
                const float sample = attack * tail * (sweep * 0.38f + crystal + lowNoise * 0.12f + air * 0.12f);
                sfxSamples[SFX_FREEZE][i] = (int16_t)(sample * 12500.0f);
            }
        }
        // Tier-up: low anvil impact under a bright seven-note ascending forge fanfare.
        {
            const int n = sr * 1450 / 1000;
            sfxSamples[SFX_TIER_UP].resize(n);
            static const float notes[7] = {196.0f, 293.66f, 392.0f, 523.25f, 659.25f, 783.99f, 1046.5f};
            float lowNoise = 0.0f;
            for (int i = 0; i < n; ++i) {
                const float u = (float)i / n, t = (float)i / sr;
                const int step = std::min(6, (int)(u * 7.0f));
                const float local = u * 7.0f - step;
                const float raw = nextNoise(); lowNoise += (raw - lowNoise) * 0.045f;
                const float env = powf(std::max(0.0f, 1.0f - u * 0.72f), 0.8f);
                const float bell = sinf(6.2831853f * notes[step] * t) + 0.22f * sinf(6.2831853f * notes[step] * 2.01f * t);
                const float strike = expf(-local * 24.0f) * (raw * 0.55f + sinf(6.2831853f * (74.0f - 24.0f * u) * t) * 0.45f);
                const float sample = bell * env * (0.46f + 0.16f * local) + strike * 0.38f + lowNoise * (1.0f - u) * 0.08f;
                sfxSamples[SFX_TIER_UP][i] = (int16_t)(sample * 11500.0f);
            }
        }
        // Boss telegraph: rising sub rumble and breath-noise crest before a signature attack.
        {
            const int n = sr * 820 / 1000;
            sfxSamples[SFX_BOSS_TELEGRAPH].resize(n);
            float lowNoise = 0.0f; double phase = 0.0;
            for (int i = 0; i < n; ++i) {
                const float u = (float)i / n, t = (float)i / sr, raw = nextNoise();
                lowNoise += (raw - lowNoise) * 0.04f;
                phase += (48.0 + 42.0 * u) / sr;
                const float build = powf(u, 1.55f), pulse = 0.64f + 0.36f * sinf(6.2831853f * (3.0f + 4.0f * u) * t);
                const float snap = expf(-((u - 0.91f) * (u - 0.91f)) * 520.0f) * (raw * 0.48f + sinf(6.2831853f * 155.0f * t) * 0.52f);
                const float rumble = sinf(6.2831853f * (float)phase) * 0.58f + lowNoise * 0.22f + (raw - lowNoise) * 0.15f;
                sfxSamples[SFX_BOSS_TELEGRAPH][i] = (int16_t)((rumble * build * pulse + snap * 0.55f) * 11800.0f);
            }
        }
        // Boss impact: short, heavy sub thump with a noisy crack and a descending metal tail.
        {
            const int n = sr * 460 / 1000;
            sfxSamples[SFX_BOSS_IMPACT].resize(n);
            float lowNoise = 0.0f; double phase = 0.0;
            for (int i = 0; i < n; ++i) {
                const float u = (float)i / n, t = (float)i / sr, raw = nextNoise();
                lowNoise += (raw - lowNoise) * 0.10f;
                phase += (116.0 - 72.0 * u) / sr;
                const float sub = sinf(6.2831853f * (float)phase) * powf(1.0f - u, 1.8f);
                const float crack = (raw - lowNoise) * expf(-u * 20.0f);
                const float metal = sinf(6.2831853f * (265.0f - 150.0f * u) * t) * powf(1.0f - u, 2.2f);
                const float thump = expf(-u * 58.0f) * sinf(6.2831853f * 54.0f * t);
                sfxSamples[SFX_BOSS_IMPACT][i] = (int16_t)((sub * 0.54f + crack * 0.34f + metal * 0.18f + thump * 0.72f) * 14500.0f);
            }
        }
        // Boss phase: a 2.3-second ritual transition — heavy gong, rising three-step minor harmony, breath and final metallic flare.
        {
            const int n = sr * 2300 / 1000;
            sfxSamples[SFX_BOSS_PHASE].resize(n);
            static const float chords[3][3] = {
                {73.42f, 110.00f, 146.83f}, {87.31f, 130.81f, 174.61f}, {98.00f, 146.83f, 196.00f}
            };
            float lowNoise = 0.0f; double subPhase = 0.0, gongPhase = 0.0;
            for (int i = 0; i < n; ++i) {
                const float u = (float)i / n, t = (float)i / sr, raw = nextNoise();
                lowNoise += (raw - lowNoise) * 0.028f;
                const int step = std::min(2, (int)(u * 3.0f));
                const float local = u * 3.0f - step;
                subPhase += (52.0 + 24.0 * u) / sr;
                gongPhase += (62.0 - 18.0 * u) / sr;
                const float strike = sinf(6.2831853f * (float)gongPhase) * expf(-u * 7.0f);
                const float sub = sinf(6.2831853f * (float)subPhase) * expf(-u * 1.4f);
                const float ringIn = std::min(1.0f, local / 0.18f);
                const float ringOut = std::min(1.0f, (1.0f - local) / 0.20f);
                const float ring = ringIn * ringOut;
                float chord = 0.0f;
                for (int v = 0; v < 3; ++v) {
                    const float f = chords[step][v];
                    chord += sinf(6.2831853f * f * t + v * 0.27f) * (v == 0 ? 0.42f : 0.27f);
                    chord += sinf(6.2831853f * f * 2.01f * t) * (v == 2 ? 0.10f : 0.045f);
                }
                const float breath = (raw - lowNoise) * (0.12f + 0.42f * u);
                const float finalFlare = expf(-((u - 0.91f) * (u - 0.91f)) * 180.0f) *
                    (sinf(6.2831853f * 523.25f * t) * 0.25f + (raw - lowNoise) * 0.32f);
                const float sample = strike * 0.47f + sub * 0.25f + chord * ring * (0.32f + 0.42f * u) + breath + finalFlare;
                sfxSamples[SFX_BOSS_PHASE][i] = (int16_t)(sample * 11200.0f);
            }
        }
        // Projectile flight: a short airy whistle with a curved pitch glide and a thin turbulent tail.
        {
            const int n = sr * 235 / 1000;
            sfxSamples[SFX_PROJECTILE_FLIGHT].resize(n);
            float lowNoise = 0.0f; double phase = 0.0;
            for (int i = 0; i < n; ++i) {
                const float u = (float)i / n, t = (float)i / sr, raw = nextNoise();
                lowNoise += (raw - lowNoise) * 0.08f;
                phase += (980.0 - 520.0 * u) / sr;
                const float attack = std::min(1.0f, u / 0.025f);
                const float env = attack * powf(std::max(0.0f, 1.0f - u), 1.5f);
                const float whistle = sinf(6.2831853f * (float)phase) * 0.48f;
                const float air = (raw - lowNoise) * (0.38f + 0.12f * sinf(6.2831853f * 6.0f * u));
                const float body = sinf(6.2831853f * (320.0f - 115.0f * u) * t) * 0.14f;
                sfxSamples[SFX_PROJECTILE_FLIGHT][i] = (int16_t)((whistle + air + body) * env * 10500.0f);
            }
        }
        // Projectile impact: a crisp crack over a compact low thud and a tiny glass/metal tick.
        {
            const int n = sr * 175 / 1000;
            sfxSamples[SFX_PROJECTILE_IMPACT].resize(n);
            float lowNoise = 0.0f; double subPhase = 0.0;
            for (int i = 0; i < n; ++i) {
                const float u = (float)i / n, t = (float)i / sr, raw = nextNoise();
                lowNoise += (raw - lowNoise) * 0.13f;
                subPhase += (128.0 - 64.0 * u) / sr;
                const float crack = (raw - lowNoise) * expf(-u * 33.0f);
                const float thud = sinf(6.2831853f * (float)subPhase) * expf(-u * 13.0f);
                const float tick = sinf(6.2831853f * (1120.0f - 420.0f * u) * t) * expf(-u * 25.0f);
                const float sample = crack * 0.48f + thud * 0.38f + tick * 0.24f;
                sfxSamples[SFX_PROJECTILE_IMPACT][i] = (int16_t)(sample * 14400.0f);
            }
        }
    }

    // Which file (if any) covers the current music mode. Modes come from game.h:
    // 0 exploration, 1 horde, 2 boss, 3 recovery, 4 victory, 5 menus/shop, 6 level-up, 7 game over.
    static int externalSceneForMode(int mode, int sceneHint) {
        if (mode == 5) return sceneHint == 1 ? MUSIC_SCENE_FORGE : MUSIC_SCENE_MENU;
        if (mode == 2) return MUSIC_SCENE_BOSS;
        if (mode == 4) return MUSIC_SCENE_VICTORY;
        if (mode == 7) return MUSIC_SCENE_GAMEOVER;
        return MUSIC_SCENE_GAMEPLAY; // run, horde, recovery and the level-up overlay keep one bed
    }
    bool externalMusicActive() const { return externalMusic.available(); }

    // --- announcer: recorded phrases from audio/voice/ (see voice.h) ---
    bool speakMessage(const std::string& key) { return voice.speak(key); }
    bool hasVoiceClip(const std::string& key) { return voice.hasClip(key); }
    int voiceSpokenCount() const { return voice.spokenCount; }
    bool voiceEnabled() const { return voice.enabled; }
    void setVoiceEnabled(bool on) { voice.setEnabled(on); }
    bool voiceSpeaking() const { return voice.speaking(); }
    int externalMusicTrackCount() const { return externalMusic.trackCount(); }
    // Short read-out of which scenes actually have a file, for the settings screen.
    std::string externalMusicSummary() const {
        std::string out;
        for (int s = 0; s < MUSIC_SCENE_COUNT; ++s) {
            if (externalMusic.files[s].empty()) continue;
            if (!out.empty()) out += " • ";
            out += MUSIC_SCENE_TITLES[s];
        }
        return out;
    }
    void playLevelUpCue() { externalCues[0].trigger(); }
    void playUpgradeCue() { externalCues[1].trigger(); }

    // Ten fully arranged score identities: their tempo, instrument families, grooves and dynamics differ.
    static constexpr double MUSIC_BPM = 96.0; // base tempo of the Ash theme
    static constexpr float MUSIC_BUS_GAIN = 0.80f;
    static constexpr float SFX_BUS_GAIN = 1.75f;
    static double musicVariantBpm(int variant) { return MusicComposer::tempo(variant); }
    static double musicVariantBarBeats(int variant) { return MusicComposer::beatsPerBar(variant); }
    static int nextMusicTempoTier(int currentTier,int liveMonsters) {
        int tier=std::max(0,std::min(3,currentTier));
        liveMonsters=std::max(0,liveMonsters);
        if(tier<1&&liveMonsters>=40) tier=1;
        if(tier>0&&liveMonsters<30) tier=0;
        if(tier<2&&liveMonsters>=90) tier=2;
        if(tier>1&&liveMonsters<75) tier=1;
        if(tier<3&&liveMonsters>=160) tier=3;
        if(tier>2&&liveMonsters<140) tier=2;
        return tier;
    }
    static double musicModeBpm(int mode, int variant, int tempoTier=1) {
        if(mode==5) return 68.0;
        if(mode==6) return MusicComposer::UPGRADE_BPM;
        if(mode==7) return MusicComposer::GAMEOVER_BPM;
        static constexpr double factors[4]={0.92,1.0,1.28,1.55};
        const int tier=std::max(0,std::min(3,tempoTier));
        return std::max(60.0,std::min(180.0,MusicComposer::tempo(variant)*factors[tier]));
    }
    static double musicModeBarBeats(int mode, int variant) { return mode == 5 ? 3.0 : (mode == 6 ? MusicComposer::UPGRADE_BAR_BEATS : (mode == 7 ? MusicComposer::GAMEOVER_BAR_BEATS : musicVariantBarBeats(variant))); }
    int renderedMusicMode = 0;
    int pendingMusicMode = -1;
    int renderedMusicVariant = 0;
    int pendingMusicVariant = -1;
    int previousMusicVariant = 0;
    int renderedMusicBossType = 0;
    int previousMusicBossType = 0;
    double renderedMusicStartTime = 0.0;
    double previousMusicStartTime = 0.0;
    int lastMusicBar = -1;
    float musicModeBlend = 1.0f;
    bool musicSequentialHandoff = false; // the level-up cue hands off without two themes layered together
    int previousMusicMode = 0;
    std::atomic<int> musicTempoTier{0}; // 0 slow, 1 medium, 2 fast, 3 very fast; set from live crowd size
    int renderedMusicTempoTier = 0;
    int previousMusicTempoTier = 0;

    static const char* musicVariantName(int variant) {
        static const char* names[10] = {
            "ПЕПЕЛ • СТРУНЫ", "РАССВЕТ • АРФА", "СЕВЕР • ФЛЕЙТА", "КОЛОКОЛ • ОРГАН", "КАМЕРНАЯ • ПИЦЦИКАТО",
            "ПРАХ • ХОР", "РУНЫ • ЭЛЕКТРО", "ХРУСТАЛЬ • ЦЕЛЕСТА", "ГЕРОЙ • БРАСС", "СТРАННИК • ФЛЕЙТА"
        };
        return names[std::max(0, std::min(9, variant))];
    }
    static const char* musicVariantDescription(int variant) {
        static const char* text[10] = {
            "96 BPM • 4/4 • СТРУНЫ И ЩИПКОВЫЕ АККОРДЫ • МРАК",
            "108 BPM • 4/4 • АРФА И СТРУНЫ • СВЕТЛО",
            "76 BPM • 4/4 • ФЛЕЙТА И СТРУННЫЙ БАС • ХОЛОД",
            "84 BPM • 3/4 • ОРГАН И КОЛОКОЛ • ГОТИКА",
            "120 BPM • 4/4 • ПИЦЦИКАТО И КЛАВЕСИН • МОТОРИКА",
            "72 BPM • 4/4 • ХОР И ПРОЦЕССИОННЫЙ БАРАБАН",
            "128 BPM • 4/4 • СИНТ-БАС, БИТ И СИНКОПЫ",
            "88 BPM • 3/4 • ЦЕЛЕСТА • ХРУПКИЙ ВАЛЬС",
            "116 BPM • 4/4 • БРАСС И МАРШЕВЫЙ СНЭР",
            "90 BPM • 6/8 • ФЛЕЙТА, ЩИПКОВЫЕ И РУЧНОЙ БАРАБАН"
        };
        return text[std::max(0, std::min(9, variant))];
    }
    static float smooth01(float x) {
        x = std::max(0.0f, std::min(1.0f, x));
        return x * x * (3.0f - 2.0f * x);
    }
    static float mixTransitionScores(float oldScore, float newScore, float blend, bool sequential) {
        blend = std::max(0.0f, std::min(1.0f, blend));
        if (!sequential) return oldScore * (1.0f - blend) + newScore * blend;
        if (blend < 0.5f) return oldScore * (1.0f - smooth01(blend * 2.0f));
        return newScore * smooth01((blend - 0.5f) * 2.0f);
    }
    float renderMusicArrangement(double t, int mode, int variant, int tempoTier, float noiseHigh, float noiseLow, int bossType = 0) {
        if (mode == 2) {
            static constexpr double tempoFactors[4] = {0.92, 1.0, 1.28, 1.55};
            const int tier = std::max(0, std::min(3, tempoTier));
            const double scoreTime = t * tempoFactors[tier];
            const double phraseLength = BossScore::durationSeconds(BossScore::themeFor(bossType));
            const double loopedTime = scoreTime < 0.0 ? scoreTime : fmod(scoreTime, phraseLength);
            return BossScore::render(bossType, loopedTime, (uint32_t)(std::max(0.0, scoreTime) * 44100.0));
        }
        if (mode == 5) return MusicComposer::renderMenu(t, noiseHigh, noiseLow);
        if (mode == 6) return MusicComposer::renderUpgrade(t, noiseHigh, noiseLow);
        if (mode == 7) return MusicComposer::renderGameOver(t, noiseHigh, noiseLow);
        return MusicComposer::render(variant, t, mode, noiseHigh, noiseLow, musicModeBpm(mode,variant,tempoTier));
    }

    void requestImmediateMusicChange(int mode,int variant) {
        mode=std::max(0,std::min(7,mode));
        variant=std::max(0,std::min(9,variant));
        immediateMusicRequest.store(mode*10+variant,std::memory_order_relaxed);
    }

    float sampleMusic(double t) {
        const bool musicOn = musicEnabled.load(std::memory_order_relaxed);
        const float muteTarget = musicOn ? 1.0f : 0.0f;
        musicMuteGain += (muteTarget - musicMuteGain) * 0.00055f;
        const float volumeTarget = musicVolume.load(std::memory_order_relaxed);
        musicVolumeSmooth += (volumeTarget - musicVolumeSmooth) * 0.00045f;
        // ~120 ms to settle, so opening or closing a level-up card ducks smoothly.
        const float sceneGainTarget = std::max(0.0f, std::min(1.0f, musicSceneGain.load(std::memory_order_relaxed)));
        musicSceneGainSmooth += (sceneGainTarget - musicSceneGainSmooth) * (1.0f / (44100.0f * 0.12f));
        if (!musicOn && musicMuteGain < 0.001f) return 0.0f;

        // Player tracks in audio/ own the bus whenever the current scene has one.
        // The built-in score is only rendered when no file covers the scene.
        const int sceneMode = std::max(0, std::min(7, musicMode.load(std::memory_order_relaxed)));
        externalMusic.variantSeed = musicVariantSeed.load(std::memory_order_relaxed);
        externalMusic.shuffle = musicShuffle.load(std::memory_order_relaxed);
        externalMusic.requestScene(externalSceneForMode(sceneMode, musicSceneHint.load(std::memory_order_relaxed)));
        float music = externalMusic.render();
        const bool trackOwnsBus = externalMusic.ownsMusicSlot();
        const float proceduralTarget = trackOwnsBus ? 0.0f : 1.0f;
        proceduralMusicGain += (proceduralTarget - proceduralMusicGain) * (1.0f / (44100.0f * 0.25f));
        if (!trackOwnsBus) {
            if (proceduralMusicGain > 0.001f) music = renderProceduralScore(t) * proceduralMusicGain;
        }
        return music * musicVolumeSmooth * musicMuteGain * musicSceneGainSmooth;
    }

    // The synthesised score (music_composer.h / boss_score.h) used when no file exists.
    float renderProceduralScore(double t) {
        const int targetMode = std::max(0, std::min(7, musicMode.load(std::memory_order_relaxed)));
        const int targetVariant = std::max(0, std::min(9, musicVariant.load(std::memory_order_relaxed)));
        // Consume any explicit test-arena request; normal gameplay transitions use the same
        // immediate path below. No transition waits for a beat, barline, or note release.
        const int immediateRequest = immediateMusicRequest.exchange(-1,std::memory_order_relaxed);
        (void)immediateRequest;
        const int targetTempoTier=std::max(0,std::min(3,musicTempoTier.load(std::memory_order_relaxed)));
        const int targetBossType=std::max(0,std::min(3,musicBossType.load(std::memory_order_relaxed)));
        const int wantedVariant=targetMode>=5?renderedMusicVariant:targetVariant;
        const int wantedTier=targetMode>=5?renderedMusicTempoTier:targetTempoTier;
        const int wantedBossType=targetMode==2?targetBossType:renderedMusicBossType;
        const bool sceneChanged=targetMode!=renderedMusicMode;
        const bool variantChanged=targetMode<5 && wantedVariant!=renderedMusicVariant;
        const bool speedChanged=targetMode<5 && wantedTier!=renderedMusicTempoTier;
        const bool bossChanged=targetMode==2 && wantedBossType!=renderedMusicBossType;
        if(sceneChanged||variantChanged||speedChanged||bossChanged){
            const int fromMode = renderedMusicMode;
            previousMusicMode=renderedMusicMode;
            previousMusicVariant=renderedMusicVariant;
            previousMusicTempoTier=renderedMusicTempoTier;
            previousMusicBossType=renderedMusicBossType;
            previousMusicStartTime=renderedMusicStartTime;
            renderedMusicMode=targetMode;
            renderedMusicVariant=wantedVariant;
            renderedMusicTempoTier=wantedTier;
            renderedMusicBossType=wantedBossType;
            renderedMusicStartTime=t;
            musicSequentialHandoff = (fromMode == 6 || targetMode == 6);
            lastMusicBar=0;
            pendingMusicMode=pendingMusicVariant=-1;
            musicModeBlend=0.0f;
        }
        // 100 ms crossfade only softens the edge; the new scene starts on this sample.
        musicModeBlend = std::min(1.0f, musicModeBlend + 1.0f / (44100.0f * 0.10f));
        const float blend = smooth01(musicModeBlend);
        const float rawNoise = nextNoise();
        musicNoiseLow += (rawNoise - musicNoiseLow) * 0.028f;
        const float noiseHigh = rawNoise - musicNoiseLow;
        const double oldTime = t - previousMusicStartTime;
        const double newTime = t - renderedMusicStartTime;
        const float oldScore = renderMusicArrangement(oldTime, previousMusicMode, previousMusicVariant, previousMusicTempoTier, noiseHigh, musicNoiseLow, previousMusicBossType);
        const float newScore = renderMusicArrangement(newTime, renderedMusicMode, renderedMusicVariant, renderedMusicTempoTier, noiseHigh, musicNoiseLow, renderedMusicBossType);
        return mixTransitionScores(oldScore, newScore, blend, musicSequentialHandoff);
    }

    void mixBuffer(int16_t* outBuf, int numSamples) {
#ifdef _WIN32
        EnterCriticalSection(&cs);
#endif
        const double invSr = 1.0 / 44100.0;
        const bool sfxOn = enabled.load(std::memory_order_relaxed);
        const float hordeSfxGain = musicMode.load(std::memory_order_relaxed) == 1 ? 0.82f : 1.0f;
        bool importantVoice = false;
        int liveVoices = 0;
        for (int v = 0; v < MAX_VOICES; ++v) {
            if (!voices[v].active) continue;
            ++liveVoices;
            if (sfxOn && voices[v].priority >= 4) importantVoice = true;
        }
        if (!sfxOn) { externalCues[0].stop(); externalCues[1].stop(); voice.stop(); }
        const bool announcing = sfxOn && voice.speaking();
        const float duckTarget = (importantVoice || announcing) ? 0.60f : (liveVoices > 0 ? 0.84f : 1.0f);
        const float voiceHeadroom = liveVoices > 10 ? 0.78f : (liveVoices > 6 ? 0.88f : 1.0f);
        for (int i = 0; i < numSamples; ++i) {
            const float sfxTarget = sfxOn ? 1.0f : 0.0f;
            sfxMuteGain += (sfxTarget - sfxMuteGain) * 0.00065f;
            sfxVolumeSmooth += (sfxVolume.load(std::memory_order_relaxed) - sfxVolumeSmooth) * 0.00045f;
            const float duckRate = duckTarget < musicDuckGain ? 0.00050f : 0.00010f;
            musicDuckGain += (duckTarget - musicDuckGain) * duckRate;
            // Keep music as the bed, not the foreground: raise one-shot effects and let active voices duck the score gently.
            float mix = sampleMusic(musicTime) * musicDuckGain * MUSIC_BUS_GAIN;
            musicTime += invSr;
            // Level-up / upgrade stings use the SFX bus, so [M] and the effects slider apply.
            float cueSample = 0.0f;
            for (int c = 0; c < 2; ++c) cueSample += externalCues[c].next();
            if (cueSample != 0.0f) mix += cueSample * sfxVolumeSmooth * sfxMuteGain * 1.15f;
            // The announcer is a one-shot on the same bus: honours [M] and the effects slider.
            const float phrase = voice.next();
            if (phrase != 0.0f) mix += phrase * sfxVolumeSmooth * sfxMuteGain * 1.20f;

            for (int v = 0; v < MAX_VOICES; ++v) {
                ActiveVoice& vc = voices[v];
                if (!vc.active) continue;
                if (!sfxOn && sfxMuteGain < 0.001f) { vc.active = false; continue; }
                int idx = (int)vc.pos;
                if (!vc.data || idx >= vc.length - 1) {
                    vc.active = false;
                    continue;
                }
                float frac = vc.pos - idx;
                float sample = (vc.data[idx] + (vc.data[idx + 1] - vc.data[idx]) * frac) / 32768.0f;
                // A tiny edge envelope prevents clicks even when a high-priority cue steals a voice.
                const float attack = std::min(1.0f, vc.pos / 132.0f); // 3 ms at 44.1 kHz
                const float tail = std::min(1.0f, (vc.length - 1.0f - vc.pos) / 220.0f); // 5 ms release
                const float voiceBusGain = (vc.id == SFX_GEM || vc.id == SFX_MAGNET) ? 1.10f : SFX_BUS_GAIN;
                mix += sample * attack * tail * vc.volume * sfxVolumeSmooth * sfxMuteGain * hordeSfxGain * voiceHeadroom * voiceBusGain;
                vc.pos += vc.speed;
            }

            // Smooth soft-knee limiter: stacked combat sounds compress gently instead of hard-clipping.
            if (!isfinite(mix)) mix = 0.0f;
            float magnitude = fabsf(mix);
            if (magnitude > 0.62f) {
                float limited = 0.62f + 0.30f * (1.0f - expf(-(magnitude - 0.62f) / 0.30f));
                mix = copysignf(std::min(limited, 0.92f), mix);
            }
            outBuf[i] = (int16_t)(mix * 30000.0f);
        }
#ifdef _WIN32
        LeaveCriticalSection(&cs);
#endif
    }

#ifdef _WIN32
    static DWORD WINAPI audioThreadProc(LPVOID param) {
        SoundEngine* self = (SoundEngine*)param;
        while (self->running) {
            bool anyFilled = false;
            for (int b = 0; b < NUM_BUF; ++b) {
                if (self->headers[b].dwFlags & WHDR_DONE) {
                    self->mixBuffer(self->buffers[b], BUF_SAMPLES);
                    waveOutWrite(self->hWaveOut, &self->headers[b], sizeof(WAVEHDR));
                    anyFilled = true;
                }
            }
            if (!anyFilled) Sleep(5);
        }
        return 0;
    }
#endif

    void init() {
        externalMusic.setOutputRate(44100);
        externalMusic.scan();
        voice.setOutputRate(44100);
        voice.scan();
        loadExternalCues();
        (void)MusicComposer::allTracks(); // prewarm the immutable score table before the audio callback
        generateSfxBank();
#ifdef _WIN32
        InitializeCriticalSection(&cs);
        WAVEFORMATEX wfx = {};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 1;
        wfx.nSamplesPerSec = 44100;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = 2;
        wfx.nAvgBytesPerSec = 44100 * 2;

        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
            running = true;
            for (int b = 0; b < NUM_BUF; ++b) {
                memset(&headers[b], 0, sizeof(WAVEHDR));
                headers[b].lpData = (LPSTR)buffers[b];
                headers[b].dwBufferLength = BUF_SAMPLES * sizeof(int16_t);
                mixBuffer(buffers[b], BUF_SAMPLES);
                waveOutPrepareHeader(hWaveOut, &headers[b], sizeof(WAVEHDR));
                waveOutWrite(hWaveOut, &headers[b], sizeof(WAVEHDR));
            }
            hThread = CreateThread(NULL, 0, audioThreadProc, this, 0, NULL);
        }
#endif
    }

    void loadExternalCues() {
        // The two UI stings live in the same folder as the tracks.
        static const char* const cueKeys[2] = {"ss_levelup_ding", "ss_upgrade_pick"};
        for (int i = 0; i < 2; ++i) {
            std::vector<MusicPath> found;
            externalMusic.findFiles(cueKeys[i], found);
            if (!found.empty()) externalCues[i].load(found[0], 44100);
        }
    }

    void shutdown() {
        voice.stop();
        externalMusic.shutdown();
#ifdef _WIN32
        if (running.load(std::memory_order_relaxed)) {
            running.store(false, std::memory_order_relaxed);
            if (hThread) {
                WaitForSingleObject(hThread, 500);
                CloseHandle(hThread);
                hThread = NULL;
            }
            if (hWaveOut) {
                waveOutReset(hWaveOut);
                for (int b = 0; b < NUM_BUF; ++b) {
                    waveOutUnprepareHeader(hWaveOut, &headers[b], sizeof(WAVEHDR));
                }
                waveOutClose(hWaveOut);
                hWaveOut = NULL;
            }
            DeleteCriticalSection(&cs);
        }
#endif
    }

    void update(float dt) {
        for (int i = 0; i < SFX_COUNT; ++i) {
            if (cooldowns[i] > 0.0f) cooldowns[i] -= dt;
        }
        if (gemComboResetTimer > 0.0f) {
            gemComboResetTimer -= dt;
            if (gemComboResetTimer <= 0.0f) gemComboStep = 0;
        }
    }

    void play(SfxId id, float minInterval = 0.09f) {
        if (!enabled.load(std::memory_order_relaxed) || id < 0 || id >= SFX_COUNT) return;
        // Throttle gem pickups strongly (220ms min interval) so mass pickups sound gentle & musical!
        if (id == SFX_GEM) minInterval = std::max(minInterval, 0.22f);
        if (id == SFX_MAGNET) minInterval = std::max(minInterval, 0.75f);
        if (cooldowns[id] > 0.0f) return;
        cooldowns[id] = minInterval;

        float pitch = 1.0f;
        static const float baseVolumes[SFX_COUNT] = {
            0.24f, 0.25f, 0.28f, 0.23f, 0.15f, 0.42f, 0.38f, 0.18f,
            0.40f, 0.25f, 0.38f, 0.40f, 0.42f, 0.32f, 0.18f, 0.42f, 0.48f,
            0.48f, 0.54f, 0.66f, 0.56f, 0.38f, 0.46f
        };
        float vol = baseVolumes[id];
        if (id == SFX_GEM) {
            // Soft pentatonic chord notes: 1.0, 1.12, 1.26, 1.33, 1.50
            static const float pentatonic[5] = {1.0f, 1.122f, 1.260f, 1.335f, 1.498f};
            pitch = pentatonic[gemComboStep % 5];
            gemComboStep = (gemComboStep + 1) % 5;
            gemComboResetTimer = 0.8f;
            vol = 0.12f; // Gentle volume for pickups; player-controlled SFX level remains available.
        } else if (id == SFX_BLADE) {
            pitch = 0.92f + (rand() % 16) * 0.01f;
        }
        // Avoid identical, machine-like repeats: vary replay rate and gain per event.
        // Collection and magnet cues keep their calm character; combat/UI cues get a small natural spread.
        const bool deterministicProjectileCue = id == SFX_PROJECTILE_FLIGHT || id == SFX_PROJECTILE_IMPACT;
        if (id != SFX_GEM && id != SFX_MAGNET && !deterministicProjectileCue) {
            const float spread = id == SFX_CLICK ? 0.035f : ((id == SFX_REVIVE || id == SFX_FREEZE) ? 0.045f : 0.095f);
            const float pitchJitter = (rand() % 10001) / 10000.0f;
            const float gainJitter = (rand() % 10001) / 10000.0f;
            pitch *= (1.0f - spread) + 2.0f * spread * pitchJitter;
            vol *= 0.90f + 0.20f * gainJitter;
        }

#ifdef _WIN32
        EnterCriticalSection(&cs);
#endif
        if (sfxSamples[id].empty()) {
#ifdef _WIN32
            LeaveCriticalSection(&cs);
#endif
            return;
        }
        const uint8_t priority = sfxPriority(id);
        int sameIdCount = 0;
        int slot = -1;
        for (int i = 0; i < MAX_VOICES; ++i) {
            if (!voices[i].active) { if (slot < 0) slot = i; }
            else if (voices[i].id == id) ++sameIdCount;
        }
        const int polyphony = sfxPolyphony(id);
        if (sameIdCount >= polyphony) {
            // Replace only the most advanced copy of this sound; do not create a noisy stack.
            int sameSlot = -1; float progress = -1.0f;
            for (int i = 0; i < MAX_VOICES; ++i) {
                if (voices[i].active && voices[i].id == id && voices[i].pos / std::max(1, voices[i].length) > progress) {
                    progress = voices[i].pos / (float)std::max(1, voices[i].length); sameSlot = i;
                }
            }
            slot = sameSlot;
        }
        if (slot < 0) {
            // Preserve important cues: only steal a voice with no higher priority than the new event.
            uint8_t lowestPriority = 255; float mostComplete = -1.0f;
            for (int i = 0; i < MAX_VOICES; ++i) {
                if (voices[i].priority > priority) continue;
                const float progress = voices[i].pos / (float)std::max(1, voices[i].length);
                if (voices[i].priority < lowestPriority ||
                    (voices[i].priority == lowestPriority && progress > mostComplete)) {
                    lowestPriority = voices[i].priority; mostComplete = progress; slot = i;
                }
            }
            if (slot < 0) {
#ifdef _WIN32
                LeaveCriticalSection(&cs);
#endif
                return;
            }
        }
        voices[slot].active = true;
        voices[slot].data = sfxSamples[id].data();
        voices[slot].length = (int)sfxSamples[id].size();
        voices[slot].pos = 0.0f;
        voices[slot].speed = pitch;
        voices[slot].volume = vol;
        voices[slot].id = id;
        voices[slot].priority = priority;
#ifdef _WIN32
        LeaveCriticalSection(&cs);
#endif
    }
};
