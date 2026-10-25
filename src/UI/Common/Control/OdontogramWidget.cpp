#include "UI/Common/Control/OdontogramWidget.h"

#include "Core/LabelTable.h"

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <vector>

namespace
{
constexpr int kColumns = 16;
constexpr qreal kMidGap = 8.0;
constexpr qreal kRowLabelHeight = 14.0;
constexpr qreal kGlyphHeight = 20.0;
constexpr qreal kNumberHeight = 13.0;
constexpr qreal kRowGap = 6.0;
constexpr qreal kFooterHeight = 13.0;

qreal GetRowHeight()
{
    return kRowLabelHeight + kGlyphHeight + kNumberHeight;
}

const std::vector<int>& GetRow(int row)
{
    return LabelTable::GetArchOrder(row == 0 ? EJaw::UPPER : EJaw::LOWER);
}
}

OdontogramWidget::OdontogramWidget(QWidget* parent)
    : QWidget(parent)
    , m_eJaw(EJaw::UNKNOWN)
    , m_iActiveLabel(kGingiva)
    , m_hashCells()
{
    setMouseTracking(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(sizeHint().height());
}

void OdontogramWidget::SetJaw(EJaw jaw)
{
    m_eJaw = jaw;
    update();
}

void OdontogramWidget::SetActiveLabel(int label)
{
    m_iActiveLabel = label;
    update();
}

void OdontogramWidget::SetCells(const QHash<int, ToothCell>& hashCells)
{
    m_hashCells = hashCells;
    update();
}

QSize OdontogramWidget::sizeHint() const
{
    return QSize(296, static_cast<int>(GetRowHeight() * 2 + kRowGap + kFooterHeight));
}

void OdontogramWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor colMuted(0x8B, 0x96, 0xA1);
    const QColor colOutline(0x59, 0x62, 0x6C);
    QFont fontLabel = font();
    fontLabel.setPixelSize(10);
    QFont fontNumber = font();
    fontNumber.setPixelSize(10);
    fontNumber.setFamily(QStringLiteral("Consolas"));

    for (int iRow = 0; iRow < 2; ++iRow)
    {
        const bool bEnabled = IsRowEnabled(iRow);
        painter.setOpacity(bEnabled ? 1.0 : 0.34);
        const qreal fTop = iRow * (GetRowHeight() + kRowGap);

        painter.setFont(fontLabel);
        painter.setPen(colMuted);
        QString strRowLabel = iRow == 0 ? QStringLiteral("상악") : QStringLiteral("하악");
        if (!bEnabled)
        {
            strRowLabel += QStringLiteral(" · 이 케이스에 없음");
        }
        painter.drawText(QRectF(0, fTop, width(), kRowLabelHeight), Qt::AlignLeft | Qt::AlignVCenter, strRowLabel);

        const std::vector<int>& vecRow = GetRow(iRow);
        for (int iColumn = 0; iColumn < kColumns; ++iColumn)
        {
            const int iLabel = vecRow[static_cast<size_t>(iColumn)];
            const QRectF rcCell = GetCellRect(iRow, iColumn);
            const QRectF rcGlyph(rcCell.center().x() - 7.5, rcCell.top() + 1.0, 15.0, kGlyphHeight - 3.0);
            const ToothCell cell = m_hashCells.value(iLabel);
            const QColor colLabel = LabelTable::GetColor(iLabel);
            const bool bMissing = cell.m_eState == EToothState::MISSING;

            QPainterPath pathGlyph;
            pathGlyph.addRoundedRect(rcGlyph, 4.0, 4.0);

            if (bEnabled && iLabel == m_iActiveLabel)
            {
                painter.setPen(QPen(Qt::white, 1.5));
                painter.setBrush(Qt::NoBrush);
                painter.drawRoundedRect(rcGlyph.adjusted(-3.0, -3.0, 3.0, 3.0), 6.0, 6.0);
            }

            if (bMissing)
            {
                painter.setPen(QPen(colOutline, 1.3, Qt::DashLine));
                painter.setBrush(Qt::NoBrush);
                painter.drawPath(pathGlyph);
                painter.setPen(QPen(QColor(0x7C, 0x87, 0x92), 1.3));
                painter.drawLine(rcGlyph.topLeft() + QPointF(3, 3), rcGlyph.bottomRight() - QPointF(3, 3));
                painter.drawLine(rcGlyph.topRight() + QPointF(-3, 3), rcGlyph.bottomLeft() + QPointF(3, -3));
            }
            else if (bEnabled && cell.m_iCount > 0)
            {
                painter.setPen(QPen(colLabel, 1.3));
                painter.setBrush(colLabel);
                painter.drawPath(pathGlyph);
            }
            else
            {
                painter.setPen(QPen(colOutline, 1.3));
                painter.setBrush(Qt::NoBrush);
                painter.drawPath(pathGlyph);
            }

            if (cell.m_eState != EToothState::NORMAL && !bMissing)
            {
                // 임플란트 · 보철 등은 칸 오른쪽 위 작은 점
                painter.setPen(QPen(QColor(0x22, 0x27, 0x2D), 1.5));
                painter.setBrush(QColor(0xB9, 0xAE, 0xFF));
                painter.drawEllipse(QPointF(rcGlyph.right() - 1.0, rcGlyph.top() + 1.0), 3.0, 3.0);
            }

            painter.setFont(fontNumber);
            painter.setPen(bEnabled && iLabel == m_iActiveLabel ? Qt::white : colMuted);
            painter.drawText(QRectF(rcCell.left() - 2.0, rcCell.top() + kGlyphHeight, rcCell.width() + 4.0, kNumberHeight),
                             Qt::AlignHCenter | Qt::AlignTop, QString::number(iLabel));
        }

        // 정중선
        painter.setPen(QPen(colOutline, 1.0, Qt::DashLine));
        const qreal fMid = GetCellRect(iRow, 7).right() + kMidGap * 0.5;
        painter.drawLine(QPointF(fMid, fTop + kRowLabelHeight), QPointF(fMid, fTop + GetRowHeight()));
    }

    painter.setOpacity(1.0);
    painter.setFont(fontLabel);
    painter.setPen(QColor(0x6C, 0x76, 0x82));
    const QRectF rcFooter(0, height() - kFooterHeight, width(), kFooterHeight);
    painter.drawText(rcFooter, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("환자 오른쪽"));
    painter.drawText(rcFooter, Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("왼쪽"));
}

void OdontogramWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }
    const int iLabel = HitTest(event->pos());
    if (iLabel > 0)
    {
        emit ToothClicked(iLabel);
    }
}

void OdontogramWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    const int iLabel = HitTest(event->pos());
    if (iLabel > 0 && event->button() == Qt::LeftButton)
    {
        emit ToothDoubleClicked(iLabel);
    }
}

void OdontogramWidget::contextMenuEvent(QContextMenuEvent* event)
{
    const int iLabel = HitTest(event->pos());
    if (iLabel > 0)
    {
        emit ToothContextMenuRequested(iLabel, event->globalPos());
    }
}

bool OdontogramWidget::IsRowEnabled(int row) const
{
    if (m_eJaw == EJaw::UNKNOWN)
    {
        return true;
    }
    return row == 0 ? m_eJaw == EJaw::UPPER : m_eJaw == EJaw::LOWER;
}

QRectF OdontogramWidget::GetCellRect(int row, int column) const
{
    const qreal fCellWidth = (width() - kMidGap) / kColumns;
    const qreal fLeft = column * fCellWidth + (column >= 8 ? kMidGap : 0.0);
    const qreal fTop = row * (GetRowHeight() + kRowGap) + kRowLabelHeight;
    return QRectF(fLeft, fTop, fCellWidth, kGlyphHeight + kNumberHeight);
}

int OdontogramWidget::HitTest(const QPoint& point) const
{
    for (int iRow = 0; iRow < 2; ++iRow)
    {
        if (!IsRowEnabled(iRow))
        {
            continue;
        }
        for (int iColumn = 0; iColumn < kColumns; ++iColumn)
        {
            if (GetCellRect(iRow, iColumn).contains(point))
            {
                return GetRow(iRow)[static_cast<size_t>(iColumn)];
            }
        }
    }
    return -1;
}
