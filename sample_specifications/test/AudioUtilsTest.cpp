/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2013-2014 Intel
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


#include <AudioUtils.hpp>
#include <SampleSpec.hpp>
#include <mock/UnistdMock.hpp>
#include <hardware_legacy/AudioSystemLegacy.h>
#include <limits>
#include <gtest/gtest.h>

using ::android_audio_legacy::AudioSystem;
using ::android_audio_legacy::AudioUtils;
using ::android_audio_legacy::SampleSpec;
using ::android_audio_legacy::SampleSpecItem;
using ::testing::_;
using ::testing::EndsWith;
using ::testing::Return;
using ::testing::SetErrnoAndReturn;

TEST(AudioUtils, alignOn16)
{
    EXPECT_EQ(0x00000000u, AudioUtils::alignOn16(0x00000000u));
    EXPECT_EQ(0x00000010u, AudioUtils::alignOn16(0x00000001u));
    EXPECT_EQ(0x00000010u, AudioUtils::alignOn16(0x00000002u));
    EXPECT_EQ(0x00000010u, AudioUtils::alignOn16(0x00000003u));
    // ...
    EXPECT_EQ(0x00000010u, AudioUtils::alignOn16(0x0000000Fu));
    EXPECT_EQ(0x00000010u, AudioUtils::alignOn16(0x00000010u));
    EXPECT_EQ(0x00000020u, AudioUtils::alignOn16(0x00000011u));
    EXPECT_EQ(0x00000020u, AudioUtils::alignOn16(0x00000012u));
    // ...
    EXPECT_EQ(0x00000020u, AudioUtils::alignOn16(0x0000001Fu));
    EXPECT_EQ(0x00000020u, AudioUtils::alignOn16(0x00000020u));
    EXPECT_EQ(0x00000030u, AudioUtils::alignOn16(0x00000021u));
    // ...
    EXPECT_EQ(0xFFFFFFE0u, AudioUtils::alignOn16(0xFFFFFFE0u));
    EXPECT_EQ(0xFFFFFFF0u, AudioUtils::alignOn16(0xFFFFFFE1u));
    EXPECT_EQ(0xFFFFFFF0u, AudioUtils::alignOn16(0xFFFFFFE2u));
    // ...
    EXPECT_EQ(0xFFFFFFF0u, AudioUtils::alignOn16(0xFFFFFFEFu));
    EXPECT_EQ(0xFFFFFFF0u, AudioUtils::alignOn16(0xFFFFFFF0u));
}

TEST(AudioUtils, convertion)
{
    SampleSpec sampleSpecSrc(2, AUDIO_FORMAT_PCM_16_BIT, 8000);
    SampleSpec sampleSpecDst(2, AUDIO_FORMAT_PCM_16_BIT, 48000);

    const ssize_t srcFrames = 1000;
    const ssize_t srcBytes = 4000;

    EXPECT_EQ(srcBytes * 6,
              AudioUtils::convertSrcToDstInBytes(srcBytes, sampleSpecSrc, sampleSpecDst));

    EXPECT_EQ(srcFrames * 6,
              AudioUtils::convertSrcToDstInFrames(srcFrames, sampleSpecSrc, sampleSpecDst));

}

TEST(AudioUtils, formatConversion)
{
    // Valid Tiny formats
    EXPECT_EQ(AUDIO_FORMAT_PCM_16_BIT, AudioUtils::convertTinyToHalFormat(PCM_FORMAT_S16_LE));
    EXPECT_EQ(AUDIO_FORMAT_PCM_8_24_BIT, AudioUtils::convertTinyToHalFormat(PCM_FORMAT_S32_LE));

    // iny format not supported by AudioHAL
    EXPECT_EQ(AUDIO_FORMAT_INVALID, AudioUtils::convertTinyToHalFormat(PCM_FORMAT_MAX));
    EXPECT_EQ(AUDIO_FORMAT_INVALID, AudioUtils::convertTinyToHalFormat(PCM_FORMAT_S24_LE));

    // Invalid Tiny format
    EXPECT_EQ(AUDIO_FORMAT_INVALID,
              AudioUtils::convertTinyToHalFormat(static_cast<pcm_format>(42)));

    // Valid AudioHAL format
    EXPECT_EQ(PCM_FORMAT_S16_LE, AudioUtils::convertHalToTinyFormat(AUDIO_FORMAT_PCM_16_BIT));
    EXPECT_EQ(PCM_FORMAT_S32_LE, AudioUtils::convertHalToTinyFormat(AUDIO_FORMAT_PCM_8_24_BIT));

    // Invalid format, returns default
    EXPECT_EQ(PCM_FORMAT_S16_LE, AudioUtils::convertHalToTinyFormat(AUDIO_FORMAT_PCM_8_BIT));
    EXPECT_EQ(PCM_FORMAT_S16_LE, AudioUtils::convertHalToTinyFormat(AUDIO_FORMAT_PCM_32_BIT));
    EXPECT_EQ(PCM_FORMAT_S16_LE,
              AudioUtils::convertHalToTinyFormat(static_cast<audio_format_t>(0xFFFFFFFF)));
    EXPECT_EQ(PCM_FORMAT_S16_LE,
              AudioUtils::convertHalToTinyFormat(static_cast<audio_format_t>(-1)));

}


