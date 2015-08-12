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
#include "FunctionalTestHost.hpp"
#include <media/AudioParameter.h>
#include <KeyValuePairs.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <utils/Vector.h>

#include <iostream>
#include <algorithm>

using namespace android;
using namespace std;
using audio_comms::utilities::Log;

intel_audio::Device *AudioHalTest::mDevice = NULL;

void AudioHalTest::setConfig(uint32_t rate, audio_channel_mask_t mask, audio_format_t format,
                             audio_config_t &config)
{
    config.sample_rate = rate;
    config.channel_mask = mask;
    config.format = format;
}

void AudioHalTest::SetUpTestCase()
{
    Log::Debug() << "*** SetUpTestCase";

    mDevice = new intel_audio::Device();
    AUDIOCOMMS_ASSERT(mDevice != NULL, "Invalid Audio Device");
}

void AudioHalTest::TearDownTestCase()
{
    Log::Debug() << "*** TearDownTestCase";
    delete mDevice;
}

void AudioHalTest::SetUp()
{
    const ::testing::TestInfo *const testInfo =
        ::testing::UnitTest::GetInstance()->current_test_info();

    Log::Debug() << "*** Starting test " << testInfo->name()
                 << " in test case " << testInfo->test_case_name();
}

void AudioHalTest::TearDown()
{
    const ::testing::TestInfo *const testInfo =
        ::testing::UnitTest::GetInstance()->current_test_info();

    Log::Debug() << "*** Stopping test " << testInfo->name()
                 << " in test case " << testInfo->test_case_name();
}

TEST_P(AudioHalAndroidModeTest, audio_mode_t)
{
    audio_mode_t mode = GetParam();

    ASSERT_EQ(android::OK, getDevice()->setMode(mode));
}

INSTANTIATE_TEST_CASE_P(
    AudioHalAndroidModeTestAll,
    AudioHalAndroidModeTest,
    ::testing::Values(
        AUDIO_MODE_NORMAL,
        AUDIO_MODE_RINGTONE,
        AUDIO_MODE_IN_CALL,
        AUDIO_MODE_IN_COMMUNICATION
        )
    );


TEST_P(AudioHalMicMuteTest, bool)
{
    bool micMute = GetParam();
    bool readMicMute;
    ASSERT_EQ(android::OK, getDevice()->getMicMute(readMicMute));

    ASSERT_EQ(android::OK, getDevice()->setMicMute(micMute));

    ASSERT_EQ(android::OK, getDevice()->getMicMute(readMicMute));
    EXPECT_EQ(micMute, readMicMute);
}

INSTANTIATE_TEST_CASE_P(
    AudioHalMicMuteTestAll,
    AudioHalMicMuteTest,
    ::testing::Values(
        true,
        false,
        true,
        false,
        true
        )
    );

TEST_P(AudioHalMasterMuteTest, bool)
{
    bool masterMute = GetParam();
    bool readMasterMute;
    /** @note API not implemented in our Audio HAL */
    ASSERT_EQ(android::INVALID_OPERATION, getDevice()->getMasterMute(readMasterMute));
    /** @note API not implemented in our Audio HAL */
    ASSERT_EQ(android::INVALID_OPERATION, getDevice()->setMasterMute(masterMute));
}

// Note that master mute is not implemented within Audio HAL, just test the APIs return code
INSTANTIATE_TEST_CASE_P(
    AudioHalMasterMuteTestAll,
    AudioHalMasterMuteTest,
    ::testing::Values(
        true,
        false
        )
    );

TEST_P(AudioHalMasterVolumeTest, float)
{
    float masterVolume = GetParam();
    float readMasterVolume;
    ASSERT_EQ(android::INVALID_OPERATION, getDevice()->getMasterVolume(readMasterVolume));

    ASSERT_EQ(android::INVALID_OPERATION, getDevice()->setMasterVolume(masterVolume));
}

// Note that master volume is not implemented within Audio HAL, just test the APIs return code
INSTANTIATE_TEST_CASE_P(
    AudioHalMasterVolumeTestAll,
    AudioHalMasterVolumeTest,
    ::testing::Values(
        0.0f,
        0.5f,
        1.0f,
        1.1f
        )
    );


