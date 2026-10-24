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
#include "dma.h"
#include "i2c.h"
#include "stm32f4xx_hal.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "app.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* ======================== 应用状态机工作逻辑 ========================
 * 现在有两个独立状态机，每次 App_Process 都各推进一步：
 *
 * 采样：START_READ -> WAIT_READ -> 保存最新结果 -> START_READ（持续循环）
 * 输出：PRINT_IDLE -> 收到打印请求 -> FORMAT_DATA/I2C_ERROR
 *                 -> START_TX -> WAIT_TX -> PRINT_IDLE
 * 串口故障：START_TX/WAIT_TX -> UART_RECOVER -> PRINT_IDLE
 *
 * 打印周期在 main 的 while 循环中控制，不再是状态机中的等待状态。
 * 采样不会因串口发送或打印间隔暂停；实际读取速度受 IIC 总线限制。
 * 连续读取并不保证传感器每次都产生新测量，仍取决于其内部刷新率。
 * IIC 驱动还有自己的两段读取状态机，具体流转见 jy61p.c 文件顶部。
 */
typedef enum
{
  APP_START_READ,    /* 申请一轮陀螺仪采样 */
  APP_WAIT_READ      /* 等待两段 IIC DMA 完成，取走结果后立即继续采样 */
} App_SampleState;

typedef enum
{
  APP_PRINT_IDLE,    /* 没有待发送文本；仅等待主循环的打印请求 */
  APP_FORMAT_DATA,   /* 将本次打印的数据快照转换为原有文本 */
  APP_START_TX,      /* 启动 USART1 DMA，将这一行发往电脑 */
  APP_WAIT_TX,       /* 等待串口真正发送结束，期间保持缓冲区不变 */
  APP_I2C_ERROR,     /* 故障状态：格式化原来的 I2C error 文本 */
  APP_UART_RECOVER  /* 故障状态：停止异常串口发送并恢复可用状态 */
} App_PrintState;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define APP_STARTUP_DELAY_MS  1500U /* 沿用原来的上电等待时间 */
#define APP_PRINT_PERIOD_MS   1000U /* 主循环每 1000 ms 请求一次串口打印 */
#define APP_UART_TIMEOUT_MS   100U  /* 沿用原来串口发送的超时限制 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static App_SampleState sample_state = APP_START_READ;
static App_PrintState print_state = APP_PRINT_IDLE;

/* 最新测量数据供 PID 等主循环任务使用，不必等串口发送完成。
 * 只有两段 IIC 都成功才更新 app_data；失败时保留数值但置 valid=0，
 * 避免 PID 把旧数据当作当前有效测量。采样时间通过 getter 一起返回。
 */
static JY61P_Data app_data;
static HAL_StatusTypeDef app_i2c_status;
static uint32_t app_i2c_error;
static uint32_t app_data_tick;
static uint8_t app_data_valid;
static uint8_t app_has_sample_result;

/* 打印使用独立快照，采样可以在 FORMAT_DATA/WAIT_TX 期间继续更新。
 * 这里同时快照状态、错误码和数据，避免一条文本混用不同轮的结果。
 */
static JY61P_Data print_data;
static HAL_StatusTypeDef print_i2c_status;
static uint32_t print_i2c_error;
static uint8_t print_requested;

/* DMA 发送启动后函数会立即返回，因此这一行不能放在循环内的临时数组。
 * 仅 FORMAT_DATA/I2C_ERROR 状态允许写它；WAIT_TX 期间绝不修改。
 */
static char uart_line[192];
static uint16_t uart_line_length;
static uint32_t uart_start_tick;
static uint32_t last_print_tick;

