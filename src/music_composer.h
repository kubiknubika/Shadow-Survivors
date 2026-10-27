#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace MusicComposer {
constexpr double PI = 3.14159265358979323846;
struct Chord { int root; int third; int fifth; int seventh; }; // MIDI root + chord intervals
struct Event { float start; float length; int midi; }; // quarter-note beats from phrase start
struct ResponseEvent { float start; float length; int degree; }; // counterline beat; degree 0=root,1=third,2=fifth,3=seventh
struct Track {
    const char* name;
    const char* style;
    int bpm;
    float barBeats; // 4/4=4, 3/4=3, 6/8=3 quarter-note units
    int leadFamily, chordFamily, bassFamily, drumStyle;
    float gain;
    std::array<Chord,4> chords;
    std::vector<Event> melody;
    std::vector<ResponseEvent> responseA, responseB, responseC; // written answer lines for A, B, and bridge sections
};

// Instrument identity comes from a distinct partial spectrum *and* its motion over a note:
// plucks lose their upper harmonics, strings sway, reeds stay breathy, and bells ring inharmonically.
// This keeps the score from sounding like identical sine notes merely moved to new pitches.
static float tone(int family, float phase, float age = 0.0f) {
    const float p = phase;
    const float t = std::max(0.0f, age);
    const float vibrato = 0.014f * sinf(2.0f*(float)PI*5.1f*t);
    switch (family) {
    case 0: { // bowed strings: warm, gently moving bow color
        const float q=p+vibrato;
        return 0.70f*sinf(q)+0.20f*sinf(2*q)+0.095f*sinf(3*q)+0.045f*sinf(4*q)+0.020f*sinf(5*q);
    }
    case 1: // harp: upper partials bloom at the pluck, then quickly mellow
        return 0.66f*sinf(p)+0.31f*expf(-t*3.5f)*sinf(2*p)+0.17f*expf(-t*7.0f)*sinf(3*p)+0.085f*expf(-t*11.0f)*sinf(4*p)+0.040f*expf(-t*15.0f)*sinf(5*p);
    case 2: // flute: nearly pure, airy fundamental with a small breath-edge partial
        return 0.88f*sinf(p+vibrato*0.65f)+0.075f*sinf(2*p)+0.022f*sinf(3*p)+0.012f*sinf(7*p);
    case 3: { // organ: hollow odd harmonics and a slow drawbar-like amplitude swell
        const float tremolo=0.97f+0.03f*sinf(2.0f*(float)PI*4.2f*t);
        return tremolo*(0.70f*sinf(p)+0.27f*sinf(3*p)+0.15f*sinf(5*p)+0.085f*sinf(7*p)+0.045f*sinf(9*p));
    }
    case 4: // pizzicato: woody transient, with the bright attack decaying into a round body
        return 0.63f*sinf(p)+0.33f*expf(-t*5.0f)*sinf(2*p)+0.19f*expf(-t*9.0f)*sinf(3*p)+0.105f*expf(-t*14.0f)*sinf(4*p)+0.055f*expf(-t*19.0f)*sinf(5*p);
    case 5: { // choir: two slightly detuned singers beat against each other; slow breath adds life
        const float swell=0.96f+0.04f*sinf(2.0f*(float)PI*0.72f*t);
        return swell*(0.30f*(sinf(0.994f*p)+sinf(1.006f*p))+0.15f*sinf(2*p)+0.055f*sinf(3*p));
    }
    case 6: // synth: focused saw-like stack, gradually darkening on the held note
        return 0.48f*sinf(p)+0.25f*expf(-t*0.9f)*sinf(2*p)+0.17f*expf(-t*1.6f)*sinf(3*p)+0.12f*expf(-t*2.3f)*sinf(4*p)+0.075f*expf(-t*3.0f)*sinf(5*p)+0.04f*expf(-t*4.0f)*sinf(6*p);
    case 7: // celesta: inharmonic metal partials ring out at different rates
        return 0.53f*sinf(p)+0.24f*expf(-t*2.3f)*sinf(2.01f*p)+0.15f*expf(-t*4.8f)*sinf(3.98f*p)+0.095f*expf(-t*7.0f)*sinf(5.43f*p)+0.05f*expf(-t*9.0f)*sinf(7.8f*p);
    case 8: // brass: bright attack and slight pitch sway, like a small section rather than a sine lead
        return 0.50f*sinf(p+vibrato*0.4f)+0.31f*expf(-t*0.8f)*sinf(2*p)+0.22f*expf(-t*1.3f)*sinf(3*p)+0.15f*expf(-t*2.0f)*sinf(4*p)+0.095f*expf(-t*2.8f)*sinf(5*p)+0.05f*expf(-t*3.8f)*sinf(6*p);
    default: // folk flute: soft reed color with a gently wavering upper resonance
        return 0.72f*sinf(p+vibrato*0.8f)+0.20f*sinf(2*p)+0.075f*sinf(3*p)+0.025f*sinf(4*p)+0.012f*sinf(6*p);
    }
}
static float midiHz(int n) { return 440.0f*powf(2.0f,(n-69)/12.0f); }
static int consonantChordTone(int melodyNote,const Chord& chord) {
    const int intervals[4]={0,chord.third,chord.fifth,chord.seventh};
    const int count=chord.seventh>=0?4:3;
    int best=-1,bestDistance=1000;
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
static float envelope(int family, float age, float duration) {
    static const float attack[10]={0.045f,0.003f,0.025f,0.010f,0.002f,0.080f,0.002f,0.001f,0.008f,0.030f};
    static const float decay [10]={0.75f,3.2f,0.45f,0.20f,6.5f,0.38f,1.3f,3.8f,0.65f,0.55f};
    static const float sustain[10]={0.72f,0.0f,0.78f,0.92f,0.0f,0.72f,0.45f,0.0f,0.58f,0.76f};
    const float a=std::max(0.0001f,attack[family]);
    if (age < 0.0f || age > duration+0.16f) return 0.0f;
    const float atk=std::min(1.0f,age/a);
    if (age <= duration) {
        const float shaped=sustain[family]+(1.0f-sustain[family])*expf(-age*decay[family]);
        return atk*shaped;
    }
    const float release=expf(-(age-duration)*18.0f);
    return atk*sustain[family]*release;
}
static uint32_t noiseHash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16; return x;
}

