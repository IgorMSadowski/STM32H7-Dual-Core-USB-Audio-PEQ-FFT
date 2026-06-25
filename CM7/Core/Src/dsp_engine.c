#include "dsp_engine.h"
#include <math.h>
#include <string.h>

#define EQ_BANDS 10
#ifndef PI
#define PI 3.14159265358979f
#endif

/* Estruturas do CMSIS-DSP */
/* Guarda secções internas, coeficientes e pointers para os calculos em cascata dos filtros */
static arm_biquad_casd_df1_inst_f32 EQ_L, EQ_R;
/* coeficientes para os cálculos dos filtros para cada banda precisa ter 5 coeficientes, 10 bandas -> 50 coeffs*/
static float32_t coeffs_L[EQ_BANDS * 5], coeffs_R[EQ_BANDS * 5];
/* cada banda precisa de 4 variaveis de estado, 2 entradas e 2 saidas anteriores por banda, para o calculo */
static float32_t state_L[EQ_BANDS * 4], state_R[EQ_BANDS * 4];

/* Variáveis internas */
static DSP_Mode current_mode = MODE_FLAT;
static uint32_t dsp_sample_rate = 48000;

/* --- DEFINIÇÃO DOS PERFIS (DATABASE) --- */

// Perfil 0: FLAT
const EQ_BandConfig profile_flat[EQ_BANDS] = {
    {31, 1.41, 0, FILTER_LOW_SHELF}, {62, 1.41, 0, FILTER_PEAKING},
    {125, 1.41, 0, FILTER_PEAKING},  {250, 1.41, 0, FILTER_PEAKING},
    {500, 1.41, 0, FILTER_PEAKING},  {1000, 1.41, 0, FILTER_PEAKING},
    {2000, 1.41, 0, FILTER_PEAKING}, {4000, 1.41, 0, FILTER_PEAKING},
    {8000, 1.41, 0, FILTER_PEAKING}, {16000, 1.41, 0, FILTER_HIGH_SHELF}
};

// Perfil 1: RADIO ANTIGO
const EQ_BandConfig profile_radio[EQ_BANDS] = {
    {100, 0.70, -20.0, FILTER_LOW_SHELF}, {250, 1.0, -10.0, FILTER_PEAKING},
    {500, 1.0, -5.0, FILTER_PEAKING},     {1000, 2.0, 8.0, FILTER_PEAKING},
    {2000, 2.0, 6.0, FILTER_PEAKING},      {4000, 1.0, 0.0, FILTER_PEAKING},
    {6000, 1.0, -10.0, FILTER_PEAKING},    {8000, 1.0, -15.0, FILTER_PEAKING},
    {12000, 1.0, -20.0, FILTER_PEAKING},   {16000, 0.70, -25.0, FILTER_HIGH_SHELF}
};

// Perfil 2: BASS BOOST
const EQ_BandConfig profile_bass[EQ_BANDS] = {
    {31,  0.70,  8.0, FILTER_LOW_SHELF},
    {62,  1.00,  6.0, FILTER_PEAKING},
    {125, 1.20,  3.0, FILTER_PEAKING},
    {250, 1.00,  0.0, FILTER_PEAKING},
    {500, 1.00, -2.0, FILTER_PEAKING},
    {1000, 1.00, 0.0, FILTER_PEAKING},
    {2000, 1.00, 0.0, FILTER_PEAKING},
    {4000, 1.00, 2.0, FILTER_PEAKING},
    {8000, 1.00, 4.0, FILTER_PEAKING},
    {16000, 0.70, 6.0, FILTER_HIGH_SHELF}
};

/* Array que guarda ponteiros para os perfis do EQ,
*  melhora a escalabilidade do projeto evitando ter de usar um switch case no set mode
*  */
const EQ_BandConfig* const profiles_table[] = {
    profile_flat,   // MODE_FLAT (0)
    profile_radio,  // MODE_RADIO (1)
    profile_bass    // MODE_BASS_BOOST (2)
};

