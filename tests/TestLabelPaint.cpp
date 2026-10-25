#include "Algorithm/Geometry/Bvh.h"
#include "Algorithm/Geometry/Curvature.h"
#include "Algorithm/Geometry/MeshTopology.h"
#include "Algorithm/LabelPaint/LabelPaint.h"
#include "Algorithm/LabelPaint/Strategy/LassoPaintCPU.h"
#include "Document/Mesh.h"
#include "TestHelpers.h"

#include <QtTest>

#include <algorithm>
#include <set>
#include <vector>

namespace
{
struct Scene
{
    Mesh m_mesh;
    MeshTopology m_topology;
    Bvh m_bvh;
    std::vector<float> m_vecCurvature;
    std::vector<LabelValue> m_vecLabels;
    std::set<int> m_setLocked;
    std::set<int> m_setHidden;

    explicit Scene(const Mesh& mesh)
        : m_mesh(mesh)
    {
        m_topology.Build(m_mesh);
        m_bvh.Build(m_mesh);
        m_vecCurvature = Curvature::ComputeVertexCurvature(m_mesh, m_topology);
        m_vecLabels.assign(static_cast<size_t>(m_mesh.GetVertexCount()), kUnlabeled);
    }

    PaintInput MakeInput(LabelValue target) const
    {
        PaintInput input;
        input.m_kpMesh = &m_mesh;
        input.m_kpTopology = &m_topology;
        input.m_kpBvh = &m_bvh;
        input.m_kpCurvature = &m_vecCurvature;
        input.m_kpLabels = &m_vecLabels;
        input.m_kpLockedLabels = &m_setLocked;
        input.m_kpHiddenLabels = &m_setHidden;
        input.m_target = target;
        input.m_v3Camera = QVector3D(0.0f, 0.0f, 50.0f);
        return input;
    }

    // 격자 가운데 정점 (count 가 홀수일 때 원점)
    int GetCenterVertex(int count) const
    {
        return (count / 2) * count + count / 2;
    }
};

std::set<std::uint32_t> Run(EPaintType type, const PaintInput& input)
{
    LabelPaint paint;
    paint.SetStrategy(type);
    PaintOutput output;
    paint.Execute(input, output);
    return std::set<std::uint32_t>(output.m_vecIndices.begin(), output.m_vecIndices.end());
}
}

class TestLabelPaint : public QObject
{
    Q_OBJECT

private slots:
    void BrushPaintsWithinGeodesicRadius()
    {
        Scene scene(TestHelpers::MakeGrid(11, 1.0f));
        PaintInput input = scene.MakeInput(37);
        input.m_iSeedVertex = scene.GetCenterVertex(11);
        input.m_v3SeedPoint = QVector3D(0.0f, 0.0f, 0.0f);
        input.m_fRadius = 2.0f;
        const std::set<std::uint32_t> setPainted = Run(EPaintType::BRUSH, input);

        QVERIFY(setPainted.count(static_cast<std::uint32_t>(input.m_iSeedVertex)) == 1);
        // 변을 따라 잰 거리라 직선거리 2 를 넘는 정점은 없다
        for (std::uint32_t uVertex : setPainted)
        {
            QVERIFY(scene.m_mesh.GetPosition(static_cast<int>(uVertex)).length() <= 2.0f + 1e-4f);
        }
        // (2,0) 은 들어가고 (2,1) 은 측지 거리 3 이라 빠진다
        QVERIFY(setPainted.count(static_cast<std::uint32_t>(5 * 11 + 7)) == 1);
        QVERIFY(setPainted.count(static_cast<std::uint32_t>(6 * 11 + 7)) == 0);
    }

    void BrushRespectsOverwriteRuleAndLocks()
    {
        Scene scene(TestHelpers::MakeGrid(11, 1.0f));
        const std::uint32_t uCenter = static_cast<std::uint32_t>(scene.GetCenterVertex(11));
        scene.m_vecLabels[uCenter] = 36;
        scene.m_vecLabels[uCenter + 1] = kGingiva;
        scene.m_vecLabels[uCenter - 1] = 35;

        PaintInput input = scene.MakeInput(37);
        input.m_iSeedVertex = static_cast<int>(uCenter);
        input.m_fRadius = 1.5f;

        input.m_eOverwrite = EOverwriteRule::UNLABELED_AND_GINGIVA;
        std::set<std::uint32_t> setPainted = Run(EPaintType::BRUSH, input);
        QVERIFY(setPainted.count(uCenter) == 0);
        QVERIFY(setPainted.count(uCenter + 1) == 1);

        input.m_eOverwrite = EOverwriteRule::UNLABELED_ONLY;
        setPainted = Run(EPaintType::BRUSH, input);
        QVERIFY(setPainted.count(uCenter + 1) == 0);

        input.m_eOverwrite = EOverwriteRule::ALL;
        scene.m_setLocked.insert(35);
        setPainted = Run(EPaintType::BRUSH, input);
        QVERIFY(setPainted.count(uCenter) == 1);
        QVERIFY(setPainted.count(uCenter - 1) == 0);
    }

