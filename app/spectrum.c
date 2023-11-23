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

uint16_t RegisterBackup[1];
uint32_t CurrentFreq;
uint8_t CurrentFreqIndex;
uint32_t FreqCenter;
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

uint16_t COLOR_BAR;

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

void DrawCurrentFreq(void){
	uint32_t Divider = 10000000U;
	uint8_t X = 38;
	uint8_t Y = 65;

	for (uint8_t i = 0; i < 7; i++) {
		UI_DrawBigDigit(X, Y, (CurrentFreq / Divider) % 10U);
		Divider /= 10;
		if (i == 2) {
			X += 16;
		} else {
			X += 12;
		}
	}
}

void DrawLabels(void){
	Int2Ascii(FreqMin / 10, 7);
	UI_DrawSmallString(2, 2, gShortString, 8);
	Int2Ascii(FreqMax / 10, 7);
	UI_DrawSmallString(116, 2, gShortString, 8);

	gShortString[2] = ' ';
	Int2Ascii(CurrentStepCount, (CurrentStepCount > 100) ? 3 : 2);
	UI_DrawSmallString(2, 76, gShortString, 3);

	Int2Ascii(CurrentScanDelay, 2);
	UI_DrawSmallString(142, 74, gShortString, 2);

	UI_DrawSmallString(2, 64, StepStrings[CurrentFreqStepIndex], 5);

	Int2Ascii(CurrentFreqChangeStep, 5);
	UI_DrawSmallString(68, 2, gShortString, 5);
}

void SetFreqMinMax(void){
	CurrentFreqChangeStep = CurrentFreqStep*(CurrentStepCount >> 1);
	FreqMin = FreqCenter - CurrentFreqChangeStep;
	FreqMax = FreqCenter + CurrentFreqChangeStep;
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
}

void IncrementScanDelay(void){
	CurrentScanDelay = (CurrentScanDelay + 5) % 45;
	DrawLabels();
}

void ChangeCenterFreq(uint8_t Up){
	if (Up) {
		FreqCenter += CurrentFreqChangeStep;
	} else {
		FreqCenter -= CurrentFreqChangeStep;
	}
	SetFreqMinMax();
	DrawLabels();
}

