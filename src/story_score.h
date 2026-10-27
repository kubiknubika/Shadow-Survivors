#pragma once
#include "music_composer.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace StoryScore {
using namespace MusicComposer;
// Production guardrails: original project-authored motifs only; oscillator-based
// instrumental synthesis (no lyric/voice recordings or sampled soundtrack excerpts);
// one stable tempo, with percussion reserved for story-specific accents rather than
// a continuous trailer-style beat. Keep this score unclipped and leave melodic space.
constexpr double BPM=72.0;
constexpr int BEATS_PER_BAR=4;
constexpr int BARS_PER_CHAPTER=4;
constexpr int CHAPTERS_PER_MOVEMENT=6;
constexpr double BAR_SECONDS=BEATS_PER_BAR*60.0/BPM;
constexpr double CHAPTER_SECONDS=BARS_PER_CHAPTER*BAR_SECONDS;
constexpr double MOVEMENT_SECONDS=CHAPTERS_PER_MOVEMENT*CHAPTER_SECONDS;
constexpr double TOTAL_SECONDS=2.0*MOVEMENT_SECONDS; // first movement plus its thematic finale
constexpr double OPENING_FADE_SECONDS=0.65;
constexpr double SECOND_MOVEMENT_OPEN_SECONDS=0.55;
constexpr double FIRST_MOVEMENT_CLOSE_SECONDS=1.8;
constexpr double SECOND_MOVEMENT_CLOSE_SECONDS=2.2;
constexpr double ARCHIVE_WITHDRAWAL_START=63.2;
constexpr double ARCHIVE_WITHDRAWAL_SECONDS=3.45;
constexpr double HEART_EXPANSION_START=68.0;
constexpr double HEART_EXPANSION_SECONDS=5.0;

struct HarmonicBar { int root, third, fifth, seventh; };
struct StoryNote { int midi; };
struct RhythmicEvent { float start, length; }; // beats from the start of a four-bar chapter
struct PercussionPattern { unsigned beatMask, barMask; }; // 4/4 quarter-beat and bar accents
// Beat units: 1.0 = quarter, 0.5 = eighth, 0.25 = sixteenth. Every key event
// stays on this grid so later game triggers can quantize without rhythmic guesswork.
static const StoryNote witnessTheme[11]={{62},{69},{70},{65},{64},{62},{67},{70},{69},{64},{62}};
static const RhythmicEvent witnessRhythm[6][11]={
    {{0,1.5f},{2,.75f},{3.5f,.5f},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0},{0,0}},
    {{0,.65f},{1,.55f},{2,.45f},{4,.65f},{5,.45f},{6,.65f},{8,.65f},{9,.5f},{10,.65f},{12,.5f},{14,1.7f}},
    {{0,1.25f},{1.5f,1.0f},{3,.9f},{4.5f,1.25f},{6,.9f},{7.5f,1.0f},{8.5f,.95f},{10,1.0f},{11.5f,1.0f},{13,.8f},{14.5f,1.25f}},
    {{0,.42f},{.75f,.28f},{2.5f,.42f},{4,.48f},{5.5f,.32f},{6.75f,.45f},{8,.42f},{9.5f,.30f},{10.25f,.4f},{12.5f,.35f},{14.5f,1.0f}},
    {{0,.9f},{1,.55f},{2.5f,.45f},{4,1.0f},{5.5f,.5f},{6.5f,.9f},{8,1.0f},{9,.5f},{11,.9f},{12.5f,.45f},{14,1.7f}},
    {{0,1.3f},{1.5f,.7f},{2.5f,1.1f},{4,1.3f},{5.5f,.7f},{6.5f,1.2f},{8,1.3f},{9.5f,.8f},{11,1.2f},{13,.5f},{14,1.8f}}
};
static const float archiveStarts[6][4]={
    {0,2,0,2},{0,1,2,3},{0,.75f,2,2.75f},
    {0,.75f,1.5f,2.5f},{0,1,2,3},{0,1.5f,2.5f,3.5f}
};
static const float archiveDurations[6]={.30f,.42f,.62f,.32f,.88f,.82f};
static const float bassStarts[6][8]={
    {0,12,0,0,0,0,0,0}, {0,2,4,6,8,10,12,14}, {0,0,0,0,0,0,0,0},
    {0,1.5f,4,5.5f,8,9.5f,12,13.5f}, {0,0,0,0,0,0,0,0}, {0,2,4,6,8,10,12,14}
};
static const int bassCounts[6]={2,8,0,8,0,8};
static const float bassDurations[6]={3.52f,.48f,0,.24f,0,.28f};
static const PercussionPattern firstPercussion[6]={{1,1},{5,15},{0,0},{1,5},{0,0},{5,15}};
static const PercussionPattern secondPercussion[6]={{5,15},{0,0},{9,15},{0,0},{1,15},{0,0}};
enum ToneFamily { BowedStrings=0, HarpTone=1, FluteTone=2, OrganTone=3, PizzicatoTone=4,
                  ChoirTone=5, SynthTone=6, CelestaTone=7, BrassTone=8, FolkFluteTone=9 };
