/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_usbx_host.c
  * @author  MCD Application Team
  * @brief   USBX host applicative file
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
#include "app_usbx_host.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_QUEUE_SIZE                               5
#define USBX_APP_STACK_SIZE                          1024
#define USBX_MEMORY_SIZE                             (64 * 1024)
#define USBX_APP_BYTE_POOL_SIZE                      (4096 + (USBX_MEMORY_SIZE))

/* Set usbx_pool start address to 0x24027000 */
#if defined ( __ICCARM__ ) /* IAR Compiler */
#pragma location = 0x24027000
#elif defined ( __GNUC__ ) /* GNU Compiler */
__attribute__((section(".UsbxPoolSection")))
#endif
static uint8_t usbx_pool[USBX_APP_BYTE_POOL_SIZE];

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
extern HCD_HandleTypeDef            hhcd_USB_OTG_HS;
TX_THREAD                           ux_app_thread;
TX_BYTE_POOL                        ux_byte_pool;
TX_QUEUE                            MsgQueue;
UX_HOST_CLASS_STORAGE               *storage;
UX_HOST_CLASS_STORAGE_MEDIA         *storage_media;
FX_MEDIA                            *media;
CHAR                                file_name[64];
UINT                                status;
Device_info                         dev_info;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
extern UINT App_File_Write(void);
extern UINT App_File_Create(void);
extern UINT App_File_Read(void);
extern void MX_USB_OTG_HS_HCD_Init(void);
/* USER CODE END PFP */
/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  Application USBX Host Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_USBX_Host_Init(VOID *memory_ptr)
{
  UINT ret = UX_SUCCESS;

  /* USER CODE BEGIN  App_USBX_Host_Init */
  CHAR *pointer;

  /* Create a byte memory pool from which to allocate the thread stacks.  */
  if (tx_byte_pool_create(&ux_byte_pool, "ux byte pool 0", usbx_pool,
                          USBX_APP_BYTE_POOL_SIZE) != TX_SUCCESS)
  {
    ret = TX_POOL_ERROR;
  }

  /* Allocate the stack for thread 0.  */
  if (tx_byte_allocate(&ux_byte_pool, (VOID **) &pointer,
                       USBX_MEMORY_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    ret = TX_POOL_ERROR;
  }

  /* Initialize USBX memory. */
  if (ux_system_initialize(pointer, USBX_MEMORY_SIZE, UX_NULL, 0) != UX_SUCCESS)
  {
    ret = UX_ERROR;
  }
  /* register a callback error function */

  _ux_utility_error_callback_register(&ux_host_error_callback);

  /* Allocate the stack for thread 0.  */
  if (tx_byte_allocate(&ux_byte_pool, (VOID **) &pointer,
                       USBX_APP_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    ret = TX_POOL_ERROR;
  }

  /* Create the main App thread.  */
  if (tx_thread_create(&ux_app_thread, "thread 0", usbx_app_thread_entry, 0,
                       pointer, USBX_APP_STACK_SIZE, 20, 20, 1,
                       TX_AUTO_START) != TX_SUCCESS)
  {
    ret = TX_THREAD_ERROR;
  }

  /* Allocate Memory for the Queue  */
  if (tx_byte_allocate(&ux_byte_pool, (VOID **) &pointer,
                       APP_QUEUE_SIZE * sizeof(Device_info), TX_NO_WAIT) != TX_SUCCESS)
  {
    ret = TX_POOL_ERROR;
  }

  /* Create the MsgQueue   */
  if (tx_queue_create(&MsgQueue, "Message Queue app", sizeof(Device_info),
                      pointer, APP_QUEUE_SIZE * sizeof(Device_info)) != TX_SUCCESS)
  {
    ret = TX_QUEUE_ERROR;
  }
  /* USER CODE END  App_USBX_Host_Init */

  return ret;
}

/* USER CODE BEGIN 1 */
/**
  * @brief  Application_thread_entry .
  * @param  ULONG arg
  * @retval Void
  */
void  usbx_app_thread_entry(ULONG arg)
{
  /* Initialize USBX_Host */
  MX_USB_Host_Init();

  /* Start Application */
  USBH_UsrLog(" **** USB OTG HS in FS MSC Host **** \n");
  USBH_UsrLog("USB Host library started.\n");

  /* Initialize Application and MSC process */
  USBH_UsrLog("Starting MSC Application");
  USBH_UsrLog("Connect your MSC Device\n");

  while (1)
  {
    /* wait for message queue from callback event */
    status = tx_queue_receive(&MsgQueue, &dev_info, TX_WAIT_FOREVER);

    if (dev_info.Dev_state == Device_connected)
    {
      switch (dev_info.Device_Type)
      {
        case MSC_Device :
          /* Device_information */
          USBH_UsrLog("USB Mass Storage Device Found");
          USBH_UsrLog("PID: %#x ", (UINT)storage -> ux_host_class_storage_device -> ux_device_descriptor.idProduct);
          USBH_UsrLog("VID: %#x ", (UINT)storage -> ux_host_class_storage_device -> ux_device_descriptor.idVendor);

          /* start File operations */
          USBH_UsrLog("\n*** Start Files operations ***\n");

          /* Create a file */
          status = App_File_Create();

          /* check status */
          if (status == UX_SUCCESS)
          {
            USBH_UsrLog("File TEST.TXT Created \n");
          }
          else
          {
            USBH_ErrLog(" !! Could Not Create TEST.TXT File !! ");
            break;
          }

          /* Start write File Operation */
          USBH_UsrLog("Write Process ...... \n");
          status = App_File_Write();

          /* check status */
          if (status == UX_SUCCESS)
          {
            USBH_UsrLog("Write Process Success \n");
          }
          else
          {
            USBH_ErrLog("!! Write Process Fail !! ");
            break;
          }

          /* Start Read File Operation and comparison operation */
          USBH_UsrLog("Read Process  ...... \n");
          status = App_File_Read();

          /* check status */
          if (status == UX_SUCCESS)
          {
            USBH_UsrLog("Read Process Success  \n");
            USBH_UsrLog("File Closed \n");
            USBH_UsrLog("*** End Files operations ***\n")
          }
          else
          {
            USBH_ErrLog("!! Read Process Fail !! \n");
          }
          break;

        case Unknown_Device :
          USBH_ErrLog("!! Unsupported MSC_Device plugged !!");
          dev_info.Dev_state = No_Device;
          break;

        case Unsupported_Device :
          USBH_ErrLog("!! Unabble to start Device !!");
          break;

        default :
          break;
      }
    }
    else
    {
      /*clear storage instance*/
      storage_media  = NULL;
      media = NULL;
      tx_thread_sleep(50);
    }
  }
}

/**
  * @brief MX_USB_Host_Init
  *        Initialization of USB Host.
  * Init USB Host Library, add supported class and start the library
  * @retval None
  */
UINT MX_USB_Host_Init(void)
{
  UINT ret = UX_SUCCESS;
  /* USER CODE BEGIN USB_Host_Init_PreTreatment_0 */
  /* USER CODE END USB_Host_Init_PreTreatment_0 */

  /* The code below is required for installing the host portion of USBX.  */
  if (ux_host_stack_initialize(ux_host_event_callback) != UX_SUCCESS)
  {
    status = UX_ERROR;
  }

  /* Register storage class. */
  if ((status =  ux_host_stack_class_register(_ux_system_host_class_storage_name,
                                              _ux_host_class_storage_entry)) != UX_SUCCESS)
  {
    status = UX_ERROR;
  }

  /* Initialize the LL driver */
  MX_USB_OTG_HS_HCD_Init();

  /* Register all the USB host controllers available in this system. */
  if (ux_host_stack_hcd_register(_ux_system_host_hcd_stm32_name,
                                 _ux_hcd_stm32_initialize,
                                 USB_OTG_HS_PERIPH_BASE,
                                 (ULONG)&hhcd_USB_OTG_HS) != UX_SUCCESS)
  {
    status = UX_ERROR;
  }

  /* Drive vbus to be addedhere */
  USBH_DriverVBUS(USB_VBUS_TRUE);

  /* Enable USB Global Interrupt */
  HAL_HCD_Start(&hhcd_USB_OTG_HS);

  /* USER CODE BEGIN USB_Host_Init_PreTreatment_1 */
  /* USER CODE END USB_Host_Init_PreTreatment_1 */


  /* USER CODE BEGIN USB_Host_Init_PostTreatment */
  /* USER CODE END USB_Host_Init_PostTreatment */
  return ret ;
}

/**
* @brief ux_host_event_callback
* @param ULONG event
           This parameter can be one of the these values:
             1 : UX_DEVICE_INSERTION
             2 : UX_DEVICE_REMOVAL
         UX_HOST_CLASS * Current_class
         VOID * Current_instance
* @retval Status
*/
UINT ux_host_event_callback(ULONG event, UX_HOST_CLASS *Current_class, VOID *Current_instance)
{
  UINT status;
  UX_HOST_CLASS *msc_class;

  switch (event)
  {
    case UX_DEVICE_INSERTION :
      /* Get current Hid Class */
      status = ux_host_stack_class_get(_ux_system_host_class_storage_name, &msc_class);

      if (status == UX_SUCCESS)
      {
        if ((msc_class == Current_class) && (storage == NULL))
        {
          /* get current msc Instance */
          storage = Current_instance;

          if (storage == NULL)
          {
            USBH_UsrLog("unable to start media ");
            dev_info.Device_Type = Unsupported_Device;
            dev_info.Dev_state   = Device_connected;
            tx_queue_send(&MsgQueue, &dev_info, TX_NO_WAIT);
          }
          else
          {
            /* get the storage media */
            storage_media = (UX_HOST_CLASS_STORAGE_MEDIA *)msc_class->ux_host_class_media;
            media = &storage_media->ux_host_class_storage_media;
          }

          if (storage->ux_host_class_storage_state != (ULONG) UX_HOST_CLASS_INSTANCE_LIVE)
          {
            dev_info.Device_Type = Unsupported_Device;
            dev_info.Dev_state = Device_connected;
            tx_queue_send(&MsgQueue, &dev_info, TX_NO_WAIT);
          }
          else
          {
            /* USB _MSC_ Device found */
            USBH_UsrLog("USB Device Plugged");

            /* update USB device Type */
            dev_info.Device_Type = MSC_Device;
            dev_info.Dev_state = Device_connected ;

            /* put a message queue to usbx_app_thread_entry */
            tx_queue_send(&MsgQueue, &dev_info, TX_NO_WAIT);
          }
        }
      }
      else
      {
        /* No MSC class found */
        USBH_ErrLog("NO MSC Class found");
      }
      break;

    case UX_DEVICE_REMOVAL :

      if (Current_instance == storage)
      {
        /* free Instance */
        storage = NULL;
        USBH_UsrLog("USB Device Unplugged");
        dev_info.Dev_state   = No_Device;
        dev_info.Device_Type = Unknown_Device;
        tx_queue_send(&MsgQueue, &dev_info, TX_NO_WAIT);
      }
      break;

    default:
      break;
  }

  return (UINT) UX_SUCCESS;
}

/**
* @brief ux_host_error_callback
* @param ULONG event
         UINT system_context
         UINT error_code
* @retval Status
*/
VOID ux_host_error_callback(UINT system_level, UINT system_context, UINT error_code)
{
  switch (error_code)
  {
    case UX_DEVICE_ENUMERATION_FAILURE :
      dev_info.Device_Type = Unknown_Device;
      dev_info.Dev_state   = Device_connected;
      tx_queue_send(&MsgQueue, &dev_info, TX_NO_WAIT);
      break;

    case UX_NO_DEVICE_CONNECTED :
      USBH_UsrLog("USB Device disconnected");
      break;

    default:
      break;
  }
}

/**
  * @brief  Drive VBUS.
  * @param  state : VBUS state
  *          This parameter can be one of the these values:
  *           1 : VBUS Active
  *           0 : VBUS Inactive
  * @retval Status
  */
void USBH_DriverVBUS(uint8_t state)
{
  /* USER CODE BEGIN 0 */

  /* USER CODE END 0*/

  if (state == USB_VBUS_TRUE)
  {
    /* Drive high Charge pump */
    /* Add IOE driver control */
    /* USER CODE BEGIN DRIVE_HIGH_CHARGE_FOR_HS */
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_SET);
    /* USER CODE END DRIVE_HIGH_CHARGE_FOR_HS */
  }
  else
  {
    /* Drive low Charge pump */
    /* Add IOE driver control */
    /* USER CODE BEGIN DRIVE_LOW_CHARGE_FOR_HS */
    HAL_GPIO_WritePin(GPIOH, GPIO_PIN_5, GPIO_PIN_RESET);
    /* USER CODE END DRIVE_LOW_CHARGE_FOR_HS */
  }

  HAL_Delay(200);
}

/* USER CODE END 1 */