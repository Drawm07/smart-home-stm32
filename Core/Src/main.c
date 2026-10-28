/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <string.h>    // pour strlen()
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum { HOME, AWAY } Mode_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SEUIL_GAZ_ON   1250   // au-dessus, on déclenche l'alarme gaz
#define SEUIL_GAZ_OFF  1050   // en dessous, l'alarme s'arrête

#define SEUIL_PLUIE  2000   // mesuré : environ 100 à sec, plus de 3600 mouillé
#define POS_OUVERTE  1500   // impulsion de 1,5 ms : fenêtre ouverte (trouvée en debug)
#define POS_FERMEE    500   // impulsion de 0,5 ms : fenêtre fermée

#define TMP102_ADR  (0x48 << 1)   // ADD0 est à la masse donc adresse 0x48, décalée d'un bit pour la HAL
#define REG_TEMP    0x00          // registre de température
#define REG_CONFIG  0x01          // registre de configuration

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

volatile Mode_t   mode = HOME;          // mode actuel de la maison
volatile uint8_t  boutonAppuye = 0;     // passe à 1 quand on appuie sur B1
volatile uint32_t dernierAppui = 0;     // instant du dernier appui, pour l'anti-rebond
volatile uint8_t mouvement = 0;   // passe à 1 quand le PIR détecte quelqu'un
volatile uint8_t alarme = 0;      // alarme intrusion en cours

volatile uint8_t mesureGaz = 0;   // TIM11 le met à 1 toutes les 3 s
uint16_t valeurGaz = 0;           // valeur brute de l'ADC, de 0 à 4095
volatile uint8_t alarmeGaz = 0;   // alarme gaz en cours

volatile uint8_t boutonFenetre = 0;     // passe à 1 quand on appuie sur le bouton fenêtre
volatile uint32_t dernierAppuiFen = 0;  // même anti-rebond que pour B1
uint16_t valeurPluie = 0;               // valeur brute du capteur de pluie
uint8_t pluie = 0;                      // 1 tant qu'il pleut
uint8_t fenetreOuverte = 0;             // état actuel de la fenêtre

float temperature = 0;            // dernière température lue, en degrés

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void envoyer(char *texte)       // envoie un texte au PC par l'UART2
{
	HAL_UART_Transmit(&huart2, (uint8_t*)texte, strlen(texte), 100);
}
/* ouvre la fenêtre : on change juste la largeur d'impulsion du servo */
void ouvrirFenetre(void)
{
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, POS_OUVERTE); // écrit la valeur dans CCR1 de TIM4
	fenetreOuverte = 1;
	envoyer("WINDOW OPEN\r\n");
}
/* ferme la fenêtre */
void fermerFenetre(void)
{
	__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, POS_FERMEE);
	fenetreOuverte = 0;
	envoyer("WINDOW CLOSED\r\n");
}

/* Driver du TMP102, écrit à partir de la datasheet TI (SBOS397) */

void tmp102_init(void)            // met le capteur en veille (bit SD à 1) : 0x60A0 devient 0x61A0
{
	uint8_t trame[3] = {REG_CONFIG, 0x61, 0xA0};
	HAL_I2C_Master_Transmit(&hi2c1, TMP102_ADR, trame, 3, 100);
}

void tmp102_mesurer(void)         // demande une seule mesure (bit OS à 1) : 0xE1A0
{
	uint8_t trame[3] = {REG_CONFIG, 0xE1, 0xA0};
	HAL_I2C_Master_Transmit(&hi2c1, TMP102_ADR, trame, 3, 100);
}

int16_t tmp102_lire(void)         // lit les 2 octets du registre de température
{
	uint8_t pointeur = REG_TEMP;
	uint8_t octets[2] = {0, 0};
	HAL_I2C_Master_Transmit(&hi2c1, TMP102_ADR, &pointeur, 1, 100);  // on pointe d'abord sur le registre de température
	HAL_I2C_Master_Receive(&hi2c1, TMP102_ADR, octets, 2, 100);      // puis on lit l'octet de poids fort et celui de poids faible
	return (int16_t)((octets[0] << 8) | octets[1]) >> 4;             // on garde les 12 bits utiles, le signe est conservé
}

