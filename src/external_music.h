#pragma once
// Player-supplied music.
//
// Owns everything related to the MP3 tracks the player puts into audio/ next to
// the .exe (see AUDIO_NAMING.md). The tracks are streamed through a bundled
// public-domain MP3 decoder (src/third_party/minimp3*, CC0) and mixed into the
// game's own output, so the existing volume sliders, the [N] music toggle and
// the ducking on loud cues keep working.
//
// Files win over the built-in score in music_composer.h. A scene without a file
// (or a build with an empty audio/ folder) simply keeps the synthesised score,
// so the game never ends up silent or broken.
//
// This header is single-translation-unit by design, exactly like the generated
// atlases: it carries the decoder implementation.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#define MINIMP3_IMPLEMENTATION
#include "third_party/minimp3_ex.h"

enum MusicScene {
    MUSIC_SCENE_MENU = 0,   // main menu, hero/mission select, help, hall, settings
    MUSIC_SCENE_GAMEPLAY,   // run, horde, recovery, level-up overlay
    MUSIC_SCENE_BOSS,
    MUSIC_SCENE_VICTORY,
    MUSIC_SCENE_GAMEOVER,
    MUSIC_SCENE_FORGE,      // Soul Forge shop
    MUSIC_SCENE_COUNT
};

static const char* const MUSIC_SCENE_KEYS[MUSIC_SCENE_COUNT] = {
    "ss_menu_main", "ss_gameplay_run", "ss_boss_theme", "ss_victory_boss", "ss_gameover_death", "ss_forge_shop"
};
static const char* const MUSIC_SCENE_TITLES[MUSIC_SCENE_COUNT] = {
    "МЕНЮ", "БОЙ", "БОСС", "ПОБЕДА", "СМЕРТЬ", "КУЗНИЦА"
};

#ifdef _WIN32
typedef std::wstring MusicPath;
#else
typedef std::string MusicPath;
#endif

// One streamed track. Only the audio thread touches the decoder.
struct ExternalMusicTrack {
    mp3dec_ex_t dec = {};
    bool open = false;
    MusicPath path;

    static constexpr int RING = 8192;         // mono frames, ~170 ms at 48 kHz
    int16_t ring[RING] = {0};
    int rd = 0;
    int avail = 0;
    int channels = 2;
    int rate = 48000;
    int outRate = 44100;
    double step = 1.0;                        // input frames per output frame
    double frac = 0.0;
    int16_t frameBuf[MINIMP3_MAX_SAMPLES_PER_FRAME] = {0};

    bool openFile(const MusicPath& file, int outputRate) {
        closeFile();
#ifdef _WIN32
        if (mp3dec_ex_open_w(&dec, file.c_str(), MP3D_DO_NOT_SCAN) != 0) return false;
#else
        if (mp3dec_ex_open(&dec, file.c_str(), MP3D_DO_NOT_SCAN) != 0) return false;
#endif
        open = true;
        path = file;
        channels = std::max(1, std::min(2, dec.info.channels));
        rate = dec.info.hz > 0 ? dec.info.hz : outputRate;
        outRate = outputRate;
        step = (double)rate / (double)std::max(1, outputRate);
        rd = 0; avail = 0; frac = 0.0;
        return true;
    }

    void closeFile() {
        if (open) {
            mp3dec_ex_close(&dec);
            open = false;
        }
        avail = 0; rd = 0; frac = 0.0;
    }

    // Decodes one MP3 block into the ring; loops the file at the end.
    bool decodeBlock() {
        if (!open) return false;
        int got = (int)mp3dec_ex_read(&dec, frameBuf, MINIMP3_MAX_SAMPLES_PER_FRAME);
        if (got <= 0) {
            // End of file: start the same track over instead of falling silent.
            const MusicPath file = path;
            const int reopenRate = outRate;
            closeFile();
            if (!openFile(file, reopenRate)) return false;
            got = (int)mp3dec_ex_read(&dec, frameBuf, MINIMP3_MAX_SAMPLES_PER_FRAME);
            if (got <= 0) return false;
        }
        const int frames = got / channels;
        for (int i = 0; i < frames && avail < RING; ++i) {
            int sum = 0;
            for (int c = 0; c < channels; ++c) sum += frameBuf[i * channels + c];
            ring[(rd + avail) % RING] = (int16_t)(sum / channels);
            ++avail;
        }
        return true;
    }

