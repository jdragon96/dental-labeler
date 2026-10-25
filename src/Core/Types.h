#pragma once

#include <cstdint>

// 정점 하나의 라벨. -1 미분류, 0 치은, 11–48 FDI 치아 번호
using LabelValue = std::int16_t;

constexpr LabelValue kUnlabeled = -1;
constexpr LabelValue kGingiva = 0;

enum class ETool
{
    NAVIGATE,
    BRUSH,
    LASSO,
    FILL,
    ERASER,
};

enum class EDisplayMode
{
    SHADED,
    LABEL,
};

enum class EPaintReach
{
    VISIBLE_ONLY,
    THROUGH,
};

enum class EOverwriteRule
{
    ALL,
    UNLABELED_AND_GINGIVA,
    UNLABELED_ONLY,
};

enum class EToothState
{
    NORMAL,
    MISSING,
    IMPLANT,
    CROWN,
    PARTIAL_ERUPTION,
    SUPERNUMERARY,
};

enum class EJaw
{
    UNKNOWN,
    UPPER,
    LOWER,
};

enum class EViewPreset
{
    OCCLUSAL,
    FRONT,
    LEFT,
    RIGHT,
    LINGUAL,
};

struct PaintSettings
{
    float m_fRadius = 2.0f;
    EPaintReach m_eReach = EPaintReach::VISIBLE_ONLY;
    EOverwriteRule m_eOverwrite = EOverwriteRule::UNLABELED_AND_GINGIVA;
    bool m_bCurvatureStop = true;
    float m_fSensitivity = 0.35f;
};