static std::vector<Track> makeTracks() {
    std::vector<Track> tracks = {
        {"Пепел", "готическая кинематографическая тема", 96,4, 0,1,0,0,0.82f,
         {{{50,3,7,-1},{46,4,7,-1},{41,4,7,-1},{45,4,7,10}}},
         {{0,.5f,74},{.5f,.5f,69},{1,1,74},{2,1,77},{3,.5f,76},{3.5f,.5f,74},
          {4,1,77},{5,1,74},{6,1,70},{7,1,74},
          {8,1,69},{9,1,72},{10,2,77},
          {12,1,76},{13,1,79},{14,1,73},{15,1,69}}},
        {"Рассвет", "светлая арфа и широкие струны", 108,4, 1,1,1,1,0.78f,
         {{{50,4,7,-1},{45,4,7,-1},{47,3,7,-1},{43,4,7,-1}}},
         {{0,1,74},{1,.5f,76},{1.5f,.5f,78},{2,1,81},{3,1,78},
          {4,1,76},{5,1,74},{6,2,78},
          {8,1,78},{9,1,81},{10,1,83},{11,1,81},
          {12,1,79},{13,1,78},{14,1,76},{15,1,74}}},
        {"Север", "холодная камерная мелодия и редкие колокола", 76,4, 2,0,2,2,0.78f,
         {{{52,3,7,-1},{48,4,7,-1},{43,4,7,-1},{50,4,7,-1}}},
         {{0,1,71},{1,1,76},{2,2,74},
          {4,1,67},{5,1,71},{6,2,72},
          {8,1,71},{9,1,74},{10,2,79},
          {12,1,78},{13,1,76},{14,2,74}}},
        {"Колокол", "органный хорал и бронзовый звон, 3/4", 84,3, 3,3,3,3,0.72f,
         {{{43,3,7,-1},{51,4,7,-1},{46,4,7,-1},{50,4,7,-1}}},
         {{0,1,74},{1,1,70},{2,1,67},
          {3,1,70},{4,1,67},{5,1,63},
          {6,1,77},{7,1,74},{8,1,70},
          {9,1,69},{10,1,66},{11,1,74}}},
        {"Камерная", "барочное пиццикато и клавесиновые атаки", 120,4, 4,4,4,4,0.72f,
         {{{45,3,7,10},{41,4,7,-1},{48,4,7,-1},{40,4,7,10}}},
         {{0,.5f,76},{.5f,.5f,69},{1,1,72},{2,1,76},{3,1,79},
          {4,1,81},{5,.5f,79},{5.5f,.5f,77},{6,1,72},{7,1,69},
          {8,1,79},{9,1,76},{10,2,84},
          {12,1,71},{13,1,74},{14,1,80},{15,1,76}}},
        {"Прах", "минорный хор и траурный процесс", 72,4, 5,5,5,5,0.70f,
         {{{48,3,7,-1},{44,4,7,-1},{41,3,7,-1},{43,4,7,10}}},
         {{0,2,67},{2,1,72},{3,1,75},
          {4,2,72},{6,1,70},{7,1,68},
          {8,1,68},{9,1,72},{10,2,77},
          {12,1,74},{13,1,71},{14,2,67}}},
        {"Руны", "электронный рифф и чёткий бит", 128,4, 6,6,6,6,0.70f,
         {{{42,3,7,-1},{50,4,7,-1},{45,4,7,-1},{52,4,7,-1}}},
         {{0,.5f,66},{.5f,.5f,69},{1,1,73},{2,.5f,76},{2.5f,.5f,73},{3,1,69},
          {4,.5f,74},{4.5f,.5f,73},{5,1,69},{6,.5f,66},{6.5f,.5f,69},{7,1,74},
          {8,1,73},{9,.5f,76},{9.5f,.5f,78},{10,1,81},{11,1,80},
          {12,.5f,76},{12.5f,.5f,73},{13,1,71},{14,1,73},{15,1,66}}},
        {"Хрусталь", "стеклянный вальс и высокая целеста, 3/4", 88,3, 7,7,7,7,0.72f,
         {{{47,3,7,-1},{43,4,7,-1},{50,4,7,-1},{45,4,7,-1}}},
         {{0,.5f,78},{.5f,1,83},{1.5f,.5f,81},{2,1,78},
          {3,1,79},{4,1,74},{5,1,71},
          {6,.5f,78},{6.5f,.5f,81},{7,1,86},{8,1,81},
          {9,1,76},{10,1,73},{11,1,71}}},
        {"Герой", "медная фанфара и марш", 116,4, 8,8,8,8,0.76f,
         {{{41,4,7,-1},{46,4,7,-1},{50,3,7,-1},{48,4,7,-1}}},
         {{0,1,77},{1,1,81},{2,2,84},
          {4,1,82},{5,1,81},{6,2,77},
          {8,1,81},{9,1,86},{10,2,84},
          {12,1,79},{13,1,76},{14,2,72}}},
        {"Странник", "фольклорная флейта и ручной барабан, 6/8, dotted-quarter 90", 135,3, 9,1,9,9,0.76f,
         {{{50,3,7,-1},{43,4,7,-1},{48,4,7,-1},{50,3,7,-1}}},
         {{0,.5f,74},{.5f,.5f,77},{1.0f,1.0f,81},{2.0f,1.0f,79},
          {3.0f,.5f,76},{3.5f,.5f,74},{4.0f,1.0f,71},{5.0f,1.0f,67},
          {6.0f,.5f,67},{6.5f,.5f,72},{7.0f,1.0f,76},{8.0f,1.0f,74},
          {9.0f,.5f,69},{9.5f,.5f,71},{10.0f,1.0f,72},{11.0f,1.0f,74}}}
    };

    // Each four-bar phrase has a separately written instrumental answer. A is the
    // opening/reprise, B is a new response phrase, and C supplies a contrasting bridge.
    tracks[0].responseA={{0.75f,.5f,0},{2.5f,.5f,1},{4.75f,.5f,2},{7.0f,.75f,1},{9.5f,.5f,0},{12.25f,.5f,3},{14.5f,.5f,0}};
    tracks[0].responseB={{0.0f,.75f,2},{1.5f,.5f,1},{3.25f,.5f,0},{5.5f,.5f,2},{8.5f,.5f,3},{11.25f,.75f,1},{14.0f,.5f,0}};
    tracks[0].responseC={{0.5f,.5f,1},{2.25f,.5f,2},{5.0f,.75f,0},{6.75f,.5f,3},{8.25f,.5f,2},{10.5f,.75f,1},{13.25f,.5f,0},{15.0f,.5f,2}};

    tracks[1].responseA={{0.5f,.75f,2},{2.0f,.5f,1},{4.5f,.5f,0},{6.5f,.75f,2},{8.5f,.5f,1},{10.75f,.5f,3},{13.0f,.75f,2},{15.0f,.5f,0}};
    tracks[1].responseB={{0.0f,.5f,0},{1.0f,.75f,2},{3.0f,.5f,1},{5.0f,.75f,3},{7.5f,.5f,2},{9.0f,.5f,0},{11.5f,.75f,1},{14.5f,.5f,2}};
    tracks[1].responseC={{0.75f,.5f,1},{2.5f,.75f,3},{4.0f,.5f,2},{6.75f,.5f,0},{8.0f,.75f,1},{10.5f,.5f,2},{12.25f,.75f,3},{15.0f,.5f,0}};

    tracks[2].responseA={{1.0f,.75f,0},{3.0f,.5f,2},{5.5f,.75f,1},{8.0f,.5f,0},{10.5f,.75f,3}};
    tracks[2].responseB={{0.25f,.5f,2},{2.0f,.75f,0},{4.0f,.5f,3},{6.5f,.75f,1},{9.0f,.5f,2},{11.0f,.75f,0}};
    tracks[2].responseC={{1.5f,.75f,1},{3.5f,.5f,3},{6.0f,.75f,2},{8.5f,.5f,0},{10.75f,.75f,1}};

    tracks[3].responseA={{0.5f,.5f,0},{2.0f,.75f,2},{3.5f,.5f,1},{5.0f,.5f,0},{7.0f,.75f,2},{8.5f,.5f,3},{10.0f,.75f,1}};
    tracks[3].responseB={{0.0f,.75f,2},{1.5f,.5f,0},{3.0f,.5f,3},{4.5f,.75f,1},{6.0f,.5f,2},{8.0f,.75f,0},{9.5f,.5f,2},{11.0f,.5f,1}};
    tracks[3].responseC={{0.75f,.5f,1},{2.25f,.75f,3},{4.0f,.5f,0},{5.5f,.75f,2},{7.25f,.5f,1},{9.0f,.75f,0},{10.5f,.5f,3}};

    tracks[4].responseA={{0.5f,.5f,2},{1.75f,.5f,0},{3.0f,.5f,1},{4.5f,.5f,2},{6.0f,.75f,1},{7.5f,.5f,0},{9.0f,.5f,2},{10.5f,.75f,3},{12.0f,.5f,1},{14.0f,.5f,0}};
    tracks[4].responseB={{0.0f,.5f,0},{1.0f,.5f,2},{2.5f,.75f,1},{4.0f,.5f,3},{5.5f,.5f,2},{7.0f,.75f,0},{8.0f,.5f,1},{10.0f,.5f,2},{11.5f,.75f,3},{13.5f,.5f,1},{15.0f,.5f,0}};
    tracks[4].responseC={{0.75f,.5f,1},{2.0f,.75f,3},{3.5f,.5f,2},{5.0f,.5f,0},{6.5f,.75f,1},{8.5f,.5f,3},{10.0f,.5f,2},{12.0f,.75f,0},{13.5f,.5f,1},{15.0f,.5f,2}};

    tracks[5].responseA={{0.0f,1.0f,0},{2.5f,.5f,2},{4.0f,1.0f,1},{6.5f,.5f,0},{8.0f,1.0f,2},{10.5f,.5f,3},{12.0f,1.0f,1},{14.5f,.5f,0}};
    tracks[5].responseB={{1.0f,.75f,2},{3.0f,.5f,3},{4.5f,1.0f,0},{7.0f,.5f,1},{8.5f,.75f,2},{11.0f,.5f,0},{12.5f,1.0f,3},{15.0f,.5f,1}};
    tracks[5].responseC={{0.5f,.75f,1},{2.0f,.5f,0},{3.5f,1.0f,2},{6.0f,.5f,3},{7.5f,.75f,1},{10.0f,.5f,2},{11.5f,1.0f,0},{14.0f,.5f,3}};

    tracks[6].responseA={{0.25f,.35f,0},{1.5f,.35f,2},{2.75f,.5f,1},{4.25f,.35f,0},{5.5f,.35f,3},{6.75f,.5f,2},{8.25f,.35f,0},{9.5f,.35f,1},{10.75f,.5f,2},{12.25f,.35f,3},{13.5f,.35f,1},{14.75f,.5f,0}};
    tracks[6].responseB={{0.0f,.5f,2},{1.0f,.35f,1},{2.25f,.35f,0},{3.5f,.5f,3},{4.75f,.35f,2},{6.0f,.35f,0},{7.25f,.5f,1},{8.5f,.35f,3},{9.75f,.35f,2},{11.0f,.5f,0},{12.25f,.35f,1},{14.0f,.5f,2}};
    tracks[6].responseC={{0.5f,.35f,1},{1.75f,.5f,3},{3.0f,.35f,2},{4.25f,.35f,0},{5.5f,.5f,1},{6.75f,.35f,3},{8.0f,.35f,0},{9.25f,.5f,2},{10.5f,.35f,1},{11.75f,.35f,3},{13.0f,.5f,2},{14.5f,.35f,0}};

    tracks[7].responseA={{0.5f,.5f,2},{2.0f,.5f,1},{3.5f,.5f,0},{5.0f,.75f,2},{6.5f,.5f,1},{8.0f,.5f,3},{9.5f,.75f,2},{11.0f,.5f,0}};
    tracks[7].responseB={{0.0f,.75f,0},{1.5f,.5f,2},{3.0f,.5f,1},{4.5f,.75f,3},{6.0f,.5f,2},{7.5f,.5f,0},{9.0f,.75f,1},{10.5f,.5f,2}};
    tracks[7].responseC={{0.75f,.5f,1},{2.25f,.75f,3},{3.75f,.5f,2},{5.25f,.5f,0},{6.75f,.75f,1},{8.25f,.5f,3},{9.75f,.5f,2},{11.25f,.75f,0}};

    tracks[8].responseA={{0.0f,.75f,0},{1.5f,.5f,2},{3.0f,.75f,1},{4.5f,.5f,0},{6.0f,.75f,3},{7.5f,.5f,2},{9.0f,.75f,1},{10.5f,.5f,0},{12.0f,.75f,2},{13.5f,.5f,1},{15.0f,.75f,0}};
    tracks[8].responseB={{0.5f,.5f,2},{2.0f,.75f,0},{3.5f,.5f,3},{5.0f,.75f,1},{6.5f,.5f,2},{8.0f,.75f,0},{9.5f,.5f,1},{11.0f,.75f,3},{12.5f,.5f,2},{14.0f,.75f,0}};
    tracks[8].responseC={{0.0f,.5f,1},{1.25f,.75f,3},{3.0f,.5f,2},{4.25f,.75f,0},{6.0f,.5f,1},{7.25f,.75f,2},{9.0f,.5f,3},{10.25f,.75f,1},{12.0f,.5f,0},{13.25f,.75f,2},{15.0f,.5f,1}};

    tracks[9].responseA={{0.25f,.5f,0},{1.75f,.75f,2},{3.25f,.5f,1},{4.75f,.75f,0},{6.25f,.5f,3},{7.75f,.75f,2},{9.25f,.5f,1},{10.75f,.75f,0}};
    tracks[9].responseB={{0.0f,.5f,2},{1.25f,.75f,1},{2.75f,.5f,3},{4.25f,.75f,2},{5.75f,.5f,0},{7.25f,.75f,1},{8.75f,.5f,2},{10.25f,.75f,3}};
    tracks[9].responseC={{0.5f,.75f,1},{2.0f,.5f,3},{3.5f,.75f,0},{5.0f,.5f,2},{6.5f,.75f,1},{8.0f,.5f,3},{9.5f,.75f,2},{11.0f,.5f,0}};
    return tracks;
}

