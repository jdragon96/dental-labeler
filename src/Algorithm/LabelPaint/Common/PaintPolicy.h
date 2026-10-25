#pragma once

#include "Interface/IPaintStrategy.h"

namespace PaintPolicy
{
// 덮어쓰기 규칙 · 잠금 · 숨김을 보고 이 정점을 바꿔도 되는지
bool CanPaint(const PaintInput& in, LabelValue current);
// 정점 법선이 카메라 쪽을 보는지. 법선이 없으면 true
bool IsFacingCamera(const PaintInput& in, int vertex);
}
