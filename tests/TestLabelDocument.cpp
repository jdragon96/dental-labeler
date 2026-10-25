#include "Core/LabelTable.h"
#include "Document/EditHistory.h"
#include "Document/LabelDocument.h"
#include "Document/LabelFile.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
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
        document.Assign({ 0, 1, 2 }, static_cast<LabelValue>(37));
        document.Assign({ 2, 3 }, kGingiva);
        QCOMPARE(document.GetCount(37), 2);
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

        LabelEdit first = MakeEdit(document, { 0, 1 }, 31);
        document.Assign(first.m_vecIndices, first.m_vecAfter);
        history.Push(std::move(first));
        LabelEdit second = MakeEdit(document, { 1, 2 }, 32);
        document.Assign(second.m_vecIndices, second.m_vecAfter);
        history.Push(std::move(second));
        QVERIFY(history.GetStateId() != llSaved);

        QVERIFY(history.Undo(document) != nullptr);
        QCOMPARE(static_cast<int>(document.GetLabel(1)), 31);
        QCOMPARE(static_cast<int>(document.GetLabel(2)), static_cast<int>(kUnlabeled));
        QVERIFY(history.Undo(document) != nullptr);
        QCOMPARE(history.GetStateId(), llSaved);
        QCOMPARE(document.GetCount(kUnlabeled), 6);

        QVERIFY(history.Redo(document) != nullptr);
        QCOMPARE(static_cast<int>(document.GetLabel(0)), 31);

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
            LabelEdit edit = MakeEdit(document, { i }, 41);
            document.Assign(edit.m_vecIndices, edit.m_vecAfter);
            history.Push(std::move(edit));
        }
        QCOMPARE(static_cast<int>(history.GetEdits().size()), 3);
        while (history.Undo(document) != nullptr)
        {
        }
        // 버린 두 편집은 되돌릴 수 없다
        QCOMPARE(document.GetCount(41), 2);
    }

    void FileRoundTrip()
    {
        QTemporaryDir dir;
        const QString strPath = dir.filePath(QStringLiteral("case.labels.json"));

        LabelDocument document;
        document.Reset(MakeSource(5), EJaw::LOWER);
        document.Assign({ 0, 1 }, static_cast<LabelValue>(46));
        document.Assign({ 4 }, kGingiva);
        ToothInfo missing;
        missing.m_eState = EToothState::MISSING;
        document.SetTooth(38, missing);
        ToothInfo noted;
        noted.m_strNote = QStringLiteral("원심면이 스캔 구멍과 맞닿음");
        document.SetTooth(37, noted);
        document.SetUpdatedAt(QDateTime::currentDateTime());

        QString strError;
        QVERIFY2(LabelFile::Save(strPath, document, strError, 7), qPrintable(strError));

        LabelDocument loaded;
        int iEditCount = 0;
        QVERIFY2(LabelFile::Load(strPath, loaded, strError, &iEditCount), qPrintable(strError));
        QCOMPARE(iEditCount, 7);
        QCOMPARE(loaded.GetLabels(), document.GetLabels());
        QCOMPARE(loaded.GetJaw(), EJaw::LOWER);
        QCOMPARE(loaded.GetSource().m_strSha256, QStringLiteral("abc123"));
        QCOMPARE(loaded.GetTooth(38).m_eState, EToothState::MISSING);
        QCOMPARE(loaded.GetTooth(37).m_strNote, noted.m_strNote);
        QCOMPARE(loaded.GetCount(46), 2);
    }

    void FileRejectsInvalidLabels()
    {
        QTemporaryDir dir;
        const QString strPath = dir.filePath(QStringLiteral("bad.labels.json"));
        QFile file(strPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"format":"dentallabeler.labels","version":1,"source":{"vertex_count":2},"labels":[0,39]})");
        file.close();

        LabelDocument loaded;
        QString strError;
        QVERIFY(!LabelFile::Load(strPath, loaded, strError));
        QVERIFY(strError.contains(QStringLiteral("FDI")));
    }

    void GuessesJawFromFileName()
    {
        QCOMPARE(LabelTable::GuessJawFromFileName(QStringLiteral("case_0001_lower.ply")), EJaw::LOWER);
        QCOMPARE(LabelTable::GuessJawFromFileName(QStringLiteral("P12-Maxilla.ply")), EJaw::UPPER);
        QCOMPARE(LabelTable::GuessJawFromFileName(QStringLiteral("scan_0193.ply")), EJaw::UNKNOWN);
        QCOMPARE(LabelTable::GetName(37), QStringLiteral("하악 좌측 제2대구치"));
        QVERIFY(!LabelTable::IsValidLabel(39));
    }
};

int RunLabelDocumentTests(int argc, char** argv)
{
    TestLabelDocument test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestLabelDocument.moc"