float tmp102_convertir(int16_t brut)
{
	return brut * 0.0625f;          // 1 bit = 0,0625 °C ; le f évite un calcul en double
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

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART2_UART_Init();
	MX_TIM11_Init();
	MX_TIM3_Init();
	MX_TIM10_Init();
	MX_ADC1_Init();
	MX_TIM4_Init();
	MX_I2C1_Init();
	/* USER CODE BEGIN 2 */

	// TIM11 : une interruption toutes les 3 s pour lancer les mesures
	// (PSC 9999 donne 10 kHz, ARR 29999 donne 30 000 tops, soit 3 s)
	HAL_TIM_Base_Start_IT(&htim11);
	HAL_TIM_Base_Start_IT(&htim10);   // TIM10 : rythme du clignotement de la LED, toutes les 250 ms

	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);   // la PWM du servo tourne en continu
	fermerFenetre();   // on démarre toujours fenêtre fermée

	if (HAL_I2C_IsDeviceReady(&hi2c1, TMP102_ADR, 3, 100) == HAL_OK)   // on vérifie que le TMP102 répond
		envoyer("TMP102 OK\r\n");
	else
		envoyer("TMP102 absent !\r\n");
	tmp102_init();
	tmp102_mesurer();                 // première mesure, lue au premier passage de TIM11

	envoyer("\r\n=== Smart Home STM32 - Bienvenue ===\r\n");
	envoyer("Mode HOME\r\n");

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
		if (boutonAppuye == 1)
		{
			boutonAppuye = 0;

			if (mode == HOME)
			{
				mode = AWAY;
				envoyer("Mode AWAY\r\n");
				if (fenetreOuverte == 1) fermerFenetre();   // en AWAY, la fenêtre doit rester fermée
			}
			else
			{
				mode = HOME;
				alarme = 0;      // le retour en HOME coupe l'alarme intrusion
				if (alarmeGaz == 0)   // buzzer et LED sont communs : on ne les coupe que s'il n'y a pas d'alarme gaz
				{
					HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
					HAL_GPIO_WritePin(LED_ALARME_GPIO_Port, LED_ALARME_Pin, GPIO_PIN_RESET);
				}

				envoyer("Mode HOME\r\n");
			}
		}

		if (mouvement == 1)   // le PIR a détecté quelqu'un
		{
			mouvement = 0;

			if (mode == AWAY && alarme == 0)   // l'alarme ne part qu'en AWAY, et une seule fois
			{
				alarme = 1;
				HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);   // le buzzer se met à sonner
				envoyer("INTRUSION\r\n");
			}
		}

		if (boutonFenetre == 1)   // bloqué en AWAY ou s'il pleut, sinon il ouvre ou ferme
		{
			boutonFenetre = 0;
			if (mode == AWAY || pluie == 1)
				envoyer("Fenetre bloquee\r\n");
			else if (fenetreOuverte == 1)
				fermerFenetre();
			else
				ouvrirFenetre();
		}


		if (mesureGaz == 1)   // toutes les 3 s : gaz, pluie et température
		{
			mesureGaz = 0;

			HAL_ADC_Start(&hadc1);
			HAL_ADC_PollForConversion(&hadc1, 10);
			valeurGaz = HAL_ADC_GetValue(&hadc1);     // rang 1 : capteur de gaz
			HAL_ADC_PollForConversion(&hadc1, 10);
			valeurPluie = HAL_ADC_GetValue(&hadc1);   // rang 2 : capteur de pluie
			HAL_ADC_Stop(&hadc1);
			char texte[50];   // texte envoyé sur l'UART
			sprintf(texte, "Gaz = %u  Pluie = %u\r\n", valeurGaz, valeurPluie);
			envoyer(texte);

			if (valeurPluie > SEUIL_PLUIE && pluie == 0)   // il commence à pleuvoir : on ferme
			{
				pluie = 1;
				envoyer("RAIN\r\n");
				if (fenetreOuverte == 1) fermerFenetre();
			}
			else if (valeurPluie < SEUIL_PLUIE && pluie == 1)   // fin de la pluie : on débloque sans rouvrir
			{
				pluie = 0;
				envoyer("Plus de pluie\r\n");
			}

			temperature = tmp102_convertir(tmp102_lire());  // on lit la mesure lancée il y a 3 s
			tmp102_mesurer();                               // et on relance la suivante
			int t10 = (int)(temperature * 10);              // sprintf ne gère pas %f : 23,5 °C devient 235
			sprintf(texte, "Temp = %d.%d C\r\n", t10 / 10, t10 % 10);
			envoyer(texte);

			if (valeurGaz > SEUIL_GAZ_ON && alarmeGaz == 0)   // surveillé dans les deux modes
			{
				alarmeGaz = 1;
				HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);   // même buzzer pour les deux alarmes
				envoyer("GAS ALERT\r\n");
			}
			else if (valeurGaz < SEUIL_GAZ_OFF && alarmeGaz == 1)
			{
				alarmeGaz = 0;
				if (alarme == 0)   // plus aucune alarme : on coupe tout
				{
					HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
					HAL_GPIO_WritePin(LED_ALARME_GPIO_Port, LED_ALARME_Pin, GPIO_PIN_RESET);
				}
				envoyer("Gaz normal\r\n");
			}
		}

	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 100;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
	{
		Error_Handler();
	}
}

/* USER CODE BEGIN 4 */

// Appelée par la HAL à chaque débordement de timer. TIM10 et TIM11 passent
// tous les deux par ici, donc on regarde lequel a débordé.
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM11)
	{
		mesureGaz = 1;                // on lève juste le drapeau, la mesure se fait dans la boucle
	}
	else if (htim->Instance == TIM10)
	{
		if (alarme == 1 || alarmeGaz == 1)   // la LED ne clignote que si une alarme est active
			HAL_GPIO_TogglePin(LED_ALARME_GPIO_Port, LED_ALARME_Pin);   // une seule LED pour les deux alarmes
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)   // GPIO_Pin indique quelle broche a déclenché
{
	if (GPIO_Pin == B1_Pin)
	{
		if (HAL_GetTick() - dernierAppui > 200)   // anti-rebond : on ignore un appui à moins de 200 ms du précédent
		{
			dernierAppui = HAL_GetTick();
			boutonAppuye = 1;
		}
	}

	else if (GPIO_Pin == PIR_Pin)
	{
		mouvement = 1;   // on signale le mouvement, le traitement se fait dans la boucle
	}

	else if (GPIO_Pin == BOUTON_FEN_Pin)
	{
		if (HAL_GetTick() - dernierAppuiFen > 200)   // même anti-rebond que pour B1
		{
			dernierAppuiFen = HAL_GetTick();
			boutonFenetre = 1;
		}
	}
}

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
