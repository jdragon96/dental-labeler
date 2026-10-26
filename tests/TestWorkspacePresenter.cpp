#include "Core/LabelTable.h"
#include "Core/Observers.h"
#include "Core/SystemStatus.h"
#include "Document/CaseData.h"
#include "Document/FileHash.h"
#include "Document/LabelFile.h"
#include "Document/Mesh.h"
#include "Document/PlyReader.h"
#include "Presenter/Workspace/WorkspacePresenter.h"
#include "TestHelpers.h"

#include <QFile>
#include <QFileInfo>
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

        fixture.m_presenter.SelectFdi(36);
        QCOMPARE(fixture.m_system.GetActiveLabel(), 136);
        // 하악 케이스에서 상악 번호는 고를 수 없다
        fixture.m_presenter.SelectFdi(16);
        QCOMPARE(fixture.m_system.GetActiveLabel(), 136);

        const PickResult pick = fixture.PickSomewhere(0.0f);
        QVERIFY(pick.m_bHit);
        const QVector3D v3Camera = fixture.m_presenter.GetCase()->m_frame.m_v3Center
                                   + fixture.m_presenter.GetCase()->m_frame.m_v3Occlusal * 200.0f;
        fixture.m_presenter.BeginStroke(false);
        fixture.m_presenter.PaintDab(pick, v3Camera);
        fixture.m_presenter.EndStroke();
        const int iPainted = fixture.m_presenter.GetLabelRow(136).m_iCount;
        QVERIFY2(iPainted > 0, "브러시가 정점을 칠해야 한다");
        QVERIFY(fixture.m_presenter.IsDirty());
        QCOMPARE(fixture.m_presenter.GetHistoryRows().size(), 1);

        fixture.m_presenter.Undo();
        QCOMPARE(fixture.m_presenter.GetLabelRow(136).m_iCount, 0);
        QVERIFY(!fixture.m_presenter.IsDirty());
        fixture.m_presenter.Redo();
        QCOMPARE(fixture.m_presenter.GetLabelRow(136).m_iCount, iPainted);

        fixture.m_presenter.SetToothState(38, EToothState::MISSING);
        const QString strShaBefore = FileHash::ComputeSha256(strMesh);
        QVERIFY(fixture.m_presenter.Save());
        QVERIFY(!fixture.m_presenter.IsDirty());
        const QString strLabeled = dir.filePath(QStringLiteral("case_0001_lower.labeled.ply"));
        QVERIFY(QFile::exists(strLabeled));
        // 원본 스캔은 한 바이트도 바뀌지 않는다
        QCOMPARE(FileHash::ComputeSha256(strMesh), strShaBefore);

        Fixture reopened;
        QVERIFY(reopened.Open(strMesh));
        QVERIFY(reopened.m_presenter.GetCase()->m_bLabelFileLoaded);
        QCOMPARE(reopened.m_presenter.GetLabelRow(136).m_iCount, iPainted);
        QCOMPARE(reopened.m_presenter.GetLabelRow(138).m_eState, EToothState::MISSING);
        QCOMPARE(reopened.m_presenter.GetCase()->m_document.GetStatus(), QStringLiteral("in_progress"));
        QVERIFY(reopened.m_presenter.GetCase()->m_lstSourceComments.join(QString()).contains(QStringLiteral("Medit")));

        // 라벨 파일을 직접 열어도 원본을 형상으로 쓰는 같은 케이스가 된다
        Fixture viaLabeled;
        QVERIFY(viaLabeled.Open(strLabeled));
        QCOMPARE(QFileInfo(viaLabeled.m_presenter.GetCase()->m_strMeshPath), QFileInfo(strMesh));
        QCOMPARE(viaLabeled.m_presenter.GetLabelRow(136).m_iCount, iPainted);
    }

    void MigratesLegacyJsonLabels()
    {
        QTemporaryDir dir;
        const QString strMesh = CopySample(dir);
        // 2026-10-25 M1 이 쓰던 형식
        QString strLabels;
        for (int i = 0; i < 214104; ++i)
        {
            strLabels += i < 1000 ? QStringLiteral("42,") : QStringLiteral("-1,");
        }
        strLabels.chop(1);
        QFile json(dir.filePath(QStringLiteral("case_0001_lower.labels.json")));
        QVERIFY(json.open(QIODevice::WriteOnly));
        json.write(QStringLiteral(R"({"format":"dentallabeler.labels","version":1,"jaw":"lower","status":"in_progress",)"
                                  R"("source":{"file":"case_0001_lower.ply","vertex_count":214104,"face_count":422306,"sha256":"%1"},)"
                                  R"("labels":[%2],"teeth":{"38":{"state":"missing"}}})")
                       .arg(FileHash::ComputeSha256(strMesh), strLabels)
                       .toUtf8());
        json.close();

        Fixture fixture;
        QSignalSpy spyMessage(&fixture.m_presenter, &WorkspacePresenter::Message);
        QVERIFY(fixture.Open(strMesh));
        QCOMPARE(fixture.m_presenter.GetLabelRow(142).m_iCount, 1000);
        QCOMPARE(fixture.m_presenter.GetLabelRow(138).m_eState, EToothState::MISSING);
        QVERIFY(fixture.m_presenter.IsDirty());
        QVERIFY(!spyMessage.isEmpty() && spyMessage.last().first().toString().contains(QStringLiteral("이전 형식")));

        QVERIFY(fixture.m_presenter.Save());
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("case_0001_lower.labeled.ply"))));
        QVERIFY(json.exists());

        Fixture reopened;
        QVERIFY(reopened.Open(strMesh));
        QCOMPARE(reopened.m_presenter.GetLabelRow(142).m_iCount, 1000);
        QVERIFY(!reopened.m_presenter.IsDirty());
    }

    void PaintsScanbodyWithClassSwitch()
    {
        QTemporaryDir dir;
        const QString strMesh = CopySample(dir);
        Fixture fixture;
        QVERIFY(fixture.Open(strMesh));

        // Alt+2 다음 3 6 과 같다
        fixture.m_presenter.SelectClass(LabelTable::kClassScanbody);
        fixture.m_presenter.SelectFdi(36);
        QCOMPARE(fixture.m_system.GetActiveLabel(), 236);

        const PickResult pick = fixture.PickSomewhere(0.0f);
        fixture.m_presenter.BeginStroke(false);
        fixture.m_presenter.PaintDab(pick, fixture.m_presenter.GetCase()->m_frame.m_v3Center
                                               + fixture.m_presenter.GetCase()->m_frame.m_v3Occlusal * 200.0f);
        fixture.m_presenter.EndStroke();
        const int iScanbody = fixture.m_presenter.GetLabelRow(236).m_iCount;
        QVERIFY(iScanbody > 0);

        // 클래스를 바꾸면 같은 자리의 치아가 된다. 치은은 자리 없이 바로 고른다
        fixture.m_presenter.SelectClass(LabelTable::kClassTooth);
        QCOMPARE(fixture.m_system.GetActiveLabel(), 136);
        fixture.m_presenter.SelectClass(LabelTable::kClassGingiva);
        QCOMPARE(fixture.m_system.GetActiveLabel(), static_cast<int>(kGingiva));
        QVERIFY(fixture.m_presenter.GetLabelRows().size() > 18);

        QVERIFY(fixture.m_presenter.Save());
        Mesh written;
        PlyReader reader;
        QVERIFY(reader.Read(dir.filePath(QStringLiteral("case_0001_lower.labeled.ply")), written));
        const std::vector<int>& vecFdi = reader.GetField("fdi");
        const std::vector<int>& vecLabel = reader.GetField("label");
        const std::vector<int>& vecInstance = reader.GetField("instance");
        int iFound = 0;
        for (size_t i = 0; i < vecLabel.size(); ++i)
        {
            if (vecLabel[i] == LabelTable::kClassScanbody)
            {
                QCOMPARE(vecFdi[i], 36);
                QCOMPARE(vecInstance[i], 1);
                ++iFound;
            }
            else
            {
                QCOMPARE(vecLabel[i], -1);
                QCOMPARE(vecInstance[i], 0);
            }
        }
        QCOMPARE(iFound, iScanbody);
    }

    void StartsFromLabelFieldOfOtherTools()
    {
        QTemporaryDir dir;
        auto writeScan = [&](const QString& name, const QByteArray& labels) {
            QFile file(dir.filePath(name));
            if (file.open(QIODevice::WriteOnly))
            {
                file.write("ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
                           "property uchar label\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n");
                const QList<QByteArray> lstLabels = labels.split(' ');
                file.write("0 0 0 " + lstLabels[0] + "\n1 0 0 " + lstLabels[1] + "\n0 1 0 " + lstLabels[2] + "\n3 0 1 2\n");
            }
            return dir.filePath(name);
        };

        Fixture fixture;
        QSignalSpy spyMessage(&fixture.m_presenter, &WorkspacePresenter::Message);
        QVERIFY(fixture.Open(writeScan(QStringLiteral("scan_lower.ply"), "0 0 31")));
        QCOMPARE(fixture.m_presenter.GetLabelRow(131).m_iCount, 1);
        QCOMPARE(fixture.m_presenter.GetLabelRow(kGingiva).m_iCount, 2);
        QVERIFY(!spyMessage.isEmpty() && spyMessage.last().first().toString().contains(QStringLiteral("라벨 필드를 불러왔다")));

        // FDI 가 아닌 클래스 번호(예: 0–32 순번)는 쓰지 않고 미분류로 시작한다
        Fixture other;
        QSignalSpy spyOther(&other.m_presenter, &WorkspacePresenter::Message);
        QVERIFY(other.Open(writeScan(QStringLiteral("other_lower.ply"), "1 2 3")));
        QCOMPARE(other.m_presenter.GetLabelRow(kUnlabeled).m_iCount, 3);
        QVERIFY(!spyOther.isEmpty() && spyOther.last().first().toString().contains(QStringLiteral("쓰지 않았다")));
    }

    void OpensLabeledFileWithoutOriginal()
    {
        QTemporaryDir dir;
        const QString strMesh = CopySample(dir);
        const QString strLabeled = dir.filePath(QStringLiteral("case_0001_lower.labeled.ply"));
        int iPainted = 0;
        {
            Fixture fixture;
            QVERIFY(fixture.Open(strMesh));
            fixture.m_presenter.SelectFdi(46);
            const PickResult pick = fixture.PickSomewhere(0.0f);
            fixture.m_presenter.BeginStroke(false);
            fixture.m_presenter.PaintDab(pick, fixture.m_presenter.GetCase()->m_frame.m_v3Center
                                                   + fixture.m_presenter.GetCase()->m_frame.m_v3Occlusal * 200.0f);
            fixture.m_presenter.EndStroke();
            iPainted = fixture.m_presenter.GetLabelRow(146).m_iCount;
            QVERIFY(fixture.m_presenter.Save());
        }
        QVERIFY(QFile::remove(strMesh));

        Fixture fixture;
        QVERIFY(fixture.Open(strLabeled));
        QVERIFY(fixture.m_presenter.GetCase()->m_bStandalone);
        QCOMPARE(fixture.m_presenter.GetLabelRow(146).m_iCount, iPainted);
        fixture.m_presenter.SetToothState(48, EToothState::MISSING);
        QVERIFY(fixture.m_presenter.Save());
        QVERIFY(!QFile::exists(strMesh));

        Fixture reopened;
        QVERIFY(reopened.Open(strLabeled));
        QCOMPARE(reopened.m_presenter.GetLabelRow(148).m_eState, EToothState::MISSING);
        QCOMPARE(reopened.m_presenter.GetLabelRow(146).m_iCount, iPainted);
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
            const CaseData* kpCase = fixture.m_presenter.GetCase();
            LabelDocument autosave = kpCase->m_document;
            autosave.Assign({ 0, 1, 2 }, static_cast<LabelValue>(141));
            QString strError;
            QTest::qWait(1100);
            QVERIFY2(LabelFile::Save(kpCase->m_strAutosavePath, kpCase->m_mesh, kpCase->m_lstSourceComments, autosave, strError, 5),
                     qPrintable(strError));
        }

        Fixture fixture;
        QSignalSpy spyRecovery(&fixture.m_presenter, &WorkspacePresenter::RecoveryAvailable);
        QVERIFY(fixture.Open(strMesh));
        QCOMPARE(spyRecovery.size(), 1);
        QCOMPARE(spyRecovery.first().at(1).toInt(), 5);
        QCOMPARE(fixture.m_presenter.GetLabelRow(141).m_iCount, 0);

        fixture.m_presenter.ResolveRecovery(true);
        QCOMPARE(fixture.m_presenter.GetLabelRow(141).m_iCount, 3);
        QVERIFY(fixture.m_presenter.IsDirty());
        // 복구도 실행 취소할 수 있다
        fixture.m_presenter.Undo();
        QCOMPARE(fixture.m_presenter.GetLabelRow(141).m_iCount, 0);
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

        // 저장한 뒤 원본의 첫 정점 x 를 바꿔 원본이 다시 내보내진 상황을 만든다 (binary_little_endian)
        QFile file(strMesh);
        QVERIFY(file.open(QIODevice::ReadWrite));
        const QByteArray content = file.readAll();
        const int iBody = content.indexOf("end_header\n") + static_cast<int>(qstrlen("end_header\n"));
        QVERIFY(iBody > 20);
        const float fMoved = 123.0f;
        QVERIFY(file.seek(iBody));
        file.write(reinterpret_cast<const char*>(&fMoved), sizeof(fMoved));
        file.close();

        Fixture fixture;
        QSignalSpy spyFailed(&fixture.m_presenter, &WorkspacePresenter::LoadFailed);
        QVERIFY(!fixture.Open(strMesh));
        QCOMPARE(spyFailed.size(), 1);
        const QString strMessage = spyFailed.first().first().toString();
        QVERIFY2(strMessage.contains(QStringLiteral("메시와 맞지 않는다")) && strMessage.contains(QStringLiteral("위치")),
                 qPrintable(strMessage));
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
