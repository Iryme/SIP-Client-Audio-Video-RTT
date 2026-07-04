#pragma once
#include <QList>
#include <QString>

#include "core/DiagnosticsTimelineEntry.h"

// Pure string-building + file-writing helpers for the Timeline "Export"
// buttons (task K). Kept free of QWidget/QFileDialog so serialization can be
// unit tested without a GUI event loop.
namespace DiagnosticsTimelineExporter {

QString toJsonString(const QList<DiagnosticsTimelineEntry> &entries);
QString toTxtString(const QList<DiagnosticsTimelineEntry> &entries);

bool exportJsonToFile(const QList<DiagnosticsTimelineEntry> &entries, const QString &path, QString *errorOut = nullptr);
bool exportTxtToFile(const QList<DiagnosticsTimelineEntry> &entries, const QString &path, QString *errorOut = nullptr);

} // namespace DiagnosticsTimelineExporter