struct ChapterVoicing {
    const char* scene;
    int harmonyFamily, archiveFamily, witnessFamily, bassFamily, responseFamily;
    int archiveOctave, witnessTranspose;
    float harmonyGain, archiveGain, witnessGain, bassGain, responseGain;
};
// Functional roles, not just a list of colors: each scene assigns a carrier to
// the harmony, Archive loop, Witness melody, bass, and answer line.
static const ChapterVoicing firstVoicing[6]={
    {"Ruins",BowedStrings,CelestaTone,FluteTone,OrganTone,BrassTone,24,0,.040f,.105f,.130f,.100f,.065f},
    {"Crypt / Morgar",OrganTone,PizzicatoTone,BowedStrings,OrganTone,BrassTone,12,-12,.050f,.105f,.130f,.100f,.065f},
    {"Cathedral / Malakor",OrganTone,FluteTone,ChoirTone,BowedStrings,BowedStrings,24,0,.058f,.105f,.115f,.100f,.065f},
    {"Ash Throne / Cardinal",OrganTone,BrassTone,BowedStrings,OrganTone,FluteTone,24,0,.060f,.105f,.130f,.100f,.065f},
    {"Archive / Azarot",ChoirTone,CelestaTone,FluteTone,ChoirTone,BowedStrings,24,0,.070f,.105f,.130f,.100f,.065f},
    {"Heart",ChoirTone,CelestaTone,BowedStrings,BowedStrings,FluteTone,24,12,.085f,.120f,.145f,.120f,.095f}
};
struct FinaleVoicing {
    const char* scene;
    int harmonyFamily, witnessFamily;
    float harmonyGain, witnessGain;
};
static const FinaleVoicing secondVoicing[6]={
    {"Morgar / Duty",BowedStrings,BowedStrings,.060f,.000f},
    {"Malakor / Voice",BowedStrings,ChoirTone,.058f,.000f},
    {"Cardinal / Name",OrganTone,BrassTone,.060f,.000f},
    {"Azarot / Archive",ChoirTone,OrganTone,.062f,.000f},
    {"Three Echo Orders",BowedStrings,BrassTone,.055f,.000f},
    {"Right to Remember",ChoirTone,BowedStrings,.075f,.170f}
};
static const int guardianFamily[4]={PizzicatoTone,ChoirTone,BrassTone,OrganTone};
static const HarmonicBar chords[6][4]={
    // 1 Ruins: D minor, a distant relative, then the first unresolved pull toward A.
    {{50,3,7,-1},{46,4,7,-1},{41,4,7,-1},{45,4,7,10}},
    // 2 Crypt: the duty motif moves into the low register.
    {{43,3,7,-1},{50,3,7,-1},{46,4,7,-1},{45,4,7,10}},
    // 3 Cathedral: a more open, vocal space; the harmony briefly looks outward.
    {{48,4,7,-1},{43,3,7,-1},{46,4,7,-1},{50,3,7,-1}},
    // 4 Throne: identity destabilizes; the dominant arrives early and refuses to settle.
    {{46,4,7,-1},{48,4,7,-1},{45,4,7,10},{43,3,7,-1}},
    // 5 Archive: the original minor circuit returns, now fully exposed.
    {{50,3,7,-1},{46,4,7,-1},{43,3,7,-1},{45,4,7,10}},
    // 6 Heart: the old cadence is transformed into D major rather than merely repeated.
    {{50,3,7,-1},{41,4,7,-1},{43,3,7,-1},{50,4,7,-1}}
};

