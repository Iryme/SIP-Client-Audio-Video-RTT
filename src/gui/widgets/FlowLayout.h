#pragma once
#include <QLayout>
#include <QRect>

// Responsive flow layout: items wrap to the next row when they don't fit.
// Cards expand horizontally and wrap vertically — no horizontal scroll.
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget *parent, int margin = 6, int hSpacing = 6, int vSpacing = 6);
    ~FlowLayout() override;

    void         addItem(QLayoutItem *item) override;
    int          count()             const override { return m_items.size(); }
    QLayoutItem *itemAt(int index)   const override;
    QLayoutItem *takeAt(int index)         override;

    Qt::Orientations expandingDirections() const override { return {}; }
    bool             hasHeightForWidth()   const override { return true; }
    int              heightForWidth(int w) const override;
    QSize            minimumSize()         const override;
    void             setGeometry(const QRect &rect)  override;
    QSize            sizeHint()            const override;

private:
    int doLayout(const QRect &rect, bool testOnly) const;

    QList<QLayoutItem *> m_items;
    int m_hSpace;
    int m_vSpace;
};
