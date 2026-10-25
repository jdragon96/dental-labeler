#include "Core/Observers.h"
#include "Core/SystemStatus.h"
#include "Document/CaseData.h"
#include "Document/LabelFile.h"
#include "Presenter/Workspace/WorkspacePresenter.h"
#include "TestHelpers.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace
{
struct Fixture
{
    Observers m_observers;
    SystemStatus m_system;
    WorkspacePresenter m_presenter;

    Fixture()
        : m_observers()
        , m_system(&m_observers)
        , m_presenter(&m_observers, &m_system)
    {
    }

    bool Open(const QString& path)
    {
        QSignalSpy spyOpened(&m_presenter, &WorkspacePresenter::CaseOpened);
        QSignalSpy spyFailed(&m_presenter, &WorkspacePresenter::LoadFailed);
        m_presenter.OpenFile(path);
        for (int i = 0; i < 100 && spyOpened.isEmpty() && spyFailed.isEmpty(); ++i)
        {
            spyOpened.wait(100);
        }
        return !spyOpened.isEmpty();
    }

    // 교합면 쪽에서 아래로 광선을 쏘아 맞는 첫 표면
    PickResult PickSomewhere(float offset) const
    {
        const CaseData* kpCase = m_presenter.GetCase();
        const DentalFrame& frame = kpCase->m_frame;
        const QVector3D v3Origin = frame.m_v3Center + frame.m_v3Occlusal * frame.m_fRadius * 3.0f;
        for (int i = 0; i < 400; ++i)
        {
            const QVector3D v3Target = frame.m_v3Center + frame.m_v3PatientLeft * (offset + static_cast<float>(i % 20) - 10.0f)
                                       + frame.m_v3Anterior * (static_cast<float>(i / 20) - 10.0f);
            const PickResult pick = m_presenter.Pick(v3Origin, (v3Target - v3Origin).normalized(), 0.001f);
            if (pick.m_bHit)
            {
                return pick;
            }
        }
        return PickResult();
    }
};

QString CopySample(const QTemporaryDir& dir)
{
    const QString strPath = dir.filePath(QStringLiteral("case_0001_lower.ply"));
    QFile::copy(TestHelpers::GetSamplePath(), strPath);
    return strPath;
}
}