    // Next mono sample at the output rate, linearly resampled from the file rate.
    float nextSample() {
        if (!open) return 0.0f;
        if (avail < 2) {
            int guard = 0;
            while (avail < 2 && guard++ < 4) if (!decodeBlock()) break;
            if (avail < 2) return 0.0f;
        }
        const float a = (float)ring[rd];
        const float b = (float)ring[(rd + 1) % RING];
        const float sample = a + (b - a) * (float)frac;
        frac += step;
        while (frac >= 1.0) {
            frac -= 1.0;
            rd = (rd + 1) % RING;
            --avail;
        }
        return sample / 32768.0f;
    }
};

// Short UI sting decoded once at start-up (level-up / upgrade-picked).
struct ExternalMusicCue {
    std::vector<int16_t> samples;  // mono, already at the output rate
    size_t pos = 0;
    bool active = false;
    float gain = 0.5f;

    bool load(const MusicPath& file, int outputRate) {
        samples.clear();
        mp3dec_ex_t dec = {};
#ifdef _WIN32
        if (mp3dec_ex_open_w(&dec, file.c_str(), MP3D_DO_NOT_SCAN) != 0) return false;
#else
        if (mp3dec_ex_open(&dec, file.c_str(), MP3D_DO_NOT_SCAN) != 0) return false;
#endif
        const int channels = std::max(1, std::min(2, dec.info.channels));
        const int rate = dec.info.hz > 0 ? dec.info.hz : outputRate;
        std::vector<int16_t> mono;
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
        if (mono.size() < 2) return false;
        const double ratio = (double)rate / (double)std::max(1, outputRate);
        const size_t outCount = (size_t)((double)mono.size() / ratio);
        samples.resize(outCount);
        for (size_t i = 0; i < outCount; ++i) {
            const double src = (double)i * ratio;
            const size_t i0 = std::min((size_t)src, mono.size() - 1);
            const size_t i1 = std::min(i0 + 1, mono.size() - 1);
            const float f = (float)(src - (double)i0);
            samples[i] = (int16_t)((float)mono[i0] + ((float)mono[i1] - (float)mono[i0]) * f);
        }
        pos = 0;
        active = false;
        return !samples.empty();
    }

    void trigger() {
        if (samples.size() > 1) { pos = 0; active = true; }
    }

    void stop() { active = false; pos = 0; }

    float next() {
        if (!active) return 0.0f;
        if (pos + 1 >= samples.size()) { stop(); return 0.0f; }
        const float sample = (float)samples[pos] / 32768.0f;
        ++pos;
        return sample * gain;
    }
};

struct ExternalMusic {
    std::vector<MusicPath> files[MUSIC_SCENE_COUNT];
    std::vector<MusicPath> searchDirs;
    bool scanned = false;
    int outputRate = 44100;

    ExternalMusicTrack track;
    int requestedScene = -1;
    int currentScene = -1;
    // 0 = steady, 1 = fading the old bed out, 2 = fading the new one in.
    int phase = 0;
    float env = 0.0f;
    int lastVariant = -1;
    uint32_t rngState = 0x2545F491u;

    uint32_t random() {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return rngState;
    }

    void setOutputRate(int rate) { outputRate = std::max(8000, rate); }

    bool hasTrack(int scene) const {
        return scene >= 0 && scene < MUSIC_SCENE_COUNT && !files[scene].empty();
    }
    int trackCount() const {
        int n = 0;
        for (int s = 0; s < MUSIC_SCENE_COUNT; ++s) if (!files[s].empty()) ++n;
        return n;
    }
    bool available() const { return trackCount() > 0; }
    // True while an external track owns the music bus (the built-in score stays silent).
    bool ownsMusicSlot() const { return track.open; }
    int scene() const { return currentScene; }

    static bool fileExists(const MusicPath& path) {
#ifdef _WIN32
        const DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
#else
        FILE* f = fopen(path.c_str(), "rb");
        if (!f) return false;
        fclose(f);
        return true;
#endif
    }

    static MusicPath exeFolder() {
#ifdef _WIN32
        wchar_t buffer[MAX_PATH * 2] = {};
        const DWORD len = GetModuleFileNameW(NULL, buffer, (DWORD)(sizeof(buffer) / sizeof(buffer[0])));
        MusicPath folder(buffer, len);
        const size_t slash = folder.find_last_of(L"\\/");
        if (slash != MusicPath::npos) folder.resize(slash + 1);
        return folder;
#else
        return MusicPath();
#endif
    }

