#pragma once
// Text-to-speech for in-game messages.
//
// Uses the SAPI 5 voice that ships with Windows (the same engine Narrator uses),
// so nothing has to be bundled and nothing is downloaded. If the machine has no
// voice installed — or the game is built without the Windows speech headers — the
// engine quietly reports "unsupported" and the rest of the game is unaffected.

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <algorithm>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sapi.h>
#include <ole2.h>

// The SAPI GUIDs are spelled out here instead of relying on DEFINE_GUID + INITGUID.
// INITGUID only works when it is defined before the *first* Windows header, and this
// header is pulled in after other includes that already brought in <guiddef.h>.
// Hard-coding the four constants keeps the link clean on MSVC, MinGW and clang alike.
static const GUID kSpVoiceClsid =
    {0x96749377, 0x3391, 0x11d2, {0x9e, 0xe3, 0x00, 0xc0, 0x4f, 0x79, 0x73, 0x96}};
static const GUID kSpVoiceIid =
    {0x6c44df74, 0x72b9, 0x4992, {0xa1, 0xec, 0xef, 0x99, 0x6e, 0x04, 0x22, 0xd4}};
static const GUID kSpTokenCategoryClsid =
    {0xa910187f, 0x0c7a, 0x45ac, {0x92, 0xcc, 0x59, 0xed, 0xaf, 0xb7, 0x7b, 0x53}};
static const GUID kSpTokenCategoryIid =
    {0x2d3d3845, 0x39af, 0x4850, {0xbb, 0xf9, 0x40, 0xb4, 0x97, 0x80, 0x01, 0x1d}};
#endif

struct SpeechEngine {
    bool supported = false;   // a voice was found and can be used
    bool enabled = true;      // the player's switch in the settings
    int volume = 92;          // 0..100
    int rate = 0;             // -10..10
    int spokenCount = 0;
    std::string lastSpoken;
    std::string voiceLabel = "нет голоса";

#ifdef _WIN32
    ISpVoice* voice = nullptr;
    bool comInitialized = false;
#endif

