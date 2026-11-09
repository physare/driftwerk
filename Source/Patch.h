// Module, jack and default-patch definitions shared by the engine and the UI.
#pragma once

namespace dw
{
enum Module
{
    M_CLOCK, M_ARP, M_VCO1, M_VCO2, M_DRONE, M_NOISE, M_LFO1, M_LFO2,
    M_ENV1, M_ENV2, M_MIX, M_VCF1, M_VCF2, M_VCA1, M_VCA2, M_MASTER, M_CAMERA, M_DREAM, M_GRAIN,
    NumModules
};

enum Out
{
    O_CLK, O_CLK4,
    O_ARP_PITCH, O_ARP_GATE, O_ARP_RND,
    O_VCO1_SAW, O_VCO1_PULSE, O_VCO1_SUB,
    O_VCO2_SAW, O_VCO2_PULSE, O_VCO2_SUB,
    O_DRONE,
    O_NOISE, O_SNH,
    O_LFO1, O_LFO2,
    O_ENV1, O_ENV2,
    O_MIX,
    O_VCF1, O_VCF2,
    O_VCA1, O_VCA2,
    O_CAM_X, O_CAM_Y, O_CAM_MOTION, O_CAM_TRIG,
    O_DREAM, O_GRAIN,
    O_CAM_PINCH, O_CAM_SPREAD, O_CAM_FIST, O_CAM_TILT,
    NumOuts
};

enum In
{
    I_ARP_CLK, I_ARP_TRANS,
    I_VCO1_PITCH, I_VCO1_PWM, I_VCO1_FM,
    I_VCO2_PITCH, I_VCO2_PWM, I_VCO2_FM,
    I_DRONE_PITCH,
    I_SNH_CLK,
    I_LFO1_RATE, I_LFO2_RATE,
    I_ENV1_GATE, I_ENV2_GATE,
    I_MIX1, I_MIX2, I_MIX3, I_MIX4,
    I_VCF1_IN, I_VCF1_CV1, I_VCF1_CV2,
    I_VCF2_IN, I_VCF2_CV1, I_VCF2_CV2,
    I_VCA1_IN, I_VCA1_CV,
    I_VCA2_IN, I_VCA2_CV,
    I_OUT1, I_OUT2,
    I_GRAIN_POS, I_GRAIN_PITCH, I_GRAIN_TRIG,
    NumIns
};

struct JackDef { const char* id; const char* label; int module; };

inline const JackDef outDefs[NumOuts] = {
    { "clk.out",   "CLK",   M_CLOCK }, { "clk.div4", "/4",    M_CLOCK },
    { "arp.pitch", "PITCH", M_ARP },   { "arp.gate", "GATE",  M_ARP },   { "arp.rnd", "RND", M_ARP },
    { "vco1.saw",  "SAW",   M_VCO1 },  { "vco1.pls", "PULSE", M_VCO1 },  { "vco1.sub", "SUB", M_VCO1 },
    { "vco2.saw",  "SAW",   M_VCO2 },  { "vco2.pls", "PULSE", M_VCO2 },  { "vco2.sub", "SUB", M_VCO2 },
    { "drn.out",   "OUT",   M_DRONE },
    { "nz.noise",  "NOISE", M_NOISE }, { "nz.snh",   "S&H",   M_NOISE },
    { "lfo1.out",  "OUT",   M_LFO1 },  { "lfo2.out", "OUT",   M_LFO2 },
    { "env1.out",  "OUT",   M_ENV1 },  { "env2.out", "OUT",   M_ENV2 },
    { "mix.out",   "OUT",   M_MIX },
    { "vcf1.out",  "OUT",   M_VCF1 },  { "vcf2.out", "OUT",   M_VCF2 },
    { "vca1.out",  "OUT",   M_VCA1 },  { "vca2.out", "OUT",   M_VCA2 },
    { "cam.x",     "HAND X", M_CAMERA }, { "cam.y",  "HAND Y", M_CAMERA },
    { "cam.mot",   "MOTION", M_CAMERA }, { "cam.trig", "TRIG", M_CAMERA },
    { "dream.out", "OUT",   M_DREAM },  { "grain.out", "OUT",  M_GRAIN },
    { "cam.pinch", "PINCH", M_CAMERA }, { "cam.spread", "SPREAD", M_CAMERA },
    { "cam.fist",  "FIST",  M_CAMERA }, { "cam.tilt",   "TILT",   M_CAMERA },
};

inline const JackDef inDefs[NumIns] = {
    { "arp.clk",   "CLK",   M_ARP },   { "arp.trans", "TRANS", M_ARP },
    { "vco1.pitch","PITCH", M_VCO1 },  { "vco1.pwm",  "PWM",   M_VCO1 }, { "vco1.fm", "FM", M_VCO1 },
    { "vco2.pitch","PITCH", M_VCO2 },  { "vco2.pwm",  "PWM",   M_VCO2 }, { "vco2.fm", "FM", M_VCO2 },
    { "drn.pitch", "PITCH", M_DRONE },
    { "nz.clk",    "CLK",   M_NOISE },
    { "lfo1.rate", "RATE",  M_LFO1 },  { "lfo2.rate", "RATE",  M_LFO2 },
    { "env1.gate", "GATE",  M_ENV1 },  { "env2.gate", "GATE",  M_ENV2 },
    { "mix.in1",   "IN1",   M_MIX },   { "mix.in2",   "IN2",   M_MIX }, { "mix.in3", "IN3", M_MIX }, { "mix.in4", "IN4", M_MIX },
    { "vcf1.in",   "IN",    M_VCF1 },  { "vcf1.cv1",  "CV1",   M_VCF1 }, { "vcf1.cv2", "CV2", M_VCF1 },
    { "vcf2.in",   "IN",    M_VCF2 },  { "vcf2.cv1",  "CV1",   M_VCF2 }, { "vcf2.cv2", "CV2", M_VCF2 },
    { "vca1.in",   "IN",    M_VCA1 },  { "vca1.cv",   "CV",    M_VCA1 },
    { "vca2.in",   "IN",    M_VCA2 },  { "vca2.cv",   "CV",    M_VCA2 },
    { "out.in1",   "IN1",   M_MASTER },{ "out.in2",   "IN2",   M_MASTER },
    { "grain.pos", "POS",   M_GRAIN },  { "grain.pitch", "PITCH", M_GRAIN }, { "grain.trig", "TRIG", M_GRAIN },
};

// The out-of-the-box patch: a self-running random sequence through VCF1/VCA1,
// plus a drifting drone bank swept by a very slow LFO through VCF2.
inline const char* defaultPatch =
    "clk.out>arp.clk;"
    "arp.pitch>vco1.pitch;arp.pitch>vco2.pitch;"
    "vco1.saw>mix.in1;vco2.pls>mix.in2;"
    "mix.out>vcf1.in;"
    "arp.gate>env1.gate;env1.out>vcf1.cv1;env1.out>vca1.cv;"
    "lfo2.out>vcf1.cv2;lfo2.out>vco2.pwm;"
    "vcf1.out>vca1.in;vca1.out>out.in1;"
    "drn.out>vcf2.in;lfo1.out>vcf2.cv1;nz.snh>vcf2.cv2;clk.div4>nz.clk;"
    "vcf2.out>out.in2";

inline int findOut (const char* id) { for (int i = 0; i < NumOuts; ++i) { const char* a = outDefs[i].id; const char* b = id; while (*a && *a == *b) { ++a; ++b; } if (*a == 0 && *b == 0) return i; } return -1; }
inline int findIn  (const char* id) { for (int i = 0; i < NumIns;  ++i) { const char* a = inDefs[i].id;  const char* b = id; while (*a && *a == *b) { ++a; ++b; } if (*a == 0 && *b == 0) return i; } return -1; }

} // namespace dw