    static MusicPath fileName(const MusicPath& path) {
        const size_t slash = path.find_last_of(
#ifdef _WIN32
            L"\\/"
#else
            "/"
#endif
        );
        return slash == MusicPath::npos ? path : path.substr(slash + 1);
    }
    static bool alreadyListed(const std::vector<MusicPath>& out, const MusicPath& path) {
        const MusicPath name = fileName(path);
        for (size_t i = 0; i < out.size(); ++i) if (fileName(out[i]) == name) return true;
        return false;
    }

    // Collects <base>.mp3 and optional <base>_a.mp3 style variants in one folder.
    void collectVariants(const MusicPath& dir, const char* base, std::vector<MusicPath>& out) const {
#ifdef _WIN32
        std::wstring wideBase;
        for (const char* p = base; *p; ++p) wideBase.push_back((wchar_t)(unsigned char)*p);
        WIN32_FIND_DATAW data;
        HANDLE handle = FindFirstFileW((dir + wideBase + L"*.mp3").c_str(), &data);
        if (handle != INVALID_HANDLE_VALUE) {
            do {
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                const MusicPath path = dir + data.cFileName;
                if (!alreadyListed(out, path)) out.push_back(path);
            } while (FindNextFileW(handle, &data));
            FindClose(handle);
        }
#else
        const MusicPath exact = dir + base + ".mp3";
        if (fileExists(exact) && !alreadyListed(out, exact)) out.push_back(exact);
#endif
    }

    void findFiles(const char* base, std::vector<MusicPath>& out) const {
        for (size_t d = 0; d < searchDirs.size(); ++d) collectVariants(searchDirs[d], base, out);
    }

    void scan() {
        if (scanned) return;
        scanned = true;
        searchDirs.clear();
#ifdef _WIN32
        searchDirs.push_back(exeFolder() + L"audio\\");
        wchar_t cwd[MAX_PATH * 2] = {};
        if (GetCurrentDirectoryW((DWORD)(sizeof(cwd) / sizeof(cwd[0])), cwd) > 0)
            searchDirs.push_back(MusicPath(cwd) + L"\\audio\\");
#else
        searchDirs.push_back(MusicPath("audio/"));
#endif
        for (int s = 0; s < MUSIC_SCENE_COUNT; ++s) {
            findFiles(MUSIC_SCENE_KEYS[s], files[s]);
            // The exe folder and the working directory are often the same place.
            std::sort(files[s].begin(), files[s].end());
            files[s].erase(std::unique(files[s].begin(), files[s].end()), files[s].end());
        }
    }

    // Called once per output sample by the audio thread.
    void requestScene(int scene) {
        if (!hasTrack(scene)) scene = -1;
        if (scene == requestedScene) return;
        requestedScene = scene;
        if (phase != 1) phase = 1; // fade out first, so scene changes never click
    }

    bool startScene(int scene) {
        if (!hasTrack(scene)) return false;
        std::vector<MusicPath>& list = files[scene];
        size_t pick = list.size() > 1 ? (size_t)(random() % list.size()) : 0;
        if (list.size() > 1 && (int)pick == lastVariant) pick = (pick + 1) % list.size();
        lastVariant = (int)pick;
        if (!track.openFile(list[pick], outputRate)) return false;
        currentScene = scene;
        return true;
    }

    void advanceEnvelope() {
        if (phase == 1) {
            env -= 1.0f / (float)(outputRate * 0.12);  // 120 ms fade out
            if (env <= 0.0f) {
                env = 0.0f;
                track.closeFile();
                if (startScene(requestedScene)) {
                    phase = 2;
                } else {
                    currentScene = -1;
                    phase = 0;
                }
            }
        } else if (phase == 2) {
            env += 1.0f / (float)(outputRate * 0.30);  // 300 ms fade in
            if (env >= 1.0f) { env = 1.0f; phase = 0; }
        }
    }

    float render() {
        advanceEnvelope();
        if (!track.open) return 0.0f;
        return track.nextSample() * env;
    }

    void shutdown() {
        track.closeFile();
        currentScene = requestedScene = -1;
        phase = 0;
        env = 0.0f;
    }
};
