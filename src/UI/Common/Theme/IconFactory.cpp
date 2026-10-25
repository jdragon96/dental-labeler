#include "UI/Common/Theme/IconFactory.h"

#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace
{
// 20×20 격자 위 선 아이콘. currentColor 는 그릴 때 색으로 바꾼다
const char* GetShapes(EIcon icon)
{
    switch (icon)
    {
    case EIcon::NAVIGATE:
        return R"(<path d="M10 2v16M2 10h16M10 2 7.6 4.4M10 2l2.4 2.4M10 18l-2.4-2.4M10 18l2.4-2.4M2 10l2.4-2.4M2 10l2.4 2.4M18 10l-2.4-2.4M18 10l-2.4 2.4"/>)";
    case EIcon::BRUSH:
        return R"(<circle cx="10" cy="10" r="7"/><circle cx="10" cy="10" r="2.6" fill="currentColor" stroke="none"/>)";
    case EIcon::LASSO:
        return R"(<path d="M4.5 12.5C3 11.4 2.5 10.2 2.5 9c0-3.3 3.4-5.5 7.5-5.5s7.5 2.2 7.5 5.5-3.4 5.5-7.5 5.5c-1 0-2-.1-2.9-.4"/><path d="M5.3 12.2c-1 .9-1 2.3 0 3.1.8.7 1 1.6.4 2.6"/>)";
    case EIcon::FILL:
        return R"(<path d="M3.5 9.5 9 4l6.5 6.5L10 16z"/><path d="M3.5 9.5h12"/><path d="M17.5 12.5c-.8 1.3-1.3 2.1-1.3 2.8a1.3 1.3 0 0 0 2.6 0c0-.7-.5-1.5-1.3-2.8z" fill="currentColor" stroke="none"/>)";
    case EIcon::ERASER:
        return R"(<path d="m11.5 3 5.5 5.5-8 8H5l-2.5-2.5z"/><path d="m7.5 7 5.5 5.5M9 16.5h8.5"/>)";
    case EIcon::OPEN:
        return R"(<path d="M2.5 5.5h5l1.6 2h8.4V16h-15z"/>)";
    case EIcon::SAVE:
        return R"(<path d="M3 3h11l3 3v11H3z"/><path d="M6.5 3v4.5h6V3M6 17v-5h8v5"/>)";
    case EIcon::UNDO:
        return R"(<path d="M7 4.5 3 8.5l4 4"/><path d="M3 8.5h9a4.5 4.5 0 0 1 0 9h-2"/>)";
    case EIcon::REDO:
        return R"(<path d="m13 4.5 4 4-4 4"/><path d="M17 8.5H8a4.5 4.5 0 0 0 0 9h2"/>)";
    case EIcon::EYE:
        return R"(<path d="M1.5 10S5 4.5 10 4.5s8.5 5.5 8.5 5.5-3.5 5.5-8.5 5.5S1.5 10 1.5 10z"/><circle cx="10" cy="10" r="2.4"/>)";
    case EIcon::EYE_OFF:
        return R"(<path d="M1.5 10S5 4.5 10 4.5s8.5 5.5 8.5 5.5-3.5 5.5-8.5 5.5S1.5 10 1.5 10z"/><path d="M3 3l14 14"/>)";
    case EIcon::LOCK:
        return R"(<rect x="4" y="9" width="12" height="9" rx="1.5"/><path d="M7 9V6.5a3 3 0 0 1 6 0V9"/>)";
    case EIcon::UNLOCK:
        return R"(<rect x="4" y="9" width="12" height="9" rx="1.5"/><path d="M7 9V6.5a3 3 0 0 1 5.8-1.1"/>)";
    case EIcon::FIT:
        return R"(<path d="M3 7.5V3h4.5M17 7.5V3h-4.5M3 12.5V17h4.5M17 12.5V17h-4.5"/>)";
    case EIcon::TOOTH:
        return R"(<path d="M6 2.8c1.6-.6 2.9-.1 4 .7 1.1-.8 2.4-1.3 4-.7 2.3.9 3 3.6 2.3 6.4-.5 2-1 4.3-1.4 6.3-.3 1.5-1.8 1.7-2.3.3L11.4 13c-.4-1.1-2.4-1.1-2.8 0l-1.2 2.8c-.5 1.4-2 1.2-2.3-.3-.4-2-.9-4.3-1.4-6.3C3 6.4 3.7 3.7 6 2.8z"/>)";
    }
    return "";
}

QPixmap Render(EIcon icon, const QColor& color, int size)
{
    QByteArray svg("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 20 20\" fill=\"none\" stroke=\"currentColor\" "
                   "stroke-width=\"1.6\" stroke-linecap=\"round\" stroke-linejoin=\"round\">");
    svg.append(GetShapes(icon));
    svg.append("</svg>");
    svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QSvgRenderer renderer(svg);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    return pixmap;
}
}

namespace IconFactory
{
QIcon Create(EIcon icon, const QColor& normal, const QColor& active)
{
    QIcon result;
    for (int iSize : { 16, 20, 32, 40 })
    {
        result.addPixmap(Render(icon, normal, iSize), QIcon::Normal, QIcon::Off);
        result.addPixmap(Render(icon, normal.darker(160), iSize), QIcon::Disabled, QIcon::Off);
        if (active.isValid())
        {
            result.addPixmap(Render(icon, active, iSize), QIcon::Normal, QIcon::On);
            result.addPixmap(Render(icon, active, iSize), QIcon::Active, QIcon::On);
        }
    }
    return result;
}
}
