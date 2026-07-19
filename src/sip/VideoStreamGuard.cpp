#include "VideoStreamGuard.h"

namespace VideoStreamGuard {

Decision evaluate(const Snapshot &snapshot)
{
    if (!snapshot.callObjectExists)
        return {};
    if (snapshot.callId < 0)
        return {false, -1, "invalid call id"};
    if (snapshot.teardownInProgress)
        return {false, -1, "call teardown in progress"};
    if (snapshot.callState == CallState::Null
            || snapshot.callState == CallState::Disconnected) {
        return {false, -1, "call is disconnected"};
    }
    if (snapshot.currentVideoStreamIndex < 0)
        return {false, -1, "video stream was not created"};

    for (const Media &media : snapshot.media) {
        if (media.type != MediaType::Video)
            continue;
        if (media.index != snapshot.currentVideoStreamIndex)
            continue;
        if (media.index < 0)
            return {false, -1, "invalid video media index"};
        if (media.status != MediaStatus::Active)
            return {false, -1, "video media is not active"};
        if (!media.canTransmit)
            return {false, -1, "video media has no transmit direction"};
        return {true, media.index, "active video stream"};
    }

    return {false, -1, "active video stream index has no matching video media"};
}

} // namespace VideoStreamGuard
