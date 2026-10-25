#pragma once

#include <vector>

struct Mesh;
class MeshTopology;

namespace Curvature
{
// 정점마다 이웃 방향의 법곡률 평균 (1/mm). 볼록(교두)은 +, 오목(치아와 치은 사이 고랑)은 -
std::vector<float> ComputeVertexCurvature(const Mesh& mesh, const MeshTopology& topology);

// 도구 옵션의 민감도(0–1)를 '이보다 더 오목하면 멈춘다' 문턱값으로 바꾼다. 민감할수록 덜 오목해도 멈춘다
float GetStopThreshold(float sensitivity);
}
