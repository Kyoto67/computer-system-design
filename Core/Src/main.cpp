/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.cpp
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
#include "usart.h"
#include <cstdint>
#include <cctype>

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
enum LockResponse {
	CORRECT, WRONG, OPEN, BLOCKED
};

enum TimeoutResult {
	OK, EXPIRED
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

class Timeout: private Timer {
public:
	Timeout(bool (*pred)(void), uint32_t time = UINT32_MAX) :
			predicate(pred), timeout(time) {
	}

	TimeoutResult wait() {
		Timer::begin();
		while (!predicate()) {
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

class Delay: private Timeout {
public:
	Delay(uint32_t time = UINT32_MAX) :
			Timeout(&falsePred, time) {
	}

	using Timeout::wait;

private:
	static bool falsePred() {
		return false;
	}
};

class Lock {
public:
	LockResponse tryUnlock(char input) {
		if (isCorrectInput(input)) {
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
		resetPinPosition();
		return WRONG;
	}

	void reset() {
		resetPinPosition();
		resetCurrentWrongAttempts();
	}

private:
	uint8_t pinPosition = 0;
	char code[8] = { 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z' };
	uint8_t wrongAttemptsAvailable = 3;
	uint8_t currentWrongAttempts = 0;

	bool isOpen() {
		return pinPosition == 8;
	}

	bool isBlocked() {
		return currentWrongAttempts == wrongAttemptsAvailable;
	}

	bool isCorrectInput(char input) {
		return std::tolower(code[pinPosition]) == std::tolower(input);
	}

	void resetPinPosition() {
		pinPosition = 0;
	}

	void resetCurrentWrongAttempts() {
		currentWrongAttempts = 0;
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


class RingBuffer {
public:
    static const int MAX_CAPACITY = 256;


    char* push(bool* result) {
        if(isFull()) {
            *result = false;
            return nullptr;
        }
        *result = true;
        return &buffer[tail++];
    }

    char* pop(bool* result) {
        if(isEmpty()) {
            *result = false;
            return nullptr;
        }

        *result = true;
        return &buffer[head++];
    }

    bool isEmpty() const {
        return head == tail;
    }

    bool isFull() const {
        return tail + 1 == head;
    }

private:
    uint8_t head = 0, tail = 0;
    char buffer[MAX_CAPACITY];
};


class Input {
public:
	bool readChar() {
        char buf;
        if(HAL_OK == HAL_UART_Receive(&huart6, (uint8_t*) &buf, 1, 1)) {
            bool result = true;
            *buffer.push(&result) = buf;
            return result;
        }
        return false;
	}

	char getChar() {
        bool ret = false;
        char c;
        while(!ret) {
        	c = *buffer.pop(&ret);
        }
		return c;
	}

private:
	RingBuffer buffer;
};

class Output {
public:
	bool printChar(char c) {
        bool result = true;
        *buffer.push(&result) = c;
        return HAL_OK == HAL_UART_Transmit(&huart6, (uint8_t*) buffer.pop(&result), 1, 10) && result;
	}


private:
    RingBuffer buffer;
};

class Session {
public:
	void ensureForSessionStarted() {
		if (!isSessionStarted) {
			sessionStart = HAL_GetTick();
		}
		isSessionStarted = true;
	}

	uint32_t getSessionDuration() {
		if (isSessionStarted) {
			return HAL_GetTick() - sessionStart;
		} else {
			return 0;
		}
	}

	bool isSessionTimeouted() {
		return getSessionDuration() > timeout;
	}

	void abortSession() {
		isSessionStarted = false;
	}

private:
	bool isSessionStarted;
	uint32_t sessionStart;
	uint32_t timeout;
};

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */
// initialise_monitor_handles();
	Lock lock;
	LampControl lampControl;
	Input input;
	Output output;
	Session session;

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
	MX_USART6_UART_Init();
	/* USER CODE BEGIN 2 */
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		if (input.readChar()) {
			char c = input.getChar();
			output.printChar(c);
			session.ensureForSessionStarted();
			switch (lock.tryUnlock(c)) {
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
		} else {
			if (session.isSessionTimeouted()) {
				session.abortSession();
				lock.reset();
				lampControl.reset();
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
