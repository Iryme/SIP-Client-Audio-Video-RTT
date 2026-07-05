#pragma once
#include <QWidget>

// Horizontal audio level meter with a green→yellow→red color scale.
// Level is 0-100 (same range as AudioMediaManager input/output levels).
// The gradient is fixed across the full width and revealed up to the
// current level, so quiet input is green and loud input reaches red.
class AudioLevelMeter : public QWidget
{
    Q_OBJECT
public:
    explicit AudioLevelMeter(QWidget *parent = nullptr);

    int level() const { return m_level; }

public slots:
    void setLevel(int level);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_level{0};
    int m_peak{0};      // recent peak indicator (decays)
    qint64 m_peakTs{0}; // when the peak was last raised
};
