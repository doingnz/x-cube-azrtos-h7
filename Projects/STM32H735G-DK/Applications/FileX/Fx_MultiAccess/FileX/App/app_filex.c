/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_filex.c
  * @author  MCD Application Team
  * @brief   FileX applicative file
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "app_filex.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32h735g_discovery.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define DEFAULT_STACK_SIZE               (2 * 1024)
#define DEFAULT_THREAD_PRIO              10
#define DEFAULT_TIME_SLICE               4
#define DEFAULT_PREEMPTION_THRESHOLD     DEFAULT_THREAD_PRIO

#define THREAD_ID_M                      0
#define THREAD_ID_1                      1
#define THREAD_ID_2                      2

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* Buffer for FileX FX_MEDIA sector cache. this should be 32-Bytes
aligned to avoid cache maintenance issues */
ALIGN_32BYTES (uint32_t media_memory[512]);

/* Define FileX global data structures.  */
FX_MEDIA        sdio_disk;
FX_FILE         fx_file_one;
FX_FILE         fx_file_two;
/* Define ThreadX global data structures.  */
TX_THREAD       fx_thread_main;
TX_THREAD       fx_thread_one;
TX_THREAD       fx_thread_two;
/* Define child threads completion event flags */
TX_EVENT_FLAGS_GROUP    finish_flag;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
VOID fx_thread_main_entry(ULONG thread_input);
VOID fx_thread_one_entry(ULONG thread_input);
VOID fx_thread_two_entry(ULONG thread_input);
VOID App_Error_Handler(INT id);

void Error_Handler(void);

/* USER CODE END PFP */

/* Global user code ---------------------------------------------------------*/
/* USER CODE BEGIN Global_User_Code */
static VOID os_delay(ULONG delay)
{
  ULONG start = tx_time_get();
  while ((tx_time_get() - start) < delay)
  {
  }
}
/* USER CODE END Global_User_Code */

/**
  * @brief  Application FileX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_FileX_Init(VOID *memory_ptr)
{
    UINT ret = FX_SUCCESS;
    /* USER CODE BEGIN App_FileX_Init */

    UCHAR *fx_thread_one_stack_start = (UCHAR*)memory_ptr + DEFAULT_STACK_SIZE;
    UCHAR *fx_thread_two_stack_start = (UCHAR*)memory_ptr + DEFAULT_STACK_SIZE * 2;

    /* Create the main thread.  */
    tx_thread_create(&fx_thread_main, "fx_thread_main", fx_thread_main_entry, 0,
                   memory_ptr, DEFAULT_STACK_SIZE,
                   DEFAULT_THREAD_PRIO,
                   DEFAULT_PREEMPTION_THRESHOLD,
                   DEFAULT_TIME_SLICE, TX_AUTO_START);

    /* Create the 1st concurrent thread.  */
    tx_thread_create(&fx_thread_one, "fx_thread_one", fx_thread_one_entry, 0,
                   (VOID *)fx_thread_one_stack_start, DEFAULT_STACK_SIZE,
                   DEFAULT_THREAD_PRIO,
                   DEFAULT_PREEMPTION_THRESHOLD,
                   DEFAULT_TIME_SLICE, TX_DONT_START);

    /* Create the 2nd concurrent thread */
    tx_thread_create(&fx_thread_two, "fx_thread_two", fx_thread_two_entry, 0,
                   (VOID *)fx_thread_two_stack_start, DEFAULT_STACK_SIZE,
                   DEFAULT_THREAD_PRIO,
                   DEFAULT_PREEMPTION_THRESHOLD,
                   DEFAULT_TIME_SLICE, TX_DONT_START);

    /* An event flag to indicate the status of execution */
    tx_event_flags_create(&finish_flag, "event_flag");

    /* Initialize FileX.  */
    fx_system_initialize();

    /* USER CODE END App_FileX_Init */

    return ret;
}

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN Private_User_Code */

/* USER CODE END Private_User_Code */

/* USER CODE BEGIN 1 */

