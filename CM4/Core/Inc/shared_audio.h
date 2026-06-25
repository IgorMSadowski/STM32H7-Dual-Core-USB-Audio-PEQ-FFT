/*
 * shared_audio.h
 *
 *  Created on: May 9, 2026
 *      Author: TK-isadowski
 */

#ifndef INC_SHARED_AUDIO_H_
#define INC_SHARED_AUDIO_H_

#include "stdint.h"
#include "arm_math.h"

#define FFT_SIZE 4096

typedef struct {
    volatile uint8_t data_ready; // 0 = aguardando, 1 = buf0 pronto, 2 = buf1 pronto
    float32_t audio_buffer_0[FFT_SIZE];
    float32_t audio_buffer_1[FFT_SIZE];
} SharedAudioData_t;

// variável externa que será alocada na SRAM4
extern SharedAudioData_t shared_audio_data;

#define SHARED_DATA ((SharedAudioData_t *)0x38000000)

#endif /* INC_SHARED_AUDIO_H_ */
