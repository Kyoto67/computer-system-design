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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <string>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define CLOCK_SCALED_FREQUENCY    1000000        // frequency after scaling with PSC (supposed to be same on every timer in use)
#define LED_PWM_FREQUENCY        500
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
class UartDriver {
public:
	bool recv(char *c) {
		return HAL_OK == HAL_UART_Receive(&huart6, (uint8_t*) c, 1, 0);
	}

	bool send(char c) {
		return HAL_OK == HAL_UART_Transmit(&huart6, (uint8_t*) &c, 1, 1);
	}

} DRIVER;

class Writer {
public:
	static void printChar(char c) {
		while (!DRIVER.send(c))
			;
	}

	static void printString(char *arr, uint32_t size) {
		for (uint32_t i = 0; i < size; i++) {
			printChar(arr[i]);
		}
	}

	static void printString(std::string str) {
		for (uint32_t i = 0; i < str.size(); i++) {
			printChar(str[i]);
		}
	}

	static void printNumber(uint32_t number) {
		if (number < 0) {
			printChar('-');
			number = -number;
		}

		std::string numStr = std::to_string(number);
		printString(numStr);
	}
};

class Reader {
public:
	static void tick() {
		isExistsUnread = DRIVER.recv(&input) || isExistsUnread;
	}

	static bool canRead() {
		return isExistsUnread;
	}

	static char read() {
		isExistsUnread = false;
		return input;
	}

private:
	static bool isExistsUnread;
	static char input;
};

bool Reader::isExistsUnread = false;
char Reader::input = 0;

class SoundDriver {
public:
	static void play_sound(uint32_t frequency) {
		htim1.Instance->ARR = (1000000 / (frequency)) - 1; // Set The PWM Frequency
		htim1.Instance->CCR1 = (htim1.Instance->ARR >> 1); // Set Duty Cycle 50%
	}

	static void mute() {
		htim1.Instance->CCR1 = 0;
	}

};

class LedDriver {
public:

	enum LED {
		GREEN, YELLOW, RED
	};

	static void disable_all_leds(void) {
		htim4.Instance->CCR2 = 0;
		htim4.Instance->CCR3 = 0;
		htim4.Instance->CCR4 = 0;
	}

	static void light_led(LED led, uint8_t brightness) {
		disable_all_leds();
		uint16_t ccr_value = CLOCK_SCALED_FREQUENCY / LED_PWM_FREQUENCY
				* (brightness) / 100;
		switch (led) {
		case GREEN:
			htim4.Instance->CCR2 = ccr_value;
			break;
		case YELLOW:
			htim4.Instance->CCR3 = ccr_value;
			break;
		case RED:
			htim4.Instance->CCR4 = ccr_value;
			break;
		}
	}

};

enum Impulse {
	UNKNOWN, Q, W, E, A, S, D, Z, X, C
};

enum RoundResult {
	WRONG, CORRECT, TIMEOUT, BREAK
};

class MusicImpulseSequence {
public:
	bool hasNext() {
		return position < 20;
	}

	Impulse next() {
		return baseSequence[position++];
	}

private:
	uint32_t position = 0;
	Impulse baseSequence[20] = { Q, W, E, A, S, D, Z, X, C, Q, W, E, A, S, D, Z,
			X, C, Q, W };
};

class ImpulseResolver {
public:
	static Impulse resolveFor(char c) {
		switch (c) {
		case '0':
			return Q;
		case '1':
			return W;
		case '2':
			return E;
		case '3':
			return A;
		case '4':
			return S;
		case '5':
			return D;
		case '6':
			return Z;
		case '7':
			return X;
		case '8':
			return C;
		default:
			return UNKNOWN;
		}
	}
};

enum ImpulsePlayerAction {
	none,
	red20,
	red50,
	red100,
	yellow20,
	yellow50,
	yellow100,
	green20,
	green50,
	green100,
	sound1,
	sound2,
	sound3,
	sound4,
	sound5,
	sound6,
	sound7,
	sound8,
	sound9
};

class ImpulsePlayerActionsResolver {
public:
	static ImpulsePlayerAction resolveLedActionFor(Impulse impulse) {
		switch (impulse) {
		case Q:
			return red20;
		case W:
			return red50;
		case E:
			return red100;
		case A:
			return yellow20;
		case S:
			return yellow50;
		case D:
			return yellow100;
		case Z:
			return green20;
		case X:
			return green50;
		case C:
			return green100;
		default:
			return none;
		}
	}

	static ImpulsePlayerAction resolveSoundActionFor(Impulse impulse) {
		switch (impulse) {
		case Q:
			return sound1;
		case W:
			return sound2;
		case E:
			return sound3;
		case A:
			return sound4;
		case S:
			return sound5;
		case D:
			return sound6;
		case Z:
			return sound7;
		case X:
			return sound8;
		case C:
			return sound9;
		default:
			return none;
		}
	}
};

