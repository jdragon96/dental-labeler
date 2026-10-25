#include "Algorithm/Geometry/Bvh.h"
#include "Algorithm/Geometry/Curvature.h"
#include "Algorithm/Geometry/DentalFrame.h"
#include "Algorithm/Geometry/MeshTopology.h"
#include "Document/Mesh.h"
#include "Document/PlyReader.h"
#include "TestHelpers.h"

#include <QElapsedTimer>
#include <QFile>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <vector>

class TestGeometry : public QObject
{
    Q_OBJECT

private slots:
    void TopologyOfGrid()
    {
        const Mesh mesh = TestHelpers::MakeGrid(5, 1.0f);
        MeshTopology topology;
        topology.Build(mesh);
        QCOMPARE(topology.GetVertexCount(), 25);
        // 대각선으로 나눈 격자의 안쪽 정점은 이웃이 6개, 모서리 (0,0) 은 3개
        QCOMPARE(topology.GetNeighborCount(12), 6);
        QCOMPARE(topology.GetNeighborCount(0), 3);
    }

    void BvhHitsNearestAndDetectsOcclusion()
    {
        const Mesh top = TestHelpers::MakeGrid(4, 1.0f, 1.0f);
        const Mesh bottom = TestHelpers::MakeGrid(4, 1.0f, 0.0f);
        const Mesh mesh = TestHelpers::Merge(bottom, top);
        Bvh bvh;
        bvh.Build(mesh);
        QVERIFY(!bvh.IsEmpty());

        RayHit hit;
        QVERIFY(bvh.Raycast(QVector3D(0.2f, 0.3f, 10.0f), QVector3D(0, 0, -1), hit));
        QVERIFY(qAbs(hit.m_fT - 9.0f) < 1e-4f);
        // 위쪽 격자의 면 번호는 아래쪽 면 수(18) 뒤에 온다
        QVERIFY(hit.m_iFace >= bottom.GetFaceCount());

        QVERIFY(!bvh.Raycast(QVector3D(5.0f, 5.0f, 10.0f), QVector3D(0, 0, -1), hit));
        QVERIFY(bvh.IsOccluded(QVector3D(0.2f, 0.3f, 10.0f), QVector3D(0, 0, -1), 9.9f));
        QVERIFY(!bvh.IsOccluded(QVector3D(0.2f, 0.3f, 10.0f), QVector3D(0, 0, -1), 8.9f));
    }

    void CurvatureSignsOfValley()
    {
        const Mesh flat = TestHelpers::MakeGrid(7, 1.0f);
        MeshTopology flatTopology;
        flatTopology.Build(flat);
        for (float fValue : Curvature::ComputeVertexCurvature(flat, flatTopology))
        {
            QVERIFY(std::fabs(fValue) < 1e-5f);
        }

        const Mesh valley = TestHelpers::MakeValley(7, 1.0f, 0.8f);
        MeshTopology topology;
        topology.Build(valley);
        const std::vector<float> vecCurvature = Curvature::ComputeVertexCurvature(valley, topology);
        // 가운데 열(x = 0)은 오목하다
        const int iCenter = 3 * 7 + 3;
        QVERIFY2(vecCurvature[static_cast<size_t>(iCenter)] < -0.3f, qPrintable(QString::number(vecCurvature[static_cast<size_t>(iCenter)])));
        QVERIFY(std::fabs(vecCurvature[static_cast<size_t>(3 * 7 + 1)]) < 0.05f);
    }

    void SampleFrameAndCurvatureDistribution()
    {
        if (!QFile::exists(TestHelpers::GetSamplePath()))
        {
            QSKIP("샘플 데이터가 없다");
        }
        Mesh mesh;
        PlyReader reader;
        QVERIFY(reader.Read(TestHelpers::GetSamplePath(), mesh));

        QElapsedTimer timer;
        timer.start();
        mesh.ComputeNormals();
        MeshTopology topology;
        topology.Build(mesh);
        const qint64 llTopologyMs = timer.restart();
        const std::vector<float> vecCurvature = Curvature::ComputeVertexCurvature(mesh, topology);
        const qint64 llCurvatureMs = timer.restart();
        Bvh bvh;
        bvh.Build(mesh);
        const qint64 llBvhMs = timer.restart();
        const DentalFrame frame = DentalFrameEstimator::Compute(mesh, EJaw::LOWER);
        qInfo("topology %lld ms, curvature %lld ms, bvh %lld ms (%d nodes), frame %lld ms",
              static_cast<long long>(llTopologyMs), static_cast<long long>(llCurvatureMs), static_cast<long long>(llBvhMs),
              bvh.GetNodeCount(), static_cast<long long>(timer.elapsed()));

        QVERIFY(frame.m_bValid);
        QVERIFY(std::fabs(QVector3D::dotProduct(frame.m_v3Occlusal, frame.m_v3Anterior)) < 1e-3f);
        QVERIFY(std::fabs(QVector3D::dotProduct(frame.m_v3PatientLeft, frame.m_v3Anterior)) < 1e-3f);
        QVector3D v3Min;
        QVector3D v3Max;
        mesh.GetBounds(v3Min, v3Max);
        qInfo("bounds (%.1f %.1f %.1f) - (%.1f %.1f %.1f)", double(v3Min.x()), double(v3Min.y()), double(v3Min.z()),
              double(v3Max.x()), double(v3Max.y()), double(v3Max.z()));
        qInfo("occlusal (%.2f %.2f %.2f) anterior (%.2f %.2f %.2f) left (%.2f %.2f %.2f)",
              double(frame.m_v3Occlusal.x()), double(frame.m_v3Occlusal.y()), double(frame.m_v3Occlusal.z()),
              double(frame.m_v3Anterior.x()), double(frame.m_v3Anterior.y()), double(frame.m_v3Anterior.z()),
              double(frame.m_v3PatientLeft.x()), double(frame.m_v3PatientLeft.y()), double(frame.m_v3PatientLeft.z()));

        std::vector<float> vecSorted = vecCurvature;
        std::sort(vecSorted.begin(), vecSorted.end());
        auto percentile = [&](double p) { return double(vecSorted[static_cast<size_t>(p * (vecSorted.size() - 1))]); };
        qInfo("curvature p1 %.3f p5 %.3f p10 %.3f p25 %.3f p50 %.3f p75 %.3f p90 %.3f p99 %.3f",
              percentile(0.01), percentile(0.05), percentile(0.10), percentile(0.25), percentile(0.50),
              percentile(0.75), percentile(0.90), percentile(0.99));

        // 광선 하나는 수 마이크로초 안에 끝나야 한다 (커서를 움직일 때마다 쏜다)
        timer.restart();
        int iHits = 0;
        const QVector3D v3Origin = frame.m_v3Center + frame.m_v3Occlusal * frame.m_fRadius * 3.0f;
        for (int i = 0; i < 1000; ++i)
        {
            const float fOffset = (static_cast<float>(i % 40) - 20.0f) * 1.2f;
            const QVector3D v3Target = frame.m_v3Center + frame.m_v3PatientLeft * fOffset
                                       + frame.m_v3Anterior * (static_cast<float>(i / 40) - 12.0f);
            RayHit hit;
            if (bvh.Raycast(v3Origin, (v3Target - v3Origin).normalized(), hit))
            {
                ++iHits;
            }
        }
        qInfo("1000 rays: %lld ms, %d hits", static_cast<long long>(timer.elapsed()), iHits);
        QVERIFY(iHits > 100);
    }
};

int RunGeometryTests(int argc, char** argv)
{
    TestGeometry test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestGeometry.moc"