static float leadLengthBeats(const Track& tr,std::size_t eventIndex) {
    const Event& e=tr.melody[eventIndex];
    const double phraseBeats=4.0*tr.barBeats;
    const float nextStart=eventIndex+1<tr.melody.size()?tr.melody[eventIndex+1].start:(float)phraseBeats;
    const float available=std::max(0.05f,nextStart-e.start+0.10f);
    const float legatoTarget=0.78f;
    return std::min(std::min(std::max(e.length,legatoTarget),available),(float)phraseBeats-e.start);
}

static float renderTrack(const Track& tr, double seconds, int mode, float noiseHigh, float noiseLow, double bpmOverride=0.0) {
    const double bpm=bpmOverride>0.0?bpmOverride:tr.bpm;
    const double beat=seconds*bpm/60.0;
    const int bar=(int)floor(beat/tr.barBeats);
    const int section=(bar%32)/8;
    const int barInPhrase=bar&3;
    const double beatInBar=beat-floor(beat/tr.barBeats)*tr.barBeats;
    const double secPerBeat=60.0/bpm;
    const double phraseBeats=4.0*tr.barBeats;
    const double phraseBeat=beat-floor(beat/phraseBeats)*phraseBeats;
    const Chord& chord=tr.chords[barInPhrase];
    float sum=0.0f;

    // Lead: preserve authored onsets, but extend clipped values toward the next note
    // so the line breathes/overlaps instead of sounding like a string of disconnected dots.
    static const int harmonyFamilies[10]={5,0,0,5,0,0,5,5,0,0};
    for(std::size_t eventIndex=0;eventIndex<tr.melody.size();++eventIndex) {
        const Event& e=tr.melody[eventIndex];
        const float durationBeats=leadLengthBeats(tr,eventIndex);
        const double ageBeats=phraseBeat-e.start;
        if(ageBeats<0.0 || ageBeats>=durationBeats) continue;
        int note=e.midi;
        if(section==1 && (barInPhrase==2 || barInPhrase==3)) note+=12;
        if(section==2 && barInPhrase>=2) note+=12;
        if(section==3 && barInPhrase==3 && e.start>=4.0f*tr.barBeats-1.0f) note=tr.chords[0].root+24;
        const float age=(float)(ageBeats*secPerBeat);
        const float duration=durationBeats*(float)secPerBeat;
        const float env=envelope(tr.leadFamily,age,duration);
        const float vibrato=(tr.leadFamily==2 || tr.leadFamily==9) ? sinf(2.0f*(float)PI*4.8f*age)*0.00035f : 0.0f;
        sum += tone(tr.leadFamily,2.0f*(float)PI*midiHz(note)*age,age)*env*(0.23f+vibrato);
        const int harmonyNote=consonantChordTone(note,chord);
        if(harmonyNote>=0) {
            const int harmonyFamily=harmonyFamilies[std::max(0,std::min(9,tr.drumStyle))];
            sum+=tone(harmonyFamily,2.0f*(float)PI*midiHz(harmonyNote)*age,age)*
                 envelope(harmonyFamily,age,duration)*0.045f;
        }
    }

    // Chordal accompaniment: short, voiced attacks on the style's pulse, never an unbroken drone.
    const float pulse=tr.barBeats*0.5f;
    const int pulseNo=(int)floor(beatInBar/pulse);
    const float chordAge=(float)((beatInBar-pulseNo*pulse)*secPerBeat);
    const bool chordOn=(tr.drumStyle==2) ? (pulseNo==0) : true;
    if (chordOn) {
        const float chordDur=(pulse*0.90f)*(float)secPerBeat;
        const int chordNotes[4]={chord.root+12,chord.root+12+chord.third,chord.root+12+chord.fifth,
                                 chord.root+24+(chord.seventh>=0?chord.seventh:chord.fifth)};
        const int voices=(chord.seventh>=0)?4:3;
        const float chordLevel=(tr.drumStyle==6 || tr.drumStyle==8)?0.105f:0.085f;
        for (int v=0;v<voices;++v) {
            const float env=envelope(tr.chordFamily,chordAge,chordDur);
            sum += tone(tr.chordFamily,2.0f*(float)PI*midiHz(chordNotes[v])*chordAge,chordAge)*env*chordLevel;
        }
    }

    // Bass: tonic on the strong pulse, fifth on the answer pulse; styles 4 and 6 move more often.
    const bool fastBass=(tr.drumStyle==4 || tr.drumStyle==6);
    const float bassPulse=fastBass?pulse*0.5f:pulse;
    const int bassNo=(int)floor(beatInBar/bassPulse);
    const float bassAge=(float)((beatInBar-bassNo*bassPulse)*secPerBeat);
    const int bassMidi=chord.root-12+((bassNo%2)?7:0);
    const float bassDur=(fastBass?0.52f:0.82f)*(float)secPerBeat;
    const float bassEnv=envelope(tr.bassFamily,bassAge,bassDur);
    float bassWave=sinf(2.0f*(float)PI*midiHz(bassMidi)*bassAge);
    if (tr.bassFamily==6 || tr.bassFamily==4) bassWave+=0.20f*sinf(4.0f*(float)PI*midiHz(bassMidi)*bassAge);
    sum += bassWave*bassEnv*((tr.drumStyle==6)?0.20f:0.15f);

    // Arpeggiated instruments and pulse percussion, with role and density chosen per style.
    const double eighthPos=beatInBar*2.0;
    const int eighthIndex=(int)floor(eighthPos);
    const float eighthPhase=(float)(eighthPos-floor(eighthPos));
    const float eighthAge=eighthPhase*(float)(secPerBeat*0.5);
    const bool arpStyle=(tr.drumStyle==1 || tr.drumStyle==4 || tr.drumStyle==6 || tr.drumStyle==7 || tr.drumStyle==9);
    if (arpStyle && (eighthIndex%2==0 || tr.drumStyle==6 || tr.drumStyle==4)) {
        const int arpDegree=(eighthIndex/2+barInPhrase)%3;
        const int arpMidi=chord.root+12+(arpDegree==0?0:(arpDegree==1?chord.third:chord.fifth));
        const float arpEnv=envelope(tr.chordFamily,eighthAge,0.20f*(float)secPerBeat);
        sum += tone(tr.chordFamily,2.0f*(float)PI*midiHz(arpMidi)*eighthAge,eighthAge)*arpEnv*(tr.drumStyle==6?0.075f:0.055f);
    }

    // A full-bar bowed/choral triad (or seventh chord) makes the harmony continuous;
    // the pulse attacks above add groove without leaving gaps between chord changes.
    const int padFamily=(tr.leadFamily==5 || tr.chordFamily==5)?0:5;
    const float padAge=(float)(beatInBar*secPerBeat);
    const float padDur=(float)(tr.barBeats*0.98*secPerBeat);
    const int padNotes[4]={chord.root+12,chord.root+12+chord.third,chord.root+12+chord.fifth,
                           chord.root+24+(chord.seventh>=0?chord.seventh:chord.fifth)};
    const int padVoices=chord.seventh>=0?4:3;
    for(int v=0;v<padVoices;++v)
        sum+=tone(padFamily,2.0f*(float)PI*midiHz(padNotes[v])*padAge,padAge)*envelope(padFamily,padAge,padDur)*0.045f;

    // Written answer phrases make the 32-bar form genuinely evolve: A returns in the
    // reprise, while B and C use their own rhythms/contours rather than an octave shift.
    static const int counterFamilies[10]={2,0,7,5,2,1,0,5,0,3};
    const int counterFamily=counterFamilies[std::max(0,std::min(9,tr.drumStyle))];
    const std::vector<ResponseEvent>& answerLine=(section==1)?tr.responseB:((section==2)?tr.responseC:tr.responseA);
    for(const ResponseEvent& e:answerLine) {
        const double ageBeats=phraseBeat-e.start;
        if(ageBeats<0.0 || ageBeats>=e.length) continue;
        const int answerBar=std::max(0,std::min(3,(int)(e.start/tr.barBeats)));
        const Chord& answerChord=tr.chords[answerBar];
        const int interval=e.degree==0?0:(e.degree==1?answerChord.third:(e.degree==2?answerChord.fifth:(answerChord.seventh>=0?answerChord.seventh:answerChord.fifth)));
        const int note=answerChord.root+12+interval;
        const float age=(float)(ageBeats*secPerBeat);
        const float dur=e.length*(float)secPerBeat;
        sum+=tone(counterFamily,2.0f*(float)PI*midiHz(note)*age,age)*envelope(counterFamily,age,dur)*0.075f;
    }

    // A short original Archive/Resonator cell (root, fifth, tritone, semitone) is a
    // shared leitmotif, re-voiced to bind states without making all ten scores alike.
    // It is sparse in the horde layer and lower/brass-led for the boss.
    if ((mode==1 && (bar&1)==0) || mode==2) {
        static const int archiveCell[4]={0,7,6,1};
        const float cellDuration=tr.barBeats*0.19f;
        const int cellFamily=mode==1?tr.chordFamily:8;
        const int cellOctave=mode==1?24:12;
        const float cellLevel=mode==1?0.052f:0.085f;
        for(int n=0;n<4;++n) {
            const float start=tr.barBeats*(0.25f*n);
            const float ageBeats=(float)beatInBar-start;
            if(ageBeats<0.0f || ageBeats>=cellDuration) continue;
            const int note=chord.root+cellOctave+archiveCell[n];
            const float age=ageBeats*(float)secPerBeat;
            const float dur=cellDuration*(float)secPerBeat;
            sum+=tone(cellFamily,2.0f*(float)PI*midiHz(note)*age,age)*envelope(cellFamily,age,dur)*cellLevel;
        }
    }

    // Horde and boss states add genuinely different stems, not only a louder mix.
    // The horde gets an eighth-note bass ostinato and a brisk shaker layer; the boss
    // gets a low heartbeat pedal plus short brass calls that answer the written lead.
    const int eighthStep=(int)floor(beatInBar*2.0);
    const float combatEighthAge=(float)((beatInBar*2.0-eighthStep)*0.5*secPerBeat);
    if (mode==1) {
        if ((eighthStep&1)==0) {
            const int pulseNote=chord.root-12+(((eighthStep/2)&1)?7:0);
            const float pulseEnv=envelope(6,combatEighthAge,0.34f*(float)secPerBeat);
            sum+=tone(6,2.0f*(float)PI*midiHz(pulseNote)*combatEighthAge,combatEighthAge)*pulseEnv*0.16f;
        }
        const float shakerEnv=expf(-combatEighthAge*64.0f);
        sum+=noiseHigh*shakerEnv*((eighthStep&1)?0.060f:0.025f);
    } else if (mode==2) {
        const int bossBeat=(int)floor(beatInBar);
        const float bossAge=(float)((beatInBar-bossBeat)*secPerBeat);
        if ((bossBeat&1)==0) {
            const int pedalNote=chord.root-24+((bossBeat%4==2)?7:0);
            const float pedalEnv=envelope(3,bossAge,0.72f*(float)secPerBeat);
            sum+=tone(3,2.0f*(float)PI*midiHz(pedalNote)*bossAge,bossAge)*pedalEnv*0.19f;
        }
        if (bossBeat&1) {
            const int answerDegree=(bossBeat%4==1)?chord.third:(chord.seventh>=0?chord.seventh:chord.fifth);
            const int answerNote=chord.root+24+answerDegree;
            const float brassEnv=envelope(8,bossAge,0.42f*(float)secPerBeat);
            sum+=tone(8,2.0f*(float)PI*midiHz(answerNote)*bossAge,bossAge)*brassEnv*0.14f;
        } else {
            // The boss state lands with a voiced brass power-chord, not just a louder
            // bass note. It answers the existing beat-1/3 calls and follows each track's harmony.
            const int brassNotes[3]={chord.root+12,chord.root+12+chord.third,chord.root+12+chord.fifth};
            const float stabEnv=envelope(8,bossAge,0.78f*(float)secPerBeat);
            for(int v=0;v<3;++v)
                sum+=tone(8,2.0f*(float)PI*midiHz(brassNotes[v])*bossAge,bossAge)*stabEnv*0.040f;
        }
    }

    const double beatPhase=beatInBar-floor(beatInBar);
    const float dAge=(float)(beatPhase*secPerBeat);
    const float noise=noiseHigh;
    const float kickEnv=expf(-dAge*18.0f);
    const float kick=sinf(2.0f*(float)PI*(46.0f*dAge+4.0f*(1.0f-expf(-dAge*18.0f))))*kickEnv;
    const int beatNo=(int)floor(beatInBar);
    const bool fourFour=(tr.barBeats>3.5f);
    const bool downbeat=(beatNo==0 || (fourFour && beatNo==2));
    const bool backbeat=fourFour && (beatNo==1 || beatNo==3);
    const float snareEnv=expf(-dAge*22.0f)*std::min(1.0f,dAge/0.003f);
    float drums=0.0f;
    switch (tr.drumStyle) {
    case 0: // timpani/low cinematic drum
        if (beatNo==0 || (fourFour && beatNo==2 && barInPhrase%2==0)) drums+=kick*0.17f;
        break;
    case 1: // brushed/shaker pulse
        if (downbeat) drums+=kick*0.07f;
        if (eighthIndex%2==1) drums+=noise*expf(-eighthAge*48.0f)*0.028f;
        break;
    case 2: // northern score: no kit, one sparse high accent
        if (beatNo==0 && barInPhrase==0) drums+=noise*expf(-dAge*6.0f)*0.018f;
        break;
    case 3: // bell toll; chord family provides pitch, this is a soft low impulse
        if (beatNo==0) drums+=kick*0.10f;
        break;
    case 4: // chamber wood clicks on weak eighths
        if (eighthIndex%2==1) drums+=noise*expf(-eighthAge*65.0f)*0.045f;
        break;
    case 5: // processional drum
        if (beatNo==0 || (fourFour && beatNo==2)) drums+=kick*0.22f;
        if (fourFour && beatNo==3) drums+=(noise*0.55f+sinf(2.0f*(float)PI*180.0f*dAge)*0.45f)*snareEnv*0.06f;
        break;
    case 6: // electronic kick/snare/hat groove
        if (downbeat) drums+=kick*0.25f;
        if (backbeat) drums+=(noise*0.7f+sinf(2.0f*(float)PI*190.0f*dAge)*0.3f)*snareEnv*0.12f;
        if (eighthIndex%2==1) drums+=noise*expf(-eighthAge*70.0f)*0.040f;
        break;
    case 7: // glass shimmer, no rock kit
        if (beatNo==0) drums+=noise*expf(-dAge*12.0f)*0.045f;
        break;
    case 8: // marching snare
        if (downbeat) drums+=kick*0.25f;
        if (backbeat) drums+=(noise*0.68f+sinf(2.0f*(float)PI*210.0f*dAge)*0.32f)*snareEnv*0.15f;
        break;
    default: // hand drum in compound 6/8 feel
        if (beatNo==0 || (beatInBar>1.4 && beatInBar<1.6)) drums+=kick*0.16f;
        if (eighthIndex%3==2) drums+=noise*expf(-eighthAge*42.0f)*0.045f;
        break;
    }

    if(mode==2) {
        // Add a cinematic impact layer on the phrase downbeat and a smaller answer
        // every other bar. This changes attack, low-end and orchestration, not only gain.
        if(beatNo==0) {
            drums+=kick*0.36f;
            drums+=noise*expf(-dAge*52.0f)*0.055f;
        } else if(beatNo==2 && (barInPhrase&1)==0) {
            drums+=kick*0.22f;
            drums+=noise*expf(-dAge*58.0f)*0.028f;
        }
        if(barInPhrase==3 && beatNo==3)
            drums+=(noise*0.72f+sinf(2.0f*(float)PI*320.0f*dAge)*0.28f)*snareEnv*0.075f;
    }

    float sectionGain[4]={0.78f,0.94f,1.10f,1.00f};
    float gain=tr.gain*sectionGain[section];
    if (section==2) gain*=1.08f; // development section grows, while the lead remains foregrounded
    const float modeGain = mode==3 ? 0.78f : (mode==1 ? 1.10f : (mode==2 ? 1.18f : (mode==4 ? 1.07f : 1.0f)));
    const float drumMode = mode==3 ? 0.16f : (mode==1 ? 1.18f : (mode==2 ? 1.42f : (mode==4 ? 0.50f : 1.0f)));
    const float y=(sum+drums*drumMode+noiseLow*0.006f)*gain*modeGain;
    return std::max(-0.92f,std::min(0.92f,y));
}


