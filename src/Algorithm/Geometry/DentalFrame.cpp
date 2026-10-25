#include "Algorithm/Geometry/DentalFrame.h"

#include "Document/Mesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
using Matrix3 = std::array<std::array<double, 3>, 3>;

// 대칭 3×3 행렬의 고유값 분해 (순환 야코비). 고유벡터는 열에 담긴다
void SolveEigen(Matrix3 a, Matrix3& vectors, std::array<double, 3>& values)
{
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            vectors[i][j] = i == j ? 1.0 : 0.0;
        }
    }

    for (int iSweep = 0; iSweep < 50; ++iSweep)
    {
        const double dOff = std::fabs(a[0][1]) + std::fabs(a[0][2]) + std::fabs(a[1][2]);
        if (dOff < 1e-12)
        {
            break;
        }
        for (int p = 0; p < 2; ++p)
        {
            for (int q = p + 1; q < 3; ++q)
            {
                if (std::fabs(a[p][q]) < 1e-15)
                {
                    continue;
                }
                const double dTheta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double dT = (dTheta >= 0.0 ? 1.0 : -1.0) / (std::fabs(dTheta) + std::sqrt(dTheta * dTheta + 1.0));
                const double dC = 1.0 / std::sqrt(dT * dT + 1.0);
                const double dS = dT * dC;
                for (int k = 0; k < 3; ++k)
                {
                    const double dKp = a[k][p];
                    const double dKq = a[k][q];
                    a[k][p] = dC * dKp - dS * dKq;
                    a[k][q] = dS * dKp + dC * dKq;
                }
                for (int k = 0; k < 3; ++k)
                {
                    const double dPk = a[p][k];
                    const double dQk = a[q][k];
                    a[p][k] = dC * dPk - dS * dQk;
                    a[q][k] = dS * dPk + dC * dQk;
                }
                for (int k = 0; k < 3; ++k)
                {
                    const double dKp = vectors[k][p];
                    const double dKq = vectors[k][q];
                    vectors[k][p] = dC * dKp - dS * dKq;
                    vectors[k][q] = dS * dKp + dC * dKq;
                }
            }
        }
    }
    for (int i = 0; i < 3; ++i)
    {
        values[static_cast<size_t>(i)] = a[i][i];
    }
}

// 축 axis 의 양 끝 12% 에 있는 점들이 다른 축(other) 방향으로 얼마나 퍼져 있는지
void MeasureEndSpread(const std::vector<QVector3D>& vecPoints, const QVector3D& axis, const QVector3D& other,
                      double& spreadLow, double& spreadHigh)
{
    double dMin = 1e300;
    double dMax = -1e300;
    for (const QVector3D& v3Point : vecPoints)
    {
        const double dValue = QVector3D::dotProduct(v3Point, axis);
        dMin = std::min(dMin, dValue);
        dMax = std::max(dMax, dValue);
    }
    const double dBand = 0.12 * (dMax - dMin);

    double arrSum[2] = { 0.0, 0.0 };
    double arrSquared[2] = { 0.0, 0.0 };
    int arrCount[2] = { 0, 0 };
    for (const QVector3D& v3Point : vecPoints)
    {
        const double dValue = QVector3D::dotProduct(v3Point, axis);
        int iSide = -1;
        if (dValue <= dMin + dBand)
        {
            iSide = 0;
        }
        else if (dValue >= dMax - dBand)
        {
            iSide = 1;
        }
        if (iSide < 0)
        {
            continue;
        }
        const double dOther = QVector3D::dotProduct(v3Point, other);
        arrSum[iSide] += dOther;
        arrSquared[iSide] += dOther * dOther;
        ++arrCount[iSide];
    }

    double arrSpread[2] = { 0.0, 0.0 };
    for (int iSide = 0; iSide < 2; ++iSide)
    {
        if (arrCount[iSide] > 0)
        {
            const double dMean = arrSum[iSide] / arrCount[iSide];
            arrSpread[iSide] = std::sqrt(std::max(0.0, arrSquared[iSide] / arrCount[iSide] - dMean * dMean));
        }
    }
    spreadLow = arrSpread[0];
    spreadHigh = arrSpread[1];
}
}

