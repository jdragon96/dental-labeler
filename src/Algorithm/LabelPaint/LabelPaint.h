#pragma once

#include "Interface/IPaintStrategy.h"

#include <memory>
#include <unordered_map>

enum class EPaintType
{
    BRUSH = 0,
    FILL = 1,
    LASSO = 2,
};

// 라벨 칠하기 (docs/Patterns.md 1.2 전략 패턴)
class LabelPaint
{
public:
    LabelPaint();
    ~LabelPaint();

public:
    void SetStrategy(EPaintType type);
    bool Execute(const PaintInput& in, PaintOutput& out);

private:
    std::unordered_map<EPaintType, std::unique_ptr<IPaintStrategy>> m_hashStrategies;
    IPaintStrategy* m_pCurrent;
};
