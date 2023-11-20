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

#ifndef RADIO_SPECTRUM_H
#define RADIO_SPECTRUM_H

enum {
  STEPS_128,
  STEPS_64,
  STEPS_32,
  STEPS_16,
  STEPS_COUNT,
};

enum STEP_Setting_t {
  STEP_0_01kHz,
  STEP_0_1kHz,
  STEP_0_5kHz,
  STEP_1_0kHz,
  STEP_2_5kHz,
  STEP_5_0kHz,
  STEP_6_25kHz,
  STEP_8_33kHz,
  STEP_10_0kHz,
  STEP_12_5kHz,
  STEP_25_0kHz,
  STEP_100_0kHz,
};

typedef enum STEP_Setting_t STEP_Setting_t;
typedef STEP_Setting_t ScanStep;

/*
typedef struct SpectrumSettings {
  StepsCount stepCount;
  ScanStep scanStepIndex;
  ModulationType modulationType;
} SpectrumSettings;
*/

void APP_Spectrum(void);

#endif