    void BrushVisibleOnlySkipsBackFacing()
    {
        Scene scene(TestHelpers::MakeGrid(7, 1.0f));
        PaintInput input = scene.MakeInput(37);
        input.m_iSeedVertex = scene.GetCenterVertex(7);
        input.m_fRadius = 1.5f;
        input.m_v3Camera = QVector3D(0.0f, 0.0f, -50.0f);

        input.m_eReach = EPaintReach::VISIBLE_ONLY;
        QVERIFY(Run(EPaintType::BRUSH, input).empty());
        input.m_eReach = EPaintReach::THROUGH;
        QVERIFY(!Run(EPaintType::BRUSH, input).empty());
    }

    void BrushStopsAtConcaveGroove()
    {
        Scene scene(TestHelpers::MakeValley(13, 0.5f, 0.8f));
        // 고랑(x = 0) 왼쪽에서 칠하기 시작한다
        const int iSeed = 6 * 13 + 4;
        PaintInput input = scene.MakeInput(37);
        input.m_iSeedVertex = iSeed;
        input.m_v3SeedPoint = scene.m_mesh.GetPosition(iSeed);
        input.m_fRadius = 3.0f;
        input.m_eReach = EPaintReach::THROUGH;

        auto countRightSide = [&](const std::set<std::uint32_t>& setPainted) {
            return std::count_if(setPainted.begin(), setPainted.end(), [&](std::uint32_t uVertex) {
                return scene.m_mesh.GetPosition(static_cast<int>(uVertex)).x() > 0.1f;
            });
        };

        input.m_bCurvatureStop = false;
        QVERIFY(countRightSide(Run(EPaintType::BRUSH, input)) > 0);

        // 고랑 정점의 곡률은 약 -1.05/mm 다
        input.m_bCurvatureStop = true;
        input.m_fCurvatureThreshold = 0.5f;
        const std::set<std::uint32_t> setStopped = Run(EPaintType::BRUSH, input);
        QCOMPARE(static_cast<int>(countRightSide(setStopped)), 0);
        // 고랑 정점 자체는 칠한다
        QVERIFY(setStopped.count(static_cast<std::uint32_t>(6 * 13 + 6)) == 1);
    }

    void FillStopsAtOtherLabels()
    {
        Scene scene(TestHelpers::MakeGrid(9, 1.0f));
        // x = 0 열을 36 으로 칠해 격자를 둘로 나눈다
        for (int iy = 0; iy < 9; ++iy)
        {
            scene.m_vecLabels[static_cast<size_t>(iy * 9 + 4)] = 36;
        }
        PaintInput input = scene.MakeInput(37);
        input.m_iSeedVertex = 0;
        input.m_eOverwrite = EOverwriteRule::UNLABELED_ONLY;
        const std::set<std::uint32_t> setFilled = Run(EPaintType::FILL, input);
        QCOMPARE(static_cast<int>(setFilled.size()), 9 * 4);
        for (std::uint32_t uVertex : setFilled)
        {
            QVERIFY(scene.m_mesh.GetPosition(static_cast<int>(uVertex)).x() < 0.0f);
        }
    }

    void LassoPicksInsidePolygonAndRespectsOcclusion()
    {
        const Mesh bottom = TestHelpers::MakeGrid(9, 1.0f, 0.0f);
        const Mesh top = TestHelpers::MakeGrid(9, 1.0f, 1.0f);
        Scene scene(TestHelpers::Merge(bottom, top));

        QMatrix4x4 matProjection;
        matProjection.perspective(40.0f, 1.0f, 0.1f, 100.0f);
        QMatrix4x4 matView;
        matView.lookAt(QVector3D(0.0f, 0.0f, 20.0f), QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f, 1.0f, 0.0f));

        PaintInput input = scene.MakeInput(37);
        input.m_v3Camera = QVector3D(0.0f, 0.0f, 20.0f);
        input.m_matViewProjection = matProjection * matView;
        input.m_viewportSize = QSize(400, 400);
        // 화면 왼쪽 절반
        input.m_vecPolygon = { QPointF(0, 0), QPointF(199, 0), QPointF(199, 400), QPointF(0, 400) };

        input.m_eReach = EPaintReach::VISIBLE_ONLY;
        const std::set<std::uint32_t> setVisible = Run(EPaintType::LASSO, input);
        QVERIFY(!setVisible.empty());
        const std::uint32_t uTopStart = static_cast<std::uint32_t>(bottom.GetVertexCount());
        for (std::uint32_t uVertex : setVisible)
        {
            QVERIFY2(uVertex >= uTopStart, "아래 격자는 위 격자에 가려 칠하면 안 된다");
            QVERIFY(scene.m_mesh.GetPosition(static_cast<int>(uVertex)).x() < 0.0f);
        }

        input.m_eReach = EPaintReach::THROUGH;
        const std::set<std::uint32_t> setThrough = Run(EPaintType::LASSO, input);
        QVERIFY(std::any_of(setThrough.begin(), setThrough.end(), [&](std::uint32_t uVertex) { return uVertex < uTopStart; }));
    }

    void PointInPolygon()
    {
        const std::vector<QPointF> vecTriangle = { QPointF(0, 0), QPointF(10, 0), QPointF(0, 10) };
        QVERIFY(LassoPaintCPU::IsInsidePolygon(vecTriangle, QPointF(2, 2)));
        QVERIFY(!LassoPaintCPU::IsInsidePolygon(vecTriangle, QPointF(8, 8)));
    }
};

int RunLabelPaintTests(int argc, char** argv)
{
    TestLabelPaint test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestLabelPaint.moc"
