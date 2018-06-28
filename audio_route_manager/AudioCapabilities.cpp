/*
 * Copyright (C) 2017-2018 Intel Corporation
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
#define LOG_TAG "RouteManager/AudioCapabilities"
// #define LOG_NDEBUG 0

#include "AudioCapabilities.hpp"
#include <AudioConversion.hpp>
#include <typeconverter/TypeConverter.hpp>
#include <utils/String8.h>
#include <unistd.h>
namespace intel_audio
{

bool AudioCapability::supportRate(uint32_t rate) const
{
    return std::find(mSupportedRates.begin(), mSupportedRates.end(), rate) != mSupportedRates.end();
}

uint32_t AudioCapability::getRateNear(uint32_t rate, bool isOut) const
{
    for (const auto &supportedRate : mSupportedRates) {
        uint32_t srcRate = isOut ? rate : supportedRate;
        uint32_t dstRate = isOut ? supportedRate : rate;
        if ((rate == supportedRate) || AudioConversion::supportResample(srcRate, dstRate)) {
            return supportedRate;
        }
    }
    return 0;
}

audio_channel_mask_t AudioCapability::getChannelMaskNear(audio_channel_mask_t mask,
                                                         bool isOut) const
{
    for (const auto &supportedMask : mSupportedChannelMasks) {
        uint32_t srcChannels = isOut ? audio_channel_count_from_out_mask(mask) :
                               audio_channel_count_from_in_mask(supportedMask);
        uint32_t dstChannels = isOut ? audio_channel_count_from_out_mask(supportedMask) :
                               audio_channel_count_from_in_mask(mask);
        if ((mask == supportedMask) || AudioConversion::supportRemap(srcChannels, dstChannels)) {
            return supportedMask;
        }
    }
    return AUDIO_CHANNEL_NONE;
}

audio_format_t AudioCapability::getFormatNear(audio_format_t format, bool isOut) const
{
    if ((mSupportedFormat == format) ||
        (isOut ? AudioConversion::supportReformat(format, mSupportedFormat) :
         AudioConversion::supportReformat(mSupportedFormat, format))) {
        return mSupportedFormat;
    }
    return AUDIO_FORMAT_DEFAULT;
}

const std::string AudioCapability::getSupportedChannelMasks(bool isOut) const
{
    return isOut ? OutputChannelConverter::collectionToString(mSupportedChannelMasks) :
           InputChannelConverter::collectionToString(mSupportedChannelMasks);
}

void AudioCapability::reset()
{
    if (isChannelMaskDynamic) {
        mSupportedChannelMasks.clear();
    }
    if (isRateDynamic) {
        mSupportedRates.clear();
    }
    if (isFormatDynamic) {
        mSupportedFormat = AUDIO_FORMAT_DEFAULT;
    }
}

android::status_t AudioCapability::dump(const int fd, int spaces, bool isOut) const
{
    const size_t SIZE = 512;
    char buffer[SIZE];
    android::String8 result;

    snprintf(buffer, SIZE, "%*s Audio Profile:\n", spaces, "");
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- supported format: %s\n", spaces + 4, "",
             FormatConverter::toString(mSupportedFormat).c_str());
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- supported channel Masks: %s\n", spaces + 4, "",
             getSupportedChannelMasks(isOut).c_str());
    result.append(buffer);
    snprintf(buffer, SIZE, "%*s- supported rates: %s\n", spaces + 4, "",
             getSupportedRates().c_str());
    result.append(buffer);

    write(fd, result.string(), result.size());
    return android::OK;
}

const std::string AudioCapability::getSupportedFormat() const
{
    return FormatConverter::toString(mSupportedFormat);
}

const std::string AudioCapability::getSupportedRates() const
{
    return samplingRatesToString(mSupportedRates);
}

const std::string AudioCapabilities::getSupportedFormats() const
{
    std::string supportedFormats;
    for (const auto &capability : *this) {
        if (not supportedFormats.empty()) {
            supportedFormats += "|";
        }
        supportedFormats += capability.getSupportedFormat();
    }
    return supportedFormats;
}

const std::string AudioCapabilities::getSupportedChannelMasks(bool isOut) const
{
    std::string supportedChannelMasks;
    for (const auto &capability : *this) {
        if (not supportedChannelMasks.empty()) {
            supportedChannelMasks += "|";
        }
        supportedChannelMasks += capability.getSupportedChannelMasks(isOut);
    }
    return supportedChannelMasks;
}

const std::string AudioCapabilities::getSupportedRates() const
{
    std::string supportedRates;
    for (const auto &capability : *this) {
        if (not supportedRates.empty()) {
            supportedRates += "|";
        }
        supportedRates += capability.getSupportedRates();
    }
    return supportedRates;
}

} // namespace intel_audio
