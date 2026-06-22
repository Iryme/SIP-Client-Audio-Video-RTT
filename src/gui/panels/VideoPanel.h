#pragma once
#include <QWidget>

class QLabel;

class VideoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit VideoPanel(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void repositionPreview();

    QLabel *m_remoteLabel{nullptr};
    QLabel *m_localPreview{nullptr};
    QLabel *m_signalIndicator{nullptr};
};
