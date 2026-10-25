#include "Thread/MeshLoadThread.h"

#include "Algorithm/Geometry/Curvature.h"
#include "Core/LabelTable.h"
#include "Document/CaseData.h"
#include "Document/FileHash.h"
#include "Document/LabelFile.h"
#include "Document/PlyReader.h"

#include <QFileInfo>
#include <QThread>

#include <atomic>
#include <mutex>

////////////////////////////////////////////////////////////////////////////////
// WorkerImpl
////////////////////////////////////////////////////////////////////////////////
class MeshLoadWorker::MeshLoadWorkerImpl
{
public:
    bool Load(const QString& path, CaseData& caseData, QString& strError, MeshLoadWorker* pWorker);

public:
    std::atomic<bool> m_bRun{ false };
    std::mutex m_mtxResult;
    std::shared_ptr<CaseData> m_spResult;
    QString m_strError;

private:
    bool IsCancelled(QString& strError) const;
    void LoadRecovery(const QString& meshPath, const SourceInfo& source, bool hasLabelFile, CaseData& caseData) const;
};

bool MeshLoadWorker::MeshLoadWorkerImpl::IsCancelled(QString& strError) const
{
    if (m_bRun)
    {
        return false;
    }
    strError = QStringLiteral("열기를 취소했다.");
    return true;
}

bool MeshLoadWorker::MeshLoadWorkerImpl::Load(const QString& path, CaseData& caseData, QString& strError,
                                              MeshLoadWorker* pWorker)
{
    const QFileInfo fileInfo(path);
    if (!fileInfo.exists())
    {
        strError = QStringLiteral("파일이 없다: %1").arg(path);
        return false;
    }
    if (fileInfo.suffix().compare(QLatin1String("ply"), Qt::CaseInsensitive) != 0)
    {
        strError = QStringLiteral("지금은 PLY 파일만 열 수 있다. STL · OBJ · PCD 는 다음 단계에서 지원한다.");
        return false;
    }

    emit pWorker->Progress(QStringLiteral("파일 읽기"), 5);
    PlyReader reader;
    if (!reader.Read(path, caseData.m_mesh))
    {
        strError = reader.GetError();
        return false;
    }
    if (IsCancelled(strError))
    {
        return false;
    }

    caseData.m_strMeshPath = fileInfo.absoluteFilePath();
    caseData.m_strCaseId = fileInfo.completeBaseName();
    caseData.m_strScannerComment = reader.GetComment();
    caseData.m_eJaw = LabelTable::GuessJawFromFileName(fileInfo.fileName());

    emit pWorker->Progress(QStringLiteral("원본 해시 계산"), 30);
    SourceInfo source;
    source.m_strFile = fileInfo.fileName();
    source.m_iVertexCount = caseData.m_mesh.GetVertexCount();
    source.m_iFaceCount = caseData.m_mesh.GetFaceCount();
    source.m_strSha256 = FileHash::ComputeSha256(path);
    if (IsCancelled(strError))
    {
        return false;
    }

    Mesh& mesh = caseData.m_mesh;
    if (mesh.HasFaces())
    {
        emit pWorker->Progress(QStringLiteral("법선 계산"), 40);
        mesh.ComputeNormals();
        emit pWorker->Progress(QStringLiteral("인접 그래프 만들기"), 50);
        caseData.m_topology.Build(mesh);
        if (IsCancelled(strError))
        {
            return false;
        }
        emit pWorker->Progress(QStringLiteral("곡률 계산"), 62);
        caseData.m_vecCurvature = Curvature::ComputeVertexCurvature(mesh, caseData.m_topology);
        emit pWorker->Progress(QStringLiteral("검색 구조(BVH) 만들기"), 72);
        caseData.m_bvh.Build(mesh);
        if (IsCancelled(strError))
        {
            return false;
        }
    }

    emit pWorker->Progress(QStringLiteral("치열 방향 추정"), 88);
    caseData.m_frame = DentalFrameEstimator::Compute(mesh, caseData.m_eJaw);

    emit pWorker->Progress(QStringLiteral("라벨 불러오기"), 94);
    const QString strLabelPath = LabelFile::GetLabelPath(path);
    const bool bHasLabelFile = QFileInfo::exists(strLabelPath);
    if (bHasLabelFile)
    {
        LabelDocument document;
        QString strLoadError;
        if (!LabelFile::Load(strLabelPath, document, strLoadError))
        {
            strError = QStringLiteral("라벨 파일(%1)을 읽지 못했다. %2").arg(QFileInfo(strLabelPath).fileName(), strLoadError);
            return false;
        }

        const SourceInfo& recorded = document.GetSource();
        if (recorded.m_iVertexCount != source.m_iVertexCount)
        {
            strError = QStringLiteral("라벨 파일이 메시와 맞지 않는다. 라벨 파일은 정점 %L1개, 메시는 %L2개다. "
                                      "원본 메시가 바뀐 것으로 보인다. 원본을 되돌리거나, 라벨 파일(%3)을 다른 이름으로 옮긴 뒤 새로 시작한다.")
                           .arg(recorded.m_iVertexCount)
                           .arg(source.m_iVertexCount)
                           .arg(QFileInfo(strLabelPath).fileName());
            return false;
        }
        if (!recorded.m_strSha256.isEmpty() && recorded.m_strSha256 != source.m_strSha256)
        {
            strError = QStringLiteral("라벨 파일이 메시와 맞지 않는다. 정점 수는 같지만 원본 파일 내용(SHA-256)이 다르다. "
                                      "원본을 되돌리거나, 라벨 파일(%1)을 다른 이름으로 옮긴 뒤 새로 시작한다.")
                           .arg(QFileInfo(strLabelPath).fileName());
            return false;
        }

        if (document.GetJaw() != EJaw::UNKNOWN)
        {
            caseData.m_eJaw = document.GetJaw();
        }
        document.SetJaw(caseData.m_eJaw);
        document.SetSource(source);
        caseData.m_document = std::move(document);
        caseData.m_bLabelFileLoaded = true;
    }
    else
    {
        caseData.m_document.Reset(source, caseData.m_eJaw);
    }

    LoadRecovery(path, source, bHasLabelFile, caseData);
    emit pWorker->Progress(QStringLiteral("완료"), 100);
    return true;
}

