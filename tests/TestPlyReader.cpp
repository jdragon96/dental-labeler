#include "Document/Mesh.h"
#include "Document/PlyReader.h"
#include "TestHelpers.h"

#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <cstdint>
#include <cstring>

namespace
{
void WriteFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

template <typename T>
void AppendBigEndian(QByteArray& buffer, T value)
{
    unsigned char arrBytes[sizeof(T)];
    std::memcpy(arrBytes, &value, sizeof(T));
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        // 이 테스트는 리틀 엔디언 머신에서 돈다
        buffer.append(static_cast<char>(arrBytes[sizeof(T) - 1 - i]));
    }
}
}

class TestPlyReader : public QObject
{
    Q_OBJECT

private slots:
    void ReadsMeditSample()
    {
        // 스캔 데이터는 저장소에 올리지 않는다. 로컬에 있을 때만 확인한다
        if (!QFile::exists(TestHelpers::GetSamplePath()))
        {
            QSKIP("data/case_0001_lower.ply 가 없다");
        }
        QElapsedTimer timer;
        timer.start();
        Mesh mesh;
        PlyReader reader;
        QVERIFY2(reader.Read(TestHelpers::GetSamplePath(), mesh), qPrintable(reader.GetError()));
        qInfo("sample read: %lld ms", static_cast<long long>(timer.elapsed()));
        QCOMPARE(mesh.GetVertexCount(), 214104);
        QCOMPARE(mesh.GetFaceCount(), 422306);
        QVERIFY(reader.GetComment().contains(QStringLiteral("Medit")));
    }

    void ReadsAsciiQuadAndSkipsExtraProperties()
    {
        QTemporaryDir dir;
        const QString strPath = dir.filePath(QStringLiteral("quad.ply"));
        WriteFile(strPath,
                  "ply\r\nformat ascii 1.0\r\ncomment test\r\n"
                  "element vertex 4\r\nproperty float x\r\nproperty float y\r\nproperty float z\r\nproperty uchar red\r\n"
                  "element face 1\r\nproperty list uchar int vertex_indices\r\nproperty float quality\r\n"
                  "end_header\r\n"
                  "0 0 0 255\r\n1 0 0 255\r\n1 1 0 255\r\n0 1 0.5 255\r\n"
                  "4 0 1 2 3 0.25\r\n");
        Mesh mesh;
        PlyReader reader;
        QVERIFY2(reader.Read(strPath, mesh), qPrintable(reader.GetError()));
        QCOMPARE(mesh.GetVertexCount(), 4);
        QCOMPARE(mesh.GetFaceCount(), 2);
        QCOMPARE(mesh.GetPosition(3).z(), 0.5f);
        QCOMPARE(reader.GetComment(), QStringLiteral("test"));
    }

    void ReadsBigEndianDoublesAsPointCloud()
    {
        QTemporaryDir dir;
        const QString strPath = dir.filePath(QStringLiteral("points.ply"));
        QByteArray buffer("ply\nformat binary_big_endian 1.0\nelement vertex 2\n"
                          "property double x\nproperty double y\nproperty double z\nend_header\n");
        for (double dValue : { 1.5, -2.0, 3.25, 4.0, 5.0, 6.0 })
        {
            AppendBigEndian(buffer, dValue);
        }
        WriteFile(strPath, buffer);

        Mesh mesh;
        PlyReader reader;
        QVERIFY2(reader.Read(strPath, mesh), qPrintable(reader.GetError()));
        QCOMPARE(mesh.GetVertexCount(), 2);
        QVERIFY(!mesh.HasFaces());
        QCOMPARE(mesh.GetPosition(0), QVector3D(1.5f, -2.0f, 3.25f));
    }

    void ReportsTruncatedFaceList()
    {
        QTemporaryDir dir;
        const QString strPath = dir.filePath(QStringLiteral("broken.ply"));
        QByteArray buffer("ply\nformat binary_little_endian 1.0\nelement vertex 3\n"
                          "property float x\nproperty float y\nproperty float z\n"
                          "element face 2\nproperty list uchar int vertex_indices\nend_header\n");
        const float arrPositions[9] = { 0, 0, 0, 1, 0, 0, 0, 1, 0 };
        buffer.append(reinterpret_cast<const char*>(arrPositions), sizeof(arrPositions));
        const char cCount = 3;
        const std::int32_t arrFace[3] = { 0, 1, 2 };
        buffer.append(&cCount, 1);
        buffer.append(reinterpret_cast<const char*>(arrFace), sizeof(arrFace));
        buffer.append(&cCount, 1);
        WriteFile(strPath, buffer);

        Mesh mesh;
        PlyReader reader;
        QVERIFY(!reader.Read(strPath, mesh));
        QVERIFY2(reader.GetError().contains(QStringLiteral("면 목록이 2번째 면에서 끊겼다")), qPrintable(reader.GetError()));
        QCOMPARE(mesh.GetVertexCount(), 0);
    }

    void RejectsIndexOutOfRange()
    {
        QTemporaryDir dir;
        const QString strPath = dir.filePath(QStringLiteral("bad.ply"));
        WriteFile(strPath,
                  "ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\n"
                  "element face 1\nproperty list uchar int vertex_indices\nend_header\n"
                  "0 0 0\n1 0 0\n0 1 0\n3 0 1 7\n");
        Mesh mesh;
        PlyReader reader;
        QVERIFY(!reader.Read(strPath, mesh));
        QVERIFY2(reader.GetError().contains(QStringLiteral("없는 정점 7")), qPrintable(reader.GetError()));
    }

    void RejectsNonPly()
    {
        QTemporaryDir dir;
        const QString strPath = dir.filePath(QStringLiteral("not.ply"));
        WriteFile(strPath, "solid stl\n");
        Mesh mesh;
        PlyReader reader;
        QVERIFY(!reader.Read(strPath, mesh));
        QVERIFY(reader.GetError().contains(QStringLiteral("PLY 파일이 아니다")));
    }
};

int RunPlyReaderTests(int argc, char** argv)
{
    TestPlyReader test;
    return QTest::qExec(&test, argc, argv);
}

#include "TestPlyReader.moc"