TEST(AudioUtils, timeConversion)
{
    EXPECT_EQ(0u, AudioUtils::convertUsecToMsec(0));
    EXPECT_EQ(1u, AudioUtils::convertUsecToMsec(1000));
    EXPECT_EQ(1u, AudioUtils::convertUsecToMsec(999));
    EXPECT_EQ(1u, AudioUtils::convertUsecToMsec(1));
    EXPECT_EQ(2u, static_cast<int32_t>(AudioUtils::convertUsecToMsec(1001)));
    EXPECT_EQ(0x418938,
              AudioUtils::convertUsecToMsec(std::numeric_limits<uint32_t>::max()));
}

TEST(AudioUtils, cardNameToIndexNominalCase)
{
    // Valid card
    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("Intel"), _, _))
    .WillOnce(unistdMock::ActionReadStr("card9"));
    EXPECT_EQ(9, AudioUtils::getCardIndexByName("Intel"));

    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("Intel"), _, _))
    .WillOnce(unistdMock::ActionReadStr("card10"));
    EXPECT_EQ(10, AudioUtils::getCardIndexByName("Intel"));

    std::ostringstream number;
    number << std::numeric_limits<int>::max();
    std::string cardName = "card" + number.str();
    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("Intel"), _, _))
    .WillOnce(unistdMock::ActionReadStr(cardName.c_str()));
    EXPECT_EQ(std::numeric_limits<int>::max(), AudioUtils::getCardIndexByName("Intel"));

    EXPECT_EQ(9, AudioUtils::getCardIndexByName("card9"));

    EXPECT_EQ(0, AudioUtils::getCardIndexByName("card0"));

    EXPECT_EQ(std::numeric_limits<int>::max(), AudioUtils::getCardIndexByName(cardName.c_str()));
}

TEST(AudioUtils, cardNameToIndexWrongLink)
{
    // Link without index
    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("card9"), _, _))
    .WillOnce(unistdMock::ActionReadStr("card"));
    EXPECT_EQ(-EBADFD, AudioUtils::getCardIndexByName("_card9"));

    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("card9"), _, _))
    .WillOnce(unistdMock::ActionReadStr("card"));
    EXPECT_EQ(-EBADFD, AudioUtils::getCardIndexByName("totocard9"));

    // Link without index
    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("Intel"), _, _))
    .WillOnce(unistdMock::ActionReadStr("card"));
    EXPECT_EQ(-EBADFD, AudioUtils::getCardIndexByName("Intel"));

    // Wrong format of the link
    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("Intel"), _, _))
    .WillOnce(unistdMock::ActionReadStr("Inte"));
    EXPECT_EQ(-EBADFD, AudioUtils::getCardIndexByName("Intel"));

    // Link without valid index
    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("Intel"), _, _))
    .WillOnce(unistdMock::ActionReadStr("cardXYZ"));
    EXPECT_EQ(-EINVAL, AudioUtils::getCardIndexByName("Intel"));

    // Link with valid index but spurious characters
    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("Intel"), _, _))
    .WillOnce(unistdMock::ActionReadStr("card2XYZ"));
    EXPECT_EQ(-EINVAL, AudioUtils::getCardIndexByName("Intel"));

    // Link with valid index but spurious characters
    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("Intel"), _, _))
    .WillOnce(unistdMock::ActionReadStr("card21XYZ"));
    EXPECT_EQ(-EINVAL, AudioUtils::getCardIndexByName("Intel"));
}

TEST(AudioUtils, cardNameToIndexLinkDoesNotExist)
{
    // File does not exist
    EXPECT_CALL(UnistdMock::getInstance(), readlink(EndsWith("InvalidLink"), _, _))
    .WillOnce(SetErrnoAndReturn(EACCES, -1));
    EXPECT_EQ(-EACCES, AudioUtils::getCardIndexByName("InvalidLink"));

    // Empty file name
    EXPECT_CALL(UnistdMock::getInstance(), readlink(_, _, _))
    .WillOnce(SetErrnoAndReturn(EACCES, -1));
    EXPECT_EQ(-EACCES, AudioUtils::getCardIndexByName(""));
}


TEST(AudioUtils, isInputDevice)
{
    // Valid Input device (API REV1.0)
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_COMMUNICATION));
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_AMBIENT));
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_BUILTIN_MIC));
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET));
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_WIRED_HEADSET));
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_AUX_DIGITAL));
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_COMMUNICATION));
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_VOICE_CALL));
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_BACK_MIC));
    EXPECT_TRUE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_DEFAULT));

    // Invalid input devices
    // Output device
    EXPECT_FALSE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_OUT_SPEAKER));
    EXPECT_FALSE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_OUT_BLUETOOTH_SCO));

    // Multiple input device
    EXPECT_FALSE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_ALL));
    EXPECT_FALSE(AudioUtils::isAudioInputDevice(AudioSystem::DEVICE_IN_COMMUNICATION |
                                                AudioSystem::DEVICE_IN_BACK_MIC));

    // REV2.0 input device
    EXPECT_FALSE(AudioUtils::isAudioInputDevice(AUDIO_DEVICE_IN_COMMUNICATION));
    EXPECT_FALSE(AudioUtils::isAudioInputDevice(AUDIO_DEVICE_IN_BUILTIN_MIC));
    EXPECT_FALSE(AudioUtils::isAudioInputDevice(AUDIO_DEVICE_IN_ALL));
}