void MeshLoadWorker::MeshLoadWorkerImpl::LoadRecovery(const QString& meshPath, const SourceInfo& source,
                                                      bool hasLabelFile, CaseData& caseData) const
{
    const QString strAutosavePath = LabelFile::GetAutosavePath(meshPath);
    const QFileInfo autosaveInfo(strAutosavePath);
    if (!autosaveInfo.exists())
    {
        return;
    }
    if (hasLabelFile && autosaveInfo.lastModified() <= QFileInfo(LabelFile::GetLabelPath(meshPath)).lastModified())
    {
        return;
    }

    auto upRecovery = std::make_unique<LabelDocument>();
    QString strError;
    int iEditCount = 0;
    if (!LabelFile::Load(strAutosavePath, *upRecovery, strError, &iEditCount))
    {
        return;
    }
    if (upRecovery->GetSource().m_iVertexCount != source.m_iVertexCount
        || (!upRecovery->GetSource().m_strSha256.isEmpty() && upRecovery->GetSource().m_strSha256 != source.m_strSha256))
    {
        return;
    }
    caseData.m_upRecovery = std::move(upRecovery);
    caseData.m_dtRecovery = autosaveInfo.lastModified();
    caseData.m_iRecoveryEditCount = iEditCount;
}

////////////////////////////////////////////////////////////////////////////////
// Worker
////////////////////////////////////////////////////////////////////////////////
MeshLoadWorker::MeshLoadWorker(QObject* parent)
    : QObject(parent)
    , m_pimpl(std::make_unique<MeshLoadWorkerImpl>())
{
}

MeshLoadWorker::~MeshLoadWorker() = default;

void MeshLoadWorker::Stop()
{
    m_pimpl->m_bRun = false;
}

bool MeshLoadWorker::IsThreadAlive() const
{
    return m_pimpl->m_bRun;
}