/* 只有这两个事件标志由 USART/DMA 回调写入，其余状态由主循环维护。 */
static volatile uint8_t uart_tx_complete;
static volatile uint8_t uart_tx_error;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void App_ProcessSampling(void);
static void App_ProcessPrinting(void);
static void App_PrepareTransmission(int length);
static HAL_StatusTypeDef App_RecoverUart(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_DMA_Init();
  MX_I2C2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  /* 上电等待保留在主循环进入前；此时不在中断里，SysTick 15 正常计时。 */
  HAL_Delay(APP_STARTUP_DELAY_MS);
  /* 让第一次打印请求立即生效，但必须等首次完整采样结果才生成文本。
   * 无符号减法允许初始化值回绕，也允许系统 tick 在长期运行后回绕。
   */
  last_print_tick = HAL_GetTick() - APP_PRINT_PERIOD_MS;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* 1. 持续推进采样和串口状态机，两个状态机分别维护自己的状态。
     * PID 等任务可以在这里通过 App_GetLatestData() 使用最新有效数据。
     */
    App_Process();

    /* 2. 只有打印按 1 秒节奏请求，采样和 PID 不在这里等待 1 秒。
     * SysTick 中断更新毫秒计数，主循环读取计数并判断打印时间是否到达。
     * HAL_Delay(1000) 会暂停整个主循环，故不能用于“持续采样”的模式。
     */
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_print_tick) >= APP_PRINT_PERIOD_MS)
    {
      last_print_tick = now;
      App_RequestPrint();
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  RCC_OscInitStruct.PLL.PLLN = 168;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/* ======================== 正常工作代码 ======================== */

/* 只置请求标志，实际取快照和启动 DMA 交给打印状态机。
 * 发送忙时多个请求合并为一个，稍后打印最新结果，不堆积旧结果。
 */
void App_RequestPrint(void)
{
  print_requested = 1U;
}

uint8_t App_GetLatestData(JY61P_Data *data, uint32_t *sample_tick)
{
  if ((data == NULL) || (app_data_valid == 0U))
  {
    return 0U;
  }
  *data = app_data;
  if (sample_tick != NULL)
  {
    *sample_tick = app_data_tick;
  }
  return 1U;
}

/* 采样和输出分别推进，串口等待不会阻挡下一轮 IIC 读取。 */
void App_Process(void)
{
  App_ProcessSampling();
  App_ProcessPrinting();
}

static void App_ProcessSampling(void)
{
  HAL_StatusTypeDef status;

  /* 下一段 DMA、事件处理和故障恢复都在主循环进行，保留 SysTick 15。 */
  JY61P_Process();

  switch (sample_state)
  {
    case APP_START_READ:
      status = JY61P_StartReadData();
      if ((status == HAL_OK) || (status == HAL_BUSY))
      {
        /* BUSY 表示驱动仍有一轮采样/结果，转入等待并取走该结果。 */
        sample_state = APP_WAIT_READ;
      }
      break;

    case APP_WAIT_READ:
      if (JY61P_GetReadResult(&app_data, &app_i2c_status) != 0U)
      {
        /* 下一轮 StartReadData 会清驱动错误码，因此此处立即保存快照。
         * 失败不会修改 app_data，valid 则反映最近一次采样是否成功。
         */
        app_i2c_error = JY61P_GetError();
        app_has_sample_result = 1U;
        app_data_valid = (app_i2c_status == HAL_OK) ? 1U : 0U;
        if (app_data_valid != 0U)
        {
          app_data_tick = HAL_GetTick();
        }
        /* 没有 1 秒等待：下一次 App_Process 即可申请下一轮采样。 */
        sample_state = APP_START_READ;
      }
      break;

    default:
      break;
  }
}

/* 与原程序一样：格式化失败或超长时跳过本次发送，等待下次打印请求。 */
static void App_PrepareTransmission(int length)
{
  if ((length > 0) && ((size_t)length < sizeof(uart_line)))
  {
    uart_line_length = (uint16_t)length;
    print_state = APP_START_TX;
  }
  else
  {
    print_state = APP_PRINT_IDLE;
  }
}

static void App_ProcessPrinting(void)
{
  HAL_StatusTypeDef status;
  int length;

  switch (print_state)
  {
    case APP_PRINT_IDLE:
      if ((print_requested != 0U) && (app_has_sample_result != 0U))
      {
        /* 只有到达打印时间才复制快照，采样完成本身不会自动触发打印。 */
        print_requested = 0U;
        print_data = app_data;
        print_i2c_status = app_i2c_status;
        print_i2c_error = app_i2c_error;
        print_state = (print_i2c_status == HAL_OK)
                  ? APP_FORMAT_DATA : APP_I2C_ERROR;
      }
      break;

    case APP_FORMAT_DATA:
      /* 字段顺序、单位、小数位数、空格和换行全部保持原样。
       * snprintf/浮点格式化只在主循环执行，不占用中断处理时间。
       */
      length = snprintf(
          uart_line,
          sizeof(uart_line),
          "ACC(g)=%.3f,%.3f,%.3f "
          "GYRO(deg/s)=%.3f,%.3f,%.3f "
          "ANGLE(deg)=%.3f,%.3f,%.3f\r\n",
          (double)print_data.acc[0],
          (double)print_data.acc[1],
          (double)print_data.acc[2],
          (double)print_data.gyro[0],
          (double)print_data.gyro[1],
          (double)print_data.gyro[2],
          (double)print_data.angles.roll,
          (double)print_data.angles.pitch,
          (double)print_data.angles.yaw);
      App_PrepareTransmission(length);
      break;

    case APP_START_TX:
      /* 此前的发送已经结束/清理，本轮才能清标志并启动新的 DMA。
       * HAL_OK 只代表发送启动成功，不能在这里复用 uart_line。
       */
      uart_tx_complete = 0U;
      uart_tx_error = 0U;
      uart_start_tick = HAL_GetTick();
      status = HAL_UART_Transmit_DMA(&huart1, (uint8_t *)uart_line,
                                     uart_line_length);
      print_state = (status == HAL_OK) ? APP_WAIT_TX : APP_UART_RECOVER;
      break;

    case APP_WAIT_TX:
      if (uart_tx_error != 0U)
      {
        /* 故障分支：DMA/串口错误，不把这次发送认作正常完成。 */
        print_state = APP_UART_RECOVER;
      }
      else if (uart_tx_complete != 0U)
      {
        /* 普通 DMA 模式下，HAL 在 USART TC 中断确认最后一个字节
         * 真正发完后才调用 TxCpltCallback，此时允许复用发送缓冲区。
         */
        print_state = APP_PRINT_IDLE;
      }
      else if ((uint32_t)(HAL_GetTick() - uart_start_tick)
               >= APP_UART_TIMEOUT_MS)
      {
        print_state = APP_UART_RECOVER;
      }
      break;

    /* ==================== 故障处理状态 ==================== */
    case APP_I2C_ERROR:
      /* IIC 驱动在恢复外设前保存了错误码，输出仍使用原来的格式。 */
      length = snprintf(uart_line, sizeof(uart_line),
                        "I2C error: status=%u err=0x%08lX\r\n",
                        (unsigned int)print_i2c_status,
                        (unsigned long)print_i2c_error);
      App_PrepareTransmission(length);
      break;

    case APP_UART_RECOVER:
      if (App_RecoverUart() == HAL_OK)
      {
        /* 原程序没有输出 UART 故障文本，这里也不新增输出或自动重发。
         * 清理后回到打印空闲状态，采样状态机始终独立运行。
         */
        print_state = APP_PRINT_IDLE;
      }
      break;

    default:
      break;
  }
}

/* 正常发送完成回调：只记录事件，不格式化、不启动下一轮采样。 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    uart_tx_complete = 1U;
  }
}

/* ======================== 故障处理代码 ======================== */

/* 故障回调仍只置标志，停止发送和恢复 DMA 都交给主循环。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &huart1)
  {
    uart_tx_error = 1U;
  }
}

static HAL_StatusTypeDef App_RecoverUart(void)
{
  /* 防止清理过程中旧的完成/故障中断再次更新标志。
   * 只屏蔽 USART1 和它的发送 DMA，SysTick 仍然计时。
   */
  HAL_NVIC_DisableIRQ(USART1_IRQn);
  HAL_NVIC_DisableIRQ(DMA2_Stream7_IRQn);

  /* AbortTransmit 关闭串口发送请求和 TC 中断，并终止活动 DMA。
   * 这个 HAL 函数可能短暂等待 DMA 停止，因此只允许在主循环调用。
   */
  if (HAL_UART_AbortTransmit(&huart1) != HAL_OK)
  {
    return HAL_BUSY;
  }
  if (HAL_DMA_GetState(huart1.hdmatx) == HAL_DMA_STATE_BUSY)
  {
    if (HAL_DMA_Abort(huart1.hdmatx) != HAL_OK)
    {
      return HAL_BUSY;
    }
  }
  __HAL_DMA_DISABLE(huart1.hdmatx);
  if ((huart1.hdmatx->Instance->CR & DMA_SxCR_EN) != 0U)
  {
    /* 硬件尚未停止时继续保留发送缓冲区，下次主循环重试。 */
    return HAL_BUSY;
  }

  /* 正常终止后 DMA 已 READY；若处于错误/超时状态则重新初始化。
   * Init 仍使用 CubeMX 原来的 DMA 参数，不改变优先级或串口波特率。
   */
  if (HAL_DMA_GetState(huart1.hdmatx) != HAL_DMA_STATE_READY)
  {
    if ((HAL_DMA_DeInit(huart1.hdmatx) != HAL_OK)
        || (HAL_DMA_Init(huart1.hdmatx) != HAL_OK))
    {
      return HAL_ERROR;
    }
  }
  HAL_NVIC_ClearPendingIRQ(USART1_IRQn);
  HAL_NVIC_ClearPendingIRQ(DMA2_Stream7_IRQn);
  uart_tx_complete = 0U;
  uart_tx_error = 0U;
  HAL_NVIC_EnableIRQ(USART1_IRQn);
  HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);
  return HAL_OK;
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
