#include "Core/LabelTable.h"
#include "Document/EditHistory.h"
#include "Document/LabelDocument.h"
#include "Document/LabelFile.h"
#include "Document/LabelRenumber.h"
#include "Document/Mesh.h"
#include "Document/PlyReader.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
void WriteText(const QString& path, const QByteArray& content)
{
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(content);
    }
}

SourceInfo MakeSource(int vertexCount)
{
    SourceInfo source;
    source.m_strFile = QStringLiteral("case_0001_lower.ply");
    source.m_iVertexCount = vertexCount;
    source.m_iFaceCount = vertexCount * 2;
    source.m_strSha256 = QStringLiteral("abc123");
    return source;
}

LabelEdit MakeEdit(const LabelDocument& document, std::vector<std::uint32_t> vecIndices, LabelValue value)
{
    LabelEdit edit;
    edit.m_strName = QStringLiteral("브러시 · %1").arg(value);
    for (std::uint32_t uVertex : vecIndices)
    {
        edit.m_vecIndices.push_back(uVertex);
        edit.m_vecBefore.push_back(document.GetLabel(static_cast<int>(uVertex)));
        edit.m_vecAfter.push_back(value);
    }
    return edit;
}
}

class TestLabelDocument : public QObject
{
    Q_OBJECT

private slots:
    void CountsFollowAssignments()
    {
        LabelDocument document;
        document.Reset(MakeSource(10), EJaw::LOWER);
        QCOMPARE(document.GetCount(kUnlabeled), 10);
        document.Assign({ 0, 1, 2 }, static_cast<LabelValue>(137));
        document.Assign({ 2, 3 }, kGingiva);
        QCOMPARE(document.GetCount(137), 2);
        QCOMPARE(document.GetCount(kGingiva), 2);
        QCOMPARE(document.GetCount(kUnlabeled), 6);
        QCOMPARE(document.GetProgress(), 0.4);
    }

    void HistoryUndoRedoAndStateId()
    {
        LabelDocument document;
        document.Reset(MakeSource(6), EJaw::LOWER);
        EditHistory history(3);
        const long long llSaved = history.GetStateId();

        LabelEdit first = MakeEdit(document, { 0, 1 }, 131);
        document.Assign(first.m_vecIndices, first.m_vecAfter);
        history.Push(std::move(first));
        LabelEdit second = MakeEdit(document, { 1, 2 }, 132);
        document.Assign(second.m_vecIndices, second.m_vecAfter);
        history.Push(std::move(second));
        QVERIFY(history.GetStateId() != llSaved);

        QVERIFY(history.Undo(document) != nullptr);
        QCOMPARE(static_cast<int>(document.GetLabel(1)), 131);
        QCOMPARE(static_cast<int>(document.GetLabel(2)), static_cast<int>(kUnlabeled));
        QVERIFY(history.Undo(document) != nullptr);
        QCOMPARE(history.GetStateId(), llSaved);
        QCOMPARE(document.GetCount(kUnlabeled), 6);

        QVERIFY(history.Redo(document) != nullptr);
        QCOMPARE(static_cast<int>(document.GetLabel(0)), 131);

        // 되돌린 상태에서 새로 편집하면 다시 실행 기록은 사라진다
        LabelEdit third = MakeEdit(document, { 5 }, 0);
        document.Assign(third.m_vecIndices, third.m_vecAfter);
        history.Push(std::move(third));
        QVERIFY(!history.CanRedo());
        QCOMPARE(static_cast<int>(history.GetEdits().size()), 2);
    }

    void HistoryDropsOldestBeyondLimit()
    {
        LabelDocument document;
        document.Reset(MakeSource(10), EJaw::LOWER);
        EditHistory history(3);
        for (std::uint32_t i = 0; i < 5; ++i)
        {
            LabelEdit edit = MakeEdit(document, { i }, 141);
            document.Assign(edit.m_vecIndices, edit.m_vecAfter);
            history.Push(std::move(edit));
        }
        QCOMPARE(static_cast<int>(history.GetEdits().size()), 3);
        while (history.Undo(document) != nullptr)
        {
        }
        // 버린 두 편집은 되돌릴 수 없다
        QCOMPARE(document.GetCount(141), 2);
    }