class LedActionHandler {
public:
	static void handle(ImpulsePlayerAction action) {
		switch (action) {
		case red20:
			showRed20();
			break;
		case red50:
			showRed50();
			break;
		case red100:
			showRed100();
			break;
		case yellow20:
			showYellow20();
			break;
		case yellow50:
			showYellow50();
			break;
		case yellow100:
			showYellow100();
			break;
		case green20:
			showGreen20();
			break;
		case green50:
			showGreen50();
			break;
		case green100:
			showGreen100();
			break;
		default:
			break;
		}
	}

	static void turnOff() {
		LedDriver::disable_all_leds();
	}

private:
	static void showRed20() {
		LedDriver::light_led(LedDriver::LED::RED, 20);
	}

	static void showRed50() {
		LedDriver::light_led(LedDriver::LED::RED, 50);
	}

	static void showRed100() {
		LedDriver::light_led(LedDriver::LED::RED, 100);
	}

	static void showGreen20() {
		LedDriver::light_led(LedDriver::LED::GREEN, 20);
	}

	static void showGreen50() {
		LedDriver::light_led(LedDriver::LED::GREEN, 50);
	}

	static void showGreen100() {
		LedDriver::light_led(LedDriver::LED::GREEN, 100);
	}

	static void showYellow20() {
		LedDriver::light_led(LedDriver::LED::YELLOW, 20);
	}

	static void showYellow50() {
		LedDriver::light_led(LedDriver::LED::YELLOW, 50);
	}

	static void showYellow100() {
		LedDriver::light_led(LedDriver::LED::YELLOW, 100);
	}
};

class SoundActionHandler {
public:
	static void handle(ImpulsePlayerAction action) {
		switch (action) {
		case sound1:
			playSound1();
			break;
		case sound2:
			playSound2();
			break;
		case sound3:
			playSound3();
			break;
		case sound4:
			playSound4();
			break;
		case sound5:
			playSound5();
			break;
		case sound6:
			playSound6();
			break;
		case sound7:
			playSound7();
			break;
		case sound8:
			playSound8();
			break;
		case sound9:
			playSound9();
			break;
		default:
			break;
		}
	}

	static void stop() {
		SoundDriver::mute();
	}

private:
	static void playSound1() {
		SoundDriver::play_sound(100);
	}

	static void playSound2() {
		SoundDriver::play_sound(200);
	}

	static void playSound3() {
		SoundDriver::play_sound(300);
	}

	static void playSound4() {
		SoundDriver::play_sound(400);
	}

	static void playSound5() {
		SoundDriver::play_sound(500);
	}

	static void playSound6() {
		SoundDriver::play_sound(600);
	}

	static void playSound7() {
		SoundDriver::play_sound(700);
	}

	static void playSound8() {
		SoundDriver::play_sound(800);
	}

	static void playSound9() {
		SoundDriver::play_sound(900);
	}
};

class ImpulsePlayer {
public:
	void push(Impulse impulse) {
		queue.push_back(impulse);
	}

	void switchMode() {
		mode = (mode + 1) % 3;
	}

	void tick() {
		if (currentState == UNKNOWN) {
			if (!queue.empty()) {
				Impulse impulseToShow = queue.front();
				queue.pop_front();
				showImpulse(impulseToShow);
				currentState = impulseToShow;
				currentStateStartTimestamp = HAL_GetTick();
			}
		} else {
			uint32_t currentTimestamp = HAL_GetTick();
			if (currentTimestamp - currentStateStartTimestamp
					>= actionsLength) {

			}
		}
	}

private:
	uint8_t mode;
	std::deque<Impulse> queue;
	Impulse currentState = UNKNOWN;
	uint32_t currentStateStartTimestamp = 0;
	uint32_t actionsLength = 500;

	void showImpulse(Impulse impulse) {
		if (mode < 2) {
			LedActionHandler::handle(
					ImpulsePlayerActionsResolver::resolveLedActionFor(impulse));
		}
		if (mode != 1) {
			SoundActionHandler::handle(
					ImpulsePlayerActionsResolver::resolveSoundActionFor(
							impulse));
		}
	}

	void stopShow() {
		LedActionHandler::turnOff();
		SoundActionHandler::stop();
	}
};

class MusicGame {
public:
	MusicGame(MusicImpulseSequence seq, ImpulsePlayer ply) :
			musicImpulseSequence(seq), player(ply), currentDifficultyMode(0) {
	}

	void play() {
		HAL_Delay(3000);
		while (musicImpulseSequence.hasNext()) {
			Impulse roundImpulse = musicImpulseSequence.next();
			player.push(roundImpulse);
			RoundResult roundResult = nextRound(roundImpulse,
					calculateTimeout());
			if (roundResult == BREAK) {
				return;
			} else {
				roundResults.push_back(roundResult);
				points.push_back(calculatePointsFor(roundResult));
			}
		}
	}