// A dedicated title-screen miniature: 6/8 archive-lullaby in G minor, with a written
// celesta melody, low choir harmony and harp ostinato. This is a separate composition,
// not the battle score with its drums turned down.
static float renderMenu(double seconds, float noiseHigh, float noiseLow) {
    struct Note { float start, length; int midi; };
    static const Note motifA[] = {
        {0.0f,1.5f,74},{1.5f,0.5f,77},{2.0f,1.0f,79},
        {3.0f,1.5f,77},{4.5f,0.5f,74},{5.0f,1.0f,70},
        {6.0f,1.0f,72},{7.0f,0.5f,74},{7.5f,1.5f,77},
        {9.0f,1.0f,74},{10.0f,2.0f,69}
    };
    static const Note motifB[] = {
        {0.0f,1.0f,79},{1.0f,0.5f,82},{1.5f,1.0f,81},
        {2.5f,0.5f,79},{3.0f,1.5f,77},{4.5f,1.0f,74},
        {5.5f,0.5f,72},{6.0f,1.5f,70},{7.5f,0.5f,72},
        {8.0f,1.0f,74},{9.0f,1.5f,77},{10.5f,1.5f,74}
    };
    static const Note counterA[] = {
        {0.75f,0.75f,62},{2.5f,0.45f,67},{3.75f,0.75f,65},
        {5.5f,0.45f,62},{6.75f,0.75f,67},{8.5f,0.75f,70},{10.5f,1.0f,62}
    };
    static const Note counterB[] = {
        {0.5f,0.75f,67},{2.0f,0.45f,70},{3.5f,0.75f,69},
        {5.0f,0.75f,65},{6.5f,0.45f,62},{7.5f,0.75f,67},{9.5f,0.75f,70},{11.0f,0.75f,67}
    };
    constexpr double bpm = 68.0, beatsPerBar = 3.0;
    const double secPerBeat = 60.0 / bpm;
    const double beat = seconds * bpm / 60.0;
    const int bar = (int)floor(beat / beatsPerBar);
    const int section = (bar & 31) / 8;
    const int barInPhrase = bar & 3;
    const double beatInBar = beat - floor(beat / beatsPerBar) * beatsPerBar;
    const double phraseBeat = beat - floor(beat / 12.0) * 12.0;
    const int roots[4] = {43, 46, 39, 38}; // Gm – B♭ – E♭ – D7; dominant resolves to G minor.
    const int thirds[4] = {3, 3, 4, 4};
    const int root = roots[barInPhrase], third = thirds[barInPhrase];
    float sum = 0.0f;

    // Soft choir pad changes with the harmony once per 6/8 bar.
    const float padAge = (float)(beatInBar * secPerBeat);
    const float padDuration = 2.82f * (float)secPerBeat;
    const int padNotes[3] = {root + 12, root + third + 12, root + 19};
    for (int i = 0; i < 3; ++i) {
        const float env = envelope(5, padAge, padDuration);
        sum += tone(5, 2.0f * (float)PI * midiHz(padNotes[i]) * padAge, padAge) * env * (i == 0 ? 0.075f : 0.046f);
    }

    // A low bowed root anchors each measure without adding a drum pulse.
    const float bassEnv = envelope(0, padAge, 2.7f * (float)secPerBeat);
    sum += tone(0, 2.0f * (float)PI * midiHz(root - 12) * padAge, padAge) * bassEnv * 0.12f;

    // Gentle harp notes trace a changing 6/8 figure through each chord.
    const int eighth = (int)floor(beatInBar * 2.0);
    const float eighthPhase = (float)(beatInBar * 2.0 - eighth);
    const float harpAge = eighthPhase * (float)(0.5 * secPerBeat);
    const int arpIntervals[6] = {0, third, 7, third, 0, 7};
    const int harpMidi = root + 12 + arpIntervals[eighth % 6];
    sum += tone(1, 2.0f * (float)PI * midiHz(harpMidi) * harpAge, harpAge)
         * envelope(1, harpAge, 0.36f * (float)secPerBeat) * 0.045f;

    // The 32-bar form contrasts an answering B phrase, rises in C, then returns to A.
    const Note* motif = (section == 1 || section == 2) ? motifB : motifA;
    const int motifCount = (section == 1 || section == 2) ? (int)(sizeof(motifB) / sizeof(motifB[0])) : (int)(sizeof(motifA) / sizeof(motifA[0]));
    for (int i = 0; i < motifCount; ++i) {
        const Note& note = motif[i];
        const double ageBeats = phraseBeat - note.start;
        if (ageBeats < 0.0 || ageBeats >= note.length) continue;
        int midi = note.midi + (section == 2 ? 2 : 0);
        if (section == 3 && note.start >= 10.0f) midi = 67; // quiet tonic cadence on the reprise
        const float age = (float)(ageBeats * secPerBeat);
        const float dur = note.length * (float)secPerBeat;
        sum += tone(7, 2.0f * (float)PI * midiHz(midi) * age, age) * envelope(7, age, dur) * (section == 2 ? 0.18f : 0.15f);
    }

    // A lower bowed-string answer moves independently of the celesta lead.
    const Note* counter=(section==1 || section==2)?counterB:counterA;
    const int counterCount=(section==1 || section==2)?(int)(sizeof(counterB)/sizeof(counterB[0])):(int)(sizeof(counterA)/sizeof(counterA[0]));
    for(int i=0;i<counterCount;++i) {
        const Note& note=counter[i];
        const double ageBeats=phraseBeat-note.start;
        if(ageBeats<0.0 || ageBeats>=note.length) continue;
        int midi=note.midi+(section==2?2:0);
        if(section==3 && note.start>=10.0f) midi=55;
        const float age=(float)(ageBeats*secPerBeat), dur=note.length*(float)secPerBeat;
        sum+=tone(0,2.0f*(float)PI*midiHz(midi)*age,age)*envelope(0,age,dur)*0.09f;
    }

    // Very soft hand-drum swells mark the compound pulse without turning the menu into combat music.
    for(int pulse=0;pulse<2;++pulse) {
        const float pulseBeat=pulse==0?0.0f:1.5f;
        const float ageBeats=(float)beatInBar-pulseBeat;
        if(ageBeats<0.0f || ageBeats>0.24f) continue;
        const float age=ageBeats*(float)secPerBeat;
        const float env=expf(-age*20.0f);
        const float drum=sinf(2.0f*(float)PI*(58.0f-22.0f*age)*age)*env;
        sum+=drum*0.035f+noiseHigh*env*0.006f;
    }

    const float sectionGain[4] = {0.82f, 0.90f, 1.00f, 0.76f};
    return std::max(-0.75f, std::min(0.75f, sum * sectionGain[section] * 0.82f + noiseLow * 0.0025f));
}

