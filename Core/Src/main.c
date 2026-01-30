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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
enum BOOTLOADER_STATE {
	IDLE, // ACCEPTING COMMANDS, PUBLISHING APPLICATION INFO
	DOWNLOADING
};



/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
//#define FLASH_TARGET_ADDRESS (0x0801F800U) /* Last 2KB page of 128KB flash */

// FLASH MEMORY LAYOUT

// PAGE 0:
// 0x08000000: BOOTLOADER
// ...
// PAGE 5:
// 0x08002800: SIZE
// 0x08002804: CRC32
// 0x08002808: FLASH_APPLICATION_START
// ...
// PAGE 63

// Application firmware start address
//#define FLASH_APPLICATION_SIZE (0x08003000)
//#define FLASH_APPLICATION_CRC32 (0x08002804)
#define FLASH_APPLICATION_START (0x08003000)

// These pages will be erased before programming
#define FLASH_APPLICATION_FIRST_PAGE (6)
#define FLASH_APPLICATION_LAST_PAGE (63)



#define CAN_RX_ID 0x606 // SDO receive Node 6
#define CAN_TX_ID 0x506 // SDO respond Node 6

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;

FDCAN_HandleTypeDef hfdcan1;

/* USER CODE BEGIN PV */

// CAN send and receive data
FDCAN_RxHeaderTypeDef RxHeader;
uint8_t RxData[64];

FDCAN_TxHeaderTypeDef TxHeader;
uint8_t TxData[64];

uint8_t flash_buffer[FLASH_PAGE_SIZE]; // 2K page
uint32_t flash_buffer_position = 0;
uint32_t flash_current_page_index = 0;
uint32_t flash_total_bytes_written = 0;
uint32_t application_size = 0;
uint32_t application_crc32 = 0;

typedef  void (*pFunction)(void);
volatile pFunction JumpToApplication;
volatile uint32_t JumpAddress;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_FDCAN1_Init(void);
static void MX_CRC_Init(void);
/* USER CODE BEGIN PFP */

// Place FLASH functions into SRAM to avoid stalling. See STM32G431KBUX_FLASH.ld for section definitions.
int WriteBufferToFlash(void) __attribute__((section(".RamFunc")));
int EraseApplicationFlash(void) __attribute__((section(".RamFunc")));
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*  */

//void PushWord(const uint32_t* x)
//{
//    flash_buffer[flash_buffer_position] = *x;
//    flash_buffer_position = (flash_buffer_position + 1) & (FLASH_BUFFER_SIZE - 1); // This only works if size is a power of 2
//}

int CheckApplicationCRC32(uint32_t expected_crc32) {
	uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)FLASH_APPLICATION_START, application_size);
	crc ^= 0xFFFFFFFF;
	return expected_crc32 == crc;
}

int EraseApplicationFlash() {
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	/* Wait for any ongoing operation */
	while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}

	/* Unlock FLASH if needed */
	if ((FLASH->CR & FLASH_CR_LOCK) != 0U)
	{
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
	}

	// Check and clear all error programming flags due to a previous programming. If not, PGSERR is set.
	FLASH->SR = FLASH_SR_EOP | FLASH_FLAG_SR_ERRORS;

	for (uint32_t page = FLASH_APPLICATION_FIRST_PAGE; page <= FLASH_APPLICATION_LAST_PAGE; page++ ) {
		/* Erase the page containing the target address */
		//uint32_t page = (FLASH_APPLICATION_START / FLASH_PAGE_SIZE) + flash_current_page_index;
		FLASH->CR &= ~FLASH_CR_PNB;

		// Set the PER (PAGE ERASE) bit and select the page to erase (PNB)
		FLASH->CR |= FLASH_CR_PER | (page << FLASH_CR_PNB_Pos);

		// Set the STRT bit in the FLASH_CR register
		FLASH->CR |= FLASH_CR_STRT;
		/* Wait for any ongoing operation */
		while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}
	}


	/* Lock FLASH */
	FLASH->CR |= FLASH_CR_LOCK;

	if (primask == 0U)
	{
		__enable_irq();
	}

	// Reset counters
	flash_current_page_index = 0;
	flash_total_bytes_written = 0;
	flash_buffer_position = 0;

	return (FLASH->SR & FLASH_FLAG_SR_ERRORS) == 0;
}

