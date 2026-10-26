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
    bool LoadLabels(const PlyReader& reader, const LabelFile::CasePaths& paths, const SourceInfo& source, CaseData& caseData,
                    QString& strError) const;
    void LoadRecovery(const SourceInfo& source, CaseData& caseData) const;
    static QString MakeMismatchMessage(const Mesh& mesh, const Mesh& labeledMesh, const QString& labeledPath);
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
    const LabelFile::CasePaths paths = LabelFile::ResolveCasePaths(path);
    const QFileInfo meshInfo(paths.m_strMesh);
    if (!meshInfo.exists())
    {
        strError = QStringLiteral("파일이 없다: %1").arg(path);
        return false;
    }
    if (meshInfo.suffix().compare(QLatin1String("ply"), Qt::CaseInsensitive) != 0)
    {
        strError = QStringLiteral("지금은 PLY 파일만 열 수 있다. STL · OBJ · PCD 는 다음 단계에서 지원한다.");
        return false;
    }

    emit pWorker->Progress(QStringLiteral("파일 읽기"), 5);
    PlyReader reader;
    if (!reader.Read(paths.m_strMesh, caseData.m_mesh))
    {
        strError = reader.GetError();
        return false;
    }
    if (IsCancelled(strError))
    {
        return false;
    }

    caseData.m_strMeshPath = meshInfo.absoluteFilePath();
    caseData.m_strLabeledPath = paths.m_strLabeled;
    caseData.m_strAutosavePath = paths.m_strAutosave;
    caseData.m_bStandalone = paths.m_bStandalone;
    caseData.m_strCaseId = paths.m_strCaseId;
    caseData.m_lstSourceComments = LabelFile::StripOwnComments(reader.GetComments());
    caseData.m_eJaw = LabelTable::GuessJawFromFileName(paths.m_strCaseId);

    emit pWorker->Progress(QStringLiteral("원본 해시 계산"), 30);
    SourceInfo source;
    source.m_strFile = meshInfo.fileName();
    source.m_iVertexCount = caseData.m_mesh.GetVertexCount();
    source.m_iFaceCount = caseData.m_mesh.GetFaceCount();
    source.m_strSha256 = FileHash::ComputeSha256(paths.m_strMesh);
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

    emit pWorker->Progress(QStringLiteral("라벨 불러오기"), 85);
    if (!LoadLabels(reader, paths, source, caseData, strError))
    {
        return false;
    }
    if (caseData.m_document.GetJaw() != EJaw::UNKNOWN)
    {
        caseData.m_eJaw = caseData.m_document.GetJaw();
    }
    caseData.m_document.SetJaw(caseData.m_eJaw);

    emit pWorker->Progress(QStringLiteral("치열 방향 추정"), 92);
    caseData.m_frame = DentalFrameEstimator::Compute(mesh, caseData.m_eJaw);

    LoadRecovery(source, caseData);
    emit pWorker->Progress(QStringLiteral("완료"), 100);
    return true;
}

