/*
 * Copyright (C) 2017-2018 Intel Corporation
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

#include "RouteManagerConfig.hpp"
#include "AudioPort.hpp"
#include <stdint.h>
#include <string>
#include <utils/Errors.h>

struct _xmlNode;
struct _xmlDoc;

namespace intel_audio
{

struct AudioCriterionTypeTraits
{
    static const char *const tag;
    static const char *const collectionTag;

    struct Attributes
    {
        static const char name[];
        static const char type[];
        static const char values[];
    };

    typedef CriterionType Element;
    typedef CriterionType *PtrElement;
    typedef CriterionTypes Collection;
    typedef RouteManagerConfig *PtrSerializingCtx;

    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, PtrElement &element,
                                         PtrSerializingCtx serializingContext);
};

struct AudioCriterionTraits
{
    static const char *const tag;
    static const char *const collectionTag;

    struct Attributes
    {
        static const char name[];
        static const char type[];
        static const char defaultVal[];
        static const char parameter[];
        static const char mapping[];
    };

    typedef Criterion Element;
    typedef Criterion *PtrElement;
    typedef Criteria Collection;
    typedef RouteManagerConfig *PtrSerializingCtx;

    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, PtrElement &element,
                                         PtrSerializingCtx serializingContext);
};

struct RogueParameterTraits
{
    static const char *const tag;
    static const char *const collectionTag;

    struct Attributes
    {
        static const char name[];
        static const char type[];
        static const char mapping[];
        static const char defaultVal[];
        static const char parameter[];
        static const char path[];
    };

    typedef Parameter Element;
    typedef Parameter *PtrElement;
    typedef Parameters Collection;
    typedef RouteManagerConfig *PtrSerializingCtx;

    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, PtrElement &element,
                                         PtrSerializingCtx serializingContext);
};

struct AudioProfileTraits
{
    static const char *const tag;
    static const char *const collectionTag;

    struct Attributes
    {
        static const char samplingRates[];
        static const char format[];
        static const char channelMasks[];
    };

    typedef AudioCapability Element;
    typedef AudioCapability PtrElement;
    typedef AudioCapabilities Collection;
    typedef void *PtrSerializingCtx;

    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, PtrElement &element,
                                         PtrSerializingCtx serializingContext);
};

struct DevicePortTraits
{
    static const char *const tag;
    static const char *const collectionTag;

    struct Attributes
    {
        static const char name[];
        static const char type[];
        static const char role[];
        static const char address[];
    };

    typedef DevicePort Element;
    typedef DevicePort *PtrElement;
    typedef AudioPorts Collection;
    typedef void *PtrSerializingCtx;

    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, PtrElement &element,
                                         PtrSerializingCtx serializingContext);
};

struct RouteTraits
{
    static const char *const tag;
    static const char *const collectionTag;

    struct Attributes
    {
        static const char sink[];
        static const char sources[];
        static const char name[]; // name of the route

    };

    typedef AudioRoute Element;
    typedef AudioRoute *PtrElement;
    typedef AudioRouteCollection Collection;
    typedef RouteManagerConfig *PtrSerializingCtx;

    /**
     * Parse the AudioPorts from the strPorts string and check
     * whether it includes a MixPort
     * @param[out] ports parsed from the strPorts string.
     * @param[in]  strPorts string includes the source or sink ports.
     * @param[out] hasMixPort true if the strPorts includes a MixPort
     * false otherwise
     * @param[in]  ctx the runtime context
     */

    static android::status_t parsePorts(AudioPorts &ports,
                                        std::string strPorts,
                                        bool &hasMixPort,
                                        PtrSerializingCtx ctx);

    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, PtrElement &element,
                                         PtrSerializingCtx serializingContext);
};

struct ModuleTraits
{
    static const char *const tag;
    static const char *const collectionTag;

    struct Module {};
    typedef Module Element;
    typedef Module *PtrElement;
    typedef std::vector<Module *> Collection;
    typedef RouteManagerConfig *PtrSerializingCtx;

    static android::status_t deserialize(_xmlDoc * doc, const _xmlNode * root, PtrElement & element,
                                         PtrSerializingCtx /*serializingContext*/);
};

struct MixPortTraits
{
    static const char *const tag;
    static const char *const collectionTag;

    struct Attributes
    {
        static const char name[];
        static const char role[];
        static const char card[];
        static const char device[];
        static const char deviceAddress[];
        static const char flagMask[];
        static const char requirePreEnable[];
        static const char requirePostDisable[];
        static const char silencePrologMs[];
        static const char channelsPolicy[];
        static const char channelPolicyCopy[];
        static const char channelPolicyIgnore[];
        static const char channelPolicyAverage[];
        static const char periodSize[];
        static const char periodCount[];
        static const char startThreshold[];
        static const char stopThreshold[];
        static const char silenceThreshold[];
        static const char availMin[];
        static const char dynamicChannelMapsControl[];
        static const char dynamicSampleRatesControl[];
        static const char dynamicFormatsControl[];
        static const char supportedUseCases[];
        static const char supportedDevices[];
        static const char devicePorts[];
        static const char effects[];
    };


    typedef MixPort Element;
    typedef MixPort *PtrElement;
    typedef AudioPorts Collection;
    typedef RouteManagerConfig *PtrSerializingCtx;

    static android::status_t deserialize(_xmlDoc *doc, const _xmlNode *root, PtrElement &element,
                                         PtrSerializingCtx serializingContext);

    // MixPort has no child
};

class RouteSerializer
{
private:
    static const char *const rootName;

    static const char *const versionAttribute;
    static const uint32_t gMajor; /**< the major number of the policy xml format version. */
    static const uint32_t gMinor; /**< the minor number of the policy xml format version. */
    typedef RouteManagerConfig Element;

public:
    RouteSerializer();
    android::status_t deserialize(const char *str, RouteManagerConfig &config);

private:
    std::string mRootElementName;
    std::string mVersion;
};

}  // namespace intel_audio
