#pragma once

#include "Interface/IPaintStrategy.h"

// 화면에 그린 닫힌 다각형 안에 투영되는 정점을 고른다.
// '보이는 면만'이면 카메라를 향하고 가려지지 않은(BVH 광선 검사) 정점만 고른다
class LassoPaintCPU : public IPaintStrategy
{
public:
    bool Execute(const PaintInput& in, PaintOutput& out) override;

public:
    // 짝수-홀수 규칙
    static bool IsInsidePolygon(const std::vector<QPointF>& vecPolygon, const QPointF& point);
};
