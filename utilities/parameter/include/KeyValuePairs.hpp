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
#pragma once

#include <convert.hpp>
#include <string>
#include <map>
#include <utils/Errors.h>

namespace intel_audio
{

/**
 * Helper class to parse / retrieve a semi-colon separated string of {key, value} pairs.
 */
class KeyValuePairs
{
private:
    typedef std::map<std::string, std::string> Map;
    typedef Map::iterator MapIterator;
    typedef Map::const_iterator MapConstIterator;

public:
    KeyValuePairs() {}
    KeyValuePairs(const std::string &keyValuePairs);
    virtual ~KeyValuePairs();

    /**
     * Checks if a KeyValuePairs object has a key or not
     * @param[in] key to be checked if present or not
     * @return true if the key is found within the collection of pairs, false otherwise.
     */
    bool hasKey(const std::string &key) const { return mMap.find(key) != mMap.end(); }

    /**
     * Convert the AudioParameter into a semi-colon separated string of {key, value} pairs.
     *
     * @return semi-colon separated string of {key, value} pairs
     */
    std::string toString();

    /**
     * Add all pairs contained in a semi-colon separated string of {key, value} to the collection.
     * The collection may contains keys and/or keys-value pairs.
     * If the key is found, it will just update the value.
     * If the key is not found, it will add the new key and its associated value.
     *
     * @param[in] keyValuePairs semi-colon separated string of {key, value} pairs.
     *
     * @return OK if all key / value pairs were added correctly.
     * @return error code otherwise, at least one pair addition failed.
     */
    android::status_t add(const std::string &keyValuePairs);

    /**
     * Add a new value pair to the collection.
     *
     * @tparam T type of the value to add.
     * @param[in] key to add.
     * @param[in] value to add.
     *
     * @return OK if the key was added with its corresponding value.
     * @return ALREADY_EXISTS if the key was already added, the value is updated however.
     * @return error code otherwise.
     */
    template <typename T>
    android::status_t add(const std::string &key, const T &value)
    {
        std::string literal;
        if (!audio_comms::utilities::convertTo(value, literal)) {
            return android::BAD_VALUE;
        }
        return addLiteral(key, literal);
    }

    /**
     * Remove a value pair from the collection.
     *
     * @param[in] key to remove.
     *
     * @return OK if the key was removed successfuly.
     * @return BAD_VALUE if the key was not found.
     */
    android::status_t remove(const std::string &key);

    /**
     * Get a value from a given key from the collection.
     *
     * @tparam T type of the value to get.
     * @param[in] key associated to the value to get.
     * @param[out] value to get.
     *
     * @return OK if the key was found and the value is returned into value parameter.
     * @return error code otherwise.
     */
    template <typename T>
    android::status_t get(const std::string &key, T &value) const
    {
        std::string literalValue;
        android::status_t status = getLiteral(key, literalValue);
        if (status != android::OK) {
            return status;
        }
        if (!audio_comms::utilities::convertTo(literalValue, value)) {
            return android::BAD_VALUE;
        }
        return android::OK;
    }

    /**
     * @return the number of {key, value} pairs found in the collection.
     */
    size_t size()
    {
        return mMap.size();
    }

private:
    /**
     * Add a new value pair to the collection.
     *
     * @param[in] key to add.
     * @param[in] value to add (as literal).
     *
     * @return OK if the key was added with its corresponding value.
     * @return error code otherwise.
     */
    android::status_t addLiteral(const std::string &key, const std::string &value);

    /**
     * Get a literal value from a given key from the collection.
     *
     * @param[in] key associated to the value to get.
     * @param[out] value to get.
     *
     * @return OK if the key was found and the value is returned into value parameter.
     * @return error code otherwise.
     */
    android::status_t getLiteral(const std::string &key, std::string &value) const;

    std::map<std::string, std::string> mMap; /**< value pair collection Map indexed by the key. */

    static const char *const mPairDelimiter; /**< Delimiter between {key, value} pairs. */
    static const char *const mPairAssociator; /**< key value Pair token. */
};

}   // namespace intel_audio