TEST_F(AudioHalTest, audioRecordingBufferSize)
{

    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_IN_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    // Expected size if 20 ms * 48000 * frame size
    Log::Debug() << getDevice();
    ASSERT_EQ(48u * 20 * 2 * 2, getDevice()->getInputBufferSize(config));
}

TEST_F(AudioHalTest, inputStreamSpec)
{

    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_IN_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    intel_audio::StreamInInterface *inStream = NULL;
    audio_devices_t devices = static_cast<audio_devices_t>(AUDIO_DEVICE_IN_COMMUNICATION);
    audio_input_flags_t flags = AUDIO_INPUT_FLAG_NONE;
    const char *address = "dont_care";
    audio_source_t source = AUDIO_SOURCE_MIC;

    status_t status = getDevice()->openInputStream(0, devices, config, inStream,
                                                   flags, address, source);
    ASSERT_EQ(status, android::OK);
    ASSERT_FALSE(inStream == NULL);

    EXPECT_EQ(config.sample_rate, inStream->getSampleRate());
    EXPECT_EQ(config.format, inStream->getFormat());
    EXPECT_EQ(config.channel_mask, inStream->getChannels());

    getDevice()->closeInputStream(inStream);
}

TEST_F(AudioHalTest, outputStreamSpec)
{

    audio_config_t config;
    setConfig(48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT, config);
    audio_output_flags_t flags = AUDIO_OUTPUT_FLAG_PRIMARY;
    intel_audio::StreamOutInterface *outStream = NULL;
    audio_devices_t devices = static_cast<uint32_t>(AUDIO_DEVICE_OUT_EARPIECE);
    const char *address = "dont_care";

    status_t status = getDevice()->openOutputStream(0,
                                                    devices,
                                                    flags,
                                                    config,
                                                    outStream,
                                                    address);
    ASSERT_EQ(status, android::OK);
    ASSERT_FALSE(outStream == NULL);

    EXPECT_EQ(config.sample_rate, outStream->getSampleRate());
    EXPECT_EQ(config.format, outStream->getFormat());
    EXPECT_EQ(config.channel_mask, outStream->getChannels());

    getDevice()->closeOutputStream(outStream);
}

// This test ensure that {keys,value} pair sent with restarting key are taken into account
TEST_F(AudioHalTest, restartingKey)
{
    ASSERT_EQ(android::OK, getDevice()->setParameters("screen_state=on"));
    string returnedParam = getDevice()->getParameters("screen_state");
    EXPECT_EQ("screen_state=on", returnedParam);

    ASSERT_EQ(android::OK, getDevice()->setParameters("restarting=true;screen_state=off"));
    returnedParam.clear();
    returnedParam = getDevice()->getParameters("screen_state");
    EXPECT_EQ("screen_state=off", returnedParam);
}

TEST_P(AudioHalValidGlobalParameterTest, key)
{
    string key = GetParam().first;
    string value = GetParam().second;

    intel_audio::KeyValuePairs valuePair;
    string returnedParam = getDevice()->getParameters(key.c_str());

    valuePair.add(key, value);
    ASSERT_EQ(android::OK, getDevice()->setParameters(valuePair.toString().c_str()));
}

INSTANTIATE_TEST_CASE_P(
    AudioHalValidGlobalParameterTestAll,
    AudioHalValidGlobalParameterTest,
    ::testing::Values(
        std::make_pair("lpal_device", "-2147483644"),
        std::make_pair("lpal_device", "-2147483616")
        )
    );


TEST_P(AudioHalInvalidGlobalParameterTest, key)
{
    string key = GetParam().first;
    string value = GetParam().second;

    intel_audio::KeyValuePairs valuePair;
    string returnedParam = getDevice()->getParameters(key.c_str());

    valuePair.add(key, value);
    ASSERT_NE(android::OK, getDevice()->setParameters(valuePair.toString().c_str()));
}

INSTANTIATE_TEST_CASE_P(
    AudioHalInvalidGlobalParameterTestAll,
    AudioHalInvalidGlobalParameterTest,
    ::testing::Values(
        std::make_pair("screen_state", "neither_on_nor_off"),
        std::make_pair("screen_state", "666")
        )
    );
