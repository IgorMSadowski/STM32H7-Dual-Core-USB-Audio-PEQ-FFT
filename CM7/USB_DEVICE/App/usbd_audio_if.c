/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_audio_if.c
  * @version        : v1.0_Cube
  * @brief          : Generic media access layer.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
 /* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "usbd_audio_if.h"

/* USER CODE BEGIN INCLUDE */
#include "sai.h"
#include "dsp_engine.h"
#include "shared_audio.h"
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
/* Um frame do USB (a cada 1ms) entrega aproximadamente 48 frames de áudio stereo, ou seja 48 frames Left e 48 frames Right
 * 48 * 2 -> Pacotes de áudio
 * 100ms Buffer armazenado no total
 * */
#define SAI_BUFFER_MS 256 // 100ms funciona mas o melhor é com 200-300
#define SAI_BUFFER_SAMPLES (48 * 2 * SAI_BUFFER_MS)

__attribute__((aligned(32)))
int32_t SaiPlaybackBuffer[SAI_BUFFER_SAMPLES];

static uint32_t write_ptr = 0;
static uint32_t packets_received = 0;

/* Variáveis Externas (Definidas no main.c) */
extern uint8_t is_playing;

__attribute__((aligned(32)))
uint8_t audio_feedback_buf[32] = {0x00, 0x00, 0x0C, 0x00};

static uint32_t fback_val = 0x0C0000;
extern SAI_HandleTypeDef hsai_BlockA1;

void SAMPLE_PROCESSING(uint8_t* source, uint32_t size);

