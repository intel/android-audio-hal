/*
 * Copyright (C) 2013-2015 Intel Corporation
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

#include <Observer.hpp>
#include <semaphore.h>

namespace intel_audio
{

class AudioPort;

class AudioRouteManagerObserver : public audio_comms::utilities::Observer
{
public:
    AudioRouteManagerObserver();
    virtual ~AudioRouteManagerObserver();

    /**
     * Synchronously wait a notification from the Subject observed
     */
    void waitNotification();

    /**
     * Notification callback from the Subject observed
     */
    virtual void notify();

private:
    sem_t mSyncSem; /**< actual semaphore. */
};

} // namespace intel_audio
