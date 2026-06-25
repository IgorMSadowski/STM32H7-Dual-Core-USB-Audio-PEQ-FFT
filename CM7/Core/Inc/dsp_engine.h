/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : dsp_engine.h
  * @brief          : Header for dsp_engine.c file.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __DSP_ENGINE_H
#define __DSP_ENGINE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include "arm_math.h"

 typedef enum {
     MODE_FLAT = 0,
     MODE_RADIO,
     MODE_BASS_BOOST,
     MODE_MAX         //Fim da lista
 } DSP_Mode;

typedef enum {
    FILTER_PEAKING = 0,
    FILTER_LOW_SHELF,
    FILTER_HIGH_SHELF
} FilterType;

typedef struct {
    float frequency;
    float Q;
    float gain_db;
    FilterType type;
} EQ_BandConfig;

/* Funções Públicas - Interface */
void DSP_Init(uint32_t sample_rate);
void DSP_Process_Sample(float32_t *L, float32_t *R);
void DSP_Set_Mode(DSP_Mode mode);

#ifdef __cplusplus
}
#endif

#endif /* __DSP_ENGINE_H */
