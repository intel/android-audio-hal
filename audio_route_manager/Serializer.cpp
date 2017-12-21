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

#define LOG_TAG "RouteManager::Serializer"
// #define LOG_NDEBUG 0

#include "Serializer.hpp"
#if (defined (USE_ALSA_LIB))
#include <AlsaAudioDevice.hpp>
#endif
#include <TinyAlsaAudioDevice.hpp>
#include "MixPortConfig.hpp"
#include <convert.hpp>
#include <typeconverter/TypeConverter.hpp>
#include <libxml/parser.h>
#include <libxml/xinclude.h>
#include <string>
#include <vector>
#include <sstream>
#include <istream>

using namespace std;
using namespace android;
using audio_comms::utilities::Log;
using audio_comms::utilities::convertTo;

static const std::string gUnsignedIntegerTypeTag = "uint32_t";
static const std::string gSignedIntegerTypeTag = "int32_t";
static const std::string gStringTypeTag = "string";
static const std::string gDoubleTypeTag = "double";

namespace intel_audio
{

typedef std::pair<std::string, std::string> AndroidParamMappingValuePair;

string getXmlAttribute(const xmlNode *cur, const char *attribute)
{
    xmlChar *xmlValue = xmlGetProp(cur, (const xmlChar *)attribute);
    if (xmlValue == NULL) {
        return "";
    }
    string value((const char *)xmlValue);
    xmlFree(xmlValue);
    return value;
}

const char *const RouteSerializer::rootName = "audioPolicyConfiguration";
const char *const RouteSerializer::versionAttribute = "version";
const uint32_t RouteSerializer::gMajor = 1;
const uint32_t RouteSerializer::gMinor = 0;

static const char *const gReferenceElementName = "reference";
static const char *const gReferenceAttributeName = "name";

template <class Trait>
static void getReference(const _xmlNode *root, const _xmlNode * &refNode, const string &refName)
{
    const _xmlNode *col = root;
    while (col != NULL) {
        if (!xmlStrcmp(col->name, (const xmlChar *)Trait::collectionTag)) {
            const xmlNode *cur = col->children;
            while (cur != NULL) {
                if ((!xmlStrcmp(cur->name, (const xmlChar *)gReferenceElementName))) {
                    string name = getXmlAttribute(cur, gReferenceAttributeName);
                    if (refName == name) {
                        refNode = cur;
                        return;
                    }
                }
                cur = cur->next;
            }
        }
        col = col->next;
    }
    return;
}

template <class Trait>
static status_t deserializeCollection(_xmlDoc *doc, const _xmlNode *cur,
                                      typename Trait::Collection &collection,
                                      typename Trait::PtrSerializingCtx serializingContext)
{
    const xmlNode *root = cur->xmlChildrenNode;
    while (root != NULL) {
        if (xmlStrcmp(root->name, (const xmlChar *)Trait::collectionTag) &&
            xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            root = root->next;
            continue;
        }
        const xmlNode *child = root;
        if (!xmlStrcmp(child->name, (const xmlChar *)Trait::collectionTag)) {
            child = child->xmlChildrenNode;
        }
        while (child != NULL) {
            if (!xmlStrcmp(child->name, (const xmlChar *)Trait::tag)) {
                typename Trait::PtrElement element;
                status_t status = Trait::deserialize(doc, child, element, serializingContext);
                if (status == NO_ERROR) {
                    collection.push_back(element);
                }
            }
            child = child->next;
        }
        if (!xmlStrcmp(root->name, (const xmlChar *)Trait::tag)) {
            return NO_ERROR;
        }
        root = root->next;
    }
    return NO_ERROR;
}

/**
 * This class defines a unary function to be used when looping on the vector of value pairs
 * of a parameter.
 * It will help applying all the pairs (Android-Parameter value - PFW-Parameter value)
 * for the mapping table.
 */
class SetAndroidParamMappingPairHelper
{
public:
    SetAndroidParamMappingPairHelper(Parameter *param) : mParamCriterion(param) {}

