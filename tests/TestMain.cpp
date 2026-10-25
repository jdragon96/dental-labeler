#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>

#include <cstdio>
#include <functional>
#include <vector>

int RunPlyReaderTests(int argc, char** argv);
int RunGeometryTests(int argc, char** argv);
int RunLabelPaintTests(int argc, char** argv);
int RunLabelDocumentTests(int argc, char** argv);
int RunWorkspacePresenterTests(int argc, char** argv);

namespace
{
// MinGW 빌드에서 파이프로 넘긴 표준 출력이 비는 경우가 있어, 결과를 파일로 받은 뒤 그대로 옮겨 찍는다
int RunWithLog(const char* kpName, const std::function<int(int, char**)>& funcRun, int argc, char** argv,
               const QString& logDir)
{
    std::vector<QByteArray> vecArgs;
    for (int i = 0; i < argc; ++i)
    {
        vecArgs.emplace_back(argv[i]);
    }
    const QString strLogPath = QDir(logDir).filePath(QString::fromLatin1(kpName) + QStringLiteral(".txt"));
    vecArgs.emplace_back("-o");
    vecArgs.emplace_back((strLogPath + QStringLiteral(",txt")).toLocal8Bit());

    std::vector<char*> vecArgv;
    for (QByteArray& arg : vecArgs)
    {
        vecArgv.push_back(arg.data());
    }
    const int iResult = funcRun(static_cast<int>(vecArgv.size()), vecArgv.data());

    QFile file(strLogPath);
    if (file.open(QIODevice::ReadOnly))
    {
        const QByteArray log = file.readAll();
        std::fwrite(log.constData(), 1, static_cast<size_t>(log.size()), stdout);
        std::fflush(stdout);
    }
    return iResult;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir logDir;
    const bool bCustomOutput = QCoreApplication::arguments().contains(QStringLiteral("-o"));

    int iFailed = 0;
    if (bCustomOutput || !logDir.isValid())
    {
        iFailed += RunPlyReaderTests(argc, argv);
        iFailed += RunGeometryTests(argc, argv);
        iFailed += RunLabelPaintTests(argc, argv);
        iFailed += RunLabelDocumentTests(argc, argv);
        iFailed += RunWorkspacePresenterTests(argc, argv);
    }
    else
    {
        iFailed += RunWithLog("PlyReader", RunPlyReaderTests, argc, argv, logDir.path());
        iFailed += RunWithLog("Geometry", RunGeometryTests, argc, argv, logDir.path());
        iFailed += RunWithLog("LabelPaint", RunLabelPaintTests, argc, argv, logDir.path());
        iFailed += RunWithLog("LabelDocument", RunLabelDocumentTests, argc, argv, logDir.path());
        iFailed += RunWithLog("WorkspacePresenter", RunWorkspacePresenterTests, argc, argv, logDir.path());
    }
    std::printf("%s\n", iFailed == 0 ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return iFailed == 0 ? 0 : 1;
}
