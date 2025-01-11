/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "gpio.h"
//#include "stdio.h"
#include <cstdint>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//extern void initialise_monitor_handles(void);
enum SignalType {
    SHORT_SIGNAL, LONG_SIGNAL, TIMEOUT, BOUNCE
};

enum LockResponse {
    CORRECT, WRONG, OPEN, BLOCKED
};

enum TimeoutResult {
    OK,
    EXPIRED
};

class Timer {
public:
    void begin() {
        startTime = HAL_GetTick();
    }

    void end() {
        endTime = HAL_GetTick();
    }

    uint32_t duration() const {
        return endTime - startTime;
    }

private:
    uint32_t startTime = 0;
    uint32_t endTime = 0;   
};

class Timeout : private Timer {
public:
    Timeout(bool (*pred)(void), uint32_t time = UINT32_MAX) : predicate(pred), timeout(time) {
    }

    TimeoutResult wait() {
        Timer::begin();
        while(!predicate()) {
            Timer::end();
            if (Timer::duration() > timeout) {
                return EXPIRED;
            }
        }
        return OK;
    }

private:
    bool (*predicate)(void);
    uint32_t timeout;
};

class Delay : private Timeout {
public:
    Delay(uint32_t time = UINT32_MAX) : Timeout(&falsePred, time) {
    }

    using Timeout::wait;

private:
    static bool falsePred() {
        return false;
    }
};

class Signal: private Timer {
public:
    Signal(uint32_t minLongSignalLength) :
            minLongSignalLength(minLongSignalLength) {
    }

    using Timer::begin;

    SignalType end() {
        Timer::end();

        if (Timer::duration() > minLongSignalLength) {
            return LONG_SIGNAL;
        } 
        if (Timer::duration() > maxBounceLength) {
            return SHORT_SIGNAL;
        }

        return BOUNCE;
    }

private:
    uint32_t maxBounceLength = 10;
    uint32_t minLongSignalLength;
};

class SignalListener {
public:
    SignalType listen() {
        Signal signal(1500);

        TimeoutResult signalBeginsBeforeTimeout = Timeout(&isButtonPressed, 30000).wait();

        if (signalBeginsBeforeTimeout == EXPIRED) {
            return TIMEOUT;
        }

        signal.begin();
        Timeout(&isButtonReleased).wait();
        SignalType signalType = signal.end();

        if (signalType == BOUNCE) {
            return listen();
        } 
        return signalType;
    }

private:
    static bool isButtonPressed() {
        return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15) == 0;
    }

    static bool isButtonReleased() {
        return !isButtonPressed();
    }
};

class Lock {
public:
    LockResponse tryUnlock(SignalType signal) {
        if (isCorrectInput(signal)) {
            pinPosition++;
            if (isOpen()) {
                reset();
                return OPEN;
            }
            return CORRECT;
        } 
        
        currentWrongAttempts++;
        if (isBlocked()) {
            reset();
            return BLOCKED;
        } 
        return WRONG;
    }

    void reset() {
        pinPosition = 0;
        currentWrongAttempts = 0;
    }

private:
    uint8_t pinPosition = 0;
    SignalType code[8] = {SHORT_SIGNAL, LONG_SIGNAL, SHORT_SIGNAL, LONG_SIGNAL,
                          SHORT_SIGNAL, LONG_SIGNAL, SHORT_SIGNAL, LONG_SIGNAL};
    uint8_t wrongAttemptsAvailable = 3;
    uint8_t currentWrongAttempts = 0;

    bool isOpen() {
        return pinPosition == 8;
    }

    bool isBlocked() {
        return currentWrongAttempts == wrongAttemptsAvailable;
    }

    bool isCorrectInput(SignalType signal) {
        return code[pinPosition] == signal;
    }

};

class LampControl {
public:
    void open() {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);
        Delay(10000).wait();
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
        isSessionStarted = false;
    }

    void correct() {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
        Delay(500).wait();
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
        isSessionStarted = true;
    }

    void wrong() {
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
        Delay(500).wait();
        HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
    }

    void blocked() {
        for (int i = 0; i < 10; ++i) {
            wrong();
            Delay(500).wait();
        }
        isSessionStarted = false;
    }

    void reset() {
        if (isSessionStarted) {
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
            Delay(5000).wait();
            HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
            isSessionStarted = false;
        }
    }

private:
    bool isSessionStarted = false;
};

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
    /* USER CODE BEGIN 1 */
//	initialise_monitor_handles();
    SignalListener signalListener;
    Lock lock;
    LampControl lampControl;

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
    /* USER CODE BEGIN 2 */

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1) {
//		printf("Hello world!\n");
        SignalType signal = signalListener.listen();
        if (signal == TIMEOUT) {
            lock.reset();
            lampControl.reset();
        } else {
            switch (lock.tryUnlock(signal)) {
                case CORRECT:
                    lampControl.correct();
                    break;
                case WRONG:
                    lampControl.wrong();
                    break;
                case OPEN:
                    lampControl.open();
                    break;
                case BLOCKED:
                    lampControl.blocked();
                    break;
            }
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
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /** Configure the main internal regulator output voltage
     */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                  | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1) {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
