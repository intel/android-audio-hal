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

#include <StreamWrapper.hpp>
#include <StreamMock.hpp>
#include <gtest/gtest.h>

namespace intel_audio
{

class StreamWrapperTest : public ::testing::Test
{
private:
    virtual void SetUp()
    {
        mCInStream = InputStreamWrapper::bind(&mInMock);
        EXPECT_TRUE(NULL != mCInStream);

        mCOutStream = OutputStreamWrapper::bind(&mOutMock);
        EXPECT_TRUE(NULL != mCOutStream);
    }
    virtual void TearDown()
    {
        // Check I get back the stream I gave at creation
        EXPECT_EQ(&mInMock, InputStreamWrapper::release(mCInStream));
        EXPECT_EQ(&mOutMock, OutputStreamWrapper::release(mCOutStream));
    }

protected:
    StreamInMock mInMock;
    audio_stream_in_t *mCInStream;

    StreamOutMock mOutMock;
    audio_stream_out_t *mCOutStream;
};

TEST_F(StreamWrapperTest, CreateDelete)
{
}

TEST_F(StreamWrapperTest, OutputWrapper)
{
    audio_stream_t *stream = &mCOutStream->common;
    EXPECT_EQ(stream->get_sample_rate(stream), static_cast<uint32_t>(1234));

    EXPECT_EQ(stream->set_sample_rate(stream, 48000), 0);

    EXPECT_EQ(stream->get_buffer_size(stream), 54321u);

    EXPECT_EQ(stream->get_channels(stream), static_cast<audio_channel_mask_t>(7));

    EXPECT_EQ(stream->get_format(stream), AUDIO_FORMAT_PCM_32_BIT);

    EXPECT_EQ(stream->set_format(stream, AUDIO_FORMAT_AAC), 0);

    EXPECT_EQ(stream->standby(stream), 0);

    EXPECT_EQ(stream->dump(stream, 453), 0);

    EXPECT_EQ(stream->get_device(stream), AUDIO_DEVICE_OUT_HDMI);

    EXPECT_EQ(stream->set_device(stream, AUDIO_DEVICE_IN_AMBIENT), 0);

    std::string kvpairs("woannnagain bistoufly");
    EXPECT_EQ(stream->set_parameters(stream, kvpairs.c_str()), 0);
}

TEST_F(StreamWrapperTest, InputWrapper)
{
    audio_stream_t *stream = &mCInStream->common;
    EXPECT_EQ(stream->get_sample_rate(stream), static_cast<uint32_t>(1234));

    EXPECT_EQ(stream->set_sample_rate(stream, 48000), 0);

    EXPECT_EQ(stream->get_buffer_size(stream), 11155u);

    EXPECT_EQ(stream->get_channels(stream), static_cast<audio_channel_mask_t>(5));

    EXPECT_EQ(stream->get_format(stream), AUDIO_FORMAT_PCM_16_BIT);

    EXPECT_EQ(stream->set_format(stream, AUDIO_FORMAT_AAC), 0);

    EXPECT_EQ(stream->standby(stream), 0);

    EXPECT_EQ(stream->dump(stream, 453), 0);

    EXPECT_EQ(stream->get_device(stream), AUDIO_DEVICE_OUT_AUX_DIGITAL);

    EXPECT_EQ(stream->set_device(stream, AUDIO_DEVICE_IN_AMBIENT), 0);

    std::string kvpairs("woannnagain bistoufly");
    EXPECT_EQ(stream->set_parameters(stream, kvpairs.c_str()), 0);

    std::string keys("woannnagain");
    std::string values("Input");
    char *read_values = stream->get_parameters(stream, keys.c_str());
    EXPECT_STREQ(values.c_str(), read_values);
    free(read_values);

    effect_handle_t effect = reinterpret_cast<effect_handle_t>(alloca(sizeof(effect)));
    EXPECT_EQ(stream->add_audio_effect(stream, effect), 0);

    EXPECT_EQ(stream->remove_audio_effect(stream, effect), 0);

    // Specific API check
    EXPECT_EQ(mCInStream->set_gain(mCInStream, -3.1), 0);

    char buffer[1024];
    // Read successes
    EXPECT_EQ(1024, mCInStream->read(mCInStream, buffer, 1024));

    EXPECT_EQ(mCInStream->get_input_frames_lost(mCInStream), static_cast<uint32_t>(15));
}

}
