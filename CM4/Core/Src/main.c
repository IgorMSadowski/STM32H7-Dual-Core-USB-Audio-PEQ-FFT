/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Core CM4 - FFT & UART TX)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdlib.h>
#include <math.h>

#include "arm_math.h"
#include "shared_audio.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DUAL_CORE_BOOT_SYNC_SEQUENCE

#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U)
#endif
#endif
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#define NUM_BANDAS      128
#define ATUALIZACAO_MS  50
#define PI              3.14159265358979f

static float32_t input_buffer[FFT_SIZE];
static float32_t output_buffer[FFT_SIZE];
static arm_rfft_fast_instance_f32 fft_instance;
static float32_t magnitudes[FFT_SIZE/2];
static float32_t bandas_alturas[NUM_BANDAS];

static uint32_t ultimo_uart = 0;

// Matriz estática pré-calculada para poupar a CPU
static float32_t janela_hanning[FFT_SIZE];
// Tabela de Limites Logarítmicos para as Bandas
static uint16_t band_limits[NUM_BANDAS + 1];
// Tabela com os limites fracionários (float) - pré-calculados para poupar CPU
static float32_t band_bins_float[NUM_BANDAS + 1];

const float32_t min_freq = 20.0f;
const float32_t max_freq = 20000.0f;
const float32_t hz_por_bin = 48000.0f / FFT_SIZE; // ~11.72 Hz por bin
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void uart_send_string(const char *str);
void uart_send_float_array(const char *label, float *arr, uint16_t len);
void processar_fft_e_bandas(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void uart_send_string(const char *str) {
    HAL_UART_Transmit(&huart3, (uint8_t*)str, strlen(str), 100);
}

void uart_send_float_array(const char *label, float *arr, uint16_t len) {
    // Envia o rótulo
    HAL_UART_Transmit(&huart3, (uint8_t*)label, strlen(label), 100);
    HAL_UART_Transmit(&huart3, (uint8_t*)": ", 2, 100);

    // Envia cada valor individualmente, sem sprintf
    char num_buf[16];
    for (uint16_t i = 0; i < len; i++) {
    	// As magnitudes estao entre 0 e 1, para o nodeRed enviamos de 0 a 100
        int val = (int)(arr[i] * 100);
        if (val < 0) val = 0;
        if (val > 100) val = 100;
        int len_num = snprintf(num_buf, sizeof(num_buf), "%d ", val);
        HAL_UART_Transmit(&huart3, (uint8_t*)num_buf, len_num, 100);
    }
    // Nova linha
    HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 100);
}