    void LabeledPlyKeepsSourceAttributesAndMetadata()
    {
        QTemporaryDir dir;
        // 스캐너 원본 흉내: double 좌표, 색, 사각형 면, comment
        const QString strSourcePath = dir.filePath(QStringLiteral("case_0009_lower.ply"));
        WriteText(strSourcePath,
                  "ply\nformat ascii 1.0\ncomment scanner X 1.2\n"
                  "element vertex 4\nproperty double x\nproperty double y\nproperty double z\n"
                  "property uchar red\nproperty uchar green\nproperty uchar blue\n"
                  "element face 1\nproperty list uchar int vertex_indices\nend_header\n"
                  "0.125 0 0 250 10 20\n1 0 0 251 11 21\n1 1 0 252 12 22\n0 1 0.5 253 13 23\n4 0 1 2 3\n");
        Mesh source;
        PlyReader sourceReader;
        QVERIFY2(sourceReader.Read(strSourcePath, source), qPrintable(sourceReader.GetError()));
        QCOMPARE(static_cast<int>(source.m_vecSourceColumns.size()), 6);

        // 정점 0 · 1 은 치아 46, 2 는 36 자리 스캔바디, 3 은 치은
        LabelDocument document;
        document.Reset(MakeSource(4), EJaw::LOWER);
        document.Assign({ 0, 1 }, static_cast<LabelValue>(LabelTable::MakeCode(LabelTable::kClassTooth, 46)));
        document.Assign({ 2 }, static_cast<LabelValue>(LabelTable::MakeCode(LabelTable::kClassScanbody, 36)));
        document.Assign({ 3 }, kGingiva);
        ToothInfo missing;
        missing.m_eState = EToothState::MISSING;
        document.SetTooth(38, missing);
        ToothInfo noted;
        noted.m_strNote = QStringLiteral("원심면이 스캔 구멍과 맞닿음 (재스캔 필요)");
        document.SetTooth(37, noted);
        document.SetAssignee(QStringLiteral("labeler 01"));
        document.SetUpdatedAt(QDateTime::currentDateTime());

        const QString strLabeledPath = dir.filePath(QStringLiteral("case_0009_lower.labeled.ply"));
        QString strError;
        QVERIFY2(LabelFile::Save(strLabeledPath, source, sourceReader.GetComments(), document, strError, 7), qPrintable(strError));

        // 다른 도구가 읽는 모습: 원본 속성이 형식 · 값 그대로 있고 끝에 int fdi · label · instance 가 있다
        Mesh written;
        PlyReader writtenReader;
        QVERIFY2(writtenReader.Read(strLabeledPath, written), qPrintable(writtenReader.GetError()));
        QCOMPARE(writtenReader.GetField("fdi"), (std::vector<int>{ 46, 46, 36, 0 }));
        QCOMPARE(writtenReader.GetField("label"), (std::vector<int>{ 1, 1, 2, 0 }));
        // 치아(클래스 1)가 먼저 1, 스캔바디(클래스 2)가 2. 치은은 0
        QCOMPARE(writtenReader.GetField("instance"), (std::vector<int>{ 1, 1, 2, 0 }));
        QVERIFY(writtenReader.GetComments().contains(QStringLiteral("dentallabeler instance 2 label 2 fdi 36")));
        QVERIFY(writtenReader.GetComments().contains(QStringLiteral("dentallabeler class 2 scanbody")));
        QCOMPARE(static_cast<int>(written.m_vecSourceColumns.size()), 6);
        for (size_t i = 0; i < source.m_vecSourceColumns.size(); ++i)
        {
            QCOMPARE(written.m_vecSourceColumns[i].m_strName, source.m_vecSourceColumns[i].m_strName);
            QCOMPARE(written.m_vecSourceColumns[i].m_strType, source.m_vecSourceColumns[i].m_strType);
            QVERIFY(written.m_vecSourceColumns[i].m_vecData == source.m_vecSourceColumns[i].m_vecData);
        }
        QCOMPARE(written.m_vecSourceColumns[0].m_strType, QByteArray("double"));
        QVERIFY(written.IsSameGeometry(source));
        QCOMPARE(written.GetFaceCount(), 2);
        QVERIFY(writtenReader.GetComments().contains(QStringLiteral("scanner X 1.2")));

        QFile file(strLabeledPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().contains("property uchar blue\nproperty int fdi\nproperty int label\nproperty int instance\n"));
        file.close();

        LabelDocument loaded;
        Mesh loadedMesh;
        int iEditCount = 0;
        QVERIFY2(LabelFile::Load(strLabeledPath, loaded, loadedMesh, strError, &iEditCount), qPrintable(strError));
        QCOMPARE(iEditCount, 7);
        QCOMPARE(loaded.GetLabels(), document.GetLabels());
        QCOMPARE(loaded.GetJaw(), EJaw::LOWER);
        QCOMPARE(loaded.GetSource().m_strSha256, QStringLiteral("abc123"));
        QCOMPARE(loaded.GetSource().m_strFile, QStringLiteral("case_0001_lower.ply"));
        QCOMPARE(loaded.GetTooth(38).m_eState, EToothState::MISSING);
        QCOMPARE(loaded.GetTooth(37).m_strNote, noted.m_strNote);
        QCOMPARE(loaded.GetAssignee(), QStringLiteral("labeler 01"));
        QCOMPARE(loaded.GetCount(146), 2);
        QCOMPARE(loaded.GetCount(236), 1);

        // 다시 저장해도 스캐너 comment 는 한 번만, DentalLabeler comment 는 새로 쓴다
        QVERIFY(LabelFile::Save(strLabeledPath, loadedMesh, writtenReader.GetComments(), loaded, strError));
        QVERIFY(writtenReader.Read(strLabeledPath, written));
        QCOMPARE(writtenReader.GetComments().filter(QStringLiteral("scanner X")).size(), 1);
        QCOMPARE(writtenReader.GetComments().filter(QStringLiteral("dentallabeler source")).size(), 1);
    }

