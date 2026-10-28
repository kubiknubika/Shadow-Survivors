#pragma once
// Recorded voice phrases for in-game messages.
//
// The game does not speak through a system synthesizer. Every phrase is a real
// audio file stored in audio/voice/ next to the .exe, named after the message key
// from src/messages.h. Both MP3 (decoded with the bundled minimp3) and WAV
// (16-bit PCM, mono or stereo, any rate) are accepted:
//
//     audio/voice/boss_phase.mp3      <- "Новая фаза боя."
//     audio/voice/portal_found.wav    <- "Найден эхо-портал."
//
// Files are decoded once on first use and kept in memory, then mixed into the
// effects bus, so the volume slider, the [M] toggle and the music ducking all
// apply to the announcer exactly like any other one-shot.
//
// Missing files are not an error: the game plays what exists and the F3 overlay
// lists how many registry messages still have no clip, so nothing is forgotten
// silently. This header must be included after external_music.h (it brings the
// MP3 decoder implementation).

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

struct VoiceClip {
    std::vector<int16_t> samples; // mono, resampled to the output rate
};

struct VoiceBank {
    int outputRate = 44100;
    bool enabled = true;
    float gain = 0.92f;

    std::vector<MusicPath> dirs;
    bool scanned = false;

    // Path cache keeps the "does this message have a clip" check off the disk.
    std::map<std::string, MusicPath> paths;
    std::map<std::string, std::unique_ptr<VoiceClip>> clips;

    // Audio-thread state.
    std::unique_ptr<VoiceClip> current;
    size_t pos = 0;
    bool active = false;

    // Game-thread -> audio-thread hand-off.
    std::atomic<const VoiceClip*> pending{nullptr};

    // Stats for F3.
    int spokenCount = 0;
    int playedCount = 0;
    std::string lastKey;
    std::string lastMissing;

    void setOutputRate(int rate) { outputRate = std::max(8000, rate); }

    static MusicPath voiceFolder(const MusicPath& base) {
#ifdef _WIN32
        return base + L"audio\\voice\\";
#else
        return base + "audio/voice/";
#endif
    }

    void scan() {
        if (scanned) return;
        scanned = true;
        dirs.clear();
#ifdef _WIN32
        dirs.push_back(voiceFolder(ExternalMusic::exeFolder()));
        wchar_t cwd[MAX_PATH * 2] = {};
        if (GetCurrentDirectoryW((DWORD)(sizeof(cwd) / sizeof(cwd[0])), cwd) > 0)
            dirs.push_back(voiceFolder(MusicPath(cwd) + L"\\"));
#else
        dirs.push_back(voiceFolder(MusicPath()));
#endif
    }

    static void appendAscii(MusicPath& path, const std::string& text) {
#ifdef _WIN32
        for (size_t i = 0; i < text.size(); ++i) path.push_back((wchar_t)(unsigned char)text[i]);
#else
        path += text;
#endif
    }

    // Resolves <key>.mp3 inside audio/voice/. Returns an empty path when absent.
    const MusicPath* find(const std::string& key) {
        std::map<std::string, MusicPath>::const_iterator cached = paths.find(key);
        if (cached != paths.end()) return cached->second.empty() ? nullptr : &cached->second;
        scan();
        static const char* const extensions[2] = {".wav", ".mp3"};
        MusicPath found;
        for (size_t d = 0; d < dirs.size() && found.empty(); ++d) {
            for (int e = 0; e < 2 && found.empty(); ++e) {
                MusicPath candidate = dirs[d];
                appendAscii(candidate, key);
                appendAscii(candidate, extensions[e]);
                if (ExternalMusic::fileExists(candidate)) found = candidate;
            }
        }
        paths[key] = found;
        return found.empty() ? nullptr : &paths[key];
    }

    bool hasClip(const std::string& key) { return find(key) != nullptr; }

    // Length of a recorded phrase in seconds (0 when it is missing). The game uses it to
    // avoid stacking a second line on top of a long narration.
    float clipSeconds(const std::string& key) {
        const VoiceClip* clip = load(key);
        if (!clip || outputRate <= 0) return 0.0f;
        return (float)clip->samples.size() / (float)outputRate;
    }

    // Minimal RIFF/WAVE reader: 16-bit PCM, mono or stereo. Returns mono samples and
    // the source sample rate. Anything else (compressed WAV, 8/24-bit) is rejected
    // and the MP3 path is tried instead.
    static bool loadWav(const MusicPath& path, std::vector<int16_t>& mono, int& rate) {
        FILE* f = NULL;
#ifdef _WIN32
        f = _wfopen(path.c_str(), L"rb");
#else
        f = fopen(path.c_str(), "rb");
#endif
        if (!f) return false;
        char riff[12] = {};
        if (fread(riff, 1, 12, f) != 12 || memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
            fclose(f);
            return false;
        }
        int channels = 0, bits = 0;
        rate = 0;
        bool haveFormat = false;
        for (;;) {
            char header[8] = {};
            if (fread(header, 1, 8, f) != 8) break;
            const uint32_t size = (uint32_t)(unsigned char)header[4] | ((uint32_t)(unsigned char)header[5] << 8) |
                                  ((uint32_t)(unsigned char)header[6] << 16) | ((uint32_t)(unsigned char)header[7] << 24);
            if (memcmp(header, "fmt ", 4) == 0 && size >= 16) {
                unsigned char fmt[16];
                if (fread(fmt, 1, 16, f) != 16) break;
                const int format = fmt[0] | (fmt[1] << 8);
                channels = fmt[2] | (fmt[3] << 8);
                rate = (int)((uint32_t)fmt[4] | ((uint32_t)fmt[5] << 8) | ((uint32_t)fmt[6] << 16) | ((uint32_t)fmt[7] << 24));
                bits = fmt[14] | (fmt[15] << 8);
                haveFormat = (format == 1 && bits == 16 && channels >= 1 && channels <= 2 && rate >= 4000);
                if (size > 16) fseek(f, (long)(size - 16), SEEK_CUR);
            } else if (memcmp(header, "data", 4) == 0) {
                if (!haveFormat) break;
                std::vector<unsigned char> raw(size);
                const size_t got = fread(raw.data(), 1, size, f);
                fclose(f);
                const size_t frames = got / (size_t)(channels * 2);
                mono.reserve(frames);
                for (size_t i = 0; i < frames; ++i) {
                    int sum = 0;
                    for (int c = 0; c < channels; ++c) {
                        const size_t at = (i * (size_t)channels + (size_t)c) * 2;
                        sum += (int16_t)((uint16_t)(raw[at] | ((unsigned char)raw[at + 1] << 8)));
                    }
                    mono.push_back((int16_t)(sum / channels));
                }
                return !mono.empty();
            } else {
                fseek(f, (long)size + (long)(size & 1), SEEK_CUR);
            }
        }
        fclose(f);
        return false;
    }

