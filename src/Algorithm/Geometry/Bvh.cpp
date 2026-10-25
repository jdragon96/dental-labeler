#include "Algorithm/Geometry/Bvh.h"

#include "Document/Mesh.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace
{
constexpr std::uint32_t kLeafSize = 4;
constexpr int kMaxStack = 256;
constexpr float kRayEpsilon = 1e-6f;

struct BuildItem
{
    std::uint32_t uNode;
    std::uint32_t uBegin;
    std::uint32_t uEnd;
};

bool IntersectBox(const float* kpMin, const float* kpMax, const float* kpOrigin, const float* kpInvDir,
                  float maxT, float& tNear)
{
    float fT0 = 0.0f;
    float fT1 = maxT;
    for (int iAxis = 0; iAxis < 3; ++iAxis)
    {
        float fA = (kpMin[iAxis] - kpOrigin[iAxis]) * kpInvDir[iAxis];
        float fB = (kpMax[iAxis] - kpOrigin[iAxis]) * kpInvDir[iAxis];
        if (fA > fB)
        {
            std::swap(fA, fB);
        }
        // NaN 은 비교가 거짓이라 그 축은 통과로 본다
        fT0 = fA > fT0 ? fA : fT0;
        fT1 = fB < fT1 ? fB : fT1;
        if (fT0 > fT1)
        {
            return false;
        }
    }
    tNear = fT0;
    return true;
}

// Möller–Trumbore. 앞뒷면을 모두 맞힌다
bool IntersectTriangle(const float* kpTriangle, const float* kpOrigin, const float* kpDir,
                       float& t, float& u, float& v)
{
    const float arrE1[3] = { kpTriangle[3] - kpTriangle[0], kpTriangle[4] - kpTriangle[1], kpTriangle[5] - kpTriangle[2] };
    const float arrE2[3] = { kpTriangle[6] - kpTriangle[0], kpTriangle[7] - kpTriangle[1], kpTriangle[8] - kpTriangle[2] };
    const float arrP[3] = {
        kpDir[1] * arrE2[2] - kpDir[2] * arrE2[1],
        kpDir[2] * arrE2[0] - kpDir[0] * arrE2[2],
        kpDir[0] * arrE2[1] - kpDir[1] * arrE2[0],
    };
    const float fDet = arrE1[0] * arrP[0] + arrE1[1] * arrP[1] + arrE1[2] * arrP[2];
    if (std::fabs(fDet) < 1e-14f)
    {
        return false;
    }
    const float fInvDet = 1.0f / fDet;
    const float arrS[3] = { kpOrigin[0] - kpTriangle[0], kpOrigin[1] - kpTriangle[1], kpOrigin[2] - kpTriangle[2] };
    u = (arrS[0] * arrP[0] + arrS[1] * arrP[1] + arrS[2] * arrP[2]) * fInvDet;
    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }
    const float arrQ[3] = {
        arrS[1] * arrE1[2] - arrS[2] * arrE1[1],
        arrS[2] * arrE1[0] - arrS[0] * arrE1[2],
        arrS[0] * arrE1[1] - arrS[1] * arrE1[0],
    };
    v = (kpDir[0] * arrQ[0] + kpDir[1] * arrQ[1] + kpDir[2] * arrQ[2]) * fInvDet;
    if (v < 0.0f || u + v > 1.0f)
    {
        return false;
    }
    t = (arrE2[0] * arrQ[0] + arrE2[1] * arrQ[1] + arrE2[2] * arrQ[2]) * fInvDet;
    return t > kRayEpsilon;
}
}

Bvh::Bvh()
    : m_vecNodes()
    , m_vecTriangles()
    , m_vecFaceIds()
{
}

