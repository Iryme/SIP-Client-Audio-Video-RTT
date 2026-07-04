#pragma once
#include <QMap>
#include <QWidget>

#include "core/DiagnosticsSnapshot.h"
#include "core/DiagnosticsTimelineEntry.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;
class QTabWidget;

// Diagnostics Center: a unified dashboard that reuses (does not replace)
// the existing Logs viewer and SIP Ladder pages. Reads only DiagnosticsSnapshot
// instances emitted by DiagnosticsCollector — never touches SipManager or
// PJSIP directly, keeping the "no UI querying PJSIP on repaint" and
// "collector/UI separation" requirements intact.
class DiagnosticsCenterPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DiagnosticsCenterPanel(QWidget *parent = nullptr);

signals:
    // Pure navigation requests — MainWindow owns the actual page switch,
    // same pattern as CallHistoryPanel::redialRequested.
    void openSipLadderRequested();
    void openLogsRequested();

private slots:
    void applySnapshot(const DiagnosticsSnapshot &snapshot);
    void onGenerateBundle();

    void onTimelineEntryAppended(const DiagnosticsTimelineEntry &entry);
    void onTimelineFilterChanged();
    void onTimelineContextMenu(const QPoint &pos);
    void onExportTimelineJson();
    void onExportTimelineTxt();

private:
    QWidget *buildOverviewTab();
    QWidget *buildSipTab();
    QWidget *buildMediaTab();
    QWidget *buildRtpTab();
    QWidget *buildAudioTab();
    QWidget *buildVideoTab();
    QWidget *buildNetworkTab();
    QWidget *buildSystemTab();
    QWidget *buildTimelineTab();

    // Adds a "label: value" row to a form and remembers the value QLabel
    // under 'key' so applySnapshot() can update it later.
    QLabel *addRow(QFormLayout *form, const QString &key, const QString &labelText);
    void setValue(const QString &key, const QString &text);

    bool timelineEntryMatchesFilters(const DiagnosticsTimelineEntry &e) const;
    void appendTimelineRow(const DiagnosticsTimelineEntry &e);
    void refreshTimelineTable();
    void refreshRecentActivity(const DiagnosticsTimelineEntry &e);

    QTabWidget  *m_tabs{nullptr};
    QPushButton *m_bundleBtn{nullptr};
    QMap<QString, QLabel *> m_values;

    // Timeline tab
    QLineEdit    *m_timelineSearch{nullptr};
    QComboBox    *m_timelineFilter{nullptr};
    QCheckBox    *m_timelineAutoScroll{nullptr};
    QTableWidget *m_timelineTable{nullptr};

    // Overview "Recent Activity" widget (task M) — last 5 timeline entries.
    QListWidget *m_recentActivity{nullptr};
};
