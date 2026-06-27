#pragma once
#include <QFrame>

class QLabel;

class DashboardShortcutCard : public QFrame
{
    Q_OBJECT
public:
    explicit DashboardShortcutCard(const QString &icon,
                                   const QString &title,
                                   const QString &description,
                                   const QString &page,
                                   QWidget *parent = nullptr);

    void setStatus(const QString &text, const QString &color = "#6a7a9a");

signals:
    void navigateTo(const QString &page);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    static constexpr const char *kStyleNormal = R"(
        QFrame#ShortcutCard {
            background-color: #18202e;
            border: 1px solid #2a3448;
            border-radius: 10px;
        }
    )";
    static constexpr const char *kStyleHover = R"(
        QFrame#ShortcutCard {
            background-color: #1e2a3e;
            border: 1px solid #4a7cc0;
            border-radius: 10px;
        }
    )";

    QString  m_page;
    QLabel  *m_statusLabel{nullptr};
};
