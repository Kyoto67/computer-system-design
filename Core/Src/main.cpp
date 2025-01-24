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
#include <utility>
#include <deque>
#include <string>

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
char tmp;

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

class Timeout : private Timer {
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

class Delay : private Timeout {
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

    void setCode(const char newCode[8]) {
        reset();
        for (int i = 0; i < 8; ++i) {
            code[i] = newCode[i];
        }
    }

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

    uint8_t getCodeLen() {
        return sizeof(code) / sizeof(char) - 1;
    }

private:
    uint8_t pinPosition = 0;
    char code[9] = {'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 0};
    uint8_t wrongAttemptsAvailable = 3;
    uint8_t currentWrongAttempts = 0;

    bool isOpen() {
        return code[pinPosition] == 0;
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


class InterruptGuard {
public:
    InterruptGuard() : pmask(__get_PRIMASK()) {
        __disable_irq();
    }

    ~InterruptGuard() {
        __set_PRIMASK(pmask);
    }

private:
    uint32_t pmask;
};

class RingBuffer {
public:
    static const int MAX_CAPACITY = 256;

    void push(char c) {
        buffer.push_back(*c);
    }

    char pop() {
        if (buffer.empty()) {
            return 0;
        } else {
            char c = buffer.front();
            buffer.pop_front();
            return c;
        }
    }

    void push(std::string str) {
        for (s: str) {
            push(s);
        }
    }

    bool isEmpty() {
        return buffer.empty();
    }

    std::string flush() {
        std::string str = buffer(buffer.begin(), buffer.end());
        buffer.clear();
        return str;
    }

private:
    std::deque<char> buffer;
};

RingBuffer input;
RingBuffer output;

class UartDriver {
public:
    enum Mode {
        INT,
        BLOCK
    };

    void switchMode() {
        switch (current) {
            case INT:
                current = BLOCK;
                break;
            case BLOCK:
                current = INT;
                char dummy = 0;
                HAL_UART_Transmit_IT(huart, &dummy, 0);
                HAL_UART_Receive_IT(&huart6, (uint8_t * ) & tmp, 1);
                break;
        }
    }


    bool recv(char *c) {
        switch (current) {
            case INT: {
                if (input.isEmpty()) {
                    return false;
                } else {
                    *c = input.pop();
                    return true;
                }
            }
            case BLOCK:
                return HAL_OK == HAL_UART_Receive(&huart6, (uint8_t *) c, 1, 1);
            default:
                return false;
        }
    }

    bool send(char c) {
        switch (current) {
            case INT: {
                output.push(c);
                return true;
            }
            case BLOCK:
                return HAL_OK == HAL_UART_Transmit(&huart6, (uint8_t * ) & c, 1, 10);
            default:
                return false;
        }
    }

    Mode current = BLOCK;
} DRIVER;


class Printer {
public:
    static void printChar(char c) {
        while (!DRIVER.send(c));
    }

    static void printString(char *arr, uint32_t size) {
        for (uint32_t i = 0; i < size; i++) {
            printChar(arr[i]);
        }
    }

};

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    InterruptGuard guard{};
    input.push(tmp);
    if (DRIVER.current == UartDriver::INT) {
        HAL_UART_Receive_IT(&huart6, (uint8_t * ) & tmp, 1);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    InterruptGuard guard{};
    if (DRIVER.current == UartDriver::INT) {
        std::string out = output.flush();
        bool isSuccessful = false;
        while (isSuccessful) {
            const uint8_t *pData = reinterpret_cast<const uint8_t *>(out.c_str());
            isSuccessful = HAL_OK == HAL_UART_Transmit_IT(huart, pData, str.size());
        }
    }
}

class Session {
public:
    void recordActivity() {
        lastSessionActivity = HAL_GetTick();
        isSessionStarted = true;
    }

    uint32_t getDurationFromLastSessionActivity() {
        if (isSessionStarted) {
            return HAL_GetTick() - lastSessionActivity;
        } else {
            return 0;
        }
    }

    bool isSessionTimeouted() {
        return getDurationFromLastSessionActivity() > timeout;
    }

    void abortSession() {
        isSessionStarted = false;
    }

private:
    bool isSessionStarted;
    uint32_t lastSessionActivity;
    uint32_t timeout;
};

class Interactives {
public:
    bool askNewCode(char toFill[], uint8_t len, Session currentSession) {
        char inputInvite[] = "\nPlease, input the new passcode: ";
        char tmp[len];
        Printer::printString(inputInvite, sizeof(inputInvite) / sizeof(char));
        for (int i = 0; i < len; ++i) {
            char c;
            while (!DRIVER.recv(&c)) {
                if (currentSession.isSessionTimeouted()) {
                    currentSession.abortSession();
                    return false;
                }
            }
            currentSession.recordActivity();
            if (c == 10) {
                while (i < len) {
                    tmp[i] = 0;
                    i++;
                }
            } else {
                tmp[i] = c;
            }
        }
        char repeatNewPasscodeMessage[] = "\nThe new passcode will be: ";
        Printer::printString(repeatNewPasscodeMessage, sizeof(repeatNewPasscodeMessage) / sizeof(char));
        Printer::printString(tmp, sizeof(tmp) / sizeof(char));
        char confirmingMessage[] = "\nConfirm? (y/n): ";
        Printer::printString(confirmingMessage, sizeof(confirmingMessage) / sizeof(char));
        char c;
        while (!DRIVER.recv(&c));
        Printer::printChar(c);
        Printer::printChar('\n');
        if (c == 'y') {
            for (int i = 0; i < len; i++) {
                toFill[i] = tmp[i];
            }
            return true;
        } else {
            return false;
        }
    }

};

char modeSwitchedMessage[] = "\nMode switched to ";

class ToggleDriver {
public:
    bool isToggleActivated() {
        if (isButtonPressed()) {
            if (!buttonWasPressed) {
                buttonPressedFrom = HAL_GetTick();
            }
            buttonWasPressed = true;
        } else {
            if (buttonWasPressed) {
                buttonWasPressed = false;
                uint32_t currTime = HAL_GetTick();
                uint32_t buttonPressDuration = currTime - buttonPressedFrom;
                if (buttonPressDuration > bounceLengthBorder) {
                    return true;
                }
            }
        }
        return false;
    }

private:
    bool buttonWasPressed = false;
    uint32_t buttonPressedFrom = 0;
    const uint32_t bounceLengthBorder = 10;

    bool isButtonPressed() {
        return HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_15) == 0;
    }
};

enum UserInstruction {
    SET_PASSCODE, TRY_UNLOCK, TOGGLE_IT_MODE, NOTHING
};

class UserCommandDto {
public:
    UserCommandDto(UserInstruction instruction, char input) : instruction(instruction), input(input) {}

    char getUserInput() {
        return input;
    }

    UserInstruction getUserInstruction() {
        return instruction;
    }

private:
    UserInstruction instruction;
    char input;
};

class UserListener {
public:
    UserCommandDto listenCommand() {
        char c;
        if (DRIVER.recv(&c)) {
            DRIVER.send(c);
            if (c == '+') {
                return UserCommandDto(SET_PASSCODE, c);
            } else {
                return UserCommandDto(TRY_UNLOCK, c);
            }
        } else {
            if (toggleDriver.isToggleActivated()) {
                return UserCommandDto(TOGGLE_IT_MODE, c);
            }
        }
        return UserCommandDto(NOTHING, c);
    }


private:
    ToggleDriver toggleDriver;
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
    Session session;
    Interactives interactives;
    UserListener userListener;

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
    DRIVER.current = UartDriver::Mode::BLOCK;
    while (1) {
        UserCommandDto command = userListener.listenCommand();
        switch (command.getUserInstruction()) {
            case SET_PASSCODE:
                char newCode[9];
                if (interactives.askNewCode(newCode, lock.getCodeLen(), session)) {
                    lock.setCode(newCode);
                }
                session.abortSession();
                lampControl.reset();
                break;
            case TRY_UNLOCK:
                session.recordActivity();
                switch (lock.tryUnlock(command.getUserInput())) {
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
                break;
            case TOGGLE_IT_MODE:
                session.recordActivity();
                DRIVER.switchMode();
                Printer::printString(modeSwitchedMessage, sizeof(modeSwitchedMessage) / sizeof(char));
                if (DRIVER.ITMode) {
                    char mode[] = "INTERRUPT\n";
                    Printer::printString(mode, sizeof(mode) / sizeof(char));
                } else {
                    char mode[] = "POLLING\n";
                    Printer::printString(mode, sizeof(mode) / sizeof(char));
                }
                break;
            case NOTHING:
                if (session.isSessionTimeouted()) {
                    session.abortSession();
                    lock.reset();
                    lampControl.reset();
                }
                break;
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