/* RAM function to write page buffer to flash memory */
int WriteBufferToFlash(void) {
  static volatile uint64_t flash_payload = 0xFFFFFFFFFFFFFFFFULL;

  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  /* Wait for any ongoing operation */
  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}

  /* Unlock FLASH if needed */
  if ((FLASH->CR & FLASH_CR_LOCK) != 0U)
  {
    FLASH->KEYR = FLASH_KEY1;
    FLASH->KEYR = FLASH_KEY2;
  }

  // Check and clear all error programming flags due to a previous programming. If not, PGSERR is set.
  FLASH->SR = FLASH_SR_EOP | FLASH_FLAG_SR_ERRORS;

  // Wait
  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}

  // Turn off PER bit
  FLASH->CR &= ~FLASH_CR_PER;

  // Send page buffer to flash
  /* Program one 64-bit double-word */
  FLASH->CR |= FLASH_CR_PG;
  for(int i = 0; i < FLASH_PAGE_SIZE / 8; i++)
  {
	  flash_payload = ((uint64_t*)flash_buffer)[i];
	  *(__IO uint64_t *)(FLASH_APPLICATION_START + flash_current_page_index*FLASH_PAGE_SIZE + i*8) = (uint64_t)flash_payload;
  }
  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}
  FLASH->CR &= ~FLASH_CR_PG;

  // Advance page index
  flash_current_page_index++;

  /* Lock FLASH */
  FLASH->CR |= FLASH_CR_LOCK;

  if (primask == 0U)
  {
    __enable_irq();
  }

  return (FLASH->SR & FLASH_FLAG_SR_ERRORS) == 0;
}

//int WriteApplicationInfoToFlash(void) {
//  static volatile uint64_t flash_payload = 0xFFFFFFFFFFFFFFFFULL;
//
//  uint32_t primask = __get_PRIMASK();
//  __disable_irq();
//
//  /* Wait for any ongoing operation */
//  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}
//
//  /* Unlock FLASH if needed */
//  if ((FLASH->CR & FLASH_CR_LOCK) != 0U)
//  {
//    FLASH->KEYR = FLASH_KEY1;
//    FLASH->KEYR = FLASH_KEY2;
//  }
//
//  // Check and clear all error programming flags due to a previous programming. If not, PGSERR is set.
//  FLASH->SR = FLASH_SR_EOP | FLASH_FLAG_SR_ERRORS;
//
//  // Wait
//  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}
//
//  // Turn off PER bit
//  FLASH->CR &= ~FLASH_CR_PER;
//
//  // Send page buffer to flash
//  /* Program one 64-bit double-word */
//  FLASH->CR |= FLASH_CR_PG;
//
//  flash_payload = application_size;
//  flash_payload |= application_crc32 << 32;
//  *(__IO uint64_t *)(FLASH_APPLICATION_SIZE) = (uint64_t)flash_payload;
//
//  while ((FLASH->SR & FLASH_SR_BSY) != 0U) {}
//  FLASH->CR &= ~FLASH_CR_PG;
//
//  // Advance page index
//  flash_current_page_index++;
//
//  /* Lock FLASH */
//  FLASH->CR |= FLASH_CR_LOCK;
//
//  if (primask == 0U)
//  {
//    __enable_irq();
//  }
//
//  return (FLASH->SR & FLASH_FLAG_SR_ERRORS) == 0;
//}

