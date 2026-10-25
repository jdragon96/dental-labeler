#pragma once

#include "Core/Types.h"

#include <QHash>
#include <QWidget>

// 치식도: 상악 · 하악 16칸 두 줄 (DL-20 ⑭). 칸 색은 라벨 색, 채움은 정점이 있음, 점선 X 는 결손
class OdontogramWidget : public QWidget
{
    Q_OBJECT

public:
    struct ToothCell
    {
        int m_iCount = 0;
        EToothState m_eState = EToothState::NORMAL;
    };

public:
    explicit OdontogramWidget(QWidget* parent = nullptr);

public:
    void SetJaw(EJaw jaw);
    void SetActiveLabel(int label);
    void SetCells(const QHash<int, ToothCell>& hashCells);
    QSize sizeHint() const override;

signals:
    void ToothClicked(int label);
    void ToothDoubleClicked(int label);
    void ToothContextMenuRequested(int label, const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    bool IsRowEnabled(int row) const;
    QRectF GetCellRect(int row, int column) const;
    int HitTest(const QPoint& point) const;

private:
    EJaw m_eJaw;
    int m_iActiveLabel;
    QHash<int, ToothCell> m_hashCells;
};