void DrawBars(uint16_t RssiLow, uint16_t RssiHigh){
	uint16_t Power;
	uint8_t MaxBarHeight = 40;
	uint8_t BarWidth = 128 / CurrentStepCount;

	RssiLow = RssiLow - (RssiLow % 10); // Round down to nearest 10
	RssiHigh = (RssiHigh - (RssiHigh % 20)) + 20; //Round up to nearest 20

	for (uint8_t i = 0; i < CurrentStepCount; i++) {
//	Todo: Blacklist
//		if (blacklist[i]) {
//			continue;
//		}
//		Valid range 72-330, converted to 0-100, scaled to % based on MaxBarHeight to fit on screen.
//		Not optimized to keep readable.  Todo: Simplify equation.
		//Power = (((RssiValue[i]-72)*100)/258)*(MaxBarHeight/100); 
		//Power = (((RssiValue[i]-72)*100)/258)*.4;
		Power = (((RssiValue[i] - RssiLow) * 100) / (RssiHigh - RssiLow)) * .4; //(MaxBarHeight / 100); 
		if (Power > MaxBarHeight) {
			Power = MaxBarHeight;
		}
		COLOR_BAR = COLOR_BACKGROUND;
		if (Power <= 1) {COLOR_BAR   = COLOR_RGB(0,  0,  63);}
		if (Power > 1) {COLOR_BAR   = COLOR_RGB(0,  5,  63);}
		if (Power > 2) {COLOR_BAR   = COLOR_RGB(0,  10,  63);}
		if (Power > 3) {COLOR_BAR   = COLOR_RGB(0,  15,  63);}
		if (Power > 4) {COLOR_BAR   = COLOR_RGB(0,  20,  63);}
		if (Power > 5) {COLOR_BAR   = COLOR_RGB(0,  25,  63);}
		if (Power > 6) {COLOR_BAR   = COLOR_RGB(0,  30,  63);}
		if (Power > 7) {COLOR_BAR   = COLOR_RGB(0,  35,  60);}
		if (Power > 8) {COLOR_BAR   = COLOR_RGB(0,  40,  55);}
		if (Power > 9) {COLOR_BAR   = COLOR_RGB(0,  45,  50);}
		if (Power > 10) {COLOR_BAR   = COLOR_RGB(0,  50,  45);}
		if (Power > 11) {COLOR_BAR   = COLOR_RGB(0,  55,  40);}
		if (Power > 12) {COLOR_BAR   = COLOR_RGB(0,  60,  35);}
		if (Power > 13) {COLOR_BAR   = COLOR_RGB(0,  63,  30);}
		if (Power > 14) {COLOR_BAR   = COLOR_RGB(0,  63,  25);}
		if (Power > 15) {COLOR_BAR   = COLOR_RGB(0,  63,  20);}
		if (Power > 16) {COLOR_BAR   = COLOR_RGB(0,  63,  15);}
		if (Power > 17) {COLOR_BAR   = COLOR_RGB(0,  63,  10);}
		if (Power > 18) {COLOR_BAR   = COLOR_RGB(0,  63,  5);}
		if (Power > 19) {COLOR_BAR   = COLOR_RGB(0,  63,  0);}
		if (Power > 20) {COLOR_BAR   = COLOR_RGB(0,  63,  0);}
		if (Power > 21) {COLOR_BAR   = COLOR_RGB(5,  63,  0);}
		if (Power > 22) {COLOR_BAR   = COLOR_RGB(10,  63,  0);}
		if (Power > 23) {COLOR_BAR   = COLOR_RGB(15,  63,  0);}
		if (Power > 24) {COLOR_BAR   = COLOR_RGB(20,  63,  0);}
		if (Power > 25) {COLOR_BAR   = COLOR_RGB(25,  63,  0);}
		if (Power > 26) {COLOR_BAR   = COLOR_RGB(30,  63,  0);}
		if (Power > 27) {COLOR_BAR   = COLOR_RGB(35,  60,  0);}
		if (Power > 28) {COLOR_BAR   = COLOR_RGB(40,  55,  0);}
		if (Power > 29) {COLOR_BAR   = COLOR_RGB(45,  50,  0);}
		if (Power > 30) {COLOR_BAR   = COLOR_RGB(50,  45,  0);}
		if (Power > 31) {COLOR_BAR   = COLOR_RGB(55,  40,  0);}
		if (Power > 32) {COLOR_BAR   = COLOR_RGB(60,  35,  0);}
		if (Power > 33) {COLOR_BAR   = COLOR_RGB(63,  30,  0);}
		if (Power > 34) {COLOR_BAR   = COLOR_RGB(63,  25,  0);}
		if (Power > 35) {COLOR_BAR   = COLOR_RGB(63,  20,  0);}
		if (Power > 36) {COLOR_BAR   = COLOR_RGB(63,  15,  0);}
		if (Power > 37) {COLOR_BAR   = COLOR_RGB(63,  10,  0);}
		if (Power > 38) {COLOR_BAR   = COLOR_RGB(63,  5,  0);}
		if (Power > 39) {COLOR_BAR   = COLOR_RGB(63,  0,  0);}
		DISPLAY_DrawRectangle1(16+(i * BarWidth), 15, Power, BarWidth, (i == CurrentFreqIndex) ? COLOR_RGB(25,  25,  25) : COLOR_BAR);
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
	uint16_t RssiLow;
	uint16_t RssiHigh;

	DrawLabels();

	while (1) {
		FreqToCheck = FreqMin;
		RssiLow = 330;
		RssiHigh = 72;
		CurrentFreqIndex = 0;
		CurrentFreq = FreqMin;

		for (uint8_t i = 0; i < CurrentStepCount; i++) {

			BK4819_set_rf_frequency(FreqToCheck, TRUE);
			//BK4819_SetFrequency(FreqToCheck);
			//BK4819_WriteRegister(0x30, 0x0200);
			//BK4819_WriteRegister(0x30, 0xBFF1);

			DELAY_WaitMS(CurrentScanDelay);

			RssiValue[i] = BK4819_GetRSSI();

			if (RssiValue[i] < RssiLow) {
				RssiLow = RssiValue[i];
			} else if (RssiValue[i] > RssiHigh) {
				RssiHigh = RssiValue[i];
			}

			if (RssiValue[i] > RssiValue[CurrentFreqIndex]) {
				CurrentFreqIndex = i;
				CurrentFreq = FreqToCheck;
			}

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
		#ifdef UART_DEBUG
			Int2Ascii(CurrentFreqIndex, 8);
			UART_printf("Current Freq Index: ");
			UART_printf(gShortString);
			UART_printf("   ");

			Int2Ascii(CurrentFreq, 8);
			UART_printf("Current Freq: ");
			UART_printf(gShortString);
			UART_printf("   ");
		#endif
		DrawCurrentFreq();
		DrawBars(RssiLow, RssiHigh);
	}
}

void APP_Spectrum(void){
	bExit = FALSE;

	RegisterBackup[0] = BK4819_ReadRegister(0x7E);

	FreqCenter = gVfoState[gSettings.CurrentVfo].RX.Frequency;
	CurrentModulation = gVfoState[gSettings.CurrentVfo].gModulationType;
	CurrentFreqStepIndex = gSettings.FrequencyStep;
	CurrentFreqStep = FREQUENCY_GetStep(CurrentFreqStepIndex);
	CurrentStepCountIndex = STEPS_64;
	CurrentScanDelay = 10;

	SetStepCount();
	SetFreqMinMax(); 

	for (int i = 0; i < 8; i++) {
		gShortString[i] = ' ';
	}
	
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
