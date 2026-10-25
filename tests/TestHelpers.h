#pragma once

#include "Document/Mesh.h"

#include <QString>

namespace TestHelpers
{
// XY 평면 위 (count × count) 격자. 간격 spacing, 높이 z, 법선 +Z
Mesh MakeGrid(int count, float spacing, float z = 0.0f);
// x = 0 을 따라 오목하게 접힌 V 자 고랑. 양쪽 면이 위로 slope 만큼 올라간다
Mesh MakeValley(int count, float spacing, float slope);
// 두 메시를 하나로 합친다
Mesh Merge(const Mesh& first, const Mesh& second);
QString GetSamplePath();
}
