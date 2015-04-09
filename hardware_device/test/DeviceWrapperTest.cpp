/*
 * Copyright (C) 2014-2015 Intel Corporation
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

#include <DeviceWrapper.hpp>
#include <StreamMock.hpp>
#include <DeviceMock.hpp>
#include <gtest/gtest.h>


using namespace intel_audio;
using ::testing::Test;
using std::string;

const char *DeviceMock::AudioHalName = "test";

typedef intel_audio::DeviceWrapper<intel_audio::DeviceMock, AUDIO_DEVICE_API_VERSION_2_0> Wrapper;

class DeviceTest : public ::testing::Test
{
private:
    void SetUp()
    {
        int error =
            Wrapper::open(NULL, AUDIO_HARDWARE_INTERFACE,
                          reinterpret_cast<hw_device_t **>(&mDevice));

        EXPECT_TRUE(mDevice != NULL);
        EXPECT_EQ(0, error);
    }

    void TearDown()
    {
        EXPECT_EQ(0, Wrapper::close(&mDevice->common));
    }

protected:
    audio_hw_device *mDevice;
};

TEST(DeviceWrapper, BadOutputPointer)
{
    EXPECT_EQ(-EINVAL, Wrapper::open(NULL, AUDIO_HARDWARE_INTERFACE, NULL));
}

TEST(DeviceWrapper, BadInterfaceName)
{
    hw_device_t *dummy = NULL;
    EXPECT_EQ(-EINVAL, Wrapper::open(NULL, "This is a bad interface Name", &dummy));

    delete dummy; /* prevent KW to complain (false positive) */
}

/** Check audio hardware device API wrapping. */
TEST_F(DeviceTest, Device)
{
    EXPECT_EQ(mDevice->init_check(mDevice), 0);

    EXPECT_EQ(mDevice->set_voice_volume(mDevice, 1.1), 0);

    EXPECT_EQ(mDevice->set_master_volume(mDevice, 1.2), 0);

    float volume;
    EXPECT_EQ(mDevice->get_master_volume(mDevice, &volume), 0);

    EXPECT_EQ(mDevice->set_master_mute(mDevice, false), 0);

    bool muted;
    EXPECT_EQ(mDevice->get_master_mute(mDevice, &muted), 0);
    EXPECT_EQ(muted, true);

    EXPECT_EQ(mDevice->set_mode(mDevice, AUDIO_MODE_INVALID), 0);

    EXPECT_EQ(mDevice->set_mic_mute(mDevice, true), 0);

    EXPECT_EQ(mDevice->get_mic_mute(mDevice, &muted), 0);
    EXPECT_EQ(muted, true);

    string kvpairs("ohhh yeahhhh");
    EXPECT_EQ(mDevice->set_parameters(mDevice, kvpairs.c_str()), 0);

    string keys("ohhh");
    string expectedValues("Bla");
    char *read_values = mDevice->get_parameters(mDevice, keys.c_str());
    EXPECT_STREQ(expectedValues.c_str(), read_values);
    free(read_values);

    audio_config_t config;
    EXPECT_EQ(mDevice->get_input_buffer_size(mDevice, &config), 1234u);

    EXPECT_EQ(mDevice->dump(mDevice, 246), 0);

    // Routing Control APIs test
    uint32_t sourcesCount = 8;
    uint32_t sinksCount = 3;
    struct audio_port_config sources[sourcesCount];
    struct audio_port_config sinks[sinksCount];
    audio_patch_handle_t handle;

    EXPECT_EQ(mDevice->create_audio_patch(mDevice, sourcesCount, sources,
                                          sinksCount, sinks, &handle),
              0);

    EXPECT_EQ(mDevice->release_audio_patch(mDevice, handle), 0);

    struct audio_port port;
    struct audio_port_config portConfig;
    EXPECT_EQ(mDevice->get_audio_port(mDevice, &port), 0);

    EXPECT_EQ(mDevice->set_audio_port_config(mDevice, &portConfig), 0);
}

TEST_F(DeviceTest, OpenBadOutputpointer)
{
    audio_config_t config;
    EXPECT_EQ(mDevice->open_input_stream(mDevice, audio_io_handle_t(), audio_devices_t(), &config,
                                         static_cast<audio_stream_in **>(NULL),
                                         AUDIO_INPUT_FLAG_NONE, "device address",
                                         AUDIO_SOURCE_MIC),
              android::BAD_VALUE);

    EXPECT_EQ(mDevice->open_output_stream(mDevice, audio_io_handle_t(), audio_devices_t(),
                                          AUDIO_OUTPUT_FLAG_NONE, &config,
                                          static_cast<audio_stream_out **>(NULL),
                                          "device address"),
              android::BAD_VALUE);
}

TEST_F(DeviceTest, OpenBadDeviceAddress)
{
    audio_config_t config;
    audio_stream_in *stream_in;
    EXPECT_EQ(android::BAD_VALUE,
              mDevice->open_input_stream(mDevice, audio_io_handle_t(), audio_devices_t(), &config,
                                         &stream_in, AUDIO_INPUT_FLAG_NONE,
                                         static_cast<const char *>(NULL),
                                         AUDIO_SOURCE_MIC));

    audio_stream_out *stream_out;
    EXPECT_EQ(android::BAD_VALUE,
              mDevice->open_output_stream(mDevice, audio_io_handle_t(), audio_devices_t(),
                                          AUDIO_OUTPUT_FLAG_NONE, &config, &stream_out,
                                          static_cast<const char *>(NULL)));
}