/* --- FUNÇÕES --- */
/* https://webaudio.github.io/Audio-EQ-Cookbook/Audio-EQ-Cookbook.txt */
/* Calcula os coeficientes para os filtros */
static void CalcBiquad(float freq, float Q, float gainDB, FilterType type, float* coeffs, uint32_t sr) {
    float A = powf(10.0f, gainDB / 40.0f);
    float omega = 2.0f * PI * freq / sr;
    float sn = sinf(omega), cs = cosf(omega), alpha = sn / (2.0f * Q);
    float b0, b1, b2, a0, a1, a2;

    if (type == FILTER_LOW_SHELF) {
        b0 = A*((A+1)-(A-1)*cs+2*sqrtf(A)*alpha); b1 = 2*A*((A-1)-(A+1)*cs); b2 = A*((A+1)-(A-1)*cs-2*sqrtf(A)*alpha);
        a0 = (A+1)+(A-1)*cs+2*sqrtf(A)*alpha; a1 = -2*((A-1)+(A+1)*cs); a2 = (A+1)+(A-1)*cs-2*sqrtf(A)*alpha;
    } else if (type == FILTER_HIGH_SHELF) {
        b0 = A*((A+1)+(A-1)*cs+2*sqrtf(A)*alpha); b1 = -2*A*((A-1)+(A+1)*cs); b2 = A*((A+1)-(A-1)*cs-2*sqrtf(A)*alpha);
        a0 = (A+1)-(A-1)*cs+2*sqrtf(A)*alpha; a1 = 2*((A-1)-(A+1)*cs); a2 = (A+1)-(A-1)*cs-2*sqrtf(A)*alpha;
    } else if (type == FILTER_PEAKING){
        b0 = 1 + alpha*A; b1 = -2*cs; b2 = 1 - alpha*A; a0 = 1 + alpha/A; a1 = -2*cs; a2 = 1 - alpha/A;
    }
    coeffs[0] = b0/a0; coeffs[1] = b1/a0; coeffs[2] = b2/a0; coeffs[3] = -a1/a0; coeffs[4] = -a2/a0;
}

void DSP_Init(uint32_t sample_rate) {
    dsp_sample_rate = sample_rate;
    DSP_Set_Mode(MODE_FLAT);
}

/* Altera o perfil do equalizador, chama a função para calcular os coeficientes dos filtros necessários e guarda nas suas variaveis  */
void DSP_Set_Mode(DSP_Mode mode) {
	/* Tamanho total do array / Tamanho de um unico perfil (Tudo em bytes) */
    if (mode >= (sizeof(profiles_table)/sizeof(profiles_table[0]))) {
        mode = MODE_FLAT;
    }

    current_mode = mode;
    /* Pointer para o perfil do EQ atual, para desreferenciar pode usar p[i] ou *p
     * mas para a segunda banda seria preciso ter *(p+1), ou seja mais trabalho por nada
     * */
    const EQ_BandConfig *p = profiles_table[mode];

    for (int i = 0; i < EQ_BANDS; i++) {
        CalcBiquad(p[i].frequency, p[i].Q, p[i].gain_db, p[i].type, &coeffs_L[i * 5], dsp_sample_rate);
        // Os coeficientes são iguais para os dois canais por isso podemos só copiar os coeficientes
        memcpy(&coeffs_R[i * 5], &coeffs_L[i * 5], 5 * sizeof(float32_t));
    }

    // Prepara as estruturas dos filtros para processarem os dados
    arm_biquad_cascade_df1_init_f32(&EQ_L, EQ_BANDS, coeffs_L, state_L);
    arm_biquad_cascade_df1_init_f32(&EQ_R, EQ_BANDS, coeffs_R, state_R);

    // Zera o histórico para evitar problemas na troca de perfil
    memset(state_L, 0, sizeof(state_L));
    memset(state_R, 0, sizeof(state_R));
}

void DSP_Process_Sample(float32_t *L, float32_t *R) {
	/* Substitue o sample original pelo processado */
    arm_biquad_cascade_df1_f32(&EQ_L, L, L, 1);
    arm_biquad_cascade_df1_f32(&EQ_R, R, R, 1);

    // Limita para evitar o clipping digital no DAC
    if (*L > 0.99f) *L = 0.99f; else if (*L < -0.99f) *L = -0.99f;
    if (*R > 0.99f) *R = 0.99f; else if (*R < -0.99f) *R = -0.99f;

    // Headroom para o equalizador
    *L *= 0.80f;
    *R *= 0.80f;
}
