/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include "app/spectrum.h"
#include "app/radio.h"
#include "driver/bk4819.h"
#include "driver/delay.h"
#include "driver/key.h"
#include "driver/pins.h"
#include "helper/helper.h"
#include "radio/channels.h"
#include "radio/settings.h"
#include "ui/gfx.h"
#include "ui/helper.h"
#include "ui/main.h"

#ifdef UART_DEBUG
	#include "driver/uart.h"
	#include "external/printf/printf.h"
#endif

//static uint32_t StartFreq;
uint16_t RegisterBackup[1];
uint32_t CurrentFreq;
uint32_t FreqMin;
uint32_t FreqMax;
uint8_t CurrentModulation;
uint8_t CurrentFreqStepIndex;
uint32_t CurrentFreqStep;
uint32_t CurrentFreqChangeStep;
uint8_t CurrentStepCountIndex;
uint8_t CurrentStepCount;
uint16_t CurrentScanDelay;
uint16_t RssiValue[128] = {0};
bool bExit;
KEY_t Key;
KEY_t LastKey = KEY_NONE;

const char* StepStrings[] = {
	"0.25K",
	"1.25K",
	"2.5K ",
	"5K   ",
	"6.25K",
	"10K  ",
	"12.5k",
	"20K  ",
	"25K  ",
	"50K  ",
	"100K ",
	"500K ",
	"1M   ",
	"5M   "
};

uint8_t bDebug = 0;


void DrawLabels(void){
	uint32_t Divider = 10000000U;
	uint8_t X = 30;
	uint8_t Y = 65;

	for (uint8_t i = 0; i < 8; i++) {
		UI_DrawBigDigit(X, Y, (CurrentFreq / Divider) % 10U);
		Divider /= 10;
		if (i == 2) {
			X += 16;
		} else {
			X += 12;
		}
	}

	Int2Ascii(FreqMin, 8);
	UI_DrawSmallString(2, 2, gShortString, 8);
	Int2Ascii(FreqMax, 8);
	UI_DrawSmallString(110, 2, gShortString, 8);

	Int2Ascii(CurrentStepCount, 3);
	UI_DrawSmallString(2, 74, gShortString, 3);

	Int2Ascii(CurrentScanDelay, 2);
	UI_DrawSmallString(142, 74, gShortString, 2);

	UI_DrawSmallString(2, 64, StepStrings[CurrentFreqStepIndex], 5);

	Int2Ascii(CurrentFreqChangeStep, 5);
	UI_DrawSmallString(60, 2, gShortString, 5);
}

void SetFreqMinMax(void){
	CurrentFreqChangeStep = CurrentFreqStep*(CurrentStepCount >> 1);
	FreqMin = CurrentFreq - CurrentFreqChangeStep;
	FreqMax = CurrentFreq + CurrentFreqChangeStep;

#ifdef UART_DEBUG
	Int2Ascii(CurrentFreqChangeStep, 8);
	UART_printf("Delta: ");
	UART_printf(gShortString);
	UART_printf("   ");

	Int2Ascii(FreqMin, 8);
	UART_printf("Min: ");
	UART_printf(gShortString);
	UART_printf("   ");

	Int2Ascii(FreqMax, 8);
	UART_printf("Max: ");
	UART_printf(gShortString);
	UART_printf("   ");
#endif
}

void SetStepCount(void){
	CurrentStepCount = 128 >> CurrentStepCountIndex;
}

void IncrementStepIndex(void){
	CurrentStepCountIndex = (CurrentStepCountIndex + 1) % STEPS_COUNT;
	SetStepCount();
	SetFreqMinMax();
	DrawLabels();
}

void IncrementFreqStepIndex(void){
	CurrentFreqStepIndex = (CurrentFreqStepIndex + 1) % 10;
	CurrentFreqStep = FREQUENCY_GetStep(CurrentFreqStepIndex);
	DrawLabels();
#ifdef UART_DEBUG
	Int2Ascii(CurrentFreqStepIndex, 2);
	UART_printf("Step Index: ");
	UART_printf(gShortString);
	UART_printf("   ");

	Int2Ascii(CurrentFreqStep, 8);
	UART_printf("Step: ");
	UART_printf(gShortString);
	UART_printf("   ");
#endif
}

void IncrementScanDelay(void){
	CurrentScanDelay = (CurrentScanDelay + 5) % 45;
	DrawLabels();
}

void ChangeCenterFreq(uint8_t Up){
	if (Up) {
		CurrentFreq += CurrentFreqChangeStep;
	} else {
		CurrentFreq -= CurrentFreqChangeStep;
	}
	SetFreqMinMax();
	DrawLabels();
}

void DrawBars(){
	uint16_t Power;
	uint8_t MaxBarHeight = 40;
	uint8_t BarWidth = 128 / CurrentStepCount;
	for (uint8_t i = 0; i < CurrentStepCount; i++) {
//	Todo: Blacklist
//		if (blacklist[i]) {
//			continue;
//		}
//		Valid range 72-330, converted to 0-100, scaled to % based on MaxBarHeight to fit on screen.
//		Not optimized to keep readable.  Todo: Simplify equation.
		//Power = (((RssiValue[i]-72)*100)/258)*(MaxBarHeight/100); 
		Power = (((RssiValue[i]-72)*100)/258)*.4;
		DISPLAY_DrawRectangle1(16+(i * BarWidth), 15, Power, BarWidth, COLOR_FOREGROUND);
		DISPLAY_DrawRectangle1(16+(i * BarWidth), 15 + Power, MaxBarHeight - Power, BarWidth, COLOR_BACKGROUND);
	}
} 