class TestWorkspacePresenter : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        if (!QFile::exists(TestHelpers::GetSamplePath()))
        {
            QSKIP("샘플 데이터가 없다");
        }
    }

    void PaintUndoSaveAndReopen()
    {
        QTemporaryDir dir;
        const QString strMesh = CopySample(dir);
        Fixture fixture;
        QVERIFY(fixture.Open(strMesh));
        QCOMPARE(fixture.m_system.GetJaw(), EJaw::LOWER);
        QVERIFY(!fixture.m_presenter.IsDirty());

        fixture.m_presenter.SelectLabel(36);
        QCOMPARE(fixture.m_system.GetActiveLabel(), 36);
        // 하악 케이스에서 상악 번호는 고를 수 없다
        fixture.m_presenter.SelectLabel(16);
        QCOMPARE(fixture.m_system.GetActiveLabel(), 36);

        const PickResult pick = fixture.PickSomewhere(0.0f);
        QVERIFY(pick.m_bHit);
        const QVector3D v3Camera = fixture.m_presenter.GetCase()->m_frame.m_v3Center
                                   + fixture.m_presenter.GetCase()->m_frame.m_v3Occlusal * 200.0f;
        fixture.m_presenter.BeginStroke(false);
        fixture.m_presenter.PaintDab(pick, v3Camera);
        fixture.m_presenter.EndStroke();
        const int iPainted = fixture.m_presenter.GetLabelRow(36).m_iCount;
        QVERIFY2(iPainted > 0, "브러시가 정점을 칠해야 한다");
        QVERIFY(fixture.m_presenter.IsDirty());
        QCOMPARE(fixture.m_presenter.GetHistoryRows().size(), 1);

        fixture.m_presenter.Undo();
        QCOMPARE(fixture.m_presenter.GetLabelRow(36).m_iCount, 0);
        QVERIFY(!fixture.m_presenter.IsDirty());
        fixture.m_presenter.Redo();
        QCOMPARE(fixture.m_presenter.GetLabelRow(36).m_iCount, iPainted);

        fixture.m_presenter.SetToothState(38, EToothState::MISSING);
        QVERIFY(fixture.m_presenter.Save());
        QVERIFY(!fixture.m_presenter.IsDirty());
        QVERIFY(QFile::exists(LabelFile::GetLabelPath(strMesh)));

        Fixture reopened;
        QVERIFY(reopened.Open(strMesh));
        QVERIFY(reopened.m_presenter.GetCase()->m_bLabelFileLoaded);
        QCOMPARE(reopened.m_presenter.GetLabelRow(36).m_iCount, iPainted);
        QCOMPARE(reopened.m_presenter.GetLabelRow(38).m_eState, EToothState::MISSING);
        QCOMPARE(reopened.m_presenter.GetCase()->m_document.GetStatus(), QStringLiteral("in_progress"));
    }

    void OffersNewerAutosaveForRecovery()
    {
        QTemporaryDir dir;
        const QString strMesh = CopySample(dir);
        {
            Fixture fixture;
            QVERIFY(fixture.Open(strMesh));
            QVERIFY(fixture.m_presenter.Save());

            // 저장 뒤에 칠한 내용을 자동 저장본으로만 남기고 앱이 꺼진 상황
            LabelDocument autosave = fixture.m_presenter.GetCase()->m_document;
            autosave.Assign({ 0, 1, 2 }, static_cast<LabelValue>(41));
            QString strError;
            QTest::qWait(1100);
            QVERIFY(LabelFile::Save(LabelFile::GetAutosavePath(strMesh), autosave, strError, 5));
        }

        Fixture fixture;
        QSignalSpy spyRecovery(&fixture.m_presenter, &WorkspacePresenter::RecoveryAvailable);
        QVERIFY(fixture.Open(strMesh));
        QCOMPARE(spyRecovery.size(), 1);
        QCOMPARE(spyRecovery.first().at(1).toInt(), 5);
        QCOMPARE(fixture.m_presenter.GetLabelRow(41).m_iCount, 0);

        fixture.m_presenter.ResolveRecovery(true);
        QCOMPARE(fixture.m_presenter.GetLabelRow(41).m_iCount, 3);
        QVERIFY(fixture.m_presenter.IsDirty());
        // 복구도 실행 취소할 수 있다
        fixture.m_presenter.Undo();
        QCOMPARE(fixture.m_presenter.GetLabelRow(41).m_iCount, 0);
    }

    void RefusesLabelFileOfDifferentMesh()
    {
        QTemporaryDir dir;
        const QString strMesh = CopySample(dir);
        {
            Fixture fixture;
            QVERIFY(fixture.Open(strMesh));
            QVERIFY(fixture.m_presenter.Save());
        }

        // 라벨 파일이 기억하는 원본 해시를 바꿔 원본이 바뀐 상황을 만든다
        const QString strLabelPath = LabelFile::GetLabelPath(strMesh);
        QFile file(strLabelPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonObject objRoot = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
        QJsonObject objSource = objRoot.value(QStringLiteral("source")).toObject();
        objSource.insert(QStringLiteral("sha256"), QStringLiteral("0000"));
        objRoot.insert(QStringLiteral("source"), objSource);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(QJsonDocument(objRoot).toJson());
        file.close();

        Fixture fixture;
        QSignalSpy spyFailed(&fixture.m_presenter, &WorkspacePresenter::LoadFailed);
        QVERIFY(!fixture.Open(strMesh));
        QCOMPARE(spyFailed.size(), 1);
        QVERIFY(spyFailed.first().first().toString().contains(QStringLiteral("메시와 맞지 않는다")));
        QVERIFY(!fixture.m_presenter.HasCase());
    }

    void FillAndLassoRecordHistory()
    {
        QTemporaryDir dir;
        const QString strMesh = CopySample(dir);
        Fixture fixture;
        QVERIFY(fixture.Open(strMesh));
        const CaseData* kpCase = fixture.m_presenter.GetCase();
        const QVector3D v3Camera = kpCase->m_frame.m_v3Center + kpCase->m_frame.m_v3Occlusal * 200.0f;

        // 미분류만 채우기: 연결된 미분류 전체가 치은이 된다
        fixture.m_presenter.SelectLabel(kGingiva);
        const PickResult pick = fixture.PickSomewhere(0.0f);
        QVERIFY(pick.m_bHit);
        fixture.m_presenter.ApplyFill(pick, v3Camera, false);
        QVERIFY(fixture.m_presenter.GetLabelRow(kGingiva).m_iCount > 100000);

        // 잠근 라벨은 채우기 지우기로도 바뀌지 않는다
        fixture.m_presenter.ToggleLocked(kGingiva);
        const int iBefore = fixture.m_presenter.GetLabelRow(kGingiva).m_iCount;
        fixture.m_presenter.ApplyFill(pick, v3Camera, true);
        QCOMPARE(fixture.m_presenter.GetLabelRow(kGingiva).m_iCount, iBefore);
        QCOMPARE(fixture.m_presenter.GetHistoryRows().size(), 1);
    }
};

int RunWorkspacePresenterTests(int argc, char** argv)
{
    TestWorkspacePresenter test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestWorkspacePresenter.moc"