void LaunchApplication(){
//	/* execute the new program */
//	JumpAddress = *(__IO uint32_t*) (FLASH_APPLICATION_START + 4);
//	/* Jump to user application */
//	JumpToApplication = (pFunction) JumpAddress;
//	/* Initialize user application's Stack Pointer */
//	__set_MSP(*(__IO uint32_t*) FLASH_APPLICATION_START);
//	JumpToApplication(); // address needs to have LSB set to 1 for BLX instruction to work. Actual jump address has LSB masked.
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
  MX_FDCAN1_Init();
  MX_CRC_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  //WriteBufferToFlash();

//  int i = 5;
//  while (i-- > 0)
//  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
//  }
//

//  uint8_t a[4] = {10,10,10,10};
//  uint32_t crc = HAL_CRC_Calculate(&hcrc, (uint32_t*)&a, 4);
//  crc ^= 0xFFFFFFFF;


//  LaunchApplication();

  uint32_t start_time_ms = HAL_GetTick();
  uint32_t last_msg_time_ms = HAL_GetTick();
  while (1)
  {
    /* USER CODE END WHILE */

	  while(HAL_FDCAN_GetRxFifoFillLevel (&hfdcan1, FDCAN_RX_FIFO0) > 0) {
		  last_msg_time_ms = HAL_GetTick();

		  HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &RxHeader, (uint8_t*)&RxData);

		  if (RxHeader.Identifier == CAN_RX_ID) {
			  uint16_t index = RxData[0];
			  index |= RxData[1] << 8;
			  uint8_t subindex = RxData[2];

			  // ERASE COMMAND (little-endian)
			  if (index == 0xFFFF && subindex == 0xFF) {
				  EraseApplicationFlash();
			  }

			  // APPLICATION SIZE (little-endian)
			  if (index == 0 && subindex == 0) {
				  application_size = RxData[3];
				  application_size |= RxData[4] << 8;
				  application_size |= RxData[5] << 16;
				  application_size |= RxData[6] << 24;
			  }

			  // APPLICATION CRC32 (little-endian)
			  if (index == 0 && subindex == 1) {
				  application_crc32 = RxData[3];
				  application_crc32 |= RxData[4] << 8;
				  application_crc32 |= RxData[5] << 16;
				  application_crc32 |= RxData[6] << 24;
			  }

			  // FIRMWARE STREAM
			  if (index == 0 && subindex == 2) {
				  // Skip first 3 bytes and push the rest to buffer
				  for (int i = 3; i < RxHeader.DataLength; i++) {
					  flash_buffer[flash_buffer_position++] = RxData[i];
					  flash_total_bytes_written++;
				  }

				  if (flash_buffer_position >= FLASH_PAGE_SIZE) {
					  flash_buffer_position = 0;
					  if (!WriteBufferToFlash()) {
						  // This will fail if you did not erase first!
						  Error_Handler();
					  }
				  }
			  }
		  }
	  }

	  // PUBLISH APPLICATION INFO


	  int timeout = 0;// (HAL_GetTick() - last_msg_time_ms) > 15000;
	  int download_finished = flash_total_bytes_written >= application_size && application_size > 0;

	  if (timeout || download_finished) {
		  // Write last flash page (maybe a partial page or dropped transmission)
		  WriteBufferToFlash();
		  if (CheckApplicationCRC32 (application_crc32)) {
			  LaunchApplication();
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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 8;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV8;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_ENABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_ENABLE;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_BYTE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_ENABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief FDCAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_FDCAN1_Init(void)
{

  /* USER CODE BEGIN FDCAN1_Init 0 */

  /* USER CODE END FDCAN1_Init 0 */

  /* USER CODE BEGIN FDCAN1_Init 1 */

  /* USER CODE END FDCAN1_Init 1 */
  hfdcan1.Instance = FDCAN1;
  hfdcan1.Init.ClockDivider = FDCAN_CLOCK_DIV1;
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_CLASSIC;
  hfdcan1.Init.Mode = FDCAN_MODE_NORMAL;
  hfdcan1.Init.AutoRetransmission = DISABLE;
  hfdcan1.Init.TransmitPause = DISABLE;
  hfdcan1.Init.ProtocolException = DISABLE;
  hfdcan1.Init.NominalPrescaler = 4;
  hfdcan1.Init.NominalSyncJumpWidth = 1;
  hfdcan1.Init.NominalTimeSeg1 = 13;
  hfdcan1.Init.NominalTimeSeg2 = 2;
  hfdcan1.Init.DataPrescaler = 1;
  hfdcan1.Init.DataSyncJumpWidth = 1;
  hfdcan1.Init.DataTimeSeg1 = 1;
  hfdcan1.Init.DataTimeSeg2 = 1;
  hfdcan1.Init.StdFiltersNbr = 1;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN FDCAN1_Init 2 */

  /* Prepare Tx Header common fields */
  TxHeader.Identifier = CAN_TX_ID;
  TxHeader.IdType = FDCAN_STANDARD_ID;
  TxHeader.TxFrameType = FDCAN_DATA_FRAME;
  TxHeader.DataLength = FDCAN_DLC_BYTES_8;
  TxHeader.ErrorStateIndicator = FDCAN_ESI_PASSIVE;
  TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
  TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
  TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  TxHeader.MessageMarker = 0;

  FDCAN_FilterTypeDef sFilterConfig;

  sFilterConfig.IdType = FDCAN_STANDARD_ID;
  sFilterConfig.FilterType = FDCAN_FILTER_DUAL;
  sFilterConfig.FilterID1 = CAN_RX_ID;    // ID
  sFilterConfig.FilterID2 = 0x7FF;        // Full 11-bit mask
  //sFilterConfig.RxBufferIndex = 0;
  sFilterConfig.FilterIndex = 0;
  //sFilterConfig.IsCalibrationMsg = 0;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;

  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK)
  {
	  Error_Handler();
  }

  /* Start the FDCAN module */
  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
	Error_Handler();
  }


  /* USER CODE END FDCAN1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