bool MeshLoadWorker::MeshLoadWorkerImpl::LoadLabels(const PlyReader& reader, const LabelFile::CasePaths& paths, const SourceInfo& source,
                                                    CaseData& caseData, QString& strError) const
{
    // 1) 원본 없이 라벨 파일만 열었으면 방금 읽은 파일이 라벨 파일이다
    if (caseData.m_bStandalone)
    {
        const bool bHasFields = LabelFile::HasLabelFields(reader);
        if (bHasFields)
        {
            std::vector<int> vecCodes;
            if (!LabelFile::DecodeFields(reader, vecCodes, strError)
                || !LabelFile::BuildDocument(vecCodes, reader.GetComments(), source, caseData.m_document, strError))
            {
                return false;
            }
        }
        else
        {
            caseData.m_document.Reset(source, caseData.m_eJaw);
        }
        caseData.m_document.SetSource(source);
        caseData.m_bLabelFileLoaded = bHasFields;
        caseData.m_strLoadNotice = QStringLiteral("원본 %1.ply 가 없어 라벨 파일만 열었다. 저장은 이 라벨 파일에 한다.").arg(caseData.m_strCaseId);
        return true;
    }

    // 2) 원본 옆 <이름>.labeled.ply. 정점 수와 위치가 원본과 같아야 쓴다
    if (QFileInfo::exists(caseData.m_strLabeledPath))
    {
        LabelDocument document;
        Mesh labeledMesh;
        QString strLoadError;
        if (!LabelFile::Load(caseData.m_strLabeledPath, document, labeledMesh, strLoadError))
        {
            strError = QStringLiteral("라벨 파일(%1)을 읽지 못했다. %2").arg(QFileInfo(caseData.m_strLabeledPath).fileName(), strLoadError);
            return false;
        }
        if (!caseData.m_mesh.IsSameGeometry(labeledMesh))
        {
            strError = MakeMismatchMessage(caseData.m_mesh, labeledMesh, caseData.m_strLabeledPath);
            return false;
        }
        document.SetSource(source);
        caseData.m_document = std::move(document);
        caseData.m_bLabelFileLoaded = true;
        return true;
    }

    // 3) 이전 형식 <이름>.labels.json 이 있으면 넘겨받는다. 파일은 지우지 않고, 저장하면 새 형식으로 쓴다
    if (QFileInfo::exists(paths.m_strLegacyJson))
    {
        LabelDocument document;
        QString strLoadError;
        const QString strLegacyName = QFileInfo(paths.m_strLegacyJson).fileName();
        if (!LabelFile::LoadLegacyJson(paths.m_strLegacyJson, document, strLoadError))
        {
            strError = QStringLiteral("이전 라벨 파일(%1)을 읽지 못했다. %2").arg(strLegacyName, strLoadError);
            return false;
        }
        const SourceInfo& recorded = document.GetSource();
        if (recorded.m_iVertexCount != source.m_iVertexCount
            || (!recorded.m_strSha256.isEmpty() && recorded.m_strSha256 != source.m_strSha256))
        {
            strError = QStringLiteral("이전 라벨 파일(%1)이 메시와 맞지 않는다. 라벨 파일은 정점 %L2개, 메시는 %L3개다. "
                                      "원본 메시가 바뀐 것으로 보인다.")
                           .arg(strLegacyName)
                           .arg(recorded.m_iVertexCount)
                           .arg(source.m_iVertexCount);
            return false;
        }
        document.SetSource(source);
        caseData.m_document = std::move(document);
        caseData.m_bLabelFileLoaded = true;
        caseData.m_bNeedsSave = true;
        caseData.m_strLoadNotice = QStringLiteral("이전 형식 %1 에서 라벨을 불러왔다. 저장하면 %2 로 쓴다 (이전 파일은 그대로 둔다).")
                                       .arg(strLegacyName, QFileInfo(caseData.m_strLabeledPath).fileName());
        return true;
    }

    // 4) 다른 도구가 원본에 라벨 필드(fdi · label)를 넣어 두었으면 그것으로 시작한다
    if (LabelFile::HasLabelFields(reader))
    {
        std::vector<int> vecCodes;
        QString strFieldError;
        if (LabelFile::DecodeFields(reader, vecCodes, strFieldError)
            && LabelFile::BuildDocument(vecCodes, reader.GetComments(), source, caseData.m_document, strFieldError))
        {
            caseData.m_document.SetSource(source);
            caseData.m_strLoadNotice = QStringLiteral("원본 PLY 의 라벨 필드를 불러왔다. 저장은 %1 에 한다.")
                                           .arg(QFileInfo(caseData.m_strLabeledPath).fileName());
            return true;
        }
        caseData.m_strLoadNotice = QStringLiteral("원본 PLY 의 라벨 필드를 쓰지 않았다. %1").arg(strFieldError);
    }

    caseData.m_document.Reset(source, caseData.m_eJaw);
    return true;
}

void MeshLoadWorker::MeshLoadWorkerImpl::LoadRecovery(const SourceInfo& source, CaseData& caseData) const
{
    const QFileInfo autosaveInfo(caseData.m_strAutosavePath);
    if (!autosaveInfo.exists())
    {
        return;
    }
    const QFileInfo labeledInfo(caseData.m_strLabeledPath);
    if (labeledInfo.exists() && autosaveInfo.lastModified() <= labeledInfo.lastModified())
    {
        return;
    }

    auto upRecovery = std::make_unique<LabelDocument>();
    Mesh autosaveMesh;
    QString strError;
    int iEditCount = 0;
    if (!LabelFile::Load(caseData.m_strAutosavePath, *upRecovery, autosaveMesh, strError, &iEditCount)
        || !caseData.m_mesh.IsSameGeometry(autosaveMesh))
    {
        return;
    }
    upRecovery->SetSource(source);
    caseData.m_upRecovery = std::move(upRecovery);
    caseData.m_dtRecovery = autosaveInfo.lastModified();
    caseData.m_iRecoveryEditCount = iEditCount;
}

QString MeshLoadWorker::MeshLoadWorkerImpl::MakeMismatchMessage(const Mesh& mesh, const Mesh& labeledMesh, const QString& labeledPath)
{
    const QString strLabeledName = QFileInfo(labeledPath).fileName();
    if (mesh.GetVertexCount() != labeledMesh.GetVertexCount())
    {
        return QStringLiteral("라벨 파일이 메시와 맞지 않는다. 라벨 파일은 정점 %L1개, 메시는 %L2개다. "
                              "원본 메시가 바뀐 것으로 보인다. 원본을 되돌리거나, 라벨 파일(%3)을 다른 이름으로 옮긴 뒤 새로 시작한다.")
            .arg(labeledMesh.GetVertexCount())
            .arg(mesh.GetVertexCount())
            .arg(strLabeledName);
    }
    return QStringLiteral("라벨 파일이 메시와 맞지 않는다. 정점 수는 같지만 정점 위치가 다르다. "
                          "원본 메시가 바뀐 것으로 보인다. 원본을 되돌리거나, 라벨 파일(%1)을 다른 이름으로 옮긴 뒤 새로 시작한다.")
        .arg(strLabeledName);
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