    void LabeledPlyRejectsNonFdiValues()
    {
        QTemporaryDir dir;
        const QString strPath = dir.filePath(QStringLiteral("bad.labeled.ply"));
        WriteText(strPath,
                  "ply\nformat ascii 1.0\nelement vertex 2\nproperty float x\nproperty float y\nproperty float z\n"
                  "property int label\nend_header\n0 0 0 0\n1 0 0 39\n");
        LabelDocument loaded;
        Mesh mesh;
        QString strError;
        QVERIFY(!LabelFile::Load(strPath, loaded, mesh, strError));
        QVERIFY2(strError.contains(QStringLiteral("FDI")), qPrintable(strError));

        // 치은(label 0)에 치아 번호가 붙은 조합도 받지 않는다
        WriteText(strPath,
                  "ply\nformat ascii 1.0\nelement vertex 2\nproperty float x\nproperty float y\nproperty float z\n"
                  "property int fdi\nproperty int label\nproperty int instance\nend_header\n0 0 0 0 0 0\n1 0 0 36 0 1\n");
        QVERIFY(!LabelFile::Load(strPath, loaded, mesh, strError));
        QVERIFY2(strError.contains(QStringLiteral("조합")), qPrintable(strError));
    }

    void ReadsOlderAndAliasedFields()
    {
        QTemporaryDir dir;
        const QString strPath = dir.filePath(QStringLiteral("old.labeled.ply"));
        LabelDocument loaded;
        Mesh mesh;
        QString strError;

        // 2026-10-26 형식: int label 하나에 FDI 번호
        WriteText(strPath,
                  "ply\nformat ascii 1.0\ncomment dentallabeler format 1\nelement vertex 3\n"
                  "property float x\nproperty float y\nproperty float z\nproperty int label\nend_header\n"
                  "0 0 0 0\n1 0 0 36\n0 1 0 -1\n");
        QVERIFY2(LabelFile::Load(strPath, loaded, mesh, strError), qPrintable(strError));
        QCOMPARE(loaded.GetLabels(), (std::vector<LabelValue>{ 0, 136, -1 }));

        // CloudCompare 가 내보낸 scalar_ 이름
        WriteText(strPath,
                  "ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
                  "property int scalar_fdi\nproperty int scalar_label\nproperty int scalar_instance\nend_header\n"
                  "0 0 0 0 0 0\n1 0 0 21 1 4\n0 1 0 21 2 7\n");
        QVERIFY2(LabelFile::Load(strPath, loaded, mesh, strError), qPrintable(strError));
        QCOMPARE(loaded.GetLabels(), (std::vector<LabelValue>{ 0, 121, 221 }));
    }

