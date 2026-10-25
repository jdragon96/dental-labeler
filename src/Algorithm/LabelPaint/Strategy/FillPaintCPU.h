#pragma once

#include "Interface/IPaintStrategy.h"

// 클릭한 정점과 같은 라벨로 이어진 영역 전체를 고른다. 면 인접 정보가 필요하다
class FillPaintCPU : public IPaintStrategy
{
public:
    bool Execute(const PaintInput& in, PaintOutput& out) override;
};
