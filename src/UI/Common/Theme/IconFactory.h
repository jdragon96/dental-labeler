#pragma once

#include <QColor>
#include <QIcon>

enum class EIcon
{
    NAVIGATE,
    BRUSH,
    LASSO,
    FILL,
    ERASER,
    OPEN,
    SAVE,
    UNDO,
    REDO,
    EYE,
    EYE_OFF,
    LOCK,
    UNLOCK,
    FIT,
    TOOTH,
};

// 외부 아이콘 파일 없이 코드 안의 SVG 로 그린다 (docs/LicenseCompliance.md: 아이콘은 직접 그린다)
namespace IconFactory
{
QIcon Create(EIcon icon, const QColor& normal, const QColor& active = QColor());
}