// A dedicated level-up selection cue: a quiet four-bar Em–Cmaj7–G–B7 loop,
// clear enough to feel like a musical pause, with no combat beat or menu theme.
static constexpr int UPGRADE_BPM=72;
static constexpr float UPGRADE_BAR_BEATS=4.0f;
static double upgradeLoopSeconds(){return 4.0*UPGRADE_BAR_BEATS*60.0/UPGRADE_BPM;}
static float renderUpgrade(double seconds,float noiseHigh,float noiseLow){
    if(seconds<0.0)return 0.0f;
    struct Note{float start,length;int midi;};
    static const Chord chords[4]={{52,3,7,10},{48,4,7,11},{55,4,7,11},{47,4,7,10}};
    static const Note melody[]={
        {0,1.6f,64},{1.5f,.55f,67},{2.0f,1.35f,71},{3.5f,.42f,69},
        {4,1.55f,67},{5.5f,.55f,71},{6,1.30f,64},{7.5f,.42f,62},
        {8,1.55f,74},{9.5f,.55f,71},{10,1.35f,67},{11.5f,.42f,69},
        {12,1.55f,66},{13.5f,.55f,69},{14,1.35f,71},{15.5f,.42f,64}
    };
    static const Note answer[]={
        {0.75f,2.0f,59},{4.75f,2.0f,60},{8.75f,2.0f,62},{12.75f,2.0f,59}
    };
    const double loopSeconds=upgradeLoopSeconds();
    const double loopTime=seconds-floor(seconds/loopSeconds)*loopSeconds;
    const double phraseBeat=loopTime*UPGRADE_BPM/60.0;
    const int bar=(int)(phraseBeat/UPGRADE_BAR_BEATS);
    const double beatInBar=phraseBeat-bar*UPGRADE_BAR_BEATS;
    const float secPerBeat=60.0f/UPGRADE_BPM;
    const float age=(float)(beatInBar*secPerBeat);
    const Chord& chord=chords[bar];
    float sum=0.0f;
    const int chordNotes[4]={chord.root+12,chord.root+12+chord.third,
                             chord.root+12+chord.fifth,chord.root+24+chord.seventh};
    const float chordAge=age;
    const float chordDur=UPGRADE_BAR_BEATS*secPerBeat*.97f;
    for(int v=0;v<4;++v)
        sum+=tone(5,2.0f*(float)PI*midiHz(chordNotes[v])*chordAge,chordAge)*
             envelope(5,chordAge,chordDur)*(v==0?.054f:.040f);
    sum+=tone(0,2.0f*(float)PI*midiHz(chord.root-12)*age,age)*envelope(0,age,chordDur)*0.065f;
    for(const Note& e:melody){
        const float ageBeats=(float)(phraseBeat-e.start);
        if(ageBeats<0.0f||ageBeats>=e.length)continue;
        const float noteAge=ageBeats*secPerBeat,dur=e.length*secPerBeat;
        sum+=tone(7,2.0f*(float)PI*midiHz(e.midi)*noteAge,noteAge)*envelope(7,noteAge,dur)*0.125f;
        const int partner=consonantChordTone(e.midi,chord);
        if(partner>=0)
            sum+=tone(0,2.0f*(float)PI*midiHz(partner)*noteAge,noteAge)*envelope(0,noteAge,dur)*0.036f;
    }
    for(const Note& e:answer){
        const float ageBeats=(float)(phraseBeat-e.start);
        if(ageBeats<0.0f||ageBeats>=e.length)continue;
        const float noteAge=ageBeats*secPerBeat,dur=e.length*secPerBeat;
        sum+=tone(0,2.0f*(float)PI*midiHz(e.midi)*noteAge,noteAge)*envelope(0,noteAge,dur)*0.050f;
    }
    // No drum loop: one high celesta accent per bar leaves the choice screen calm.
    if(beatInBar<0.12){
        const float accentAge=(float)beatInBar*secPerBeat;
        const int sparkle=chord.root+36+(bar==3?chord.fifth:chord.third);
        sum+=tone(7,2.0f*(float)PI*midiHz(sparkle)*accentAge,accentAge)*expf(-accentAge*3.8f)*0.028f;
    }
    const float loopSwell=.96f+.04f*sinf(2.0f*(float)PI*(float)(phraseBeat/16.0));
    return std::max(-0.75f,std::min(0.75f,sum*loopSwell+noiseLow*0.0015f+noiseHigh*0.0008f));
}