std::shared_ptr<CaseData> MeshLoadWorker::TakeResult()
{
    std::lock_guard<std::mutex> lock(m_pimpl->m_mtxResult);
    return std::move(m_pimpl->m_spResult);
}

QString MeshLoadWorker::GetError() const
{
    std::lock_guard<std::mutex> lock(m_pimpl->m_mtxResult);
    return m_pimpl->m_strError;
}

void MeshLoadWorker::DoWork(const QString& path)
{
    m_pimpl->m_bRun = true;
    auto spCase = std::make_shared<CaseData>();
    QString strError;
    const bool bOk = m_pimpl->Load(path, *spCase, strError, this);
    {
        std::lock_guard<std::mutex> lock(m_pimpl->m_mtxResult);
        m_pimpl->m_spResult = bOk ? spCase : nullptr;
        m_pimpl->m_strError = strError;
    }
    m_pimpl->m_bRun = false;
    emit WorkFinished(bOk);
}

////////////////////////////////////////////////////////////////////////////////
// ThreadImpl
////////////////////////////////////////////////////////////////////////////////
class MeshLoadThread::MeshLoadThreadImpl
{
public:
    ~MeshLoadThreadImpl()
    {
        StopWorker();
    }

public:
    void StopWorker()
    {
        if (m_upWorker)
        {
            m_upWorker->Stop();
        }
        if (m_thWorker.isRunning())
        {
            m_thWorker.quit();
            if (!m_thWorker.wait(4000))
            {
                m_thWorker.terminate();
                m_thWorker.wait();
            }
        }
        // 스레드가 끝났으므로 여기서 바로 지워도 된다
        m_upWorker.reset();
        m_bBusy = false;
    }

public:
    QThread m_thWorker;
    std::unique_ptr<MeshLoadWorker> m_upWorker;
    bool m_bBusy = false;
    int m_iGeneration = 0;
};

////////////////////////////////////////////////////////////////////////////////
// Thread
////////////////////////////////////////////////////////////////////////////////
MeshLoadThread::MeshLoadThread(QObject* parent)
    : QObject(parent)
    , m_pimpl(std::make_unique<MeshLoadThreadImpl>())
{
}

MeshLoadThread::~MeshLoadThread() = default;

void MeshLoadThread::Start(const QString& path)
{
    if (!m_pimpl->m_upWorker)
    {
        m_pimpl->m_upWorker = std::make_unique<MeshLoadWorker>();
        MeshLoadWorker* pWorker = m_pimpl->m_upWorker.get();
        // 멈춘 작업자가 남긴 신호가 늦게 도착해도 새 작업 결과로 착각하지 않게 세대를 붙인다
        const int iGeneration = ++m_pimpl->m_iGeneration;
        connect(pWorker, &MeshLoadWorker::Progress, this, [this, iGeneration](const QString& step, int percent) {
            if (iGeneration == m_pimpl->m_iGeneration)
            {
                emit Progress(step, percent);
            }
        });
        connect(pWorker, &MeshLoadWorker::WorkFinished, this, [this, iGeneration](bool success) {
            if (iGeneration != m_pimpl->m_iGeneration)
            {
                return;
            }
            m_pimpl->m_bBusy = false;
            emit Finished(success);
        });
        pWorker->moveToThread(&m_pimpl->m_thWorker);
    }
    if (!m_pimpl->m_thWorker.isRunning())
    {
        m_pimpl->m_thWorker.start();
    }
    m_pimpl->m_bBusy = true;
    QMetaObject::invokeMethod(m_pimpl->m_upWorker.get(), "DoWork", Qt::QueuedConnection, Q_ARG(QString, path));
}

void MeshLoadThread::Stop()
{
    m_pimpl->StopWorker();
}

bool MeshLoadThread::IsRunning() const
{
    return m_pimpl->m_bBusy;
}

std::shared_ptr<CaseData> MeshLoadThread::TakeResult()
{
    return m_pimpl->m_upWorker ? m_pimpl->m_upWorker->TakeResult() : nullptr;
}

QString MeshLoadThread::GetError() const
{
    return m_pimpl->m_upWorker ? m_pimpl->m_upWorker->GetError() : QString();
}