static float clamp01(float x) { return std::max(0.0f,std::min(1.0f,x)); }

// After listening, give the principal line a genuinely legato span in the later
// chapters. Keep brief rests and syncopations, but avoid clipped note-to-note gaps.
static float witnessDurationBeats(int chapter,int noteIndex) {
    const RhythmicEvent& e=witnessRhythm[chapter][noteIndex];
    if(e.length<=0.0f) return 0.0f;
    float duration=e.length;
    if(chapter>=2) {
        const float nextStart=noteIndex+1<11?witnessRhythm[chapter][noteIndex+1].start:16.0f;
        const float available=std::max(0.05f,nextStart-e.start+0.12f);
        const float legatoTarget=(chapter==3?0.78f:1.05f);
        duration=std::min(std::max(duration,legatoTarget),available);
        duration=std::min(duration,16.0f-e.start);
    }
    return duration;
}

// Find a close consonant chord tone for the melody. Suspensions remain un-doubled
// when the triad offers no consonant interval; the sustained chord bed still supports them.
static int consonantChordTone(int melodyNote,const HarmonicBar& chord) {
    int intervals[4]={0,chord.third,chord.fifth,chord.seventh};
    const int count=chord.seventh>=0?4:3;
    int best=-1;
    int bestDistance=1000;
    for(int octave=-1;octave<=4;++octave) for(int i=0;i<count;++i) {
        const int candidate=chord.root+12*octave+intervals[i];
        const int interval=candidate-melodyNote;
        const int magnitude=std::abs(interval);
        if(magnitude!=3 && magnitude!=4 && magnitude!=5 && magnitude!=7 && magnitude!=8 && magnitude!=9) continue;
        const int distance=std::abs(magnitude-5)+(interval<0?2:0);
        if(distance<bestDistance) { best=candidate; bestDistance=distance; }
    }
    return best;
}