#ifndef PI
#define PI 3.14159265358979f
#endif
/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief Usb device library.
  * @{
  */

/** @addtogroup USBD_AUDIO_IF
  * @{
  */

/** @defgroup USBD_AUDIO_IF_Private_TypesDefinitions USBD_AUDIO_IF_Private_TypesDefinitions
  * @brief Private types.
  * @{
  */

/* USER CODE BEGIN PRIVATE_TYPES */

/* USER CODE END PRIVATE_TYPES */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Private_Defines USBD_AUDIO_IF_Private_Defines
  * @brief Private defines.
  * @{
  */

/* USER CODE BEGIN PRIVATE_DEFINES */

/* USER CODE END PRIVATE_DEFINES */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Private_Macros USBD_AUDIO_IF_Private_Macros
  * @brief Private macros.
  * @{
  */

/* USER CODE BEGIN PRIVATE_MACRO */

/* USER CODE END PRIVATE_MACRO */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Private_Variables USBD_AUDIO_IF_Private_Variables
  * @brief Private variables.
  * @{
  */

/* USER CODE BEGIN PRIVATE_VARIABLES */

/* USER CODE END PRIVATE_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Exported_Variables USBD_AUDIO_IF_Exported_Variables
  * @brief Public variables.
  * @{
  */

extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
  * @}
  */

/** @defgroup USBD_AUDIO_IF_Private_FunctionPrototypes USBD_AUDIO_IF_Private_FunctionPrototypes
  * @brief Private functions declaration.
  * @{
  */

static int8_t AUDIO_Init_FS(uint32_t AudioFreq, uint32_t Volume, uint32_t options);
static int8_t AUDIO_DeInit_FS(uint32_t options);
static int8_t AUDIO_AudioCmd_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd);
static int8_t AUDIO_VolumeCtl_FS(uint8_t vol);
static int8_t AUDIO_MuteCtl_FS(uint8_t cmd);
static int8_t AUDIO_PeriodicTC_FS(uint8_t *pbuf, uint32_t size, uint8_t cmd);
static int8_t AUDIO_GetState_FS(void);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */

/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */

/**
  * @}
  */

USBD_AUDIO_ItfTypeDef USBD_AUDIO_fops_FS =
{
  AUDIO_Init_FS,
  AUDIO_DeInit_FS,
  AUDIO_AudioCmd_FS,
  AUDIO_VolumeCtl_FS,
  AUDIO_MuteCtl_FS,
  AUDIO_PeriodicTC_FS,
  AUDIO_GetState_FS,
};

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Initializes the AUDIO media low layer over USB FS IP
  * @param  AudioFreq: Audio frequency used to play the audio stream.
  * @param  Volume: Initial volume level (from 0 (Mute) to 100 (Max))
  * @param  options: Reserved for future use
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_Init_FS(uint32_t AudioFreq, uint32_t Volume, uint32_t options)
{
  /* USER CODE BEGIN 0 */
  DSP_Init(AudioFreq);


  UNUSED(AudioFreq);
  UNUSED(Volume);
  UNUSED(options);
  return (USBD_OK);
  /* USER CODE END 0 */
}

/**
  * @brief  De-Initializes the AUDIO media low layer
  * @param  options: Reserved for future use
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_DeInit_FS(uint32_t options)
{
  /* USER CODE BEGIN 1 */
  UNUSED(options);
  return (USBD_OK);
  /* USER CODE END 1 */
}

/**
  * @brief  Handles AUDIO command.
  * @param  pbuf: Pointer to buffer of data to be sent
  * @param  size: Number of data to be sent (in bytes)
  * @param  cmd: Command opcode
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_AudioCmd_FS(uint8_t* pbuf, uint32_t size, uint8_t cmd)
{
  /* USER CODE BEGIN 2 */
	PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)hUsbDeviceFS.pData;

	  switch(cmd)
	  {
	    case AUDIO_CMD_START:
	        is_playing = 0; write_ptr = 0; packets_received = 0; fback_val = 0x0C0000;
	        hpcd->IN_ep[1].xfer_len = 0; //xfer_len signifca quantos bytes faltam transmitir, no start começa em 0 pq ta vazio
	        USBD_LL_FlushEP(&hUsbDeviceFS, 0x81); // Esvazia o buffer do endpoint
	        break;

	    case AUDIO_CMD_PLAY:
	        hpcd->IN_ep[1].xfer_len = 0;
	        USBD_LL_FlushEP(&hUsbDeviceFS, 0x81);
	        HAL_PCD_EP_Transmit(hpcd, 0x81, audio_feedback_buf, 3); // Inicia uma transmissão nova no Endpoint caso este tenha fechado
	        break;

	    case AUDIO_CMD_STOP:
	        HAL_SAI_DMAStop(&hsai_BlockA1);
	        is_playing = 0;
	        break;
	  }
	  return (USBD_OK);
  /* USER CODE END 2 */
}

/**
  * @brief  Controls AUDIO Volume.
  * @param  vol: volume level (0..100)
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_VolumeCtl_FS(uint8_t vol)
{
  /* USER CODE BEGIN 3 */
  UNUSED(vol);
  return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  Controls AUDIO Mute.
  * @param  cmd: command opcode
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_MuteCtl_FS(uint8_t cmd)
{
  /* USER CODE BEGIN 4 */
  UNUSED(cmd);
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  AUDIO_PeriodicT_FS
  * @param  cmd: Command opcode
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  *
  *
  *
  * Callback de Transfer complete chamada pelo USBD_AUDIO_DataOut(), também chamado a cada 1ms porém o SOF vem sempre primeiro
  * SOF (Start of Frame, é preciso temporalmente, usado para calcular o feedback)
  * -> USB começa a enviar dados -> Ao chegar TC é chamado (Tem os dados prontos, usado para processar as samples)
  *
  */
static int8_t AUDIO_PeriodicTC_FS(uint8_t *pbuf, uint32_t size, uint8_t cmd)
{
  /* USER CODE BEGIN 5 */
	if (cmd == AUDIO_OUT_TC)
	  {
		SAMPLE_PROCESSING(pbuf, size);
	      if (is_playing == 0)
	      {
	    	  /* Metade do tamanho total do buffer SAI_BUFFER_SAMPLES,
	    	   * começa a transmissão depois de 50ms depois do inicio da transmissao USB */
	          if (++packets_received >= (SAI_BUFFER_MS/2))
	          {
	              HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t*)SaiPlaybackBuffer, SAI_BUFFER_SAMPLES);
	              is_playing = 1;
	          }
	      }
	  }
	  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  Gets AUDIO State.
  * @retval USBD_OK if all operations are OK else USBD_FAIL
  */
