/*
 * INTEL CONFIDENTIAL
 * Copyright © 2013 Intel
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
 * disclosed in any way without Intel’s prior express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by Intel in writing.
 *
 */
#pragma once

#include "Interface.h"
#include "StreamRouteConfig.hpp"
#include <utils/Errors.h>

class Stream;

struct IStreamInterface : public NInterfaceProvider::IInterface
{
    INTERFACE_NAME("StreamInterface");

    /**
     * Starts the route manager service.
     */
    virtual android::status_t startService() = 0;

    /**
     * Stops the route manager service.
     */
    virtual android::status_t stopService() = 0;

    /**
     * Adds a streams to the list of opened streams.
     *
     * @param[in] stream: opened stream to be appended.
     */
    virtual void addStream(Stream *stream) = 0;

    /**
     * Removes a streams from the list of opened streams.
     *
     * @param[in] stream: closed stream to be removed.
     */
    virtual void removeStream(Stream *stream) = 0;

    /**
     * Trigs a routing reconsideration.
     *
     */
    virtual void reconsiderRouting() = 0;

    /**
     * Informs that a new stream is started.
     */
    virtual void startStream() = 0;

    /**
     *
     * Informs that a stream is stopped.
     */
    virtual void stopStream() = 0;

    /**
     * Sets the voice volume.
     * Called from AudioSystem/Policy to apply the volume on the voice call stream which is
     * platform dependent.
     *
     * @param[in] gain the volume to set in float format in the expected range [0 .. 1.0]
     *                 Note that any attempt to set a value outside this range will return -ERANGE.
     *
     * @return OK if success, error code otherwise.
     */
    virtual android::status_t setVoiceVolume(float gain) = 0;

    /**
     * Get the voice output stream.
     * The purpose of this function is
     * - to retrieve the instance of the voice output stream in order to write playback frames
     * as echo reference for
     *
     * @return voice output stream
     */
    virtual Stream *getVoiceOutputStream() = 0;

    /**
     * Get the latency.
     * Upon creation, streams need to provide latency. As stream are not attached
     * to any route at creation, they must get a latency dependant of the
     * platform to provide information of latency and buffersize (inferred from ALSA ring buffer).
     *
     * @param[in] isOut direction of the stream requesting the configuration
     * @param[in] flags only valid for output stream, depends on flag, might use different
     *                    buffering model.
     *
     * @return latency in microseconds
     */
    virtual uint32_t getLatencyInUs(bool isOut, uint32_t flags = 0) const = 0;

    /**
     * Get the period size.
     * Upon creation, streams need to provide buffer size. As stream are not attached
     * to any route at creation, they must get a latency dependant of the
     * platform to provide information of latency and buffersize (inferred from ALSA ring buffer).
     *
     * @param[in] isOut direction of the stream requesting the configuration
     * @param[in] flags only valid for output stream, depends on flag, might use different
     *                    buffering model.
     *
     * @return period size in microseconds
     */
    virtual uint32_t getPeriodInUs(bool isOut, uint32_t flags = 0) const = 0;
};