void StopSpectrum(void)
{
	BK4819_WriteRegister(0x7E, RegisterBackup[0]);

	SCREEN_TurnOn();

	if (gSettings.WorkMode) {
		CHANNELS_LoadChannel(gSettings.VfoChNo[!gSettings.CurrentVfo], !gSettings.CurrentVfo);
		CHANNELS_LoadChannel(gSettings.VfoChNo[gSettings.CurrentVfo], gSettings.CurrentVfo);
	} else {
		CHANNELS_LoadChannel(gSettings.CurrentVfo ? 999 : 1000, !gSettings.CurrentVfo);
		CHANNELS_LoadChannel(gSettings.CurrentVfo ? 1000 : 999, gSettings.CurrentVfo);
	}

	RADIO_Tune(gSettings.CurrentVfo);
	UI_DrawMain(true);
}

void CheckKeys(void){
	Key = KEY_GetButton();
	if (Key != LastKey) {
		switch (Key) {
			case KEY_NONE:
				break;
			case KEY_EXIT:
				StopSpectrum();
				bExit = TRUE;
				return;
				break;
			case KEY_MENU:
				break;
			case KEY_UP:
				ChangeCenterFreq(TRUE);
				break;
			case KEY_DOWN:
				ChangeCenterFreq(FALSE);
				break;
			case KEY_1:
				IncrementStepIndex();
				break;
			case KEY_2:
				break;
			case KEY_3:
				IncrementScanDelay();
				break;
			case KEY_4:
				IncrementFreqStepIndex();
				break;
			case KEY_5:
				break;
			case KEY_6:
				break;
			case KEY_7:
				break;
			case KEY_8:
				BK4819_ToggleAGCMode(FALSE);
				break;
			case KEY_9:
				BK4819_ToggleAGCMode(TRUE);
				break;
			case KEY_0:
				bDebug ^= 1;
				break;
			case KEY_HASH:
				break;
			case KEY_STAR:
				break;
			default:
				break;
		}

		LastKey = Key;
	}
}

void Spectrum_Loop(void){
	uint32_t FreqToCheck;

	DrawLabels();

	while (1) {
		FreqToCheck = FreqMin;

//	These are used in radio.c when the frequency is set.  Some combination is needed in the loop.
//			BK4819_SetSquelchGlitch(FALSE);
//			BK4819_SetSquelchNoise(FALSE);
//			BK4819_SetSquelchRSSI(FALSE);
//			BK4819_EnableRX();
//			BK4819_SetFilterBandwidth(FALSE);
//			BK4819_EnableFilter(false);

		for (uint8_t i = 0; i < CurrentStepCount; i++) {

			BK4819_set_rf_frequency(FreqToCheck, TRUE);
			//BK4819_SetFrequency(FreqToCheck);
			//BK4819_WriteRegister(0x30, 0x0200);
			//BK4819_WriteRegister(0x30, 0xBFF1);

			DELAY_WaitMS(CurrentScanDelay);

			RssiValue[i] = BK4819_GetRSSI();

			//-----------------------Test prints - remove
			if (bDebug) {
				Int2Ascii(FreqToCheck, 8);
				UI_DrawSmallString(2, 50, gShortString, 8);
				Int2Ascii(RssiValue[i], 8);
				UI_DrawSmallString(60, 50, gShortString, 8);
			}
			//------------------------end test

			FreqToCheck += CurrentFreqStep;

			CheckKeys();
			if (bExit){
				return;
			}
		}
		DrawBars();
	}
}

void APP_Spectrum(void){
	bExit = FALSE;

	RegisterBackup[0] = BK4819_ReadRegister(0x7E);

	CurrentFreq = gVfoState[gSettings.CurrentVfo].RX.Frequency;
	CurrentModulation = gVfoState[gSettings.CurrentVfo].gModulationType;
	CurrentFreqStepIndex = gSettings.FrequencyStep;
	CurrentFreqStep = FREQUENCY_GetStep(CurrentFreqStepIndex);
	CurrentStepCountIndex = STEPS_16;
	CurrentScanDelay = 15;

	SetStepCount();
	SetFreqMinMax(); 

	for (int i = 0; i < 8; i++) {
		gShortString[i] = ' ';
	}

#ifdef UART_DEBUG
	Int2Ascii(CurrentFreqStepIndex, 2);
	UART_printf("Step Index: ");
	UART_printf(gShortString);
	UART_printf("   ");

	Int2Ascii(CurrentFreqStep, 8);
	UART_printf("Step: ");
	UART_printf(gShortString);
	UART_printf("   ");
#endif
	
	DISPLAY_Fill(0, 159, 1, 81, COLOR_BACKGROUND);
	Spectrum_Loop();
}


//---------------------------------------------------------------------------------------------
// From fagci for reference - remove later
//bool IsCenterMode() { return settings.scanStepIndex < STEP_1_0kHz; }

//uint8_t GetStepsCount() { return 128 >> settings.stepsCount; }

//uint16_t GetScanStep() { return StepFrequencyTable[settings.scanStepIndex]; }

//uint32_t GetFreqStart() { return IsCenterMode() ? currentFreq - (GetBW() >> 1) : currentFreq; }

//uint32_t GetFreqEnd() { return currentFreq + GetBW(); }
//---------------------------------------------------------------------------------------------