static int8_t AUDIO_GetState_FS(void)
{
  /* USER CODE BEGIN 6 */
  return (USBD_OK);
  /* USER CODE END 6 */
}

/**
  * @brief  Manages the DMA full transfer complete event.
  * @retval None
  */
void TransferComplete_CallBack_FS(void)
{
  /* USER CODE BEGIN 7 */
  USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_FULL);
  /* USER CODE END 7 */
}

/**
  * @brief  Manages the DMA Half transfer complete event.
  * @retval None
  */
void HalfTransfer_CallBack_FS(void)
{
  /* USER CODE BEGIN 8 */
  USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_HALF);
  /* USER CODE END 8 */
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
void SAMPLE_PROCESSING(uint8_t* source, uint32_t size)
{
    // Em 24-bits (3 bytes), um frame estéreo tem 6 bytes L+R
	// Define quantas samples tem de processar. O Size é o numero de bytes no buffer o source é o buffer de entrada.
    uint32_t num_samples = size / 6;
    static uint32_t sample_count = 0;

    // Guarda o ponteiro inicial para sabermos que zona da memória limpar da cache depois
    uint32_t initial_write_ptr = write_ptr;

    for (uint32_t i = 0; i < num_samples; i++)
    {
        uint8_t *sample = source + (i * 6);

        int32_t left_24  = (sample[0]) | (sample[1] << 8) | (sample[2] << 16);
        int32_t right_24 = (sample[3]) | (sample[4] << 8) | (sample[5] << 16);

        if (left_24 & 0x00800000)  left_24  |= 0xFF000000;
        if (right_24 & 0x00800000) right_24 |= 0xFF000000;

        float32_t sL = (float32_t)left_24 / 8388608.0f;
        float32_t sR = (float32_t)right_24 / 8388608.0f;

        DSP_Process_Sample(&sL, &sR);

/* ZONA FFT */
        static uint8_t current_buf = 0;

		if (current_buf == 0) {
			SHARED_DATA->audio_buffer_0[sample_count] = (sL + sR) * 0.5f;
		} else {
			SHARED_DATA->audio_buffer_1[sample_count] = (sL + sR) * 0.5f;
		}

		sample_count++;
		        if (sample_count >= FFT_SIZE) {
		            if (current_buf == 0) {
		                SHARED_DATA->data_ready = 1;
		            } else {
		                SHARED_DATA->data_ready = 2;
		            }
		            current_buf = !current_buf; // Alterna o buffer
		            sample_count = 0;
		        }
/* FIM ZONA FFT */

        SaiPlaybackBuffer[write_ptr++] = (int32_t)(sL * 2147483647.0f);
        if (write_ptr >= SAI_BUFFER_SAMPLES) write_ptr = 0;
        SaiPlaybackBuffer[write_ptr++] = (int32_t)(sR * 2147483647.0f);
        if (write_ptr >= SAI_BUFFER_SAMPLES) write_ptr = 0;
    }

    if (write_ptr > initial_write_ptr) {
        SCB_CleanDCache_by_Addr((uint32_t*)&SaiPlaybackBuffer[initial_write_ptr], (write_ptr - initial_write_ptr) * sizeof(int32_t));
    } else {
        // Se deu a volta ao buffer (wrap-around)
        SCB_CleanDCache_by_Addr((uint32_t*)&SaiPlaybackBuffer[initial_write_ptr], (SAI_BUFFER_SAMPLES - initial_write_ptr) * sizeof(int32_t));
        if (write_ptr > 0) {
            SCB_CleanDCache_by_Addr((uint32_t*)&SaiPlaybackBuffer[0], write_ptr * sizeof(int32_t));
        }
    }
}




