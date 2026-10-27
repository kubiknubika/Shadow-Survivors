#pragma once
#include "music_composer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace BossScore {
using namespace MusicComposer;
struct Note { float start, length; int midi; };
struct BossTheme {
    const char* name;
    const char* identity;
    int bpm;
    float barBeats;
    int leadFamily, padFamily, bassFamily, brassFamily, drumKind;
    std::array<MusicComposer::Chord,8> chords;
    std::vector<Note> melody;
};

static std::array<BossTheme,4> makeThemes() {
    return {{
        {"Morgar", "the gate: weight, duty, hammering resolve", 96,4,0,8,3,8,0,
         {{{50,3,7,-1},{46,4,7,-1},{43,3,7,-1},{45,4,7,10},{50,3,7,-1},{48,4,7,-1},{46,4,7,-1},{45,4,7,10}}},
         {{0,1.25f,62},{1.5f,.42f,69},{2,.8f,65},{3.2f,.62f,74},
          {4,1.3f,58},{5.5f,.42f,65},{6,.8f,62},{7.2f,.62f,70},
          {8,1.4f,55},{9.5f,.40f,62},{10,.8f,58},{11.2f,.62f,67},
          {12,1.0f,57},{13,.55f,64},{14,1.4f,69},
          {16,1.25f,62},{17.5f,.42f,69},{18,.8f,65},{19.2f,.62f,74},
          {20,1.3f,60},{21.5f,.42f,67},{22,.8f,64},{23.2f,.62f,72},
          {24,1.25f,58},{25.5f,.42f,65},{26,.8f,62},{27.2f,.62f,70},
          {28,1.7f,57},{30,.6f,64},{31,1.0f,62}}},
        {"Malakor", "the voice: echo, breath, unanswered call", 76,3,9,5,0,8,1,
         {{{43,3,7,-1},{39,3,7,-1},{46,4,7,-1},{41,4,7,-1},{43,3,7,-1},{38,3,7,-1},{39,3,7,-1},{38,4,7,10}}},
         {{0,1.6f,67},{1.5f,.8f,70},{2.5f,.42f,69},
          {3,1.5f,65},{4.5f,.8f,67},{5.5f,.42f,65},
          {6,1.7f,70},{7.5f,.8f,74},{8.5f,.42f,72},
          {9,1.5f,69},{10.5f,.8f,67},{11.5f,.4f,65},
          {12,1.6f,67},{13.5f,.8f,70},{14.5f,.42f,69},
          {15,1.5f,65},{16.5f,.8f,62},{17.5f,.42f,65},
          {18,1.7f,67},{19.5f,.8f,70},{20.5f,.42f,74},
          {21,1.8f,69},{22.5f,.8f,67},{23.5f,1.2f,62}}},
        {"Cardinal", "the name: ritual, omission, stamped silence", 112,4,8,5,3,8,2,
         {{{48,3,7,-1},{44,4,7,-1},{41,3,7,-1},{43,4,7,10},{48,3,7,-1},{51,4,7,-1},{44,4,7,-1},{43,4,7,10}}},
         {{0,.7f,67},{1.5f,.45f,72},{2.25f,.35f,70},{3,.8f,67},
          {4,.7f,68},{5.5f,.45f,72},{6.25f,.35f,70},
          {8,.7f,65},{9.5f,.45f,68},{10.25f,.35f,72},{11,.8f,70},
          {12,.7f,67},{13.5f,.45f,70},{14.25f,.35f,68},{15,.8f,65},
          {16,.65f,67},{17.5f,.4f,72},{18.25f,.32f,75},{19,.75f,72},
          {20,.7f,70},{21.5f,.42f,75},{22.25f,.32f,72},{23,.75f,68},
          {24,.7f,65},{25.5f,.42f,68},{26.25f,.32f,72},{27,.75f,70},
          {28,.75f,67},{29.5f,.4f,70},{30.25f,.32f,68},{31,.9f,60}}},
        {"Azarot", "the archive: machine-cycle, ash, final release", 128,4,6,3,6,8,3,
         {{{42,3,7,-1},{38,4,7,10},{45,4,7,-1},{40,4,7,-1},{42,3,7,-1},{49,4,7,10},{38,4,7,-1},{40,4,7,-1}}},
         {{0,.45f,66},{.5f,.45f,69},{1,.75f,73},{2,.45f,76},{2.5f,.45f,73},{3,.8f,69},
          {4,.45f,68},{4.5f,.45f,71},{5,.75f,76},{6,.45f,80},{6.5f,.45f,76},{7,.8f,71},
          {8,.4f,73},{8.5f,.4f,76},{9,.8f,80},{10,.4f,83},{10.5f,.4f,80},{11,.8f,76},
          {12,.45f,71},{12.5f,.45f,76},{13,.8f,80},{14,.45f,83},{14.5f,.45f,80},{15,.8f,76},
          {16,.4f,66},{16.5f,.4f,69},{17,.8f,73},{18,.4f,78},{18.5f,.4f,81},{19,.8f,85},
          {20,.4f,80},{20.5f,.4f,83},{21,.8f,88},{22,.4f,85},{22.5f,.4f,81},{23,.8f,78},
          {24,.4f,73},{24.5f,.4f,76},{25,.8f,80},{26,.4f,83},{26.5f,.4f,88},{27,.8f,85},
          {28,.5f,80},{28.5f,.5f,83},{29,.8f,88},{30,1.8f,90}}}
    }};
}

