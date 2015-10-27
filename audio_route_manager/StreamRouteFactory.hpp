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
#pragma once

#include "AudioStreamRoute.hpp"
#include <map>
#include <string>

namespace intel_audio
{

class StreamRouteFactory
{
private:
    typedef std::map<std::string,
                     AudioStreamRoute *(*)(const std::string &name, bool isOut)> mapType;

public:
    static AudioStreamRoute *createStreamRoute(const std::string &name, bool isOut)
    {
        auto streamRouteIter = getMap().find(name);
        if ((streamRouteIter) != getMap().end()) {
            return streamRouteIter->second(name, isOut);
        }

        // No specific creator registered for this name, use generic
        return createT(name, isOut);
    }

    template <typename T>
    static void registerStreamRoute(const std::string &name)
    {
        getMap().insert(std::make_pair(name, &createT<T> ));
    }

private:
    static mapType &getMap()
    {
        static mapType map;
        return map;
    }
};


template <typename T>
struct RegisterStreamRoute
{
    RegisterStreamRoute(const std::string &name)
    {
        StreamRouteFactory::registerStreamRoute<T>(name);
    }
};

} // namespace intel_audio
