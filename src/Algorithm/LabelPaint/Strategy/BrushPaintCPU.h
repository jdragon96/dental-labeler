#pragma once

#include "Interface/IPaintStrategy.h"

#include <cstdint>
#include <vector>

// 커서 아래 점에서 표면을 따라(측지 거리) 반경 안의 정점을 고른다.
// 곡률 멈춤을 켜면 오목한 고랑(치아와 치은 경계)에서 더 번지지 않는다.
// 면이 없는 포인트 클라우드는 3D 구 반경으로 고른다.
class BrushPaintCPU : public IPaintStrategy
{
public:
    BrushPaintCPU();
    ~BrushPaintCPU() override;

public:
    bool Execute(const PaintInput& in, PaintOutput& out) override;

private:
    bool ExecuteGeodesic(const PaintInput& in, PaintOutput& out);
    bool ExecuteSphere(const PaintInput& in, PaintOutput& out) const;

private:
    std::vector<float> m_vecDistance;
    std::vector<std::uint32_t> m_vecTouched;
};
