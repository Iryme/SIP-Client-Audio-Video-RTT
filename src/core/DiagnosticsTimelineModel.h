#pragma once
#include <QAbstractListModel>
#include <QList>

#include "core/DiagnosticsTimelineEntry.h"

// Read/append-only list model over the Diagnostics Timeline. Entries are
// stored oldest-first (append() adds at the end), matching the existing
// Logs viewer (DiagnosticsPanel) and DiagnosticsBundleExporter conventions.
// Capped at kMaxEntries — append() drops the oldest entry via a proper
// beginRemoveRows/endRemoveRows instead of ever rebuilding the model, so
// views bound to this model never see a full reset from normal operation.
class DiagnosticsTimelineModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TimestampRole,
        CategoryRole,
        SeverityRole,
        TitleRole,
        DetailsRole,
        CallIdRole,
        ProfileIdRole,
        RemoteUriRole,
        SipCodeRole,
        ColorHintRole,
        IconHintRole,
        EntryRole
    };

    static constexpr int kMaxEntries = 5000;

    explicit DiagnosticsTimelineModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Appends one entry incrementally. Removes the oldest entry first if the
    // cap would otherwise be exceeded.
    void append(const DiagnosticsTimelineEntry &entry);

    // Replaces all entries at once. Only used when loading a persisted
    // timeline at startup — there is nothing yet to update incrementally.
    void reset(const QList<DiagnosticsTimelineEntry> &entries);

    void clear();

    QList<DiagnosticsTimelineEntry> entries() const { return m_entries; }
    DiagnosticsTimelineEntry entryAt(int row) const;
    int count() const { return m_entries.size(); }

signals:
    // Emitted right after append() inserts a new entry — lets observers
    // (e.g. a "Recent Activity" widget) react without diffing model rows.
    void entryAppended(const DiagnosticsTimelineEntry &entry);

private:
    QList<DiagnosticsTimelineEntry> m_entries;
};