    void operator()(AndroidParamMappingValuePair pair)
    {
        mParamCriterion->setMappingValuePair(pair.first, pair.second);
    }
    Parameter *mParamCriterion;
};

std::vector<AndroidParamMappingValuePair> parseMappingTable(const char *values)
{
    AUDIOCOMMS_ASSERT(values != NULL, "error in parsing file");
    char *mappingPairs = strndup(values, strlen(values));
    char *ctx;
    std::vector<AndroidParamMappingValuePair> valuePairs;

    char *mappingPair = strtok_r(mappingPairs, ",", &ctx);
    while (mappingPair != NULL) {
        if (strlen(mappingPair) != 0) {

            char *first = strtok(mappingPair, ":");
            char *second = strtok(NULL, ":");
            AUDIOCOMMS_ASSERT((first != NULL) && (strlen(first) != 0) &&
                              (second != NULL) && (strlen(second) != 0),
                              "invalid value pair");
            AndroidParamMappingValuePair pair = std::make_pair(first, second);
            Log::Verbose() << __FUNCTION__ << ": adding pair: " << first << ", " << second;
            valuePairs.push_back(pair);
        }
        mappingPair = strtok_r(NULL, ",", &ctx);
    }
    free(mappingPairs);
    return valuePairs;
}


const char *const AudioCriterionTypeTraits::tag = "criterion_type";
const char *const AudioCriterionTypeTraits::collectionTag = "criterion_types";

const char AudioCriterionTypeTraits::Attributes::name[] = "name";
const char AudioCriterionTypeTraits::Attributes::type[] = "type";
const char AudioCriterionTypeTraits::Attributes::values[] = "values";

status_t AudioCriterionTypeTraits::deserialize(_xmlDoc */*doc*/, const _xmlNode *child,
                                               PtrElement &criterionType,
                                               PtrSerializingCtx serializingContext)
{
    string name = getXmlAttribute(child, Attributes::name);
    if (name.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::name << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::name << "=" << name;

    string type = getXmlAttribute(child, Attributes::type);
    if (type.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::type << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::type << "=" << type;
    bool isInclusive(type == "inclusive");

    AUDIOCOMMS_ASSERT((serializingContext->getCriterionType(name) == nullptr),
                      " already added " << name << " criterion [" << type << "]");

    Log::Verbose() << __FUNCTION__ << ": Adding " << name << " for " << tag << " PFW";
    criterionType = new CriterionType(name, isInclusive, serializingContext->getConnector());

    string values = getXmlAttribute(child, Attributes::values);
    if (values.empty()) {
        Log::Verbose() << __FUNCTION__ << ": No attribute " << Attributes::values << " found.";
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::values << "=" << values;

    char *valueNames = strndup(values.c_str(), strlen(values.c_str()));
    uint32_t index = 0;
    char *ctx;
    char *valueName = strtok_r(valueNames, ",", &ctx);
    while (valueName != NULL) {
        if (strlen(valueName) != 0) {
            // Conf file may use or not pair, if no pair, use incremental index, else
            // use provided index.
            if (strchr(valueName, ':') != NULL) {
                char *first = strtok(valueName, ":");
                char *second = strtok(NULL, ":");
                AUDIOCOMMS_ASSERT((first != NULL) && (strlen(first) != 0) &&
                                  (second != NULL) && (strlen(second) != 0),
                                  "invalid value pair");

                bool isValueProvidedAsHexa = !string(first).compare(0, 2, "0x");
                if (isValueProvidedAsHexa) {
                    if (!convertTo<string, uint32_t>(first, index)) {
                        Log::Error() << __FUNCTION__ << ": Invalid value(" << first << ")";
                    }
                } else {
                    int32_t signedIndex = 0;
                    if (!convertTo<string, int32_t>(first, signedIndex)) {
                        Log::Error() << __FUNCTION__ << ": Invalid value(" << first << ")";
                    }
                    index = signedIndex;
                }
                Log::Verbose() << __FUNCTION__ << ": name=" << name << ", index=" << index
                               << ", value=" << second << " for " << tag << " PFW";
                criterionType->addValuePair(index, second);
            } else {
                uint32_t pfwIndex = isInclusive ? 1 << index : index;
                Log::Verbose() << __FUNCTION__ << ": name=" << name << ", index="
                               << pfwIndex << ", value=" << valueName;
                criterionType->addValuePair(pfwIndex, valueName);
                index += 1;
            }
        }
        valueName = strtok_r(NULL, ",", &ctx);
    }
    free(valueNames);
    return NO_ERROR;
}

const char *const AudioCriterionTraits::tag = "criterion";
const char *const AudioCriterionTraits::collectionTag = "criteria";

const char AudioCriterionTraits::Attributes::name[] = "name";
const char AudioCriterionTraits::Attributes::type[] = "type";
const char AudioCriterionTraits::Attributes::parameter[] = "parameter";
const char AudioCriterionTraits::Attributes::defaultVal[] = "default";
const char AudioCriterionTraits::Attributes::mapping[] = "mapping";

status_t AudioCriterionTraits::deserialize(_xmlDoc */*doc*/, const _xmlNode *child,
                                           PtrElement &criterion,
                                           PtrSerializingCtx serializingContext)
{
    string name = getXmlAttribute(child, Attributes::name);
    if (name.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::name << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::name << "=" << name;

    AUDIOCOMMS_ASSERT(serializingContext->getCriterion(name) == nullptr,
                      "Criterion " << name << " already added.");

    string defaultValue = getXmlAttribute(child, Attributes::defaultVal);
    if (defaultValue.empty()) {
        Log::Verbose() << __FUNCTION__ << ": No attribute " << Attributes::defaultVal << " found.";
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::defaultVal << "=" <<
        defaultValue;

    string paramKey = getXmlAttribute(child, Attributes::parameter);
    if (paramKey.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::parameter << " found.";
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::parameter << "=" <<
        paramKey;

    string criterionTypeName = getXmlAttribute(child, Attributes::type);
    if (criterionTypeName.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::type << " found.";
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::type << "=" <<
        criterionTypeName;
    CriterionType *criterionType = serializingContext->getCriterionType(criterionTypeName);

    std::vector<AndroidParamMappingValuePair> valuePairs;
    string mapping = getXmlAttribute(child, Attributes::mapping);
    if (not mapping.empty()) {
        Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::mapping << "=" <<
            mapping;
        valuePairs = parseMappingTable(mapping.c_str());
    }

    criterion =
        new Criterion(name, criterionType, serializingContext->getConnector(), defaultValue);

    if (not paramKey.empty()) {
        CriterionParameter *cp = new CriterionParameter(paramKey, name, *criterion, defaultValue);
        for_each(valuePairs.begin(), valuePairs.end(), SetAndroidParamMappingPairHelper(cp));
        serializingContext->addParameter(cp);
    }
    return NO_ERROR;
}


const char *const RogueParameterTraits::tag = "rogue_parameter";
const char *const RogueParameterTraits::collectionTag = "rogue_parameters";

const char RogueParameterTraits::Attributes::name[] = "name";
const char RogueParameterTraits::Attributes::path[] = "path";
const char RogueParameterTraits::Attributes::type[] = "type";
const char RogueParameterTraits::Attributes::mapping[] = "mapping";
const char RogueParameterTraits::Attributes::parameter[] = "parameter";
const char RogueParameterTraits::Attributes::defaultVal[] = "default";

status_t RogueParameterTraits::deserialize(_xmlDoc */*doc*/, const _xmlNode *child,
                                           PtrElement &paramRogue,
                                           PtrSerializingCtx serializingContext)
{
    string path = getXmlAttribute(child, Attributes::path);
    if (path.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::path << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::path << "=" << path;

    string typeName = getXmlAttribute(child, Attributes::type);
    if (typeName.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::type << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::type << "=" << typeName;


    string paramKey = getXmlAttribute(child, Attributes::parameter);
    if (paramKey.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::parameter << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::parameter << "=" <<
        paramKey;

    string defaultValue = getXmlAttribute(child, Attributes::defaultVal);
    if (defaultValue.empty()) {
        Log::Verbose() << __FUNCTION__ << ": No attribute " << Attributes::defaultVal << " found.";
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::defaultVal << "=" <<
        defaultValue;

    string mapping = getXmlAttribute(child, Attributes::mapping);
    if (not mapping.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::mapping << " found.";
        return BAD_VALUE;
    }
    std::vector<AndroidParamMappingValuePair> valuePairs = parseMappingTable(mapping.c_str());
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::mapping << "=" << mapping;

    if (typeName == gUnsignedIntegerTypeTag) {
        paramRogue = new RogueParameter<uint32_t>(paramKey, path,
                                                  serializingContext->getConnector(), defaultValue);
    } else if (typeName == gSignedIntegerTypeTag) {
        paramRogue = new RogueParameter<int32_t>(paramKey, path,
                                                 serializingContext->getConnector(), defaultValue);
    } else if (typeName == gStringTypeTag) {
        paramRogue = new RogueParameter<string>(paramKey, path,
                                                serializingContext->getConnector(), defaultValue);
    } else if (typeName == gDoubleTypeTag) {
        paramRogue = new RogueParameter<double>(paramKey, path,
                                                serializingContext->getConnector(), defaultValue);
    } else {
        AUDIOCOMMS_ASSERT(true, ": " << tag << " type " << typeName << " not supported ");
    }

    for_each(valuePairs.begin(), valuePairs.end(), SetAndroidParamMappingPairHelper(paramRogue));
    return NO_ERROR;
}

const char *const AudioProfileTraits::collectionTag = "profiles";
const char *const AudioProfileTraits::tag = "profile";

const char AudioProfileTraits::Attributes::samplingRates[] = "samplingRates";
const char AudioProfileTraits::Attributes::format[] = "format";
const char AudioProfileTraits::Attributes::channelMasks[] = "channelMasks";

status_t AudioProfileTraits::deserialize(_xmlDoc */*doc*/,
                                         const _xmlNode *child,
                                         PtrElement &profile,
                                         PtrSerializingCtx /*serializingContext*/)
{
    // Empty channel masks allowed (dynamic)
    string channelMasks = getXmlAttribute(child, Attributes::channelMasks);
    // Empty mask means dynamic channels
    if (not channelMasks.empty()) {
        profile.mSupportedChannelMasks = channelMasksFromString(channelMasks, ",");
    }
    Log::Verbose() << __FUNCTION__ << ": " << Attributes::channelMasks << "=" << channelMasks;
    // Empty channel rates allowed (dynamic)
    string rates = getXmlAttribute(child, Attributes::samplingRates);
    if (not rates.empty()) {
        profile.mSupportedRates = samplingRatesFromString(rates, ",");
    }
    Log::Verbose() << __FUNCTION__ << ": " << Attributes::samplingRates << "=" << rates;
    // Empty formats allowed (dynamic)
    string format = getXmlAttribute(child, Attributes::format);
    if (not format.empty()) {
        FormatConverter::toEnum(format, profile.mSupportedFormat);
    }
    Log::Verbose() << __FUNCTION__ << ": " << Attributes::format << "=" << format;
    profile.isChannelMaskDynamic = channelMasks.empty();
    profile.isRateDynamic = rates.empty();
    profile.isFormatDynamic = format.empty();

    return NO_ERROR;
}

const char *const DevicePortTraits::tag = "devicePort";
const char *const DevicePortTraits::collectionTag = "devicePorts";

const char DevicePortTraits::Attributes::name[] = "tagName";
const char DevicePortTraits::Attributes::role[] = "role";
const char DevicePortTraits::Attributes::type[] = "type";
const char DevicePortTraits::Attributes::address[] = "address";

status_t DevicePortTraits::deserialize(_xmlDoc */*doc*/, const _xmlNode *root, PtrElement &element,
                                       PtrSerializingCtx /*serializingContext*/)
{
    string name = getXmlAttribute(root, Attributes::name);
    if (name.empty()) {
        Log::Error() << __FUNCTION__ << ": DevicePort: No attribute " << Attributes::name <<
            " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": DevicePort: attribute " << Attributes::name << "=" << name;
    string typeName = getXmlAttribute(root, Attributes::type);
    if (typeName.empty()) {
        Log::Error() << __FUNCTION__ << ": DevicePort: No attribute " << Attributes::type <<
            " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": DevicePort: attribute " << Attributes::type << "=" <<
        typeName;
    string role = getXmlAttribute(root, Attributes::role);
    if (role.empty()) {
        Log::Error() << __FUNCTION__ << ": DevicePort: No attribute " << Attributes::role <<
            " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": DevicePort: attribute " << Attributes::role << "=" << role;
    audio_devices_t type = AUDIO_DEVICE_NONE;
    if (not DeviceConverter::toEnum(typeName, type)) {
        Log::Error() << __FUNCTION__ << ": DevicePort: Wrong " << typeName << "for attribute " <<
            Attributes::type << " found.";
        return BAD_VALUE;
    }
    string address = getXmlAttribute(root, Attributes::address);
    if (not address.empty()) {
        Log::Verbose() << __FUNCTION__ << ": DevicePort: attribute " << Attributes::address <<
            " = " << address;
    }
    element = new Element(type, name, address);

    return NO_ERROR;
}

const char *const RouteTraits::tag = "route";
const char *const RouteTraits::collectionTag = "routes";

const char RouteTraits::Attributes::sink[] = "sink";
const char RouteTraits::Attributes::sources[] = "sources";
const char RouteTraits::Attributes::name[] = "name";

status_t RouteTraits::parsePorts(AudioPorts &ports,
                                 std::string strPorts,
                                 bool &hasMixPort,
                                 PtrSerializingCtx ctx)
{
    hasMixPort = false;
    char *cPorts = strndup(strPorts.c_str(), strlen(strPorts.c_str()));
    char *iter = strtok(cPorts, ",");
    while (iter != NULL) {
        if (strlen(iter) != 0) {
            string portName((const char *)iter);
            AudioPort *port = ctx->mDevicePorts.findByName(portName);
            if (port == NULL) {
                port = ctx->mMixPorts.findByName(portName);
                if (port != NULL) {
                    hasMixPort = true;
                }
            }

            if (port != NULL) {
                ports.push_back(port);
            } else {
                Log::Error() << __FUNCTION__ << ": No port " << portName << " found.";
                free(cPorts);
                return BAD_VALUE;
            }
        }
        iter = strtok(NULL, ",");
        if (iter && hasMixPort == true) {
            Log::Error() << __FUNCTION__ << ": the route must include only one mixport";
            return NO_ERROR;
        }
    }

    free(cPorts);
    return NO_ERROR;
}

status_t RouteTraits::deserialize(_xmlDoc */*doc*/, const _xmlNode *root, PtrElement &element,
                                  PtrSerializingCtx ctx)
{
    string sinkAttr = getXmlAttribute(root, Attributes::sink);
    if (sinkAttr.empty()) {
        Log::Error() << __FUNCTION__ << ": Route: No attribute " << Attributes::sink << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": Route: attribute " << Attributes::sink << "=" << sinkAttr;

    string name = getXmlAttribute(root, Attributes::name);
    if (name.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::name << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": Route: attribute " << Attributes::name << "=" << name;

    string sourcesAttr = getXmlAttribute(root, Attributes::sources);
    if (sourcesAttr.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::sources << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": Route: attribute " << Attributes::sources << "=" <<
        sourcesAttr;

    AudioPorts sinks;
    AudioPorts sources;
    bool hasSinkMixPort = false;
    bool hasSourceMixPort = false;
    bool isOut = true;
    parsePorts(sinks,
               sinkAttr,
               hasSinkMixPort,
               ctx);
    if (hasSinkMixPort) {
        isOut = false;
    }
    parsePorts(sources,
               sourcesAttr,
               hasSourceMixPort,
               ctx);

    if (hasSinkMixPort != hasSourceMixPort) {
        element =
            new AudioStreamRoute(name,
                                 sinks,
                                 sources,
                                 (isOut ? ROUTE_TYPE_STREAM_PLAYBACK : ROUTE_TYPE_STREAM_CAPTURE));
    } else {
        if (hasSinkMixPort == false) {
            // TODO: Create the AudioBackendRoute instance
        } else {
            Log::Error() << __FUNCTION__ << "The route '" << name <<
                "' is not supported. The sink and source can not include the mixport at the same time.";
        }
    }
    return NO_ERROR;
}

const char *const MixPortTraits::tag = "mixPort";
const char *const MixPortTraits::collectionTag = "mixPorts";

const char MixPortTraits::Attributes::name[] = "name";
const char MixPortTraits::Attributes::role[] = "role";
const char MixPortTraits::Attributes::card[] = "card";
const char MixPortTraits::Attributes::device[] = "device";
const char MixPortTraits::Attributes::deviceAddress[] = "deviceAddress";
const char MixPortTraits::Attributes::flagMask[] = "flags";
const char MixPortTraits::Attributes::requirePreEnable[] = "requirePreEnable";
const char MixPortTraits::Attributes::requirePostDisable[] = "requirePostDisable";
const char MixPortTraits::Attributes::silencePrologMs[] = "silencePrologMs";
const char MixPortTraits::Attributes::channelsPolicy[] = "channelsPolicy";
const char MixPortTraits::Attributes::channelPolicyCopy[] = "copy";
const char MixPortTraits::Attributes::channelPolicyIgnore[] = "ignore";
const char MixPortTraits::Attributes::channelPolicyAverage[] = "average";
const char MixPortTraits::Attributes::periodSize[] = "periodSize";
const char MixPortTraits::Attributes::periodCount[] = "periodCount";
const char MixPortTraits::Attributes::startThreshold[] = "startThreshold";
const char MixPortTraits::Attributes::stopThreshold[] = "stopThreshold";
const char MixPortTraits::Attributes::silenceThreshold[] = "silenceThreshold";
const char MixPortTraits::Attributes::availMin[] = "availMin";
const char MixPortTraits::Attributes::dynamicChannelMapsControl[] = "dynamicChannelMapControl";
const char MixPortTraits::Attributes::dynamicSampleRatesControl[] = "dynamicSampleRateControl";
const char MixPortTraits::Attributes::dynamicFormatsControl[] = "dynamicFormatControl";
const char MixPortTraits::Attributes::supportedUseCases[] = "supportedUseCases";
const char MixPortTraits::Attributes::supportedDevices[] = "supportedDevices";
const char MixPortTraits::Attributes::devicePorts[] = "devicePorts";
const char MixPortTraits::Attributes::effects[] = "effectsSupported";

status_t MixPortTraits::deserialize(_xmlDoc *doc, const _xmlNode *child, PtrElement &mixPort,
                                    PtrSerializingCtx ctx)
{
    string name = getXmlAttribute(child, Attributes::name);
    if (name.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::name << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": " << tag << " " << Attributes::name << "=" << name.c_str();
    string role = getXmlAttribute(child, Attributes::role);
    if (role.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::role << " found.";
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": Role= " << role.c_str();
    mixPort = new Element(name, role == "source");

    MixPortConfig mixPortConfig;
    mixPortConfig.isOut = (role == "source");
    string card = getXmlAttribute(child, Attributes::card);
    if (card.empty()) {
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::card << " found.";
        delete mixPort;
        return BAD_VALUE;
    }
    Log::Verbose() << __FUNCTION__ << ": " << Attributes::card << "=" << card;
    mixPortConfig.cardName = card;

    string device = getXmlAttribute(child, Attributes::device);

    // Empty device name -> infer user side alsa card
    // Valid device name -> use tiny alsa audio device
    if (device.empty()) {
#if (defined (USE_ALSA_LIB))
        mixPort->setAlsaDevice(new AlsaAudioDevice());
#else
        Log::Error() << __FUNCTION__ << ": No attribute " << Attributes::device << " found.";
        delete mixPort;
        return BAD_VALUE;
#endif
    } else {
        if (not convertTo<string, uint32_t>(device, mixPortConfig.deviceId)) {
            Log::Error() << __FUNCTION__ << ": Invalid " << device << " for attribute " <<
                Attributes::device;
            delete mixPort;
            return BAD_VALUE;
        }
        mixPort->setAlsaDevice(new TinyAlsaAudioDevice());
    }

    mixPortConfig.flagMask = 0;
    string flags = getXmlAttribute(child, Attributes::flagMask);
    if (not flags.empty()) {
        Log::Verbose() << __FUNCTION__ << ": attribute " << Attributes::flagMask << "=" << flags;
        // Source role
        mixPortConfig.flagMask = ((role == "source") ?
                                  OutputFlagConverter::maskFromString(flags, ",") :
                                  InputFlagConverter::maskFromString(flags, ","));
    }

    // If no flags is provided for input, use custom primary input flag by default
    if (role == "sink") {
        mixPortConfig.flagMask = (mixPortConfig.flagMask == AUDIO_INPUT_FLAG_NONE) ?
                                 AUDIO_INPUT_FLAG_PRIMARY : mixPortConfig.flagMask;
    }

    string periodSize = getXmlAttribute(child, Attributes::periodSize);
    if (periodSize.empty() || !convertTo<string, uint32_t>(periodSize, mixPortConfig.periodSize)) {
        Log::Error() << __FUNCTION__ << ": No valid attribute " << Attributes::periodSize
                     << " found.";
        delete mixPort;
        return BAD_VALUE;
    }
    string periodCount = getXmlAttribute(child, Attributes::periodCount);
    if (periodCount.empty() ||
        !convertTo<string, uint32_t>(periodCount, mixPortConfig.periodCount)) {
        Log::Error() << __FUNCTION__ << ": No valid attribute " << Attributes::periodCount
                     << " found.";
        delete mixPort;
        return BAD_VALUE;
    }
    string startThreshold = getXmlAttribute(child, Attributes::startThreshold);
    if (startThreshold.empty() ||
        not convertTo<string, uint32_t>(startThreshold, mixPortConfig.startThreshold)) {
        Log::Error() << __FUNCTION__ << ": No valid attribute " << Attributes::startThreshold
                     << " found.";
        delete mixPort;
        return BAD_VALUE;
    }
    string stopThreshold = getXmlAttribute(child, Attributes::stopThreshold);
    if (stopThreshold.empty() ||
        not convertTo<string, uint32_t>(stopThreshold, mixPortConfig.stopThreshold)) {
        Log::Error() << __FUNCTION__ << ": No valid attribute " << Attributes::stopThreshold
                     << " found.";
        delete mixPort;
        return BAD_VALUE;
    }
    string silenceThreshold = getXmlAttribute(child, Attributes::silenceThreshold);
    if (silenceThreshold.empty() ||
        not convertTo<string, uint32_t>(silenceThreshold, mixPortConfig.silenceThreshold)) {
        Log::Error() << __FUNCTION__ << ": No valid attribute " << Attributes::silenceThreshold
                     << " found.";
        delete mixPort;
        return BAD_VALUE;
    }
    string availMin = getXmlAttribute(child, Attributes::availMin);
    if (availMin.empty() || not convertTo<string, uint32_t>(availMin, mixPortConfig.availMin)) {
        Log::Error() << __FUNCTION__ << ": No valid attribute " << Attributes::availMin <<
            " found.";
        delete mixPort;
        return BAD_VALUE;
    }
    string silencePrologInMs = getXmlAttribute(child, Attributes::silencePrologMs);
    if (silencePrologInMs.empty() ||
        not convertTo<string, uint32_t>(silencePrologInMs, mixPortConfig.silencePrologInMs)) {
        Log::Error() << __FUNCTION__ << ": No valid attribute " << Attributes::silencePrologMs
                     << " found.";
        delete mixPort;
        return BAD_VALUE;
    }
    string requirePreEnable = getXmlAttribute(child, Attributes::requirePreEnable);
    if (requirePreEnable.empty() ||
        not convertTo<string, bool>(requirePreEnable, mixPortConfig.requirePreEnable)) {
        Log::Error() << __FUNCTION__ << ": Invalid " << requirePreEnable << " for attribute "
                     << Attributes::requirePreEnable;
        delete mixPort;
        return BAD_VALUE;
    }
    string requirePostDisable = getXmlAttribute(child, Attributes::requirePostDisable);
    if (requirePostDisable.empty() ||
        not convertTo<string, bool>(requirePostDisable, mixPortConfig.requirePostDisable)) {
        Log::Error() << __FUNCTION__ << ": Invalid " << requirePostDisable << " for attribute "
                     << Attributes::requirePostDisable;
        delete mixPort;
        return BAD_VALUE;
    }
    mixPortConfig.dynamicChannelMapsControl = getXmlAttribute(child,
                                                              Attributes::dynamicChannelMapsControl);
    mixPortConfig.dynamicFormatsControl = getXmlAttribute(child, Attributes::dynamicFormatsControl);
    mixPortConfig.dynamicRatesControl =
        getXmlAttribute(child, Attributes::dynamicSampleRatesControl);
    //    mixPortConfig.deviceAddress = getXmlAttribute(child, Attributes::deviceAddress);

    mixPortConfig.useCaseMask = 0;
    string supportedUseCases = getXmlAttribute(child, Attributes::supportedUseCases);
    mixPortConfig.useCaseMask = (role == "source") ?
                                0 : InputSourceConverter::maskFromString(supportedUseCases, ",");
    mixPortConfig.supportedDeviceMask = 0;
    string supportedDevices = getXmlAttribute(child, Attributes::supportedDevices);
    char *devices = strndup(supportedDevices.c_str(), strlen(supportedDevices.c_str()));
    char *dev = strtok(devices, ",");
    while (dev != NULL) {
        if (strlen(dev) != 0) {
            string strDevice((const char *)dev);
            DevicePort *port = (DevicePort *)ctx->mDevicePorts.findByName(strDevice);
            if (port != NULL) {
                mixPortConfig.supportedDeviceMask |= port->getDevice();
                if (not port->getDeviceAddress().empty()) {
                    mixPortConfig.deviceAddress = port->getDeviceAddress();
                    Log::Verbose() << __FUNCTION__ << ": adding @" << port->getDeviceAddress() <<
                        " to mix port" << name;
                }
            }
        }

        dev = strtok(NULL, ",");
    }

    free(devices);
    string channelsPolicy = getXmlAttribute(child, Attributes::channelsPolicy);
    if (not channelsPolicy.empty()) {
        vector<string> channelsPolicyVector;
        collectionFromString<DefaultTraits<string> >(channelsPolicy, channelsPolicyVector, ",");
        for (auto policyLiteral : channelsPolicyVector) {
            SampleSpec::ChannelsPolicy policy;
            if (policyLiteral == Attributes::channelPolicyCopy) {
                policy = SampleSpec::Copy;
            } else if (policyLiteral == Attributes::channelPolicyAverage) {
                policy = SampleSpec::Average;
            } else if (policyLiteral == Attributes::channelPolicyIgnore) {
                policy = SampleSpec::Ignore;
            } else {
                Log::Error() << __FUNCTION__ << ": Invalid " << channelsPolicy << " for attribute "
                             << Attributes::channelsPolicy;
                delete mixPort;
                return BAD_VALUE;
            }
            mixPortConfig.channelsPolicy.push_back(policy);
        }
    }
    AudioProfileTraits::Collection profiles;
    deserializeCollection<AudioProfileTraits>(doc, child, profiles, NULL);
    mixPortConfig.mAudioCapabilities = profiles;

    mixPort->setConfig(mixPortConfig);

    string effects = getXmlAttribute(child, Attributes::effects);
    if (not effects.empty()) {
        vector<string> effectsSupported;
        collectionFromString<DefaultTraits<string> >(effects, effectsSupported, ",");
        mixPort->setEffectSupported(effectsSupported);
    }
    return NO_ERROR;
}


const char *const ModuleTraits::tag = "module";
const char *const ModuleTraits::collectionTag = "modules";

status_t ModuleTraits::deserialize(xmlDocPtr doc, const xmlNode *root, PtrElement & /*module*/,
                                   PtrSerializingCtx ctx)
{
    std::string name = getXmlAttribute(root, "name");
    if (name != "primary") {
        Log::Warning() << __FUNCTION__ << ": Module " << name <<
            " outside primary outside HAL Scope";
        return BAD_VALUE;
    }

    /**
     * Deserialize childrens: devicePorts, mixPorts and routes
     * The ctx is the RouteManagerConfig. It saves the MixPorts
     * DevciePorts and AudioRoutes.
     * @see RouteManagerConfig::mDevicePorts
     * @see RouteManagerConfig::mMixPorts
     * @see RouteManagerConfig::mRoutes
     */
    deserializeCollection<DevicePortTraits>(doc, root, ctx->mDevicePorts, nullptr);
    deserializeCollection<MixPortTraits>(doc, root, ctx->mMixPorts, ctx);
    deserializeCollection<RouteTraits>(doc, root, ctx->mRoutes, ctx);
    return NO_ERROR;
}

RouteSerializer::RouteSerializer() : mRootElementName(rootName)
{
    std::ostringstream oss;
    oss << gMajor << "." << gMinor;
    mVersion = oss.str();
    Log::Verbose() << __FUNCTION__ << ": Version=" << mVersion.c_str() << " Root="
                   << mRootElementName.c_str();
}

status_t RouteSerializer::deserialize(const char *configFile, RouteManagerConfig &config)
{
    xmlDocPtr doc;
    doc = xmlParseFile(configFile);
    if (doc == NULL) {
        Log::Error() << __FUNCTION__ << ": Could not parse document " << configFile;
        return BAD_VALUE;
    }
    xmlNodePtr cur = xmlDocGetRootElement(doc);
    if (cur == NULL) {
        Log::Error() << __FUNCTION__ << ": Could not parse: empty document " << configFile;
        xmlFreeDoc(doc);
        return BAD_VALUE;
    }
    if (xmlXIncludeProcess(doc) < 0) {
        Log::Error() << __FUNCTION__ << ": libxml failed to resolve XIncludes on document "
                     << configFile;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *)mRootElementName.c_str())) {
        Log::Error() << __FUNCTION__ << ": No " << mRootElementName.c_str()
                     << " root element found in xml data " << (const char *)cur->name;
        xmlFreeDoc(doc);
        return BAD_VALUE;
    }

    string version = getXmlAttribute(cur, versionAttribute);
    if (version.empty()) {
        Log::Error() << __FUNCTION__ << ": No version found in node " << mRootElementName.c_str();
        return BAD_VALUE;
    }
    if (version != mVersion) {
        Log::Error() << __FUNCTION__ << ": Version does not match; expect" << mVersion.c_str()
                     << ", Got: " << version.c_str();
        return BAD_VALUE;
    }
    // Lets deserialize children
    ModuleTraits::Collection modules;
    deserializeCollection<ModuleTraits>(doc, cur, modules, &config);
    deserializeCollection<RogueParameterTraits>(doc, cur, config.mParameters, &config);
    deserializeCollection<AudioCriterionTypeTraits>(doc, cur, config.mCriterionTypes, &config);
    deserializeCollection<AudioCriterionTraits>(doc, cur, config.mCriteria, &config);

    xmlFreeDoc(doc);
    return android::OK;
}

}  // namespace intel_audio
