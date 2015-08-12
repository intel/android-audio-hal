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

#pragma once

#include <Direction.hpp>
#include <string>

namespace intel_audio
{

/**
 * Helper class to provide definition of parameter key recognized by Audio HAL.
 */
class Parameters
{
private:
    template <typename T>
    struct Key
    {
        typedef const T const_bidirectionalKey[Direction::gNbDirections];
    };

public:
    /**
     * Compress Offload routing key (e.g. for Compress Offload routing use case)
     * With this key, the values used by the parameter are the output devices
     * enumeration found in @see audio_output_flags_t enum (system/audio.h)
     * If none, it follows the same routing than the policy: the use case shall be unrouted.
     */
    static const std::string &gKeyCompressOffloadRouting;

    /** Android Mode Parameter Key. */
    static const std::string &gKeyAndroidMode;

    /** Mic Mute Parameter Key. */
    static const std::string &gKeyMicMute;

    /** Output and Input Devices Parameter Key. */
    static Key<std::string>::const_bidirectionalKey gKeyDevices;

    /** Output and input flags key. */
    static Key<std::string>::const_bidirectionalKey gKeyFlags;

    /** Use case key, i.e. input source for input stream, unused for output. */
    static Key<std::string>::const_bidirectionalKey gKeyUseCases;

    /** VoIP Band Parameter Key. */
    static const std::string &gKeyVoipBandType;

    /** PreProc Parameter Key. */
    static const std::string &gKeyPreProcRequested;

    /** Always Listening Route/VTSV Parameters Keys */
    static const std::string &gkeyAlwaysListeningRoute;
    static const std::string &gKeyLpalDevice;
    static const std::string &gkeyAlwaysListeningRouteOn;
    static const std::string &gkeyAlwaysListeningRouteOff;
};

}   // namespace intel_audio
