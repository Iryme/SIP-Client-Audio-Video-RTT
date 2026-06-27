#pragma once
#include <QSize>
#include <QString>
#include <QStringList>

struct VideoSettings {
    QString     cameraId;
    QSize       resolution{1280, 720};
    int         fps{30};
    int         bitrateKbps{1024};
    QStringList codecOrder{{"H264", "VP8", "VP9", "AV1"}};
    bool        overlayEnabled{false};

    static VideoSettings load();
    void save() const;
};