namespace DentalFrameEstimator
{
DentalFrame Compute(const Mesh& mesh, EJaw jaw)
{
    DentalFrame frame;
    const int iVertexCount = mesh.GetVertexCount();
    if (iVertexCount < 10)
    {
        return frame;
    }

    // 정점이 많으면 고르게 뽑아 쓴다
    const int iStride = std::max(1, iVertexCount / 200000);
    std::vector<QVector3D> vecPoints;
    vecPoints.reserve(static_cast<size_t>(iVertexCount / iStride + 1));
    double arrCenter[3] = { 0.0, 0.0, 0.0 };
    for (int i = 0; i < iVertexCount; i += iStride)
    {
        const QVector3D v3Point = mesh.GetPosition(i);
        vecPoints.push_back(v3Point);
        arrCenter[0] += v3Point.x();
        arrCenter[1] += v3Point.y();
        arrCenter[2] += v3Point.z();
    }
    const double dCount = static_cast<double>(vecPoints.size());
    const QVector3D v3Center(static_cast<float>(arrCenter[0] / dCount), static_cast<float>(arrCenter[1] / dCount),
                             static_cast<float>(arrCenter[2] / dCount));

    Matrix3 covariance = {};
    float fRadius = 0.0f;
    for (QVector3D& v3Point : vecPoints)
    {
        v3Point -= v3Center;
        fRadius = std::max(fRadius, v3Point.length());
        const double arrP[3] = { v3Point.x(), v3Point.y(), v3Point.z() };
        for (int i = 0; i < 3; ++i)
        {
            for (int j = 0; j < 3; ++j)
            {
                covariance[i][j] += arrP[i] * arrP[j];
            }
        }
    }

    Matrix3 vectors = {};
    std::array<double, 3> arrValues = {};
    SolveEigen(covariance, vectors, arrValues);
    std::array<int, 3> arrOrder = { 0, 1, 2 };
    std::sort(arrOrder.begin(), arrOrder.end(), [&](int a, int b) { return arrValues[static_cast<size_t>(a)] > arrValues[static_cast<size_t>(b)]; });
    auto axisOf = [&](int column) {
        return QVector3D(static_cast<float>(vectors[0][column]), static_cast<float>(vectors[1][column]),
                         static_cast<float>(vectors[2][column])).normalized();
    };
    const QVector3D v3Major = axisOf(arrOrder[0]);
    const QVector3D v3Middle = axisOf(arrOrder[1]);
    QVector3D v3Occlusal = axisOf(arrOrder[2]);

    // 교합 방향의 부호: 스캔 표면은 위가 열린 껍질이라 넓이 가중 법선의 합이 치관 쪽을 향한다
    QVector3D v3NormalSum;
    if (mesh.HasFaces())
    {
        const int iFaceCount = mesh.GetFaceCount();
        for (int iFace = 0; iFace < iFaceCount; ++iFace)
        {
            const std::uint32_t* kpFace = mesh.GetFace(iFace);
            const QVector3D v3A = mesh.GetPosition(static_cast<int>(kpFace[0]));
            v3NormalSum += QVector3D::crossProduct(mesh.GetPosition(static_cast<int>(kpFace[1])) - v3A,
                                                   mesh.GetPosition(static_cast<int>(kpFace[2])) - v3A);
        }
    }
    else if (mesh.HasNormals())
    {
        for (int i = 0; i < iVertexCount; i += iStride)
        {
            v3NormalSum += mesh.GetNormal(i);
        }
    }
    if (QVector3D::dotProduct(v3NormalSum, v3Occlusal) < 0.0f)
    {
        v3Occlusal = -v3Occlusal;
    }

    // 앞뒤 축: 앞니 끝은 좁고 어금니 끝은 양쪽으로 벌어져 있다
    double dMajorLow = 0.0;
    double dMajorHigh = 0.0;
    double dMiddleLow = 0.0;
    double dMiddleHigh = 0.0;
    MeasureEndSpread(vecPoints, v3Major, v3Middle, dMajorLow, dMajorHigh);
    MeasureEndSpread(vecPoints, v3Middle, v3Major, dMiddleLow, dMiddleHigh);
    const double kEpsilon = 1e-3;
    const double dMajorRatio = std::fabs(std::log((dMajorHigh + kEpsilon) / (dMajorLow + kEpsilon)));
    const double dMiddleRatio = std::fabs(std::log((dMiddleHigh + kEpsilon) / (dMiddleLow + kEpsilon)));

    QVector3D v3Anterior;
    if (dMajorRatio > dMiddleRatio)
    {
        v3Anterior = dMajorHigh < dMajorLow ? v3Major : -v3Major;
    }
    else
    {
        v3Anterior = dMiddleHigh < dMiddleLow ? v3Middle : -v3Middle;
    }
    // 교합 방향과 정확히 수직이 되게 다듬는다
    v3Anterior = (v3Anterior - QVector3D::dotProduct(v3Anterior, v3Occlusal) * v3Occlusal).normalized();

    frame.m_v3Center = v3Center;
    frame.m_v3Occlusal = v3Occlusal;
    frame.m_v3Anterior = v3Anterior;
    frame.m_v3PatientUp = jaw == EJaw::UPPER ? -v3Occlusal : v3Occlusal;
    // 앞을 보고 선 사람의 왼쪽 = 위 × 앞
    frame.m_v3PatientLeft = QVector3D::crossProduct(frame.m_v3PatientUp, v3Anterior).normalized();
    frame.m_fRadius = std::max(fRadius, 1e-3f);
    frame.m_bValid = true;
    return frame;
}
}
