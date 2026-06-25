/*
 * shared_audio.c
 *
 *  Created on: Jun 14, 2026
 *      Author: TK-isadowski
 */

#include "shared_audio.h"

// Define a estrutura compartilhada na SRAM4 (endereço 0x38000000)
__attribute__((section(".sram4")))
__attribute__((aligned(32)))
SharedAudioData_t shared_audio_data;
