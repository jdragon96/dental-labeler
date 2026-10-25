#pragma once

#include <QObject>
#include <QString>

#include <memory>

struct CaseData;

// 메시 읽기 → 법선 → 인접 그래프 → 곡률 → BVH → 라벨 파일. 한 번 실행하고 끝나는 작업이다
class MeshLoadWorker : public QObject
{
    Q_OBJECT

public:
    explicit MeshLoadWorker(QObject* parent = nullptr);
    ~MeshLoadWorker() override;

public:
    void Stop();
    bool IsThreadAlive() const;
    std::shared_ptr<CaseData> TakeResult();
    QString GetError() const;

public slots:
    void DoWork(const QString& path);

signals:
    void Progress(const QString& step, int percent);
    void WorkFinished(bool success);

private:
    class MeshLoadWorkerImpl;
    std::unique_ptr<MeshLoadWorkerImpl> m_pimpl;
};

class MeshLoadThread : public QObject
{
    Q_OBJECT

public:
    explicit MeshLoadThread(QObject* parent = nullptr);
    ~MeshLoadThread() override;

public:
    void Start(const QString& path);
    void Stop();
    bool IsRunning() const;
    std::shared_ptr<CaseData> TakeResult();
    QString GetError() const;

signals:
    void Progress(const QString& step, int percent);
    void Finished(bool success);

private:
    class MeshLoadThreadImpl;
    std::unique_ptr<MeshLoadThreadImpl> m_pimpl;
};