VOID fx_thread_main_entry(ULONG thread_input)
{

  UINT status;
  ULONG event_flags;


  /* Open the SD disk driver.  */
  status =  fx_media_open(&sdio_disk, "STM32_SDIO_DISK", fx_stm32_sd_driver, 0, media_memory, sizeof(media_memory));

  /* Check the media open status.  */
  if (status != FX_SUCCESS)
  {
    App_Error_Handler(THREAD_ID_M);
  }

  /* Media opened successfully, we start the concurrent threads. */
  status = tx_thread_resume(&fx_thread_one) & tx_thread_resume(&fx_thread_two);

  /* Check the concurrent thread was started correctly.  */
  if (status != TX_SUCCESS)
  {
    App_Error_Handler(THREAD_ID_M);
  }

  /* block here waiting for concurrent threads to finish processing */
  status = tx_event_flags_get(&finish_flag, 0x11, TX_AND_CLEAR,
  	&event_flags, TX_WAIT_FOREVER);

  /* Check the status.  */
  if (status != TX_SUCCESS)
  {
    /* Error getting the event flags, call error handler.  */
    App_Error_Handler(THREAD_ID_M);
  }

  /* Close the media.  */
  status =  fx_media_close(&sdio_disk);

  /* Check the media close status.  */
  if (status != FX_SUCCESS)
  {
    /* Error closing the media, call error handler.  */
    App_Error_Handler(THREAD_ID_M);
  }

  /* Toggle green LED to indicate processing finish OK */
  while(1)
  {
  	BSP_LED_Toggle(LED_GREEN);
  	os_delay(40);
  }
}