static const BossTheme& themeFor(int bossType) {
    static const auto themes=makeThemes();
    return themes[(unsigned)bossType%4];
}
static float clamp01(float x){return std::max(0.0f,std::min(1.0f,x));}
static double durationSeconds(const BossTheme& theme){return 8.0*theme.barBeats*60.0/theme.bpm;}

static float render(int bossType,double seconds,uint32_t sampleIndex) {
    const BossTheme& theme=themeFor(bossType);
    const double duration=durationSeconds(theme);
    if(seconds<0.0||seconds>=duration)return 0.0f;
    const double beat=seconds*theme.bpm/60.0;
    const int bar=(int)floor(beat/theme.barBeats);
    const int barInPhrase=bar%4;
    const double beatInBar=beat-floor(beat/theme.barBeats)*theme.barBeats;
    const float age=(float)(beatInBar*60.0/theme.bpm);
    const float secPerBeat=60.0f/theme.bpm;
    const MusicComposer::Chord& chord=theme.chords[bar%8];
    float sum=0.0f;

    // Full sustained chord, with a warm low foundation and high divisi extension.
    const int chordNotes[4]={chord.root+12,chord.root+12+chord.third,
                             chord.root+12+chord.fifth,chord.root+24+(chord.seventh>=0?chord.seventh:chord.fifth)};
    const int chordVoices=chord.seventh>=0?4:3;
    const float chordEnv=envelope(theme.padFamily,age,theme.barBeats*secPerBeat*0.96f);
    for(int v=0;v<chordVoices;++v)
        sum+=tone(theme.padFamily,2.0f*(float)PI*midiHz(chordNotes[v])*age,age)*chordEnv*0.070f;
    const float stringEnv=envelope(0,age,theme.barBeats*secPerBeat*0.96f);
    for(int v=0;v<3;++v)
        sum+=tone(0,2.0f*(float)PI*midiHz(chordNotes[v])*age,age)*stringEnv*0.048f;
    const int celloNote=chord.root-12;
    sum+=tone(0,2.0f*(float)PI*midiHz(celloNote)*age,age)*stringEnv*0.060f;

    // Authored melody with a consonant companion voice, allowed to approach the next onset.
    for(const Note& event:theme.melody) {
        const float noteAgeBeats=(float)(beat-event.start);
        const float nextStart=event.start+std::max(event.length,0.78f);
        const float end=std::min(8.0f*theme.barBeats,nextStart);
        const float effectiveLength=std::min(std::max(event.length,0.72f),end-event.start);
        if(noteAgeBeats<0.0f||noteAgeBeats>=effectiveLength)continue;
        const float noteAge=noteAgeBeats*secPerBeat;
        const float noteDuration=effectiveLength*secPerBeat;
        const float leadEnv=envelope(theme.leadFamily,noteAge,noteDuration);
        sum+=tone(theme.leadFamily,2.0f*(float)PI*midiHz(event.midi)*noteAge,noteAge)*leadEnv*0.185f;
        const int harmony=consonantChordTone(event.midi,chord);
        if(harmony>=0)
            sum+=tone(0,2.0f*(float)PI*midiHz(harmony)*noteAge,noteAge)*envelope(0,noteAge,noteDuration)*0.055f;
    }

    // Each boss owns a different pulse grammar, tied to its story and attack silhouette.
    float percussion=0.0f;
    const int beatNo=(int)floor(beatInBar);
    const float beatPhase=(float)(beatInBar-beatNo);
    const float hitAge=beatPhase*secPerBeat;
    const float kick=sinf(2.0f*(float)PI*(48.0f*hitAge+3.8f*(1.0f-expf(-hitAge*18.0f))))*expf(-hitAge*20.0f);
    const float noise=((float)(noiseHash(sampleIndex)&65535u)/32768.0f-1.0f);
    if(theme.drumKind==0) { // Morgar: two heavy gate-hammers per bar.
        if(beatNo==0||beatNo==2) percussion+=kick*(beatNo==0?0.34f:0.23f)+noise*expf(-hitAge*48.0f)*0.040f;
    } else if(theme.drumKind==1) { // Malakor: only the two dotted-quarter heartbeats; no constant kit.
        if(beatInBar<0.10|| (beatInBar>=1.5&&beatInBar<1.60)) percussion+=kick*0.16f;
        percussion+=noise*expf(-hitAge*58.0f)*0.008f;
    } else if(theme.drumKind==2) { // Cardinal: 3+3+2 ritual stamps.
        const float eighthIndex=beatInBar*2.0f;
        if(fabsf(eighthIndex-0.0f)<0.12f||fabsf(eighthIndex-3.0f)<0.12f||fabsf(eighthIndex-6.0f)<0.12f)
            percussion+=kick*0.24f+noise*expf(-hitAge*52.0f)*0.032f;
    } else { // Azarot: machine ticks accelerate into a phrase-end ash blast.
        const int sixteenth=(int)floor(beatInBar*4.0);
        if((sixteenth%4)==0) percussion+=kick*0.17f;
        if((sixteenth%2)==1) percussion+=noise*expf(-hitAge*68.0f)*0.022f;
        if(barInPhrase==3&&beatNo==3) percussion+=noise*expf(-hitAge*34.0f)*0.11f;
    }
    sum+=percussion;

    // Boss-specific answer layer: gate horns, Malakor's echo, Cardinal's missing stamp,
    // or Azarot's rising machine overtone. The second four-bar phrase grows into a climax.
    const float secondPhrase=bar>=4?1.22f:0.92f;
    if(theme.drumKind==0&&beatNo==3) {
        const int horn=chord.root+24+chord.fifth;
        sum+=tone(theme.brassFamily,2.0f*(float)PI*midiHz(horn)*age,age)*envelope(theme.brassFamily,age,0.9f*secPerBeat)*0.075f;
    } else if(theme.drumKind==1&&beatNo==2) {
        const int echo=chord.root+24+(chord.seventh>=0?chord.seventh:chord.fifth);
        const float echoGain=expf(-(float)std::max(0.0,beatInBar-2.0)*1.2f);
        sum+=tone(theme.leadFamily,2.0f*(float)PI*midiHz(echo)*age,age)*envelope(theme.leadFamily,age,1.1f*secPerBeat)*0.060f*echoGain;
    } else if(theme.drumKind==2&&beatNo==3&&barInPhrase!=3) {
        const int bell=chord.root+36+chord.fifth;
        sum+=tone(7,2.0f*(float)PI*midiHz(bell)*age,age)*envelope(7,age,0.8f*secPerBeat)*0.045f;
    } else if(theme.drumKind==3) {
        const int sixteenth=(int)floor(beatInBar*4.0);
        if(sixteenth%2==0) {
            const int overtone=chord.root+24+chord.fifth+(bar>=4?12:0);
            sum+=tone(theme.brassFamily,2.0f*(float)PI*midiHz(overtone)*age,age)*envelope(theme.brassFamily,age,0.28f*secPerBeat)*0.035f;
        }
    }

    const float phraseGain=(bar>=4?1.15f:0.92f);
    const float fadeIn=clamp01((float)(seconds/0.32));
    const float fadeOut=clamp01((float)((duration-seconds)/0.8));
    return std::max(-0.82f,std::min(0.82f,sum*secondPhrase*phraseGain*fadeIn*fadeOut));
}
} // namespace BossScore