// A separate, restrained game-over lament: four bars in D minor, with a clear
// falling line over sustained choir and cello, and an A7(b9) cadence that turns
// back to D minor at the loop seam. No combat percussion.
static constexpr int GAMEOVER_BPM=60;
static constexpr float GAMEOVER_BAR_BEATS=4.0f;
static double gameOverLoopSeconds(){return 4.0*GAMEOVER_BAR_BEATS*60.0/GAMEOVER_BPM;}
static float renderGameOver(double seconds,float noiseHigh,float noiseLow){
    if(seconds<0.0)return 0.0f;
    struct Note{float start,length;int midi;};
    static const int bassNotes[4]={38,34,33,33}; // D2, Bb1, A1, A1
    static const int padNotes[4][4]={
        {62,65,69,76}, // Dm(add9)
        {58,62,65,69}, // Bbmaj7
        {60,65,69,72}, // F/A
        {61,64,67,70}  // A7(b9)
    };
    static const Note melody[]={
        {0,2.8f,77},{3,0.85f,76},{4,2.8f,74},{7,0.85f,72},
        {8,2.8f,69},{11,0.85f,72},{12,2.7f,70},{15,0.9f,69}
    };
    static const Note response[]={
        {.55f,2.9f,62},{4.55f,2.9f,58},{8.55f,2.9f,60},{12.55f,2.9f,61}
    };
    const double loopSeconds=gameOverLoopSeconds();
    const double loopTime=seconds-floor(seconds/loopSeconds)*loopSeconds;
    const double phraseBeat=loopTime*GAMEOVER_BPM/60.0;
    const int bar=std::min(3,(int)(phraseBeat/GAMEOVER_BAR_BEATS));
    const double beatInBar=phraseBeat-bar*GAMEOVER_BAR_BEATS;
    const float secPerBeat=60.0f/GAMEOVER_BPM;
    const float age=(float)(beatInBar*secPerBeat);
    const float chordDuration=GAMEOVER_BAR_BEATS*secPerBeat*.98f;
    float sum=0.0f;
    for(int v=0;v<4;++v){
        const float level=v==0?.052f:.038f;
        sum+=tone(5,2.0f*(float)PI*midiHz(padNotes[bar][v])*age,age)*
             envelope(5,age,chordDuration)*level;
    }
    sum+=tone(0,2.0f*(float)PI*midiHz(bassNotes[bar])*age,age)*
         envelope(0,age,chordDuration)*.060f;
    for(const Note& e:melody){
        const float ageBeats=(float)(phraseBeat-e.start);
        if(ageBeats<0.0f||ageBeats>=e.length)continue;
        const float noteAge=ageBeats*secPerBeat,duration=e.length*secPerBeat;
        sum+=tone(0,2.0f*(float)PI*midiHz(e.midi)*noteAge,noteAge)*
             envelope(0,noteAge,duration)*.105f;
    }
    for(const Note& e:response){
        const float ageBeats=(float)(phraseBeat-e.start);
        if(ageBeats<0.0f||ageBeats>=e.length)continue;
        const float noteAge=ageBeats*secPerBeat,duration=e.length*secPerBeat;
        sum+=tone(5,2.0f*(float)PI*midiHz(e.midi)*noteAge,noteAge)*
             envelope(5,noteAge,duration)*.038f;
    }
    const float slowBreath=.97f+.03f*sinf(2.0f*(float)PI*(float)(phraseBeat/16.0));
    return std::max(-.75f,std::min(.75f,sum*slowBreath+noiseLow*.0010f+noiseHigh*.0005f));
}