void Bvh::Build(const Mesh& mesh)
{
    Clear();
    const std::uint32_t uFaceCount = static_cast<std::uint32_t>(mesh.GetFaceCount());
    if (uFaceCount == 0)
    {
        return;
    }

    std::vector<float> vecBoxes(static_cast<size_t>(uFaceCount) * 6);
    std::vector<float> vecCentroids(static_cast<size_t>(uFaceCount) * 3);
    for (std::uint32_t uFace = 0; uFace < uFaceCount; ++uFace)
    {
        const std::uint32_t* kpFace = mesh.GetFace(static_cast<int>(uFace));
        for (int iAxis = 0; iAxis < 3; ++iAxis)
        {
            const float fA = mesh.m_vecPositions[kpFace[0] * 3 + static_cast<std::uint32_t>(iAxis)];
            const float fB = mesh.m_vecPositions[kpFace[1] * 3 + static_cast<std::uint32_t>(iAxis)];
            const float fC = mesh.m_vecPositions[kpFace[2] * 3 + static_cast<std::uint32_t>(iAxis)];
            const float fMin = std::min({ fA, fB, fC });
            const float fMax = std::max({ fA, fB, fC });
            vecBoxes[uFace * 6 + static_cast<std::uint32_t>(iAxis)] = fMin;
            vecBoxes[uFace * 6 + 3 + static_cast<std::uint32_t>(iAxis)] = fMax;
            vecCentroids[uFace * 3 + static_cast<std::uint32_t>(iAxis)] = 0.5f * (fMin + fMax);
        }
    }

    std::vector<std::uint32_t> vecOrder(uFaceCount);
    std::iota(vecOrder.begin(), vecOrder.end(), 0u);

    m_vecNodes.reserve(static_cast<size_t>(uFaceCount) / 2 + 1);
    m_vecNodes.push_back(Node());
    std::vector<BuildItem> vecStack;
    vecStack.push_back({ 0u, 0u, uFaceCount });
    const float fInf = std::numeric_limits<float>::max();
    while (!vecStack.empty())
    {
        const BuildItem item = vecStack.back();
        vecStack.pop_back();

        Node node;
        float arrCentroidMin[3] = { fInf, fInf, fInf };
        float arrCentroidMax[3] = { -fInf, -fInf, -fInf };
        for (int iAxis = 0; iAxis < 3; ++iAxis)
        {
            node.m_arrMin[iAxis] = fInf;
            node.m_arrMax[iAxis] = -fInf;
        }
        for (std::uint32_t i = item.uBegin; i < item.uEnd; ++i)
        {
            const std::uint32_t uFace = vecOrder[i];
            for (int iAxis = 0; iAxis < 3; ++iAxis)
            {
                const std::uint32_t uAxis = static_cast<std::uint32_t>(iAxis);
                node.m_arrMin[iAxis] = std::min(node.m_arrMin[iAxis], vecBoxes[uFace * 6 + uAxis]);
                node.m_arrMax[iAxis] = std::max(node.m_arrMax[iAxis], vecBoxes[uFace * 6 + 3 + uAxis]);
                arrCentroidMin[iAxis] = std::min(arrCentroidMin[iAxis], vecCentroids[uFace * 3 + uAxis]);
                arrCentroidMax[iAxis] = std::max(arrCentroidMax[iAxis], vecCentroids[uFace * 3 + uAxis]);
            }
        }

        const std::uint32_t uCount = item.uEnd - item.uBegin;
        int iAxis = 0;
        for (int i = 1; i < 3; ++i)
        {
            if (arrCentroidMax[i] - arrCentroidMin[i] > arrCentroidMax[iAxis] - arrCentroidMin[iAxis])
            {
                iAxis = i;
            }
        }
        const float fExtent = arrCentroidMax[iAxis] - arrCentroidMin[iAxis];
        if (uCount <= kLeafSize || fExtent <= 1e-9f)
        {
            node.m_uLeftOrFirst = item.uBegin;
            node.m_uCount = uCount;
            m_vecNodes[item.uNode] = node;
            continue;
        }

        // 중심점 범위의 가운데로 나누고, 한쪽으로 몰리면 개수 중앙값으로 나눈다
        const std::uint32_t uAxis = static_cast<std::uint32_t>(iAxis);
        const float fSplit = 0.5f * (arrCentroidMin[iAxis] + arrCentroidMax[iAxis]);
        auto itMid = std::partition(vecOrder.begin() + item.uBegin, vecOrder.begin() + item.uEnd,
                                    [&](std::uint32_t uFace) { return vecCentroids[uFace * 3 + uAxis] < fSplit; });
        std::uint32_t uMid = static_cast<std::uint32_t>(itMid - vecOrder.begin());
        if (uMid == item.uBegin || uMid == item.uEnd)
        {
            uMid = item.uBegin + uCount / 2;
            std::nth_element(vecOrder.begin() + item.uBegin, vecOrder.begin() + uMid, vecOrder.begin() + item.uEnd,
                             [&](std::uint32_t uA, std::uint32_t uB) {
                                 return vecCentroids[uA * 3 + uAxis] < vecCentroids[uB * 3 + uAxis];
                             });
        }

        const std::uint32_t uLeft = static_cast<std::uint32_t>(m_vecNodes.size());
        m_vecNodes.push_back(Node());
        m_vecNodes.push_back(Node());
        node.m_uLeftOrFirst = uLeft;
        node.m_uCount = 0;
        m_vecNodes[item.uNode] = node;
        vecStack.push_back({ uLeft, item.uBegin, uMid });
        vecStack.push_back({ uLeft + 1, uMid, item.uEnd });
    }

    m_vecTriangles.resize(static_cast<size_t>(uFaceCount) * 9);
    for (std::uint32_t i = 0; i < uFaceCount; ++i)
    {
        const std::uint32_t* kpFace = mesh.GetFace(static_cast<int>(vecOrder[i]));
        for (int iCorner = 0; iCorner < 3; ++iCorner)
        {
            for (int iAxis = 0; iAxis < 3; ++iAxis)
            {
                m_vecTriangles[static_cast<size_t>(i) * 9 + static_cast<size_t>(iCorner * 3 + iAxis)]
                    = mesh.m_vecPositions[static_cast<size_t>(kpFace[iCorner]) * 3 + static_cast<size_t>(iAxis)];
            }
        }
    }
    m_vecFaceIds = std::move(vecOrder);
}

