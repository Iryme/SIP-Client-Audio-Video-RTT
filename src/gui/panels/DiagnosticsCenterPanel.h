#pragma once
#include <QMap>
#include <QWidget>

#include "core/DiagnosticsSnapshot.h"

class QFormLayout;
class QLabel;
class QPushButton;
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

private:
    QWidget *buildOverviewTab();
    QWidget *buildSipTab();
    QWidget *buildMediaTab();
    QWidget *buildRtpTab();
    QWidget *buildAudioTab();
    QWidget *buildVideoTab();
    QWidget *buildNetworkTab();
    QWidget *buildSystemTab();

    // Adds a "label: value" row to a form and remembers the value QLabel
    // under 'key' so applySnapshot() can update it later.
    QLabel *addRow(QFormLayout *form, const QString &key, const QString &labelText);
    void setValue(const QString &key, const QString &text);

    QTabWidget  *m_tabs{nullptr};
    QPushButton *m_bundleBtn{nullptr};
    QMap<QString, QLabel *> m_values;
};
