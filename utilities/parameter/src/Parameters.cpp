/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2015 Intel
 * Corporation All Rights Reserved.
 *
 * The source code contained or described herein and all documents related to
 * the source code ("Material") are owned by Intel Corporation or its suppliers
 * or licensors. Title to the Material remains with Intel Corporation or its
 * suppliers and licensors. The Material contains trade secrets and proprietary
 * and confidential information of Intel or its suppliers and licensors. The
 * Material is protected by worldwide copyright and trade secret laws and
 * treaty provisions. No part of the Material may be used, copied, reproduced,
 * modified, published, uploaded, posted, transmitted, distributed, or
 * disclosed in any way without Intel's prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
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
