/*
 * Copyright (C) 2014-2016 Intel Corporation
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

#include <AudioNonCopyable.hpp>
#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <media/AudioEffect.h>
#include <binder/IServiceManager.h>
#include <media/IAudioFlinger.h>
#include <media/AudioEffect.h>
#include <gtest/gtest.h>
#include <stdint.h>
#include <string>
#include <vector>

class TestEffectParameterBase : private audio_comms::utilities::NonCopyable
{
public:
    enum Direction
    {
        input = 0,
        output
    };

    enum Access
    {
        read = 0,
        write,
        readWrite
    };

    TestEffectParameterBase(effect_uuid_t effectUuidType, uint32_t access, Direction direction)
        : mEffectUuidType(effectUuidType),
          mParam(NULL),
          mSize(0),
          mAccess(access),
          mDirection(direction)
    {
    }

    /**
     * Need this copy constructor as this class will be given as the test parameter in
     * parametrized test suite.
     */
    TestEffectParameterBase(const TestEffectParameterBase &object)
        : audio_comms::utilities::NonCopyable(),
          mEffectUuidType(object.mEffectUuidType),
          mParam(NULL),
          mSize(object.mSize),
          mAccess(object.mAccess),
          mDirection(object.mDirection)
    {
        setEffectParam(object.mParam, object.mSize);
    }
    TestEffectParameterBase &operator=(const TestEffectParameterBase &object)
    {
        // SELF ASSIGNMENT CHECK
        if (this != &object) {
            setEffectParam(object.mParam, object.mSize);
        }
        return *this;
    }

    virtual ~TestEffectParameterBase()
    {
        free(mParam);
    }

    bool readable() const { return mAccess == read || mAccess == readWrite; }
    bool writable() const { return mAccess == write || mAccess == readWrite; }
    bool rwAble() const { return mAccess == readWrite; }

    const effect_param_t *getEffectParam() const
    {
        return mParam;
    }

    size_t getSize() const { return mSize; }

    Direction getDirection() const { return mDirection; }

    const effect_uuid_t &getEffectType() const { return mEffectUuidType; }

    void setEffectParam(effect_param_t *param, size_t size)
    {
        free(mParam);
        mSize = size;
        mParam = reinterpret_cast<effect_param_t *>(malloc(mSize));
        AUDIOCOMMS_ASSERT(mParam != NULL, "Could not allocate effect param object");
        memcpy(mParam, param, mSize);

        audio_comms::utilities::Log::Verbose() << __FUNCTION__
                                               << ": param=" << *(uint32_t *)mParam->data
                                               << " val=" << *((uint16_t *)mParam->data + 2)
                                               << " paramSize=" << mParam->psize
                                               << " @data=" << mParam->data
                                               << " size=" << sizeof(mParam->status) +
            sizeof(mParam->psize) +
            sizeof(mParam->vsize);

        for (size_t i = 0; i < mSize / 2; i++) {
            audio_comms::utilities::Log::Verbose() << "setEffectParam: dump param struct = effect["
                                                   << i
                                                   << "=" << *((uint16_t *)mParam + i)
                                                   << " @=" << (mParam + i);
        }
    }

private:
    effect_uuid_t mEffectUuidType;
    effect_param_t *mParam;
    size_t mSize;
    uint32_t mAccess;
    Direction mDirection;
};

template <typename T>
struct typeSupported;

template <>
struct typeSupported<uint32_t> {};

template <>
struct typeSupported<uint16_t> {};

template <>
struct typeSupported<float> {};

template <>
struct typeSupported<std::string> {};

/**
 * This class is the object that will be given as a parameter to the parametrized test suite.
 * It contains the effect identification and the parameter to test, with the param(s) and value(s)
 * to write. It also informs of the readable / writable attribute and the direction of the
 * effect (pre or post processing) as for pre processing, creation of dummy AudioRecord track
 * will be needed to perform the param request.
 *
 * @tparam P allowed param type are: "int", "short", "float", "bool" and "string"
 * @tparam V allowed value type are: "int", "short", "float", "bool" and "string"
 */
template <typename paramType, typename valueType>
class TTestEffectParameter : public TestEffectParameterBase
{
public:
    TTestEffectParameter(effect_uuid_t effectUuidType,
                         const std::vector<paramType> &param,
                         const std::vector<valueType> &value,
                         uint32_t access,
                         Direction direction)
        : TestEffectParameterBase(effectUuidType, access, direction),
          mParamVector(param),
          mValue(value)
    {
        typeSupported<paramType>();
        typeSupported<valueType>();

        uint32_t nbParam = param.size();
        uint32_t nbValue = value.size();

        uint32_t paramSizeIncludingPadding =
            ((sizeof(paramType) * nbParam - 1) / sizeof(uint32_t) + 1) * sizeof(uint32_t);
        uint32_t expectedValueSize = sizeof(valueType) * nbValue;

        uint32_t valueSizeIncludingPadding =
            ((sizeof(valueType) * nbValue - 1) / sizeof(uint32_t) + 1) * sizeof(uint32_t);

        uint32_t buf32[(sizeof(effect_param_t) +
                        paramSizeIncludingPadding +
                        valueSizeIncludingPadding) / sizeof(uint32_t)];

        effect_param_t *p = reinterpret_cast<effect_param_t *>(buf32);
        p->psize = sizeof(paramType) * nbParam;
        p->vsize = expectedValueSize;

        std::copy(mParamVector.cbegin(),
                  mParamVector.cbegin() + sizeof(paramType) * nbParam,
                  p->data);

        uint32_t *pValue = (uint32_t *)p->data + paramSizeIncludingPadding / sizeof(uint32_t);

        std::copy(mValue.begin(), mValue.begin() + expectedValueSize, pValue);

        setEffectParam(p, sizeof(buf32));
    }

private:
    std::vector<paramType> mParamVector;
    std::vector<valueType> mValue;
};

class AudioEffectsFunctionalTest : public ::testing::Test
{
protected:
    /**
     * Per-test-case set-up.
     */
    virtual void SetUp();

    bool checkPlatformHasEffectType(const effect_uuid_t *effectType);

    void getParameterForEffect(android::AudioEffect *effect,
                               const TestEffectParameterBase *parameterToGet,
                               bool compareWithOrigParameter = false);

    void setParameterForEffect(android::AudioEffect *effect,
                               const TestEffectParameterBase *parameterToGet);

    void compareEffectParams(const effect_param_t *param1, const effect_param_t *param2);

    bool loadEffectLibrary();

public:
    static const std::string mLpeEffectLibPath;
    static const std::string mAudioServiceName;

    android::sp<android::IServiceManager> mServiceManager;
    android::sp<android::IBinder> mBinder;
    android::sp<android::IAudioFlinger> mAudioFlinger;
};

class TAudioEffectsSetParamTests
    : public AudioEffectsFunctionalTest,
      public ::testing::WithParamInterface<std::pair<const TestEffectParameterBase, bool> >
{
};