    void ResolvesCasePaths()
    {
        QTemporaryDir dir;
        const QString strOriginal = dir.filePath(QStringLiteral("case_0002_upper.ply"));
        const QString strLabeled = dir.filePath(QStringLiteral("case_0002_upper.labeled.ply"));

        LabelFile::CasePaths paths = LabelFile::ResolveCasePaths(strOriginal);
        QCOMPARE(paths.m_strCaseId, QStringLiteral("case_0002_upper"));
        QCOMPARE(QFileInfo(paths.m_strLabeled), QFileInfo(strLabeled));
        QCOMPARE(QFileInfo(paths.m_strAutosave).fileName(), QStringLiteral("case_0002_upper.autosave.ply"));

        // 라벨 파일을 열었는데 원본이 없으면 라벨 파일만으로 연다
        paths = LabelFile::ResolveCasePaths(strLabeled);
        QVERIFY(paths.m_bStandalone);
        QCOMPARE(QFileInfo(paths.m_strMesh), QFileInfo(strLabeled));

        // 원본이 있으면 원본을 형상으로 쓴다
        WriteText(strOriginal, "ply\n");
        paths = LabelFile::ResolveCasePaths(strLabeled);
        QVERIFY(!paths.m_bStandalone);
        QCOMPARE(QFileInfo(paths.m_strMesh), QFileInfo(strOriginal));
    }

    void GuessesJawFromFileName()
    {
        QCOMPARE(LabelTable::GuessJawFromFileName(QStringLiteral("case_0001_lower.ply")), EJaw::LOWER);
        QCOMPARE(LabelTable::GuessJawFromFileName(QStringLiteral("P12-Maxilla.ply")), EJaw::UPPER);
        QCOMPARE(LabelTable::GuessJawFromFileName(QStringLiteral("scan_0193.ply")), EJaw::UNKNOWN);
    }

