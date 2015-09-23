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

#include <AudioCommsAssert.hpp>
#include <utilities/Log.hpp>
#include <map>

namespace intel_audio
{

template <class T>
class ElementCollection
{
protected:
    typedef std::map<std::string, T *> Container;
    typedef typename Container::iterator Iter;
    typedef typename Container::const_iterator ConstIter;

public:
    virtual ~ElementCollection()
    {
        reset();
    }

    virtual void reset()
    {
        for (typename Container::iterator it = mElements.begin();
             it != mElements.end();
             ++it) {
            delete it->second;
        }
        mElements.clear();
    }

    /** Add a routing element referred by its name and id to a map. Routing Elements are ports,
     * port groups, route and stream route. Compile time error generated if called with wrong
     * type.
     *
     * @tparam T type of routing element to add.
     * @param[in] key to be used for indexing the map.
     * @param[in] name of the routing element to add.
     *
     * @return Newly created and added element, NULL otherwise..
     */
    bool addElement(const std::string &key, T *element)
    {
        if (getElement(key) != NULL) {
            audio_comms::utilities::Log::Warning() << __FUNCTION__
                                                   << ": element(" << key << ") already added";
            return false;
        }
        mElements[key] = element;
        return true;
    }

    /**
     * Get a routing element from a map by its name. Routing Elements are ports, port
     * groups, route and stream route. Compile time error generated if called with wrong type.
     *
     * @tparam T type of routing element to search.
     * @param[in] name name of the routing element to find.
     *
     * @return valid pointer on routing element if found, assert if element not found.
     */
    T *getElement(const std::string &name)
    {
        typename Container::iterator it = mElements.find(name);
        return it == mElements.end() ? NULL : it->second;
    }

    /**
     * Reset the availability of routing elements belonging to a map. Routing Elements are
     * ports, port, groups, route and stream route. Compile time error generated if called with
     * wrong type.
     *
     * @tparam T type of routing element.
     *
     * @return valid pointer on element if found, NULL otherwise.
     */
    virtual void resetAvailability()
    {
        typename Container::iterator it;
        for (it = mElements.begin(); it != mElements.end(); ++it) {
            it->second->resetAvailability();
        }
    }
    Iter begin()
    {
        return mElements.begin();
    }
    Iter end()
    {
        return mElements.end();
    }
    ConstIter begin() const
    {
        return mElements.begin();
    }
    ConstIter end() const
    {
        return mElements.end();
    }

protected:
    Container mElements;
};

} // namespace intel_audio