    // Decoded once, kept forever: phrases are 1-3 s of mono audio.
    const VoiceClip* load(const std::string& key) {
        std::map<std::string, std::unique_ptr<VoiceClip>>::const_iterator cached = clips.find(key);
        if (cached != clips.end()) return cached->second.get();
        const MusicPath* file = find(key);
        if (!file) return nullptr;
        std::vector<int16_t> mono;
        int rate = outputRate;
        if (loadWav(*file, mono, rate)) {
            // already decoded
        } else {
        mp3dec_ex_t dec = {};
#ifdef _WIN32
        if (mp3dec_ex_open_w(&dec, file->c_str(), MP3D_DO_NOT_SCAN) != 0) return nullptr;
#else
        if (mp3dec_ex_open(&dec, file->c_str(), MP3D_DO_NOT_SCAN) != 0) return nullptr;
#endif
        const int channels = std::max(1, std::min(2, dec.info.channels));
        rate = dec.info.hz > 0 ? dec.info.hz : outputRate;
        int16_t buf[MINIMP3_MAX_SAMPLES_PER_FRAME];
        int got;
        while ((got = (int)mp3dec_ex_read(&dec, buf, MINIMP3_MAX_SAMPLES_PER_FRAME)) > 0) {
            const int frames = got / channels;
            for (int i = 0; i < frames; ++i) {
                int sum = 0;
                for (int c = 0; c < channels; ++c) sum += buf[i * channels + c];
                mono.push_back((int16_t)(sum / channels));
            }
            if (mono.size() > (size_t)outputRate * 30) break; // never hold more than 30 s
        }
        mp3dec_ex_close(&dec);
        }
        if (mono.size() < 2) return nullptr;
        std::unique_ptr<VoiceClip> clip(new VoiceClip());
        const double ratio = (double)rate / (double)std::max(1, outputRate);
        const size_t outCount = (size_t)((double)mono.size() / ratio);
        clip->samples.resize(outCount);
        for (size_t i = 0; i < outCount; ++i) {
            const double src = (double)i * ratio;
            const size_t i0 = std::min((size_t)src, mono.size() - 1);
            const size_t i1 = std::min(i0 + 1, mono.size() - 1);
            const float f = (float)(src - (double)i0);
            clip->samples[i] = (int16_t)((float)mono[i0] + ((float)mono[i1] - (float)mono[i0]) * f);
        }
        const VoiceClip* raw = clip.get();
        clips[key] = std::move(clip);
        return raw;
    }

    // Game thread. A new phrase always replaces the previous one, so a burst of
    // combat banners can never queue up into a backlog of stale lines.
    bool speak(const std::string& key) {
        if (!enabled) return false;
        lastKey = key;
        const VoiceClip* clip = load(key);
        if (!clip || clip->samples.size() < 2) {
            lastMissing = key;
            return false;
        }
        pending.store(clip, std::memory_order_release);
        ++spokenCount;
        return true;
    }

    void setEnabled(bool on) {
        enabled = on;
        if (!on) pending.store(nullptr, std::memory_order_release);
    }

    // Audio thread. Returns the next sample of the current phrase, or 0.
    float next() {
        const VoiceClip* incoming = pending.exchange(nullptr, std::memory_order_acquire);
        if (incoming) {
            current.reset();
            // Copy-on-take keeps the cache immutable while the audio thread reads.
            current.reset(new VoiceClip(*incoming));
            pos = 0;
            active = true;
            ++playedCount;
        }
        if (!active || !current) return 0.0f;
        const std::vector<int16_t>& data = current->samples;
        if (pos + 1 >= data.size()) {
            active = false;
            current.reset();
            return 0.0f;
        }
        const size_t i = pos;
        const float frac = (float)(pos - (double)i);
        const float sample = ((float)data[i] + ((float)data[i + 1] - (float)data[i]) * frac) / 32768.0f;
        // Short edges only; the clip itself is already trimmed.
        const float attack = std::min(1.0f, (float)i / 96.0f);
        const float tail = std::min(1.0f, (float)(data.size() - 1 - i) / 220.0f);
        pos += 1.0;
        return sample * attack * tail * gain;
    }

    bool speaking() const { return active; }

    void stop() {
        active = false;
        pos = 0;
        current.reset();
        pending.store(nullptr, std::memory_order_release);
    }
};