    void RenumberArchShift()
    {
        // 하악 48 … 41, 31 이 칠해져 있고 47 자리에 스캔바디가 있다
        const std::vector<int> vecPresent = { 0, 131, 141, 142, 143, 144, 145, 146, 147, 148, 247 };

        // 48 … 42 를 한 칸 뒤로: 48 → 47 … 42 → 41. 41 은 옮기지 않으므로 42 와 합쳐진다고 알린다
        RenumberPlan plan = LabelRenumber::PlanArchShift(EJaw::LOWER, 48, 42, 1, vecPresent);
        QVERIFY2(plan.IsValid(), qPrintable(plan.m_strError));
        QCOMPARE(plan.m_mapCodes.at(148), 147);
        QCOMPARE(plan.m_mapCodes.at(142), 141);
        QCOMPARE(plan.m_mapCodes.at(247), 246);
        QCOMPARE(static_cast<int>(plan.m_mapCodes.size()), 8);
        QCOMPARE(plan.m_lstWarnings.size(), 1);
        QVERIFY(plan.m_lstWarnings.first().contains(QStringLiteral("41")));

        // 정중선을 넘어 41 → 31, 31 → 32
        plan = LabelRenumber::PlanArchShift(EJaw::LOWER, 41, 31, 1, vecPresent);
        QCOMPARE(plan.m_mapCodes.at(141), 131);
        QCOMPARE(plan.m_mapCodes.at(131), 132);
        // 미리 보기는 치식 순서로 늘어놓는다
        QCOMPARE(plan.GetOrderedPairs().front().first, 141);
        QCOMPARE(plan.Describe(), QStringLiteral("41 → 31 · 31 → 32"));
        QVERIFY(plan.m_lstWarnings.isEmpty());

        // 48 을 앞으로 밀 자리는 없다
        plan = LabelRenumber::PlanArchShift(EJaw::LOWER, 48, 47, -1, vecPresent);
        QVERIFY(!plan.IsValid());
        QVERIFY(plan.m_strError.contains(QStringLiteral("자리가 없다")));

        // 다른 턱 번호는 받지 않는다
        QVERIFY(!LabelRenumber::PlanArchShift(EJaw::LOWER, 18, 11, 1, vecPresent).IsValid());
    }

    void RenumberSingle()
    {
        const std::vector<int> vecPresent = { 0, 136, 137 };
        RenumberPlan plan = LabelRenumber::PlanSingle(136, 137, true, vecPresent);
        QCOMPARE(plan.m_mapCodes.at(136), 137);
        QCOMPARE(plan.m_mapCodes.at(137), 136);
        QVERIFY(plan.m_lstWarnings.isEmpty());

        plan = LabelRenumber::PlanSingle(136, 137, false, vecPresent);
        QCOMPARE(static_cast<int>(plan.m_mapCodes.size()), 1);
        QCOMPARE(plan.m_lstWarnings.size(), 1);

        QVERIFY(!LabelRenumber::PlanSingle(135, 136, true, vecPresent).IsValid());
        QVERIFY(!LabelRenumber::PlanSingle(136, 136, true, vecPresent).IsValid());
    }

    void ColorOverrides()
    {
        const QColor colDefault = LabelTable::GetColor(146);
        LabelTable::SetColorOverride(146, QColor(255, 0, 0));
        QCOMPARE(LabelTable::GetColor(146), QColor(255, 0, 0));
        QVERIFY(LabelTable::HasColorOverride(146));
        // 그 자리 스캔바디는 바뀐 치아 색을 옅게 쓴다
        QCOMPARE(LabelTable::GetColor(246), QColor(255, 127, 127));
        LabelTable::SetColorOverride(146, QColor());
        QCOMPARE(LabelTable::GetColor(146), colDefault);
        QVERIFY(!LabelTable::HasColorOverride(146));
    }

    void LabelCodes()
    {
        QCOMPARE(LabelTable::MakeCode(LabelTable::kClassTooth, 37), 137);
        QCOMPARE(LabelTable::GetName(137), QStringLiteral("하악 좌측 제2대구치"));
        QCOMPARE(LabelTable::GetName(236), QStringLiteral("36 자리 스캔바디"));
        QCOMPARE(LabelTable::GetShortText(236), QStringLiteral("SB36"));
        QVERIFY(LabelTable::IsValidCode(kGingiva));
        QVERIFY(LabelTable::IsValidCode(kUnlabeled));
        QVERIFY(LabelTable::IsValidCode(200));
        QVERIFY(!LabelTable::IsValidCode(139));
        QVERIFY(!LabelTable::IsValidCode(100));
        QVERIFY(!LabelTable::IsValidCode(36));
        QVERIFY(!LabelTable::IsValidCode(900));
    }
};

int RunLabelDocumentTests(int argc, char** argv)
{
    TestLabelDocument test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestLabelDocument.moc"
