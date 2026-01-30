/**
  ******************************************************************************
  * @file    relocate.c
  * @brief   Code relocation from FLASH to SRAM
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

#include <stdint.h>
#include "stm32g4xx.h"

/* External symbols from linker script */
extern uint32_t _sflash_code;      /* Start of code in FLASH */
extern uint32_t _eflash_code;       /* End of code in FLASH */
extern uint32_t _sram_code_start;  /* Start of code in SRAM */

/* Function pointer type for jumping to SRAM */
typedef void (*sram_function_t)(void);

/**
  * @brief  Relocate code sections from FLASH to SRAM
  * @retval None
  */
__attribute__((section(".relocate_code"), used, noinline, aligned(4))) void relocate_to_sram(void)
{
  uint32_t *src = (uint32_t *)&_sflash_code;
  uint32_t *dst = (uint32_t *)&_sram_code_start;
  uint32_t size_bytes = (uint32_t)((uintptr_t)&_eflash_code - (uintptr_t)&_sflash_code);
  uint32_t size_words = size_bytes / 4;
  
  /* Copy code sections from FLASH to SRAM */
  /* Copy in 32-bit words for efficiency */
  for (uint32_t i = 0; i < size_words; i++)
  {
    dst[i] = src[i];
  }
  
  /* Copy remaining bytes if size is not word-aligned */
  if (size_bytes % 4 != 0)
  {
    uint8_t *src_byte = (uint8_t *)&_sflash_code + (size_words * 4);
    uint8_t *dst_byte = (uint8_t *)&_sram_code_start + (size_words * 4);
    uint32_t remaining = size_bytes % 4;
    for (uint32_t i = 0; i < remaining; i++)
    {
      dst_byte[i] = src_byte[i];
    }
  }
  
  /* Ensure all writes are complete before jumping */
  __DSB();
  __ISB();
}

/**
  * @brief  Jump to the relocated Reset_Handler in SRAM
  * @retval None
  */
__attribute__((section(".relocate_code"), used, noinline, aligned(4))) void jump_to_sram(void)
{
  /* Relocate vector table to SRAM */
  SCB->VTOR = (uint32_t)&_sram_code_start;
  
  /* Ensure VTOR write is complete */
  __DSB();
  __ISB();
  
  /* Get the address of Reset_Handler in SRAM */
  /* The vector table is at the start of SRAM, Reset_Handler is at offset 4 */
  uint32_t *sram_vector_table = (uint32_t *)&_sram_code_start;
  sram_function_t sram_reset_handler = (sram_function_t)sram_vector_table[1];
  
  /* Jump to SRAM Reset_Handler */
  sram_reset_handler();
  
  /* Should never reach here */
  while(1);
}
