#include "Core/Application.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("DentalLabeler"));
    QApplication::setOrganizationName(QStringLiteral("DentalLabeler"));
    QApplication::setApplicationVersion(QStringLiteral(DL_VERSION));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("구강 스캔 메시 · 포인트 클라우드 치아 분할 라벨링"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("열 PLY 파일"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("케이스가 열리면 창을 그림으로 저장하고 끝낸다 (화면 확인용)"),
                                              QStringLiteral("path"));
    const QCommandLineOption demoStrokeOption(QStringLiteral("demo-stroke"),
                                              QStringLiteral("--screenshot 과 함께: 저장하기 전에 브러시 획을 몇 번 긋는다. 파일은 저장하지 않는다"));
    parser.addOption(screenshotOption);
    parser.addOption(demoStrokeOption);
    parser.process(app);

    Application application;
    application.Show();
    if (parser.isSet(screenshotOption))
    {
        application.ScheduleScreenshot(parser.value(screenshotOption), parser.isSet(demoStrokeOption));
    }
    if (!parser.positionalArguments().isEmpty())
    {
        application.OpenFile(parser.positionalArguments().first());
    }
    return app.exec();
}