	void switchDifficulty() {
		currentDifficultyMode = (currentDifficultyMode + 1)
				% DIFFICULTY_MODES_COUNT;
	}

	std::deque<uint32_t> getPoints() {
		return points;
	}

	std::deque<RoundResult> getRoundResults() {
		return roundResults;
	}

	void clear() {
		points.clear();
		roundResults.clear();
	}

private:
	MusicImpulseSequence musicImpulseSequence;
	ImpulsePlayer player;
	std::deque<uint32_t> points;
	std::deque<RoundResult> roundResults;

	const uint8_t DIFFICULTY_MODES_COUNT = 3;
	uint8_t currentDifficultyMode;
	const uint32_t defaultTimeout = 3000;
	const uint32_t defaultPoints = 777;

	uint32_t calculateTimeout() {
		uint32_t calculatedTimeout = defaultTimeout;
		for (int i = 0; i < currentDifficultyMode; i++) {
			calculatedTimeout /= 2;
		}
		return calculatedTimeout;
	}

	uint32_t calculatePoints() {
		uint32_t calculatedPoints = defaultPoints;
		for (int i = 0; i < currentDifficultyMode; i++) {
			calculatedPoints *= 2;
		}
		return calculatedPoints;
	}

	uint32_t calculatePointsFor(RoundResult roundResult) {
		return roundResult == CORRECT ? calculatePoints() : 0;
	}

	RoundResult nextRound(Impulse expectedImpulse, uint32_t timeout) {
		uint32_t startTime = HAL_GetTick();
		while (true) {
			if (Reader::canRead()) {
				char input = Reader::read();
				if (input == 10) {
					return BREAK;
				}
				Impulse impulse = ImpulseResolver::resolveFor(input);
				return impulse == expectedImpulse ? CORRECT : WRONG;
			} else {
				uint32_t currentTime = HAL_GetTick();
				if (currentTime - startTime > timeout) {
					return TIMEOUT;
				}
			}
		}
	}
};

std::string translateRoundResult(RoundResult roundResult) {
	switch (roundResult) {
	case CORRECT:
		return "верно";
	case WRONG:
		return "неверно";
	case TIMEOUT:
		return "не успел";
	default:
		return "undefined";
	}
}

void printStartGame() {
	Writer::printString("Игра начнётся через 3...");
	HAL_Delay(1000);
	Writer::printString("2...");
	HAL_Delay(1000);
	Writer::printString("1...");
	HAL_Delay(1000);
	Writer::printString("\nСтарт!\n");
}

void printSwitchMode() {
	Writer::printString("\nРежим взаимодействия переключён.\n");
}

void printSwitchDifficulty() {
	Writer::printString("\nСложность переключена.\n");
}

void printResults(std::deque<uint32_t> points,
		std::deque<RoundResult> roundResults) {
	Writer::printString("\nРезультаты игры:\n");
	for (size_t i = 0; i < points.size(); i++) {
		Writer::printString("Раунд ");
		Writer::printChar(49 + i);
		Writer::printString(": ");
		Writer::printString(translateRoundResult(roundResults[i]));
		Writer::printString(", очков ");
		Writer::printNumber(points[i]);
		Writer::printChar('\n');
	}
}

ImpulsePlayer player;

void set_timer_ms(uint32_t ms) {
    htim6.Instance->ARR = ms - 1;
    HAL_TIM_Base_Start_IT(&htim6);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM6) {
		Reader::tick();
		player.tick();
	}
}

//void stopTimer() {
//	HAL_TIM_Base_Stop_IT(&htim6);
//	htim6.Instance->ARR = 0;
//}


void init_led_pwm() {
	htim4.Instance->ARR = CLOCK_SCALED_FREQUENCY / LED_PWM_FREQUENCY;
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */
	MusicImpulseSequence musicImpulseSequence;
	MusicGame game = MusicGame(musicImpulseSequence, player);
	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_TIM4_Init();
	MX_TIM6_Init();
	MX_USART6_UART_Init();
	MX_TIM1_Init();
	/* USER CODE BEGIN 2 */

	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_4);

	HAL_TIM_Base_Start_IT(&htim1);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

//	stopTimer();
	init_led_pwm();
    set_timer_ms(10);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		if (Reader::canRead()) {
			char input = Reader::read();
			switch (input) {
			case 'a':
				player.switchMode();
				printSwitchMode();
				break;
			case 10:
				game.play();
				printResults(game.getPoints(), game.getRoundResults());
				game.clear();
				break;
			case '+':
				game.switchDifficulty();
				printSwitchDifficulty();
				break;
			default:
				Impulse neededToDemo = ImpulseResolver::resolveFor(input);
				player.push(neededToDemo);
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
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 15;
	RCC_OscInitStruct.PLL.PLLN = 72;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
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
