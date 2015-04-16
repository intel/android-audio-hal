/*
 * Copyright (C) 2013-2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "EffectHelper.hpp"

using std::string;

const string EffectHelper::mEffectsNameTable[] = {
    "Acoustic Echo Canceller",
    "Automatic Gain Control",
    "Noise Suppression",
    "Beam Forming",
    "Wind Noise Reduction"
};

const size_t EffectHelper::mEffectNameTableSize =
    (sizeof(EffectHelper::mEffectsNameTable) / sizeof((EffectHelper::mEffectsNameTable)[0]));

uint32_t EffectHelper::convertEffectNameToProcId(const std::string &name)
{
    size_t effectTableSize;
    for (effectTableSize = 0; effectTableSize < mEffectNameTableSize; effectTableSize++) {
        if (name == mEffectsNameTable[effectTableSize]) {

            return 1 << effectTableSize;
        }
    }
    return 0;
}