VOID fx_thread_one_entry(ULONG thread_input)
{

  UINT status;
  ULONG bytes_read;
  CHAR read_buffer[32];
  CHAR data[] = "This is FileX working concurrently on STM32";

  /* Create a file called STM32_FILE1.TXT in the root directory.  */
  status =  fx_file_create(&sdio_disk, "STM32_FILE1.TXT");

  /* Check the create status.  */
  if (status != FX_SUCCESS)
  {
    /* Check for an already created status. This is expected on the
    second pass of this loop!  */
    if (status != FX_ALREADY_CREATED)
    {
      /* Create error, call error handler.  */
      App_Error_Handler(THREAD_ID_1);
    }
  }

  /* Open the test file.  */
  status =  fx_file_open(&sdio_disk, &fx_file_one, "STM32_FILE1.TXT", FX_OPEN_FOR_WRITE);

  /* Check the file open status.  */
  if (status != FX_SUCCESS)
  {
    /* Error opening file, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  /* Seek to the beginning of the test file.  */
  status =  fx_file_seek(&fx_file_one, 0);

  /* Check the file seek status.  */
  if (status != FX_SUCCESS)
  {
    /* Error performing file seek, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  /* Write a string to the test file.  */
  status =  fx_file_write(&fx_file_one, data, sizeof(data));

  /* Check the file write status.  */
  if (status != FX_SUCCESS)
  {
    /* Error writing to a file, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  /* Close the test file.  */
  status =  fx_file_close(&fx_file_one);

  /* Check the file close status.  */
  if (status != FX_SUCCESS)
  {
    /* Error closing the file, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  status = fx_media_flush(&sdio_disk);

  /* Check the media flush  status.  */
  if (status != FX_SUCCESS)
  {
    /* Error closing the file, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  /* Open the test file.  */
  status =  fx_file_open(&sdio_disk, &fx_file_one, "STM32_FILE1.TXT", FX_OPEN_FOR_READ);

  /* Check the file open status.  */
  if (status != FX_SUCCESS)
  {
    /* Error opening file, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  /* Seek to the beginning of the test file.  */
  status =  fx_file_seek(&fx_file_one, 0);

  /* Check the file seek status.  */
  if (status != FX_SUCCESS)
  {
    /* Error performing file seek, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  /* Read the content of the test file.  */
  status =  fx_file_read(&fx_file_one, read_buffer, sizeof(data), &bytes_read);

  /* Check the file read status.  */
  if ((status != FX_SUCCESS) || (bytes_read != sizeof(data)))
  {
    /* Error reading file, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  /* Close the test file.  */
  status =  fx_file_close(&fx_file_one);

  /* Check the file close status.  */
  if (status != FX_SUCCESS)
  {
    /* Error closing the file, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  status = tx_event_flags_set(&finish_flag,  0x01, TX_OR);

  /* Check the event setting status.  */
  if (status != TX_SUCCESS)
  {
    /* Error calling the event setter, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  while(1);
}

VOID fx_thread_two_entry(ULONG thread_input)
{

  UINT status;
  ULONG bytes_read;
  CHAR read_buffer[32];
  CHAR data[] = "This is FileX working concurrently on STM32";

  /* Create a file called STM32_FILE2.TXT in the root directory.  */
  status =  fx_file_create(&sdio_disk, "STM32_FILE2.TXT");

  /* Check the create status.  */
  if (status != FX_SUCCESS)
  {
    /* Check for an already created status. This is expected on the
    second pass of this loop!  */
    if (status != FX_ALREADY_CREATED)
    {
      /* Create error, call error handler.  */
      App_Error_Handler(THREAD_ID_2);
    }
  }

  /* Open the test file.  */
  status =  fx_file_open(&sdio_disk, &fx_file_two, "STM32_FILE2.TXT", FX_OPEN_FOR_WRITE);

  /* Check the file open status.  */
  if (status != FX_SUCCESS)
  {
    /* Error opening file, call error handler.  */
    App_Error_Handler(THREAD_ID_2);
  }

  /* Seek to the beginning of the test file.  */
  status =  fx_file_seek(&fx_file_two, 0);

  /* Check the file seek status.  */
  if (status != FX_SUCCESS)
  {
    /* Error performing file seek, call error handler.  */
    App_Error_Handler(THREAD_ID_2);
  }

  /* Write a string to the test file.  */
  status =  fx_file_write(&fx_file_two, data, sizeof(data));

  /* Check the file write status.  */
  if (status != FX_SUCCESS)
  {
    /* Error writing to a file, call error handler.  */
    App_Error_Handler(THREAD_ID_2);
  }

  /* Close the test file.  */
  status =  fx_file_close(&fx_file_two);

  /* Check the file close status.  */
  if (status != FX_SUCCESS)
  {
    /* Error closing the file, call error handler.  */
    App_Error_Handler(THREAD_ID_2);
  }

  status = fx_media_flush(&sdio_disk);

  /* Check the media flush  status.  */
  if (status != FX_SUCCESS)
  {
    /* Error closing the file, call error handler.  */
    App_Error_Handler(THREAD_ID_2);
  }

  /* Open the test file.  */
  status =  fx_file_open(&sdio_disk, &fx_file_two, "STM32_FILE2.TXT", FX_OPEN_FOR_READ);

  /* Check the file open status.  */
  if (status != FX_SUCCESS)
  {
    /* Error opening file, call error handler.  */
    App_Error_Handler(THREAD_ID_2);
  }

  /* Seek to the beginning of the test file.  */
  status =  fx_file_seek(&fx_file_two, 0);

  /* Check the file seek status.  */
  if (status != FX_SUCCESS)
  {
    /* Error performing file seek, call error handler.  */
    App_Error_Handler(THREAD_ID_2);
  }

  /* Read the content of the test file.  */
  status =  fx_file_read(&fx_file_two, read_buffer, sizeof(data), &bytes_read);

  /* Check the file read status.  */
  if ((status != FX_SUCCESS) || (bytes_read != sizeof(data)))
  {
    /* Error reading file, call error handler.  */
    App_Error_Handler(THREAD_ID_2);
  }

  /* Close the test file.  */
  status =  fx_file_close(&fx_file_two);

  /* Check the file close status.  */
  if (status != FX_SUCCESS)
  {
    /* Error closing the file, call error handler.  */
    App_Error_Handler(THREAD_ID_2);
  }

  status = tx_event_flags_set(&finish_flag,  0x10, TX_OR);

  /* Check the event setting status.  */
  if (status != TX_SUCCESS)
  {
    /* Error calling the event setter, call error handler.  */
    App_Error_Handler(THREAD_ID_1);
  }

  while(1);
}

VOID App_Error_Handler(INT id)
{

switch (id)
  {

  case THREAD_ID_M :
    /* terminate the other threads to preserve the call stack pointing here */
    tx_thread_terminate(&fx_thread_one);
    tx_thread_terminate(&fx_thread_two);
    break;

  case THREAD_ID_1 :
  	/* terminate the other threads to preserve the call stack pointing here */
    tx_thread_terminate(&fx_thread_main);
    tx_thread_terminate(&fx_thread_two);
    break;

  case THREAD_ID_2 :
  	/* terminate the other threads to preserve the call stack pointing here */
    tx_thread_terminate(&fx_thread_main);
    tx_thread_terminate(&fx_thread_one);
    break;
  }

  /* Call the main error handler */
  Error_Handler();

}
/* USER CODE END 1 */