    static std::string toSpeakable(const std::string& utf8) {
        // Rule replacements keep the synthesised line natural: bullets become commas,
        // dashes and brackets are dropped, and repeated spaces collapse.
        struct Rule { const char* from; const char* to; };
        static const Rule rules[] = {
            {"•", ","}, {"—", ","}, {"–", "-"}, {"−", "-"}, {"…", "."}, {"×", " x "},
            {"→", ","}, {"←", ","}, {"[", " "}, {"]", " "}, {"\"", " "},
            {"%", " процентов"}, {"№", " номер "}, {"+", " плюс "},
        };
        std::string text = utf8;
        for (const Rule& rule : rules) {
            size_t pos = 0;
            while ((pos = text.find(rule.from, pos)) != std::string::npos) {
                text.replace(pos, strlen(rule.from), rule.to);
                pos += strlen(rule.to);
            }
        }
        // Abbreviations the voice would otherwise spell letter by letter.
        struct Word { const char* from; const char* to; };
        static const Word words[] = {
            {"HP", "хп"}, {"XP", "опыт"}, {"КД", "перезарядка"}, {"Крит", "крит"},
            {"F1", "эф один"}, {"F3", "эф три"}, {"ESC", "эскейп"}, {"WASD", "васд"},
        };
        for (const Word& word : words) {
            size_t pos = 0;
            while ((pos = text.find(word.from, pos)) != std::string::npos) {
                text.replace(pos, strlen(word.from), word.to);
                pos += strlen(word.to);
            }
        }
        // Whitespace cleanup.
        std::string clean;
        clean.reserve(text.size());
        bool lastSpace = true;
        for (char c : text) {
            const bool space = (c == ' ' || c == '\t' || c == '\n' || c == '\r');
            if (space) {
                if (!lastSpace) clean.push_back(' ');
            } else clean.push_back(c);
            lastSpace = space;
        }
        while (!clean.empty() && clean.back() == ' ') clean.pop_back();
        return clean;
    }

#ifdef _WIN32
    static std::wstring widen(const std::string& utf8) {
        if (utf8.empty()) return std::wstring();
        const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), NULL, 0);
        std::wstring wide((size_t)std::max(0, len), L'\0');
        if (len > 0) MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &wide[0], len);
        return wide;
    }
    static std::string narrow(const wchar_t* wide) {
        if (!wide) return std::string();
        const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
        if (len <= 1) return std::string();
        std::string utf8((size_t)(len - 1), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, &utf8[0], len, NULL, NULL);
        return utf8;
    }

    void init(bool wantEnabled) {
        enabled = wantEnabled;
        comInitialized = SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));
        if (FAILED(CoCreateInstance(kSpVoiceClsid, NULL, CLSCTX_ALL, kSpVoiceIid, (void**)&voice)) || !voice) {
            voice = nullptr;
            supported = false;
            return;
        }
        supported = true;
        selectRussianVoice();
        voice->SetVolume(std::max(0, std::min(100, volume)));
        voice->SetRate(std::max(-10, std::min(10, rate)));
    }

    // Prefer a Russian token; keep the default voice when none is installed.
    void selectRussianVoice() {
        IEnumSpObjectTokens* tokens = nullptr;
        // ISpVoice has no token enumeration of its own; the voice category owns the list.
        ISpObjectTokenCategory* category = nullptr;
        if (!voice) return;
        if (CoCreateInstance(kSpTokenCategoryClsid, NULL, CLSCTX_ALL, kSpTokenCategoryIid,
                             (void**)&category) != S_OK || !category) return;
        HRESULT enumResult = category->SetId(SPCAT_VOICES, FALSE);
        if (SUCCEEDED(enumResult)) enumResult = category->EnumTokens(NULL, NULL, &tokens);
        category->Release();
        if (enumResult != S_OK || !tokens) return;
        ULONG count = 0;
        tokens->GetCount(&count);
        ISpObjectToken* chosen = nullptr;
        for (ULONG i = 0; i < count; ++i) {
            ISpObjectToken* token = nullptr;
            if (tokens->Item(i, &token) != S_OK || !token) continue;
            LPWSTR id = nullptr;
            if (token->GetId(&id) == S_OK && id) {
                const bool russian = wcsstr(id, L"RU") || wcsstr(id, L"ru") || wcsstr(id, L"1049") || wcsstr(id, L"419");
                if (russian && !chosen) chosen = token;
                if (russian) { CoTaskMemFree(id); if (chosen != token) token->Release(); break; }
                CoTaskMemFree(id);
            }
            if (chosen != token) token->Release();
        }
        if (chosen) {
            voice->SetVoice(chosen);
            ISpObjectToken* current = nullptr;
            if (voice->GetVoice(&current) == S_OK && current) {
                LPWSTR id = nullptr;
                if (current->GetId(&id) == S_OK && id) {
                    voiceLabel = narrow(id);
                    CoTaskMemFree(id);
                }
                current->Release();
            }
            chosen->Release();
        } else {
            voiceLabel = "голос по умолчанию";
        }
        tokens->Release();
    }

    void shutdown() {
        if (voice) {
            voice->Speak(NULL, SPF_PURGEBEFORESPEAK, NULL);
            voice->Release();
            voice = nullptr;
        }
        if (comInitialized) {
            CoUninitialize();
            comInitialized = false;
        }
    }

    void setEnabled(bool on) {
        enabled = on;
        if (!voice) return;
        if (!on) voice->Speak(NULL, SPF_PURGEBEFORESPEAK, NULL);
    }

    void setVolume(int value) {
        volume = std::max(0, std::min(100, value));
        if (voice) voice->SetVolume(volume);
    }

    // Speaks one message. New messages replace the current one so a burst of
    // combat banners can never queue up into a backlog.
    bool speak(const std::string& utf8) {
        if (!supported || !enabled || !voice) return false;
        const std::string text = toSpeakable(utf8);
        if (text.empty()) return false;
        const std::wstring wide = widen(text);
        if (wide.empty()) return false;
        // SPF_IS_NOT_XML: messages may contain '&', '<' and are never markup.
        const DWORD flags = SPF_ASYNC | SPF_PURGEBEFORESPEAK | SPF_IS_NOT_XML;
        if (voice->Speak(wide.c_str(), flags, NULL) != S_OK) return false;
        ++spokenCount;
        lastSpoken = text;
        return true;
    }
#else
    void init(bool wantEnabled) { enabled = wantEnabled; }
    void shutdown() {}
    void setEnabled(bool on) { enabled = on; }
    void setVolume(int value) { volume = value; }
    bool speak(const std::string&) { return false; }
#endif
};