void Bvh::Clear()
{
    m_vecNodes.clear();
    m_vecTriangles.clear();
    m_vecFaceIds.clear();
}

bool Bvh::Raycast(const QVector3D& origin, const QVector3D& dir, RayHit& hit, float maxT) const
{
    return Traverse<false>(origin, dir, maxT, hit);
}

bool Bvh::IsOccluded(const QVector3D& origin, const QVector3D& dir, float maxT) const
{
    RayHit hit;
    return Traverse<true>(origin, dir, maxT, hit);
}

bool Bvh::IsEmpty() const
{
    return m_vecNodes.empty();
}

int Bvh::GetNodeCount() const
{
    return static_cast<int>(m_vecNodes.size());
}

template <bool AnyHit>
bool Bvh::Traverse(const QVector3D& origin, const QVector3D& dir, float maxT, RayHit& hit) const
{
    if (m_vecNodes.empty())
    {
        return false;
    }

    const float arrOrigin[3] = { origin.x(), origin.y(), origin.z() };
    const float arrDir[3] = { dir.x(), dir.y(), dir.z() };
    const float arrInvDir[3] = { 1.0f / arrDir[0], 1.0f / arrDir[1], 1.0f / arrDir[2] };

    float fBest = maxT;
    bool bHit = false;
    std::uint32_t arrStack[kMaxStack];
    int iTop = 0;
    arrStack[iTop++] = 0;
    while (iTop > 0)
    {
        const Node& node = m_vecNodes[arrStack[--iTop]];
        float fNear = 0.0f;
        if (!IntersectBox(node.m_arrMin, node.m_arrMax, arrOrigin, arrInvDir, fBest, fNear))
        {
            continue;
        }

        if (node.m_uCount > 0)
        {
            for (std::uint32_t i = 0; i < node.m_uCount; ++i)
            {
                const std::uint32_t uTriangle = node.m_uLeftOrFirst + i;
                float fT = 0.0f;
                float fU = 0.0f;
                float fV = 0.0f;
                if (IntersectTriangle(&m_vecTriangles[static_cast<size_t>(uTriangle) * 9], arrOrigin, arrDir, fT, fU, fV)
                    && fT < fBest)
                {
                    if (AnyHit)
                    {
                        return true;
                    }
                    fBest = fT;
                    hit.m_iFace = static_cast<int>(m_vecFaceIds[uTriangle]);
                    hit.m_fT = fT;
                    hit.m_fU = fU;
                    hit.m_fV = fV;
                    bHit = true;
                }
            }
            continue;
        }

        if (iTop + 2 > kMaxStack)
        {
            continue;
        }
        const std::uint32_t uLeft = node.m_uLeftOrFirst;
        const Node& left = m_vecNodes[uLeft];
        const Node& right = m_vecNodes[uLeft + 1];
        float fLeft = 0.0f;
        float fRight = 0.0f;
        const bool bLeft = IntersectBox(left.m_arrMin, left.m_arrMax, arrOrigin, arrInvDir, fBest, fLeft);
        const bool bRight = IntersectBox(right.m_arrMin, right.m_arrMax, arrOrigin, arrInvDir, fBest, fRight);
        // 가까운 쪽을 나중에 넣어 먼저 꺼낸다
        if (bLeft && bRight)
        {
            if (fLeft < fRight)
            {
                arrStack[iTop++] = uLeft + 1;
                arrStack[iTop++] = uLeft;
            }
            else
            {
                arrStack[iTop++] = uLeft;
                arrStack[iTop++] = uLeft + 1;
            }
        }
        else if (bLeft)
        {
            arrStack[iTop++] = uLeft;
        }
        else if (bRight)
        {
            arrStack[iTop++] = uLeft + 1;
        }
    }
    return bHit;
}
