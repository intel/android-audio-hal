#define LOG_TAG "RouteManager/StreamRoute"
// #define LOG_NDEBUG 0
#include <utilities/Log.hpp>
#include "AudioBackendRoute.hpp"
using namespace std;
using audio_comms::utilities::Log;
namespace intel_audio
{

void AudioBackendRoute::resetAvailability()
{
    /**
     * Reset route as available
     * Use Base class fucntions.
     * @see AudioRoute::setPreUsed
     * @see AudioRoute::isUsed
     * @see AudioRoute::setUsed
     */
    setPreUsed(isUsed());
    setUsed(false);
}

bool AudioBackendRoute::needReflow()
{
    if (stillUsed()) {
        return true;
    }
    return false;
}

bool AudioBackendRoute::needRepath() const
{
    return true;
}

android::status_t  AudioBackendRoute::dump(const int fd, int spaces) const
{
    return android::OK;
}

}