void processar_fft_e_bandas(void) {
    uint8_t ready_flag = SHARED_DATA->data_ready;
    if (ready_flag == 0) return;

    //	// Decide de qual buffer vai ser lido
    float32_t *source_buffer = (ready_flag == 1) ? SHARED_DATA->audio_buffer_0 : SHARED_DATA->audio_buffer_1;

    for (int i = 0; i < FFT_SIZE; i++) {
        float32_t val = source_buffer[i];
        // Verifica se é Not a Number ou Infinity
        if (isnan(val) || isinf(val)) {
            memset(input_buffer, 0, sizeof(input_buffer));
            SHARED_DATA->data_ready = 0;
            return;
        }
        // Copia o buffer aplicando a janela de hanning
        input_buffer[i] = val * janela_hanning[i];
    }

    // Libera o M7
    SHARED_DATA->data_ready = 0;

    // Input dos 4096 em float entre -1 e 1, o output são 2048 números complexos ou seja 2048 * 2 (Real + Imaginario)
    arm_rfft_fast_f32(&fft_instance, input_buffer, output_buffer, 0);
    // Calcula a magnitude a partir dos numeros complexos que sairam do FFT
    arm_cmplx_mag_f32(output_buffer, magnitudes, FFT_SIZE / 2);

    // Zera a componente DC
    magnitudes[0] = 0.0f;

    for (int b = 0; b < NUM_BANDAS; b++) {
        // Busca os limites fracionários já pré-calculados (em vez de recalcular com powf)
        float32_t bin_low = band_bins_float[b];
        float32_t bin_high = band_bins_float[b + 1];

        // Para nunca ler o bin 0 e começar sempre no bin 1
        if (bin_low < 1.0f) bin_low = 1.0f;
        if (bin_high < 1.0f) bin_high = 1.0f;

        float32_t pico = 0.0f;

        // Para as bandas iniciais que tem menos de 1 bin de largura
        // Calculamos o centro do bin
        if ((bin_high - bin_low) <= 1.0f) {
        	// Centro da banda, entre os bins
            float32_t center = (bin_low + bin_high) / 2.0f;
            // Só a parte inteira do centro
            uint16_t idx = (uint16_t)center;

            // Garante que nunca acedemos a magnitudes[idx+1] fora dos limites, -2 pq tem que ter em conta o centro + 1
            if (idx > (FFT_SIZE / 2) - 2) idx = (FFT_SIZE / 2) - 2;

            float32_t frac = center - idx;

            /*
             * Como a banda é mais estreita que 1 bin, o seu centro cai entre dois bins.
             * Calculamos o valor da magnitude no centro fazendo uma média ponderada das magnitudes desses dois bins,
             * onde o peso é inversamente proporcional à distância ao centro
             * */
            pico = (magnitudes[idx] * (1.0f - frac)) + (magnitudes[idx + 1] * frac);
        } else {
            // Para as bandas finais onde cada barra tem dezenas de bins a amplitude vai ser a maxima encontrada
        	uint16_t start_idx = (uint16_t)bin_low;
            uint16_t end_idx = (uint16_t)bin_high;

            // Garante que o end_idx não ultrapassa o último bin disponível
            if (end_idx > (FFT_SIZE / 2) - 1) end_idx = (FFT_SIZE / 2) - 1;

            for (uint16_t i = start_idx; i < end_idx; i++) {
                if (magnitudes[i] > pico) pico = magnitudes[i];
            }
        }

        // Converte a amplitude para dB
        float32_t db = 20.0f * log10f(pico + 0.001f);

        // Adiciona um ganho extra que aumenta progressivamente com o índice da banda
		// So para conseguir ver direito as bandas de maior frequência
		// Não muito preciso mas fica mais bonito
        db += b * 0.20f;

        /* Qualquer ruido abaixo de -20dB é ignorado porque fica abaixo do ponto inicial para as barras
         * Se o valor do pico em dB for 60 a norm vai ser igual a 1 (Máximo)
         * Tem uma range de 80dB
         *  */
        float32_t norm = (db - (-20.0f)) / 80.0f;

        // Garante que norm é de 0 a 1
        if (isnan(norm) || isinf(norm) || norm < 0.0f) norm = 0.0f;
        else if (norm > 1.0f) norm = 1.0f;

        // Afina os picos das ondas para não ficar tudo parecendo um cogumelo
        // Simplesmente eleva a norm por 1.4. Para valores mais perto de 1 nao muda quase nada, para baixos diminui um pouco
        norm = powf(norm, 1.4f);

        // Peak Hold, depois na main é que fazemos o delay diminuindo o tamanho com o tempo
        if (norm > bandas_alturas[b]) {
            bandas_alturas[b] = norm;
        }
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
#if defined(DUAL_CORE_BOOT_SYNC_SEQUENCE)
  __HAL_RCC_HSEM_CLK_ENABLE();
  HAL_HSEM_ActivateNotification(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
  HAL_PWREx_ClearPendingEvent();
  HAL_PWREx_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFE, PWR_D2_DOMAIN);
  __HAL_HSEM_CLEAR_FLAG(__HAL_HSEM_SEMID_TO_MASK(HSEM_ID_0));
#endif
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_DMA_Init();
  MX_GPIO_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

  // Espera que as operações de dados, leitura/escrita em RAM/periféricos terminem
  __DSB();

  uart_send_string("Boot do M4 (FFT Core) OK\r\n");

  // Pré calculo dos coeficientes da janela de Hanning
  for (int i = 0; i < FFT_SIZE; i++) {
      janela_hanning[i] = 0.5f * (1.0f - arm_cos_f32(2.0f * PI * i / (FFT_SIZE - 1)));
  }

  /* A FFT produz bins igualmente espaçados em frequência (resolução linear),
   * mas a percepção auditiva humana é logarítmica. Para criar um visualizador
   * com barras de aspecto musical, dividimos o espectro em bandas cujas
   * frequências de corte crescem em progressão geométrica.
   *
   * A fórmula abaixo gera, para cada banda i (0 a NUM_BANDAS), a frequência
   * de corte inferior (freq_low) e superior (freq_high), mas aqui calculamos
   * apenas os limites inferiores de cada banda (band_limits[i]) – o limite
   * superior da banda i é o limite inferior da banda i+1.
   *
   *   freq(i) = min_freq * (max_freq / min_freq)^(i / NUM_BANDAS)
   *
   * Convertemos cada frequência para o índice do bin correspondente na FFT:
   *   bin = (uint16_t)(freq / hz_por_bin)
   * com hz_por_bin = 48000 / FFT_SIZE (ex: ~11.72 Hz para FFT_SIZE=4096).
   *
   * O array band_limits[0..NUM_BANDAS] conterá, para cada banda b, o índice
   * do primeiro bin que pertence a essa banda. A banda b cobre os bins desde
   * band_limits[b] até band_limits[b+1] - 1.
   * ------------------------------------------------------------------------- */
  for (int i = 0; i <= NUM_BANDAS; i++) {
	  float32_t freq = min_freq * powf(max_freq / min_freq, (float32_t)i / NUM_BANDAS);
	  float32_t bin_float = freq / hz_por_bin;          // Guarda o valor exato (fracionário)
	  band_bins_float[i] = bin_float;                  // Para a interpolação nos graves

	  uint16_t bin = (uint16_t)bin_float;              // Só a parte inteira
	  if (bin > (FFT_SIZE / 2) - 1) bin = (FFT_SIZE / 2) - 1;

	  band_limits[i] = bin;
  }


  arm_status status = arm_rfft_fast_init_f32(&fft_instance, FFT_SIZE);
  if (status != ARM_MATH_SUCCESS) {
      uart_send_string("ERRO: Falha ao inicializar o modulo FFT!\r\n");
  } else {
      uart_send_string("FFT Inicializada com sucesso!\r\n");
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
      uint32_t agora = HAL_GetTick();

      processar_fft_e_bandas();

      // Envia para a UART a cada 50 ms e aplica um efeito onde as bandas descaem com o tempo
      if (agora - ultimo_uart >= ATUALIZACAO_MS) {
          ultimo_uart = agora;

          uart_send_float_array("B", bandas_alturas, NUM_BANDAS);

          // Efeito de decay, faz as barras caírem com o tempo
          for (int b = 0; b < NUM_BANDAS; b++) {
              bandas_alturas[b] *= 0.85f;
              if (bandas_alturas[b] < 0.01f) bandas_alturas[b] = 0.0f; // zera de vez
          }
      }
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
