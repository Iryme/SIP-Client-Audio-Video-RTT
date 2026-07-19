#pragma once

#include <vector>

// PJSIP's video stream operations assert when handed a media index whose
// stream has not been created (or has already been torn down).  Keep the
// decision to call vidSetStream in a small PJSIP-independent policy so every
// missing/inactive/teardown state can be unit tested without provoking that
// assertion.
namespace VideoStreamGuard {

enum class CallState {
    Null,
    Connecting,
    Confirmed,
    Disconnected
};

enum class MediaType {
    Audio,
    Video,
    Other
};

enum class MediaStatus {
    None,
    Active,
    LocalHold,
    RemoteHold,
    Error
};

struct Media {
    int index{-1};
    MediaType type{MediaType::Other};
    MediaStatus status{MediaStatus::None};
    bool canTransmit{false};
};

struct Snapshot {
    bool callObjectExists{false};
    int callId{-1};
    CallState callState{CallState::Null};
    bool teardownInProgress{false};
    int currentVideoStreamIndex{-1};
    bool streamExists{false};
    std::vector<Media> media;
};

struct Decision {
    bool mayOperate{false};
    int mediaIndex{-1};
    const char *reason{"call object missing"};
};

Decision evaluate(const Snapshot &snapshot);

} // namespace VideoStreamGuard