static const std::vector<Track>& allTracks() {
    static const std::vector<Track> value = makeTracks();
    return value;
}
static double tempo(int variant) {
    const auto& t=allTracks(); return t[std::max(0,std::min(9,variant))].bpm;
}
static double beatsPerBar(int variant) {
    const auto& t=allTracks(); return t[std::max(0,std::min(9,variant))].barBeats;
}
// Context changes tempo without changing pitch. The ten written tracks keep their
// own base tempi; gameplay intensity applies a measured pulse multiplier.
static double rhythmicBpm(int variant,int mode) {
    const double base=tempo(variant);
    const double factor=mode==1?1.08:(mode==2?1.14:(mode==3?0.88:(mode==4?0.94:1.0)));
    return std::max(60.0,std::min(150.0,base*factor));
}
// Independent boss theme. It keeps the player's tonal center and signature timbre,
// but writes a new bII/iv/V7 progression and a new rising semitone/tritone motif instead
// of replaying the exploration melody at a faster BPM.
static float renderBossSynchronized(int variant,double seconds,float noiseHigh,float noiseLow,double bpmOverride=0.0) {
    const auto& tracks=allTracks();
    const int index=std::max(0,std::min(9,variant));
    const Track& tr=tracks[index];
    const double bpm=bpmOverride>0.0?bpmOverride:rhythmicBpm(index,2);
    const double beat=seconds*bpm/60.0;
    const double phraseBeats=4.0*tr.barBeats;
    const double phraseBeat=beat-floor(beat/phraseBeats)*phraseBeats;
    const int bar=std::min(3,(int)(phraseBeat/tr.barBeats));
    const double beatInBar=phraseBeat-bar*tr.barBeats;
    const double secPerBeat=60.0/bpm;
    const float age=(float)(beatInBar*secPerBeat);
    const int tonic=tr.chords[0].root;
    const bool minor=tr.chords[0].third==3;
    const int tonicThird=minor?3:4;
    const Chord bossChords[4]={
        {tonic,tonicThird,7,minor?10:11},
        {tonic+1,4,7,10},            // flat-II: the Phrygian shadow
        {tonic+5,3,7,10},            // minor subdominant
        {tonic+7,4,7,10}             // major dominant seventh, left unresolved
    };
    const Chord& chord=bossChords[bar];
    const int slots=std::max(6,(int)std::lround(tr.barBeats*2.0));
    static const int motif[4][8]={
        {0,7,10,7,3,7,10,12},
        {1,8,11,8,5,8,11,13},
        {5,8,12,8,7,10,12,15},
        {11,14,17,14,21,17,14,11}
    };
    static const int majorTonicMotif[8]={0,7,11,7,4,7,11,12};
    float sum=0.0f;
    const int harmony[4]={chord.root+12,chord.root+12+chord.third,
                          chord.root+12+chord.fifth,chord.root+24+chord.seventh};
    const float padEnv=envelope(5,age,(float)(tr.barBeats*secPerBeat*.97));
    const float stringEnv=envelope(0,age,(float)(tr.barBeats*secPerBeat*.96));
    for(int v=0;v<4;++v){
        sum+=tone(5,2.0f*(float)PI*midiHz(harmony[v])*age,age)*padEnv*.043f;
        sum+=tone(0,2.0f*(float)PI*midiHz(harmony[v])*age,age)*stringEnv*.030f;
    }

    // A fresh six/eight-note hook: semitone tension, tritone color, then a rising answer.
    const float slotBeats=(float)tr.barBeats/slots;
    const int slot=(int)floor(beatInBar/slotBeats);
    const float slotAge=(float)((beatInBar-slot*slotBeats)*secPerBeat);
    const int contour=(bar==0&&!minor)?majorTonicMotif[std::min(7,slot)]:motif[bar][std::min(7,slot)];
    const bool accented=(slot==0||slot==3||slot==slots-1);
    const int leadNote=tonic+24+contour;
    const float leadDur=(accented?.44f:.31f)*(float)secPerBeat;
    const int leadFamily=(tr.drumStyle==7)?7:8;
    sum+=tone(leadFamily,2.0f*(float)PI*midiHz(leadNote)*slotAge,slotAge)*
         envelope(leadFamily,slotAge,leadDur)*(accented?.145f:.105f);
    // Brass/choir response holds a chord tone across every other weak pulse.
    if((slot&1)==1){
        const int answerNote=harmony[(slot/2)%3];
        sum+=tone(0,2.0f*(float)PI*midiHz(answerNote)*slotAge,slotAge)*
             envelope(0,slotAge,leadDur*.96f)*.060f;
    }

    // Half-time impact and dark pedal; a quick secondary pulse keeps the threat moving.
    const int beatNo=(int)floor(beatInBar);
    const float beatPhase=(float)(beatInBar-beatNo);
    const float hitAge=beatPhase*(float)secPerBeat;
    if(beatNo==0||(tr.barBeats>3.5f&&beatNo==2)){
        const float kick=sinf(2.0f*(float)PI*(43.0f*hitAge+3.5f*(1.0f-expf(-hitAge*17.0f))))*expf(-hitAge*17.0f);
        const int pedal=chord.root-24+((beatNo==2)?7:0);
        sum+=kick*.20f;
        sum+=tone(3,2.0f*(float)PI*midiHz(pedal)*hitAge,hitAge)*envelope(3,hitAge,.46f*(float)secPerBeat)*.17f;
        sum+=tone(8,2.0f*(float)PI*midiHz(chord.root+12)*hitAge,hitAge)*envelope(8,hitAge,.68f*(float)secPerBeat)*.055f;
    }
    const int eighth=(int)floor(beatInBar*2.0);
    if((eighth&1)==1){
        const float tickAge=(float)((beatInBar*2.0-eighth)*.5*secPerBeat);
        sum+=noiseHigh*expf(-tickAge*58.0f)*.018f;
    }
    const float phraseRise=.96f+.05f*(float)(bar/3);
    (void)noiseLow;
    return std::max(-.82f,std::min(.82f,sum*phraseRise));
}

