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

#include "AudioPortGroup.hpp"
#include "RoutingElement.hpp"
#include <list>
#include <string>

namespace intel_audio
{

class AudioPort;

class AudioPortGroup : public RoutingElement
{
private:
    typedef std::list<AudioPort *>::iterator PortListIterator;
    typedef std::list<AudioPort *>::const_iterator PortListConstIterator;

public:
    AudioPortGroup(const std::string &name);
    virtual ~AudioPortGroup();

    /**
     * Add a port to the group.
     *
     * An audio group represents a set of mutual exclusive ports that may be on a shared SSP bus.
     *
     * @param[in] port Port to add as a members of the group.
     */
    void addPortToGroup(AudioPort *port);

    /**
     * Block all the other port from this group except the requester.
     *
     * An audio group represents a set of mutual exclusive ports. Once a port from
     * this group is used, it protects the platform by blocking all the mutual exclusive
     * ports that may be on a shared SSP bus.
     *
     * @param[in] port Port in use that request to block all the members of the group.
     */
    void blockMutualExclusivePort(const AudioPort *port);

private:
    std::list<AudioPort *> mPortList; /**< mutual exlusive ports belonging to this PortGroup. */
};

} // namespace intel_audio