TEST_F(DeviceTest, GetMasterVolumeBadVolumePointer)
{
    EXPECT_EQ(android::BAD_VALUE, mDevice->get_master_volume(mDevice, NULL));
}

TEST_F(DeviceTest, GetMasterMuteBadOutputPointer)
{
    EXPECT_EQ(android::BAD_VALUE, mDevice->get_master_mute(mDevice, NULL));
}

TEST_F(DeviceTest, GetMicMuteBadOutputPointer)
{
    EXPECT_EQ(android::BAD_VALUE, mDevice->get_mic_mute(mDevice, NULL));
}

#if 0
/* This test was removed as the code assert that the config is not NULL. Nevertheless, it could be
 * nice to reenable it, but what size to expect when config is NULL? */
TEST_F(DeviceTest, GetInputBufferBadOutputPointer)
{
    // EXPECT_EQ(android::BAD_VALUE, mDevice->get_input_buffer_size(mDevice, NULL));
}
#endif

TEST_F(DeviceTest, GetAudioPort)
{
    EXPECT_EQ(android::BAD_VALUE, mDevice->get_audio_port(mDevice, NULL));
}

TEST_F(DeviceTest, GetAudioPortConfig)
{
    EXPECT_EQ(android::BAD_VALUE, mDevice->set_audio_port_config(mDevice, NULL));
}

TEST_F(DeviceTest, DeviceErrorHandling)
{
    /** Check audio input stream error handling. */
    audio_io_handle_t handle = static_cast<audio_io_handle_t>(0);
    audio_devices_t devices = static_cast<audio_devices_t>(0);
    audio_config_t *nullConfig = NULL;
    audio_stream_in *stream_in;
    audio_input_flags_t inputFlags = AUDIO_INPUT_FLAG_NONE;
    string deviceAddress("MyCard,myDeviceId");
    audio_source_t source = AUDIO_SOURCE_MIC;

    // Input Stream creation with a NULL configuration structure pointer
    ASSERT_EQ(mDevice->open_input_stream(mDevice, handle, devices, nullConfig, &stream_in,
                                         inputFlags, deviceAddress.c_str(), source),
              android::BAD_VALUE);

    /** Check audio output stream error handling. */
    audio_output_flags_t flags = static_cast<audio_output_flags_t>(0);
    audio_stream_out *stream_out;

    // Output Stream creation with a NULL configuration structure pointer
    ASSERT_EQ(mDevice->open_output_stream(mDevice, handle, devices, flags, nullConfig, &stream_out,
                                          deviceAddress.c_str()),
              android::BAD_VALUE);

    /** Routing control APIs error handling. */
    uint32_t numSources = 8;
    uint32_t numSinks = 3;
    struct audio_port_config *nullSources = NULL;
    struct audio_port_config *nullSinks = NULL;
    struct audio_port_config sources[numSources];
    struct audio_port_config sinks[numSinks];

    EXPECT_EQ(mDevice->create_audio_patch(mDevice, numSources, nullSources,
                                          numSinks, sinks, &handle),
              android::BAD_VALUE);
    EXPECT_EQ(mDevice->create_audio_patch(mDevice, numSources, sources,
                                          numSinks, nullSinks, &handle),
              android::BAD_VALUE);

    audio_patch_handle_t *nullPatch = NULL;
    EXPECT_EQ(mDevice->create_audio_patch(mDevice, numSources, sources,
                                          numSinks, sinks, nullPatch),
              android::BAD_VALUE);

    struct audio_port *nullPort = NULL;
    EXPECT_EQ(mDevice->get_audio_port(mDevice, nullPort), android::BAD_VALUE);

    struct audio_port_config *nullPortConfig = NULL;
    EXPECT_EQ(mDevice->set_audio_port_config(mDevice, nullPortConfig), android::BAD_VALUE);
}

TEST_F(DeviceTest, OutputStreamErrorHandling)
{
    audio_io_handle_t handle = static_cast<audio_io_handle_t>(0);
    audio_devices_t devices = static_cast<audio_devices_t>(0);
    audio_output_flags_t flags = static_cast<audio_output_flags_t>(0);
    audio_config_t config;
    audio_stream_out *stream_out;
    string deviceAddress("myCard:0,myDevice:1");

    ASSERT_EQ(mDevice->open_output_stream(mDevice, handle, devices, flags, &config,
                                          &stream_out, deviceAddress.c_str()), 0);

    // Common API check
    /** Check get render position with NULL pointer error handling. */
    uint32_t *nullFrames = NULL;
    EXPECT_EQ(stream_out->get_render_position(stream_out, nullFrames), android::BAD_VALUE);

    /** Check get next write timestamp with NULL pointer error handling. */
    int64_t *nullStamp = NULL;
    EXPECT_EQ(stream_out->get_next_write_timestamp(stream_out, nullStamp), android::BAD_VALUE);

    /** Check get next presentation position with NULL pointer error handling. */
    struct timespec *nullTime = NULL;
    uint64_t validFrame;
    EXPECT_EQ(stream_out->get_presentation_position(stream_out, &validFrame, nullTime),
              android::BAD_VALUE);

    struct timespec validTime;
    uint64_t *nullFrame = NULL;
    EXPECT_EQ(stream_out->get_presentation_position(stream_out, nullFrame, &validTime),
              android::BAD_VALUE);

    EXPECT_EQ(stream_out->get_presentation_position(stream_out, nullFrame, nullTime),
              android::BAD_VALUE);


    mDevice->close_output_stream(mDevice, stream_out);
}
