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
typedef enum {
	SHORT_SIGNAL, LONG_SIGNAL, TIMEOUT
} SignalType;

typedef enum {
	CORRECT, WRONG, OPEN, BLOCKED
} LockResponse;

class Signal {
public:
	Signal(uint32_t minLongSignalLength) :
			startTime(0), endTime(0), minLongSignalLength(minLongSignalLength) {
	}

	void begin() {
		startTime = HAL_GetTick();
	}

	SignalType end() {
		endTime = HAL_GetTick();
		uint32_t duration = endTime - startTime;

		if (duration > minLongSignalLength) {
			return LONG_SIGNAL;
		} else {
			return SHORT_SIGNAL;
		}
	}

private:
	uint32_t startTime;
	uint32_t endTime;
	uint32_t minLongSignalLength;

};

class SignalListener {
public:

	SignalType listen() {
		Signal signal(1500);
		bool signalBeginsBeforeTimeout = waitForSignalBegin();
		if (!signalBeginsBeforeTimeout) {
			return TIMEOUT;
		}
		signal.begin();
		waitForSignalEnd();
		return signal.end();
	}

private:
	uint32_t timeout = 30000;

	bool waitForSignalBegin() {
		uint32_t idleBegin = HAL_GetTick();
		while (!isButtonPressed()) {
			uint32_t idleTime = HAL_GetTick() - idleBegin;
			if (idleTime > timeout) {
				return false;
			}
		}
		return true;
	}

	void waitForSignalEnd() {
		while (isButtonPressed()) {
			//wait
		}
	}

	bool isButtonPressed() {
		return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15) == 0;
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
			} else {
				return CORRECT;
			}
		} else {
			currentWrongAttempts++;
			if (isBlocked()) {
				reset();
				return BLOCKED;
			} else {
				return WRONG;
			}
		}
	}

	void reset() {
		pinPosition = 0;
		currentWrongAttempts = 0;
	}

private:
	uint8_t pinPosition = 0;
	SignalType code[8] = { SHORT_SIGNAL, LONG_SIGNAL, SHORT_SIGNAL, LONG_SIGNAL,
			SHORT_SIGNAL, LONG_SIGNAL, SHORT_SIGNAL, LONG_SIGNAL };
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
		delay(10000);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_RESET);
		isSessionStarted = false;
	}

	void correct() {
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
		delay(500);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
		isSessionStarted = true;
	}

	void wrong() {
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_SET);
		delay(500);
		HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);
	}

	void blocked() {
		for (int i = 0; i < 10; ++i) {
			wrong();
			delay(500);
		}
		isSessionStarted = false;
	}

	void reset() {
		if (isSessionStarted) {
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
			delay(5000);
			HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);
			isSessionStarted = false;
		}
	}

private:
	bool isSessionStarted = false;

	void delay(uint32_t duration) {
		uint32_t begin = HAL_GetTick();
		while ((HAL_GetTick() - begin) < duration) {
		}
	}
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
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

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
