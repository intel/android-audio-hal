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

#include <IStreamInterface.hpp>
#include <RouteInterface.hpp>
#include <NonCopyable.hpp>

#pragma once

namespace intel_audio
{

class AudioRouteManager;

class RouteManagerInstance : public audio_comms::utilities::NonCopyable
{
protected:
    RouteManagerInstance();

public:
    virtual ~RouteManagerInstance();

    /**
     * Interface getters.
     */
    static IStreamInterface *getStreamInterface();
    static IRouteInterface *getRouteInterface();

private:
    /**
     * Get Route Manager instance.
     *
     * @return pointer to Route Manager Instance object.
     */
    static RouteManagerInstance *getInstance();

    /**< Audio route manager singleton */
    AudioRouteManager *mAudioRouteManager;
};

} // namespace intel_audio