// A through-composed, evolving draft of the campaign's musical story. The four-note
// Archive cell is introduced incomplete, fragmented, inverted under the Cardinal,
// then reconciled in a brighter final cadence. Each chapter changes register,
// rhythm, harmony and instrumentation; this is not a set of volume snapshots.
static float renderFirstMovement(double seconds, uint32_t sampleIndex) {
    if(seconds<0.0 || seconds>=MOVEMENT_SECONDS) return 0.0f;
    const double beat=seconds*BPM/60.0;
    const int absoluteBar=(int)floor(beat/4.0);
    const int chapter=std::min(CHAPTERS_PER_MOVEMENT-1,absoluteBar/BARS_PER_CHAPTER);
    const int barInChapter=absoluteBar%BARS_PER_CHAPTER;
    const double beatInBar=beat-floor(beat/BEATS_PER_BAR)*BEATS_PER_BAR;
    const float age=(float)(beatInBar*(60.0/BPM));
    const HarmonicBar& chord=chords[chapter][barInChapter];
    const float barDuration=(float)BAR_SECONDS;
    float sum=0.0f;

    const ChapterVoicing& roles=firstVoicing[chapter];
    const float chapterBeat=(float)(beat-chapter*BARS_PER_CHAPTER*BEATS_PER_BAR);
    static const int motif[6][4]={
        {0,7,6,1},     // Ruins: only the first dyad is allowed to speak.
        {0,7,6,1},     // Crypt: the whole signature becomes a measured duty pulse.
        {0,7,6,1},     // Cathedral: same identity, held and sung.
        {0,-7,-6,-1},  // Throne: inversion; the remembered pattern turns against itself.
        {0,7,6,1},     // Archive: original form finally assembled.
        {0,7,9,12}     // Heart: the altered final cell resolves into the major triad.
    };
    static const float densities[6]={0.38f,0.58f,0.66f,0.78f,0.55f,0.96f};

    // A sustained chord foundation runs through every chapter; register and orchestral
    // weight expand across the arc, while the Heart combines the earlier families.
    const int chordNotes[4]={chord.root+12,chord.root+12+chord.third,
                             chord.root+12+chord.fifth,chord.root+24+(chord.seventh>=0?chord.seventh:chord.fifth)};
    const int voices=chord.seventh>=0?4:3;
    const float chordLevel=roles.harmonyGain;
    for(int v=0;v<voices;++v) {
        const float env=envelope(roles.harmonyFamily,age,barDuration*0.94f);
        sum+=tone(roles.harmonyFamily,2.0f*(float)PI*midiHz(chordNotes[v])*age,age)*env*chordLevel;
    }

    // Keep the chord progression plainly audible in every chapter: sustained mid-
    // register triads are the harmonic floor; later scenes add cello and high divisi.
    static const float stringChordGain[6]={.030f,.035f,.042f,.048f,.060f,.068f};
    const float orchestraDuration=barDuration*0.96f;
    const float stringsEnv=envelope(BowedStrings,age,orchestraDuration);
    for(int v=0;v<3;++v) {
        sum+=tone(BowedStrings,2.0f*(float)PI*midiHz(chordNotes[v])*age,age)*stringsEnv*stringChordGain[chapter];
    }
    if(chapter>=2) {
        const float celloEnv=envelope(BowedStrings,age,orchestraDuration);
        const int celloNote=chord.root-12;
        sum+=tone(BowedStrings,2.0f*(float)PI*midiHz(celloNote)*age,age)*celloEnv*0.068f;
        if(chapter>=4) {
            for(int v=0;v<3;++v) {
                const int highNote=chordNotes[v]+12;
                sum+=tone(BowedStrings,2.0f*(float)PI*midiHz(highNote)*age,age)*stringsEnv*0.052f;
            }
        }
        if(chapter==5) {
            const int hornNote=chord.root+12+(chord.seventh>=0?chord.seventh:chord.fifth);
            const float hornEnv=envelope(BrassTone,age,barDuration*0.72f);
            sum+=tone(BrassTone,2.0f*(float)PI*midiHz(hornNote)*age,age)*hornEnv*0.030f;
        }
    }

    // Written bass onsets are chapter-relative, so the groove is explicit: two long
    // anchors in the Ruins; quarter-note-grid duty pulses in the Crypt/Heart; a
    // displaced 1.5-beat answer at the Throne; no bass ostinato in the Cathedral/Archive.
    for(int n=0;n<bassCounts[chapter];++n) {
        const float bassAgeBeats=chapterBeat-bassStarts[chapter][n];
        if(bassAgeBeats<0.0f || bassAgeBeats>=bassDurations[chapter]) continue;
        const int bassBar=(int)(bassStarts[chapter][n]/4.0f);
        const HarmonicBar& bassChord=chords[chapter][bassBar];
        const int bassNote=bassChord.root-24+((chapter==3 && bassBar==2)?1:0);
        const float bassAge=bassAgeBeats*(float)(60.0/BPM);
        const float bassDur=bassDurations[chapter]*(float)(60.0/BPM);
        sum+=tone(roles.bassFamily,2.0f*(float)PI*midiHz(bassNote)*bassAge,bassAge)*
             envelope(roles.bassFamily,bassAge,bassDur)*roles.bassGain;
    }

    // The thematic cell changes its musical meaning at each chapter, not just its gain.
    for(int n=0;n<4;++n) {
        if(chapter==0 && n>=2) continue;
        const float noteAgeBeats=(float)beatInBar-archiveStarts[chapter][n];
        if(noteAgeBeats<0.0f || noteAgeBeats>=archiveDurations[chapter]) continue;
        int interval=motif[chapter][n];
        int note=chord.root+roles.archiveOctave+interval;
        if(chapter==5 && barInChapter==3) note=chord.root+12+(n==0?0:(n==1?7:(n==2?9:12)));
        const float noteAge=noteAgeBeats*(float)(60.0/BPM);
        const float dur=archiveDurations[chapter]*(float)(60.0/BPM);
        const float level=densities[chapter]*roles.archiveGain;
        sum+=tone(roles.archiveFamily,2.0f*(float)PI*midiHz(note)*noteAge,noteAge)*envelope(roles.archiveFamily,noteAge,dur)*level;
    }

    // The Witness gets a second, independent melody: an 11-note call-and-response
    // phrase that is incomplete in the Ruins, grounded in the Crypt, sustained in the
    // Cathedral, inverted at the Throne, reunited with the Archive cell, and lifted
    // into D major in the Heart. This makes the story's human voice distinct from
    // the machine's four-note loop.
    for(int n=0;n<11;++n) {
        if(chapter==0 && n>=3) continue; // only the opening fragment is remembered
        const RhythmicEvent& rhythm=witnessRhythm[chapter][n];
        const float durationBeats=witnessDurationBeats(chapter,n);
        const float noteAgeBeats=chapterBeat-rhythm.start;
        if(noteAgeBeats<0.0f || noteAgeBeats>=durationBeats) continue;
        int note=witnessTheme[n].midi+roles.witnessTranspose;
        if(chapter==3) note=138-note; // melodic inversion around A4, the fragile point of balance
        if(chapter==5) {
            const int pc=note%12;
            if(pc==10 || pc==5) ++note; // B-flat and F become B-natural and F-sharp
        }
        const float noteAge=noteAgeBeats*(float)(60.0/BPM);
        const float dur=durationBeats*(float)(60.0/BPM);
        sum+=tone(roles.witnessFamily,2.0f*(float)PI*midiHz(note)*noteAge,noteAge)*envelope(roles.witnessFamily,noteAge,dur)*roles.witnessGain;
        const int harmonyNote=consonantChordTone(note,chord);
        static const float witnessHarmonyGain[6]={.040f,.045f,.050f,.052f,.060f,.070f};
        if(harmonyNote>=0)
            sum+=tone(BowedStrings,2.0f*(float)PI*midiHz(harmonyNote)*noteAge,noteAge)*
                 envelope(BowedStrings,noteAge,dur)*witnessHarmonyGain[chapter];
    }

    // A separate answer voice, initially absent, becomes a human counterline in the
    // Cathedral/Archive and opens into a high-register resolution at the Heart.
    if(chapter>=2) {
        static const int answer[6][4]={{0,0,0,0},{0,0,0,0},{3,2,1,0},{1,0,3,2},{2,3,1,0},{3,4,2,0}};
        const float answerStarts[4]={0.0f,1.0f,2.0f,3.0f};
        for(int n=0;n<4;++n) {
            const float noteAgeBeats=(float)beatInBar-answerStarts[n];
            const float durBeats=(chapter==2?0.86f:0.82f);
            if(noteAgeBeats<0.0f || noteAgeBeats>=durBeats) continue;
            const int degree=answer[chapter][n];
            const int interval=degree==0?0:(degree==1?chord.third:(degree==2?chord.fifth:(degree==3?(chord.seventh>=0?chord.seventh:chord.fifth):12)));
            const int answerNote=chord.root+(chapter==5?36:24)+interval;
            const float noteAge=noteAgeBeats*(float)(60.0/BPM);
            const float dur=durBeats*(float)(60.0/BPM);
            sum+=tone(roles.responseFamily,2.0f*(float)PI*midiHz(answerNote)*noteAge,noteAge)*envelope(roles.responseFamily,noteAge,dur)*roles.responseGain;
        }
    }

    // Story-linked percussion: resonator-like bell in the ruins; duty heartbeat in
    // the crypt; interrupted brass pulse at the throne; fuller hand-drum in the Heart.
    float percussion=0.0f;
    const int beatNo=(int)floor(beatInBar);
    const float beatPhase=(float)(beatInBar-beatNo);
    const float hitAge=beatPhase*(float)(60.0/BPM);
    const float decay=(chapter==1 || chapter==5)?24.0f:38.0f;
    const PercussionPattern& percussionPattern=firstPercussion[chapter];
    const bool accentedBeat=(percussionPattern.beatMask&(1u<<beatNo))!=0;
    const bool activeBar=(percussionPattern.barMask&(1u<<barInChapter))!=0;
    if(accentedBeat && activeBar) {
        const float ring=sin(2.0f*(float)PI*220.0f*hitAge)*expf(-hitAge*decay);
        percussion+=ring*(chapter==0?0.06f:0.045f);
        percussion+=((float)(noiseHash(sampleIndex)&65535u)/32768.0f-1.0f)*expf(-hitAge*70.0f)*(chapter==5?0.022f:0.012f);
    }
    sum+=percussion;

    // Mark an actual breath before the last chapter. The finale then expands and
    // resolves, with an edit-safe fade rather than an abrupt cutoff.
    float chapterGain=densities[chapter];
    if(chapter==4) chapterGain*=1.0f-0.68f*clamp01((float)(seconds-ARCHIVE_WITHDRAWAL_START)/ARCHIVE_WITHDRAWAL_SECONDS);
    if(chapter==5) chapterGain=0.72f+0.28f*clamp01((float)(seconds-HEART_EXPANSION_START)/HEART_EXPANSION_SECONDS);
    const float fadeOut=clamp01((float)(MOVEMENT_SECONDS-seconds)/FIRST_MOVEMENT_CLOSE_SECONDS);
    const float fadeIn=clamp01((float)seconds/OPENING_FADE_SECONDS);
    const float result=sum*chapterGain*fadeOut*fadeIn;
    return std::max(-0.82f,std::min(0.82f,result));
}