/* Essa função é chamada a cada 1ms no USB Full Speed Start of Frame
 *
 *  o feedback especificado pelo USB para 24bits é do formato 10.14, onde 10 bits é a parte inteira e 14 bits é a parte fracionária.
 *  Valor feedback = Amostras por Frame USB / 2^14, onde Amostras por Frame = SampleRate/1ms
 *
 *	no linux é possível ver o feedback funcionando em tempo real através do comando
 *	cat /proc/asound/cards para descobrir qual a soundcard no pc
 *	e depois watch -n 1 "cat /proc/asound/card3/stream0"
 *
 * */
void USBD_AUDIO_Feedback_SOF(USBD_HandleTypeDef *pdev)
{
	// conta quantos SOFs já ocorreram desde o último envio de feedback
    static uint32_t sof_count = 0;
    // Obtém o ponteiro para a estrutura que os registos e estados dos endpoints
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;

    if (is_playing) {
    	// A cada 16ms calcula o feedback e envia-o
        if (++sof_count >= 16) {
            sof_count = 0;
            uint32_t rem = __HAL_DMA_GET_COUNTER(hsai_BlockA1.hdmatx); //remaining bytes to transmit
            uint32_t read_ptr = SAI_BUFFER_SAMPLES - rem;

            int32_t gap = (int32_t)write_ptr - (int32_t)read_ptr;
            // Se a leitura estiver atrás da escrita
            if (gap < 0) gap += SAI_BUFFER_SAMPLES;
            // O erro é um desvio do valor ideal do gap que é metade do buffer, espaço equidistante dos dois extremos.
            int32_t error = gap - (SAI_BUFFER_SAMPLES / 2);

            if      (error > 1024) fback_val -= 20;
			else if (error < -1024) fback_val += 20;
			else if (error > 128)  fback_val -= 3;
			else if (error < -128) fback_val += 3;
			else if (error > 32)   fback_val -= 1;
			else if (error < -32)  fback_val += 1;

            // Limites para o feedback
            if (fback_val > 0x0C5000) fback_val = 0x0C5000;
            if (fback_val < 0x0BB000) fback_val = 0x0BB000;

            // Passa o valor 24 bits para 3 bytes para o endpoint
            audio_feedback_buf[0] = (uint8_t)(fback_val & 0xFF);
            audio_feedback_buf[1] = (uint8_t)((fback_val >> 8) & 0xFF);
            audio_feedback_buf[2] = (uint8_t)((fback_val >> 16) & 0xFF);
        }
    } else {
    	// 48kHz
        fback_val = 0x0C0000;
        audio_feedback_buf[0] = 0x00; audio_feedback_buf[1] = 0x00; audio_feedback_buf[2] = 0x0C;
    }
    // Transmite o feedback pelo endpoint
    if (hpcd->IN_ep[1].xfer_len == 0) HAL_PCD_EP_Transmit(hpcd, 0x81, audio_feedback_buf, 3);
}

/* Callbacks que conectam o SAI ao USB.
 *
 * No código padrão da ST, o DMA do SAI leria diretamente do buffer USB.
 * Neste projeto, interceptamos os dados, fazemos DSP e colocamos num
 * buffer gigante paralelo (SaiPlaybackBuffer).
 *
 * Portanto, o DMA agora dispara estas interrupções (Half/Full) baseadas
 * no tamanho do SaiPlaybackBuffer
 * */
void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  if(hsai->Instance == SAI1_Block_A)
  {
    // Avisa o USB que a primeira metade do buffer foi lida
    //USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_HALF);
    HalfTransfer_CallBack_FS();
  }
}

void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai)
{
  if(hsai->Instance == SAI1_Block_A)
  {
    // Avisa o USB que a segunda metade do buffer foi lida
    //USBD_AUDIO_Sync(&hUsbDeviceFS, AUDIO_OFFSET_FULL);
    TransferComplete_CallBack_FS();
  }
}

/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */

/**
  * @}
  */
