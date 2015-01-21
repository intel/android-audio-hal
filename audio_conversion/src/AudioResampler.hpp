/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2015 Intel
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
 *
 */
#pragma once
#include "AudioConverter.hpp"
#include <audio_utils/resampler.h>
#include <list>

namespace intel_audio
{

class AudioResampler : public AudioConverter
{

public:
    /**
     * Class constructor.
     *
     * @param[in] Reference sample specification.
     */
    AudioResampler(SampleSpecItem sampleSpecItem);

    virtual ~AudioResampler();

private:
    /**
     * Configures the resampler.
     * It configures the resampler that may be used to convert samples from the source
     * to destination sample rate, with option 'RESAMPLER_QUALITY_DEFAULT'.
     *
     * @param[in] ssSrc source sample specifications.
     * @param[in] ssDst destination sample specification.
     *
     * @return status OK, error code otherwise.
     */
    virtual android::status_t configure(const SampleSpec &ssSrc, const SampleSpec &ssDst);

    /**
     * Resamples buffer from source to destination sample rate.
     * Resamples input frames of the provided input buffer into the destination buffer already
     * allocated by the converter or given by the client.
     * Before using this function, configure must have been called.
     *
     * @param[in] src the source buffer.
     * @param[out] dst the destination buffer, caller to ensure the destination
     *             is large enough.
     * @param[in] inFrames number of input frames.
     * @param[out] outFrames output frames processed.
     *
     * @return error code.
     */
    android::status_t resampleFrames(const void *src,
                                     void *dst,
                                     const size_t inFrames,
                                     size_t *outFrames);
    struct resampler_itfe *mResampler;

};
}  // namespace intel_audio
