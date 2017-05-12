/*
 * Copyright (C) 2013-2017 Intel Corporation
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
#include "IoStream.hpp"
#include "AudioDevice.hpp"
#include <typeconverter/TypeConverter.hpp>
#include <IStreamRoute.hpp>
#include <AudioCommsAssert.hpp>
#include <SampleSpec.hpp>
#include <utils/RWLock.h>
#include <utilities/Log.hpp>
#include <utils/String8.h>

using audio_comms::utilities::Log;
using std::string;

namespace intel_audio
{

bool IoStream::isRouted() const
{
    AutoR lock(mStreamLock);
    return isRoutedL();
}

bool IoStream::isRoutedL() const
{
    return mCurrentStreamRoute != NULL;
}

bool IoStream::isNewRouteAvailable() const
{
    AutoR lock(mStreamLock);
    return mNewStreamRoute != NULL;
}

android::status_t IoStream::attachRoute()
{
    AutoW lock(mStreamLock);
    return attachRouteL();
}


android::status_t IoStream::detachRoute()
{
    AutoW lock(mStreamLock);
    return detachRouteL();
}

android::status_t IoStream::attachRouteL()
{
    if (mNewStreamRoute == NULL) {
        Log::Error() << __FUNCTION__ << ": Invalid new stream route to attach";
        return android::BAD_VALUE;
    }
    setCurrentStreamRouteL(mNewStreamRoute);
    setRouteSampleSpecL(mCurrentStreamRoute->getSampleSpec());
    mAudioDevice = getNewStreamRoute()->getAudioDevice();
    // now we are attached to a route, it is high time to reset need reconfigure flag
    resetNeedReconfigure();
    return android::OK;
}


android::status_t IoStream::detachRouteL()
{
    mCurrentStreamRoute = NULL;
    mAudioDevice = NULL;
    // not routed anymore, it is high time to reset need reconfigure flag
    resetNeedReconfigure();
    return android::OK;
}

void IoStream::addRequestedEffect(uint32_t effectId)
{
    mEffectsRequestedMask |= effectId;
    setNeedReconfigure();
}

void IoStream::removeRequestedEffect(uint32_t effectId)
{
    mEffectsRequestedMask &= ~effectId;
    setNeedReconfigure();
}

uint32_t IoStream::getOutputSilencePrologMs() const
{
    if (mCurrentStreamRoute == NULL) {
        Log::Warning() << __FUNCTION__ << ": called from invalid context(No route), returning 0 ms";
        return 0;
    }
    return mCurrentStreamRoute->getOutputSilencePrologMs();
}

android::status_t IoStream::setDevices(audio_devices_t devices, const std::string &address)
{
    AutoW lock(mStreamLock);
    // A change of device requires to be reconfigure (aka muted / unmuted) to garantee safe transition
    if ((mDevices != devices) || (mDeviceAddress != address)) {
        setNeedReconfigure();
    }
    mDevices = devices;
    mDeviceAddress = address;
    return android::OK;
}

void IoStream::resetNewStreamRoute()
{
    mNewStreamRoute = NULL;
}

void IoStream::setNewStreamRoute(IStreamRoute *newStreamRoute)
{
    mNewStreamRoute = newStreamRoute;
}

void IoStream::setCurrentStreamRouteL(IStreamRoute *currentStreamRoute)
{
    mCurrentStreamRoute = currentStreamRoute;
}

void IoStream::setRouteSampleSpecL(SampleSpec sampleSpec)
{
    mRouteSampleSpec = sampleSpec;
}

android::status_t IoStream::pcmReadFrames(void *buffer, size_t frames, string &error) const
{
    return mAudioDevice->pcmReadFrames(buffer, frames, error);
}

android::status_t IoStream::pcmWriteFrames(void *buffer, ssize_t frames, string &error) const
{
    return mAudioDevice->pcmWriteFrames(buffer, frames, error);
}

uint32_t IoStream::getBufferSizeInBytes() const
{
    return mAudioDevice->getBufferSizeInBytes();
}

size_t IoStream::getBufferSizeInFrames() const
{
    return mAudioDevice->getBufferSizeInFrames();
}

android::status_t IoStream::getFramesAvailable(size_t &avail, struct timespec &tStamp) const
{
    return mAudioDevice->getFramesAvailable(avail, tStamp);
}

android::status_t IoStream::pcmStop() const
{
    return mAudioDevice->pcmStop();
}

void IoStream::setNeedReconfigure()
{
    if (not isRoutedL()) {
        // No-operation if not routed
        return;
    }
    mNeedReconfigure = true;
}

android::status_t IoStream::dump(const int fd, int spaces) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    android::String8 result;

    snprintf(buffer, SIZE, "%*s- mEffectsRequestedMask: 0x%X\n", spaces, "", mEffectsRequestedMask);
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- Need Reconfigure: %s\n", spaces, "", mNeedReconfigure ? "Y" : "N");
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- is attached to route: %s\n", spaces, "",
             (mCurrentStreamRoute == nullptr ? "none" : mCurrentStreamRoute->getName().c_str()));
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- will be attached to route: %s\n", spaces, "",
             (mNewStreamRoute == nullptr ? "none" : mNewStreamRoute->getName().c_str()));
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- Device Types: %s\n", spaces, "",
             DeviceConverter::maskToString(mDevices, ",").c_str());
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- Device Address: %s\n", spaces, "", mDeviceAddress.c_str());
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- Stream Sample Specification: \n", spaces, "");
    result.append(buffer);
    write(fd, result.string(), result.size());

    mSampleSpec.dump(fd, isOut(), spaces + 2);

    return android::OK;
}

} // namespace intel_audio
