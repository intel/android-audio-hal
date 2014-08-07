/*
 * INTEL CONFIDENTIAL
 * Copyright (c) 2014 Intel
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
#define LOG_TAG "ModemProxy"

#include "ModemProxy.hpp"
#include "Value.hpp"
#include <AudioBand.h>
#include <convert.hpp>
#include <ModemCollection.hpp>
#include <AudioCommsAssert.hpp>
#include "ModemAudioManagerInterface.h"
#include <InterfaceProviderLib.h>
#include <utils/Log.h>

using namespace std;
using namespace android;
using audio_comms::utilities::convertTo;

namespace intel_audio
{

const std::string &ModemProxy::mKeyState = "modem_state_";
const std::string &ModemProxy::mKeyCallStatus = "call_status_";
const std::string &ModemProxy::mKeyBandType = "csv_band_type_";
const std::string &ModemProxy::mLiteralFalseValue = "0";

ModemProxy::ModemProxy(const string &libraryName,
                       const string &instance,
                       ValueObserver *observer,
                       ValueRegister *reg)
    : ValueSet(observer, reg),
      mModemAudioManagerInterface(NULL),
      mInstance(instance)
{
    // Try to connect a ModemAudioManager Interface
    NInterfaceProvider::IInterfaceProvider *interfaceProvider =
        audio_comms::mamgr::ModemCollection::getInstance().getModem(libraryName, mInstance);

    if (interfaceProvider == NULL) {

        ALOGI("No MAMGR library.");
    } else {

        // Retrieve the ModemAudioManager Interface
        mModemAudioManagerInterface =
            interfaceProvider->queryInterface<IModemAudioManagerInterface>();
        if (mModemAudioManagerInterface == NULL) {

            ALOGE("Failed to get ModemAudioManager interface");
        } else {

            // Declare ourselves as observer
            mModemAudioManagerInterface->setModemAudioManagerObserver(this);
            ALOGV("Connected to a ModemAudioManager interface");
        }
    }

    // Creates its own values and add them
    Value *value = new Value(mKeyState + mInstance,
                             reinterpret_cast<Value::GetValueCallback>(&ModemProxy::isModemAlive),
                             this);
    addValue(value);
    value = new Value(mKeyCallStatus + mInstance,
                      reinterpret_cast<Value::GetValueCallback>(&ModemProxy::isModemAudioAvailable),
                      this);
    addValue(value);
    value = new Value(mKeyBandType + mInstance,
                      reinterpret_cast<Value::GetValueCallback>(&ModemProxy::getAudioBand),
                      this);
    addValue(value);
}

ModemProxy::~ModemProxy()
{
    stop();
}

void ModemProxy::onModemAudioStatusChanged()
{
    ALOGV("%s on Modem instance %s", __FUNCTION__, mInstance.c_str());
    notifyValueChange(mKeyCallStatus + mInstance);
}

void ModemProxy::onModemStateChanged()
{
    ALOGV("%s on Modem instance %s", __FUNCTION__, mInstance.c_str());
    notifyValueChange(mKeyState + mInstance);
}

void ModemProxy::onModemAudioBandChanged()
{
    ALOGV("%s on Modem instance %s", __FUNCTION__, mInstance.c_str());
    notifyValueChange(mKeyBandType + mInstance);
}

const std::string ModemProxy::isModemAlive(void *context) const
{
    ModemProxy *proxy = static_cast<ModemProxy *>(context);
    AUDIOCOMMS_ASSERT(proxy != NULL, "NULL context given back");
    if (proxy->mModemAudioManagerInterface == NULL) {
        return mLiteralFalseValue;
    }
    string modemAlive;
    convertTo(proxy->mModemAudioManagerInterface->isModemAlive(), modemAlive);
    return modemAlive;
}

const std::string ModemProxy::isModemAudioAvailable(void *context) const
{
    ModemProxy *proxy = static_cast<ModemProxy *>(context);
    AUDIOCOMMS_ASSERT(proxy != NULL, "NULL context given back");
    if (proxy->mModemAudioManagerInterface == NULL) {
        return mLiteralFalseValue;
    }
    string modemAudioAvailable;
    convertTo(proxy->mModemAudioManagerInterface->isModemAudioAvailable(), modemAudioAvailable);
    return modemAudioAvailable;
}

const std::string ModemProxy::getAudioBand(void *context) const
{
    ModemProxy *proxy = static_cast<ModemProxy *>(context);
    AUDIOCOMMS_ASSERT(proxy != NULL, "NULL context given back");
    if (proxy->mModemAudioManagerInterface == NULL) {
        return CAudioBand::toLiteral(CAudioBand::ENarrow);
    }
    return CAudioBand::toLiteral(proxy->mModemAudioManagerInterface->getAudioBand());
}

bool ModemProxy::start()
{
    if (mModemAudioManagerInterface == NULL) {
        return false;
    }
    if (!mModemAudioManagerInterface->start()) {
        ALOGW("%s: could not start ModemAudioManager for instance %s",
              __FUNCTION__, mInstance.c_str());
        mModemAudioManagerInterface->setModemAudioManagerObserver(NULL);
        return false;
    }
    return true;
}

void ModemProxy::stop()
{
    if (mModemAudioManagerInterface != NULL) {
        // Unsuscribe & stop ModemAudioManager
        mModemAudioManagerInterface->setModemAudioManagerObserver(NULL);
        mModemAudioManagerInterface->stop();
    }
}

} // namespace intel_audio