static float renderSecondMovement(double seconds, uint32_t sampleIndex) {
    if(seconds<MOVEMENT_SECONDS || seconds>=TOTAL_SECONDS) return 0.0f;
    const double local=seconds-MOVEMENT_SECONDS;
    const double beat=local*BPM/60.0;
    const int bar=(int)floor(beat/4.0);
    const int chapter=std::min(CHAPTERS_PER_MOVEMENT-1,bar/BARS_PER_CHAPTER);
    const int barInChapter=bar%BARS_PER_CHAPTER;
    const double beatInBar=beat-floor(beat/BEATS_PER_BAR)*BEATS_PER_BAR;
    const int beatNo=(int)floor(beatInBar);
    const float beatPhase=(float)(beatInBar-beatNo);
    const float age=beatPhase*(float)(60.0/BPM);
    const float barAge=(float)(beatInBar*(60.0/BPM));

    static const HarmonicBar finaleChords[6][4]={
        {{50,3,7,-1},{45,4,7,10},{50,3,7,-1},{43,3,7,-1}}, // Morgar: duty/resolve
        {{43,3,7,-1},{46,4,7,-1},{50,3,7,-1},{45,4,7,10}}, // Malakor: voice/absence
        {{46,4,7,-1},{43,3,7,-1},{48,4,7,-1},{45,4,7,10}}, // Cardinal: erasure
        {{50,3,7,-1},{43,3,7,-1},{46,4,7,-1},{45,4,7,10}}, // Azarot: archive loop
        {{45,4,7,10},{46,4,7,-1},{43,3,7,-1},{50,3,7,-1}}, // echo orders
        {{50,3,7,-1},{41,4,7,-1},{43,3,7,-1},{50,4,7,-1}}  // choice, remembered
    };
    const HarmonicBar& chord=finaleChords[chapter][barInChapter];
    const FinaleVoicing& roles=secondVoicing[chapter];
    float sum=0.0f;

    const int chordNotes[4]={chord.root+12,chord.root+12+chord.third,
                             chord.root+12+chord.fifth,chord.root+24+(chord.seventh>=0?chord.seventh:chord.fifth)};
    const int voices=(chapter==5?4:(chapter==0?3:4));
    // Carry the chord across the whole bar in every mode; a per-beat retrigger made
    // the earlier accompaniment read as isolated notes instead of a harmonic bed.
    const float bedAge=barAge;
    const float bedDur=(float)(BAR_SECONDS*0.96);
    for(int v=0;v<voices;++v) {
        sum+=tone(roles.harmonyFamily,2.0f*(float)PI*midiHz(chordNotes[v])*bedAge,bedAge)*
             envelope(roles.harmonyFamily,bedAge,bedDur)*roles.harmonyGain;
    }
    const float secondStringsEnv=envelope(BowedStrings,barAge,bedDur);
    for(int v=0;v<3;++v)
        sum+=tone(BowedStrings,2.0f*(float)PI*midiHz(chordNotes[v])*barAge,barAge)*secondStringsEnv*0.052f;

    // Four guardian signatures: a firm repeated duty pulse, a remembered vocal turn,
    // a name that is stamped then omitted, and the Archive's own chromatic loop.
    const int guardian=chapter<4?chapter:(chapter==4?(barInChapter<3?barInChapter:5):5);
    const float motifStarts[4]={0.0f,1.0f,2.0f,3.0f};
    for(int n=0;n<4;++n) {
        float start=motifStarts[n];
        float durBeats=0.42f;
        int note=chord.root+12;
        float level=0.11f;
        if(guardian==0) { // Morgar: the order is a repeated, grounded fifth.
            note=chord.root-12+((n==1)?0:((n==2)?7:0));
            durBeats=0.32f; level=0.12f;
        } else if(guardian==1) { // Malakor: the sister's voice rises, catches, and falls away.
            static const int voiceNotes[4]={67,70,69,65};
            static const float voiceStarts[4]={0.0f,1.25f,2.0f,3.0f};
            start=voiceStarts[n]; note=voiceNotes[n]; durBeats=(n==1?0.62f:0.86f); level=0.105f;
        } else if(guardian==2) { // Cardinal: a repeated name loses one syllable each time.
            static const int nameNotes[4]={69,69,67,64};
            static const float nameStarts[4]={0.0f,0.75f,2.0f,3.25f};
            start=nameStarts[n]; note=nameNotes[n]; durBeats=(n==1?0.22f:0.46f); level=0.10f;
        } else if(guardian==3) { // Azarot: the whole machine-cell, from its own harmonic root.
            static const int archive[4]={0,7,6,1};
            note=chord.root+24+archive[n]; durBeats=0.38f; level=0.095f;
        } else if(chapter==4) { // The trio owns no new theme: bars quote the four recorded orders.
            if(guardian==0) note=chord.root-12+((n==2)?7:0);
            else if(guardian==1) { static const int q[4]={67,70,69,65}; note=q[n]; durBeats=0.55f; }
            else if(guardian==2) { static const int q[4]={69,69,67,64}; note=q[n]; durBeats=0.30f; }
            else { static const int q[4]={0,7,6,1}; note=chord.root+24+q[n]; durBeats=0.34f; }
            level=0.083f;
        } else { // The human story takes the foreground; leave the archive voice behind.
            continue;
        }
        const float noteAgeBeats=(float)beatInBar-start;
        if(noteAgeBeats<0.0f || noteAgeBeats>=durBeats) continue;
        const float noteAge=noteAgeBeats*(float)(60.0/BPM);
        const float duration=durBeats*(float)(60.0/BPM);
        const int family=guardianFamily[guardian];
        sum+=tone(family,2.0f*(float)PI*midiHz(note)*noteAge,noteAge)*envelope(family,noteAge,duration)*level;
    }

    // In the Echo Orders chapter, three bars quote the three recorded commands;
    // bar four contains only a brief machine residue, not a fourth character. The
    // fragments are reused from the earlier themes, never presented as resurrected bosses.
    if(chapter==4) {
        const int fragment=barInChapter;
        const int note=fragment==0?chord.root+12:(fragment==1?67:(fragment==2?69:chord.root+25));
        const int family=fragment<3?guardianFamily[fragment]:OrganTone;
        const float duration=0.48f;
        const float fragmentAge=(float)beatInBar*(float)(60.0/BPM);
        if(beatInBar<0.48)
            sum+=tone(family,2.0f*(float)PI*midiHz(note)*fragmentAge,fragmentAge)*
                 envelope(family,fragmentAge,duration)*0.085f;
    }

    // The Witness theme returns in the last chapter. The Archive's loop is not played
    // here: the human cadence must be able to end without being trapped in the cycle.
    if(chapter==5) {
        const float chapterBeat=(float)(beat-chapter*BARS_PER_CHAPTER*BEATS_PER_BAR);
        for(int n=0;n<11;++n) {
            const RhythmicEvent& rhythm=witnessRhythm[5][n];
            const float durationBeats=witnessDurationBeats(5,n);
            const float noteAgeBeats=chapterBeat-rhythm.start;
            if(noteAgeBeats<0.0f || noteAgeBeats>=durationBeats) continue;
            int note=witnessTheme[n].midi+12;
            const int pc=note%12;
            if(pc==10 || pc==5) ++note;
            const float noteAge=noteAgeBeats*(float)(60.0/BPM);
            const float duration=durationBeats*(float)(60.0/BPM);
            sum+=tone(roles.witnessFamily,2.0f*(float)PI*midiHz(note)*noteAge,noteAge)*envelope(roles.witnessFamily,noteAge,duration)*roles.witnessGain;
            const int harmonyNote=consonantChordTone(note,chord);
            if(harmonyNote>=0)
                sum+=tone(BowedStrings,2.0f*(float)PI*midiHz(harmonyNote)*noteAge,noteAge)*
                     envelope(BowedStrings,noteAge,duration)*0.070f;
        }
    }

    // Weighty strikes give the confrontation a pulse; it falls away after the copied
    // orders break, making the quiet final theme a genuine change of musical behavior.
    float percussion=0.0f;
    const PercussionPattern& finalePulse=secondPercussion[chapter];
    const bool strike=(finalePulse.beatMask&(1u<<beatNo))!=0 &&
                      (finalePulse.barMask&(1u<<barInChapter))!=0;
    if(strike) {
        const float kick=expf(-age*20.0f)*sinf(2.0f*(float)PI*(54.0f-12.0f*age)*age);
        const float noise=((float)(noiseHash(sampleIndex)&65535u)/32768.0f-1.0f)*expf(-age*58.0f);
        percussion=kick*(chapter==4?0.10f:0.13f)+noise*(chapter==4?0.014f:0.026f);
    }
    sum+=percussion;

    static const float sectionGain[6]={0.68f,0.62f,0.74f,0.72f,0.86f,0.72f};
    float gain=sectionGain[chapter];
    if(chapter==5) gain=0.62f+0.38f*clamp01((float)(local-68.0)/7.0f);
    const float fadeIn=clamp01((float)local/SECOND_MOVEMENT_OPEN_SECONDS);
    const float fadeOut=clamp01((float)(MOVEMENT_SECONDS-local)/SECOND_MOVEMENT_CLOSE_SECONDS);
    return std::max(-0.82f,std::min(0.82f,sum*gain*fadeIn*fadeOut));
}

static float render(double seconds, uint32_t sampleIndex) {
    if(seconds<0.0 || seconds>=TOTAL_SECONDS) return 0.0f;
    return seconds<MOVEMENT_SECONDS?renderFirstMovement(seconds,sampleIndex):renderSecondMovement(seconds,sampleIndex);
}
} // namespace StoryScore
