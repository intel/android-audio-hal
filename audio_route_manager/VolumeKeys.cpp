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
#define LOG_TAG "RouteManager/VolumeKeys"

#include "VolumeKeys.h"
#include <utils/Log.h>
#include <fcntl.h>

namespace android_audio_legacy
{

const char *const VolumeKeys::GPIO_KEYS_WAKEUP_ENABLE =
    "/sys/devices/platform/gpio-keys/enabled_wakeup";
const char *const VolumeKeys::GPIO_KEYS_WAKEUP_DISABLE =
    "/sys/devices/platform/gpio-keys/disabled_wakeup";

const char *const VolumeKeys::KEY_VOLUMEDOWN = "114";
const char *const VolumeKeys::KEY_VOLUMEUP = "115";

bool VolumeKeys::_wakeupEnabled = false;

int VolumeKeys::wakeup(bool isEnabled)
{
    if (_wakeupEnabled == isEnabled) {

        // Nothing to do, bailing out
        return 0;
    }
    ALOGD("%s volume keys wakeup", isEnabled ? "Enable" : "Disable");

    int fd;
    int rc;

    const char *const gpioKeysWakeup =
        isEnabled ? GPIO_KEYS_WAKEUP_ENABLE : GPIO_KEYS_WAKEUP_DISABLE;
    fd = open(gpioKeysWakeup, O_RDWR);
    if (fd < 0) {
        ALOGE("Cannot open sysfs gpio-keys interface (%d)", fd);
        goto return_error;
    }
    rc = write(fd, KEY_VOLUMEDOWN, sizeof(KEY_VOLUMEDOWN));
    rc += write(fd, KEY_VOLUMEUP, sizeof(KEY_VOLUMEUP));
    close(fd);
    if (rc != (sizeof(KEY_VOLUMEDOWN) + sizeof(KEY_VOLUMEUP))) {
        ALOGE("sysfs gpio-keys write error");
        goto return_error;
    }

    _wakeupEnabled = true;
    ALOGD("Volume keys wakeup enable OK\n");
    return 0;

return_error:

    ALOGE("Volume keys wakeup enable failed\n");
    return -1;
}
}
