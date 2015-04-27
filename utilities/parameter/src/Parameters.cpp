/*
 * Copyright (C) 2015 Intel Corporation
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

#include "Parameters.hpp"

namespace intel_audio
{

const std::string &Parameters::gKeyCompressOffloadRouting = "key_compress_offload_routing";

const std::string &Parameters::gKeyAndroidMode = "android_mode";

const std::string &Parameters::gKeyMicMute = "mic_mute";

Parameters::Key<std::string>::const_bidirectionalKey Parameters::gKeyDevices = {
    "input_devices", "output_devices"
};

Parameters::Key<std::string>::const_bidirectionalKey Parameters::gKeyFlags = {
    "input_flags", "output_flags"
};

Parameters::Key<std::string>::const_bidirectionalKey Parameters::gKeyUseCases = {
    "input_sources", "output_usecase"
};

const std::string &Parameters::gKeyVoipBandType = "voip_band_type";

const std::string &Parameters::gKeyPreProcRequested = "pre_proc_requested";

}   // namespace intel_audio