// Preview-only four-bar victory adaptation for the game's darker orchestral world.
// Minor themes keep their shadowed i-VI motion, use V7 for a clear arrival, then turn
// the final tonic major (Picardy third). Existing selected-track timbres remain central.
static float renderVictoryCadence(int variant,double seconds,float noiseHigh,float noiseLow) {
    const auto& tracks=allTracks();
    const int index=std::max(0,std::min(9,variant));
    const Track& tr=tracks[index];
    constexpr double bpm=120.0;
    constexpr double beatSeconds=60.0/bpm;
    constexpr double barSeconds=4.0*beatSeconds;
    constexpr double totalSeconds=4.0*barSeconds;
    if(seconds<0.0 || seconds>=totalSeconds+0.70) return 0.0f;

    const int tonic=tr.chords[0].root;
    const bool minor=tr.chords[0].third<4;
    Chord cadence[4];
    if(minor) {
        cadence[0]={tonic,3,7,-1};       // i: retain the world’s darker color
        cadence[1]={tonic+8,4,7,-1};     // VI: broaden the harmony
    } else {
        cadence[0]={tonic,4,7,-1};       // I
        cadence[1]={tonic+5,4,7,-1};     // IV
    }
    cadence[2]={tonic+7,4,7,10};         // V7: unmistakable dominant pull
    cadence[3]={tonic,4,7,-1};           // I: bright Picardy/major resolution

    const double beat=seconds/beatSeconds;
    const int bar=std::min(3,(int)(beat/4.0));
    const double beatInBar=beat-bar*4.0;
    const float barAge=(float)(beatInBar*beatSeconds);
    const Chord& chord=cadence[bar];
    const float swell[4]={0.76f,0.90f,1.03f,1.20f};
    float sum=0.0f;

    // Layered chord roles: selected score's chord instrument, sustained strings,
    // and low choir. The texture grows across the phrase instead of starting at full blast.
    const int chordNotes[4]={chord.root+12,chord.root+12+chord.third,chord.root+12+chord.fifth,
                             chord.root+24+(chord.seventh>=0?chord.seventh:chord.fifth)};
    const int voices=chord.seventh>=0?4:3;
    for(int v=0;v<voices;++v) {
        const float chordEnv=envelope(tr.chordFamily,barAge,(float)(barSeconds*0.94));
        sum+=tone(tr.chordFamily,2.0f*(float)PI*midiHz(chordNotes[v])*barAge,barAge)*chordEnv*0.075f*swell[bar];
        const float stringEnv=envelope(0,barAge,(float)(barSeconds*0.98));
        sum+=tone(0,2.0f*(float)PI*midiHz(chordNotes[v])*barAge,barAge)*stringEnv*0.050f*swell[bar];
        sum+=tone(5,2.0f*(float)PI*midiHz(chordNotes[v]-12)*barAge,barAge)*
             envelope(5,barAge,(float)(barSeconds*0.98))*0.035f*swell[bar];
    }

    // Measured low foundation: one sustained root per bar, reinforced by timpani on 1 and 3.
    const int bassMidi=chord.root-12;
    sum+=tone(tr.bassFamily,2.0f*(float)PI*midiHz(bassMidi)*barAge,barAge)*
         envelope(tr.bassFamily,barAge,(float)(barSeconds*0.78))*0.15f*swell[bar];

    // Theme-shaped fanfare. The minor version rises through its own triad and
    // dominant-seventh tones before resolving to a high, long major tonic.
    struct Note { float start,length; int offset; };
    static const Note minorFanfare[]={{0,1.5f,24},{1.5f,.5f,27},{2,2,31},
                                      {4,1,20},{5,1,24},{6,2,27},
                                      {8,1,19},{9,1,23},{10,1,26},{11,1,29},
                                      {12,4,36}};
    static const Note majorFanfare[]={{0,1,24},{1,1,28},{2,2,31},
                                      {4,1,29},{5,1,31},{6,1,33},{7,1,31},
                                      {8,1,19},{9,1,23},{10,1,26},{11,1,31},
                                      {12,4,36}};
    const Note* notes=minor?minorFanfare:majorFanfare;
    const int noteCount=minor?(int)(sizeof(minorFanfare)/sizeof(minorFanfare[0]))
                             :(int)(sizeof(majorFanfare)/sizeof(majorFanfare[0]));
    const double phraseBeat=seconds/beatSeconds;
    for(int i=0;i<noteCount;++i) {
        const double ageBeats=phraseBeat-notes[i].start;
        if(ageBeats<0.0 || ageBeats>=notes[i].length) continue;
        const float age=(float)(ageBeats*beatSeconds);
        const float dur=notes[i].length*(float)beatSeconds;
        const int pitch=tonic+notes[i].offset;
        sum+=tone(tr.leadFamily,2.0f*(float)PI*midiHz(pitch)*age,age)*envelope(tr.leadFamily,age,dur)*0.22f*swell[bar];
        // A small celesta overtone marks the opening, dominant arrival, and final cadence.
        if((i==0 || i==7 || i==10) && age<0.24f)
            sum+=tone(7,2.0f*(float)PI*midiHz(pitch+12)*age,age)*envelope(7,age,0.24f)*0.052f*swell[bar];
    }

    if(beatInBar<0.18 || (beatInBar>=2.0 && beatInBar<2.18)) {
        const double localBeat=beatInBar<2.0?beatInBar:beatInBar-2.0;
        const float d=(float)(localBeat*beatSeconds);
        const float env=expf(-d*17.0f);
        const float hit=(bar==3 && beatInBar<2.0)?0.23f:0.14f;
        sum+=sinf(2.0f*(float)PI*(49.0f*d+3.2f*(1.0f-expf(-d*17.0f))))*env*hit;
        if(bar==3 && beatInBar<0.18) sum+=noiseHigh*expf(-d*45.0f)*0.045f;
    }

    if(bar==3) {
        // Brass enters only for the decisive major tonic, keeping the game palette intact.
        for(int v=0;v<3;++v) {
            const int pitch=chordNotes[v];
            sum+=tone(8,2.0f*(float)PI*midiHz(pitch)*barAge,barAge)*
                 envelope(8,barAge,(float)barSeconds)*0.045f;
        }
        const float bloom=std::min(1.0f,std::max(0.0f,(float)(beatInBar-2.6)/0.8f));
        sum+=noiseHigh*bloom*0.012f;
    }
    const float tail=seconds>=totalSeconds?expf((float)(-(seconds-totalSeconds)*4.5)):1.0f;
    (void)noiseLow;
    return std::max(-0.92f,std::min(0.92f,sum*tail));
}

static float render(int variant, double seconds, int mode, float noiseHigh, float noiseLow,double bpmOverride=0.0) {
    const auto& t=allTracks();
    const int index=std::max(0,std::min(9,variant));
    const double bpm=bpmOverride>0.0?bpmOverride:rhythmicBpm(index,mode);
    if(mode==2) return renderBossSynchronized(index,seconds,noiseHigh,noiseLow,bpm);
    return renderTrack(t[index],seconds,mode,noiseHigh,noiseLow,bpm);
}
} // namespace MusicComposer
