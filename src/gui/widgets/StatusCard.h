#pragma once
#include <QFrame>

class QLabel;
class QResizeEvent;

// A compact status tile widget for the call info area.
// Background/border/title color are controlled by QSS (#StatusCard, #StatusCardTitle).
// Dot and value colors are set inline by setStatus() for semantic meaning
// that remains consistent across themes.
class StatusCard : public QFrame
{
    Q_OBJECT
public:
    explicit StatusCard(const QString &title, QWidget *parent = nullptr);

    // Update the displayed value ("—" for unknown/absent data).
    void setValue(const QString &value);

    // Set semantic status. "ok" = green, "warn" = amber, "err" = red, "" = idle/gray.
    void setStatus(const QString &status);

    void setTooltipText(const QString &tip);

private:
    void refreshColors();
    void updateElidedValue();

protected:
    void resizeEvent(QResizeEvent *event) override;

    QLabel *m_dotLabel{nullptr};
    QLabel *m_valueLabel{nullptr};
    QString m_valueText;
    QString m_status;
};
