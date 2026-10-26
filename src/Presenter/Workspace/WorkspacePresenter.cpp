#include "Presenter/Workspace/WorkspacePresenter.h"

#include "Algorithm/Geometry/Curvature.h"
#include "Algorithm/LabelPaint/LabelPaint.h"
#include "Core/LabelTable.h"
#include "Core/Observers.h"
#include "Core/SystemStatus.h"
#include "Document/CaseData.h"
#include "Document/EditHistory.h"
#include "Document/LabelFile.h"
#include "Thread/MeshLoadThread.h"

#include <QFile>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr int kAutosaveIntervalMs = 2 * 60 * 1000;

QString GetLabelText(int label)
{
    if (label == kGingiva)
    {
        return QStringLiteral("치은");
    }
    return LabelTable::GetShortText(label);
}
}

WorkspacePresenter::WorkspacePresenter(Observers* kpObservers, SystemStatus* kpSystem, QObject* parent)
    : QObject(parent)
    , m_kpObservers(kpObservers)
    , m_kpSystem(kpSystem)
    , m_upLoadThread(std::make_unique<MeshLoadThread>())
    , m_upPaint(std::make_unique<LabelPaint>())
    , m_upHistory(std::make_unique<EditHistory>(500))
    , m_spCase()
    , m_timerAutosave()
    , m_watcherAutosave()
    , m_llSavedStateId(0)
    , m_llAutosavedStateId(0)
    , m_bMetaDirty(false)
    , m_bLastDirty(false)
    , m_bStrokeActive(false)
    , m_bStrokeErase(false)
    , m_hashStrokeBefore()
    , m_dtLastAutosave()
    , m_strLoadingPath()
{
    connect(m_upLoadThread.get(), &MeshLoadThread::Progress, this, &WorkspacePresenter::LoadProgress);
    connect(m_upLoadThread.get(), &MeshLoadThread::Finished, this, &WorkspacePresenter::OnLoadFinished);

    m_timerAutosave.setInterval(kAutosaveIntervalMs);
    connect(&m_timerAutosave, &QTimer::timeout, this, &WorkspacePresenter::OnAutosaveTimer);
    m_timerAutosave.start();
    connect(&m_watcherAutosave, &QFutureWatcher<QString>::finished, this, &WorkspacePresenter::OnAutosaveFinished);
}

WorkspacePresenter::~WorkspacePresenter()
{
    m_watcherAutosave.waitForFinished();
    m_upLoadThread->Stop();
}

void WorkspacePresenter::OpenFile(const QString& path)
{
    if (m_upLoadThread->IsRunning())
    {
        m_upLoadThread->Stop();
    }
    m_strLoadingPath = path;
    emit LoadStarted(QFileInfo(path).fileName());
    m_upLoadThread->Start(path);
}

void WorkspacePresenter::CancelLoading()
{
    if (m_upLoadThread->IsRunning())
    {
        m_upLoadThread->Stop();
        m_strLoadingPath.clear();
        emit Message(QStringLiteral("열기를 취소했다."));
    }
}

bool WorkspacePresenter::Save()
{
    if (!m_spCase)
    {
        return false;
    }

    LabelDocument& document = m_spCase->m_document;
    document.SetUpdatedAt(QDateTime::currentDateTime());
    if (document.GetStatus() == QLatin1String("new") && document.GetLabeledCount() > 0)
    {
        document.SetStatus(QStringLiteral("in_progress"));
    }

    // 원본 스캔에는 절대 쓰지 않는다. 원본 없이 라벨 파일만 연 경우에만 연 파일 자체에 쓴다
    const QString strPath = m_spCase->m_strLabeledPath;
    if (!m_spCase->m_bStandalone && QFileInfo(strPath) == QFileInfo(m_spCase->m_strMeshPath))
    {
        emit SaveFailed(QStringLiteral("저장할 곳이 원본 파일과 같아 저장하지 않았다: %1").arg(strPath));
        return false;
    }
    QString strError;
    if (!LabelFile::Save(strPath, m_spCase->m_mesh, m_spCase->m_lstSourceComments, document, strError))
    {
        emit SaveFailed(strError);
        return false;
    }

    m_llSavedStateId = m_upHistory->GetStateId();
    m_llAutosavedStateId = m_llSavedStateId;
    m_bMetaDirty = false;
    QFile::remove(m_spCase->m_strAutosavePath);
    NotifyDirty();
    emit Saved(strPath);
    return true;
}

void WorkspacePresenter::ResolveRecovery(bool restore)
{
    if (!m_spCase || !m_spCase->m_upRecovery)
    {
        return;
    }

    if (restore)
    {
        const LabelDocument& recovery = *m_spCase->m_upRecovery;
        const std::vector<LabelValue>& vecRecovered = recovery.GetLabels();
        const LabelDocument& document = m_spCase->m_document;

        LabelEdit edit;
        edit.m_strName = QStringLiteral("자동 저장본 복구");
        for (size_t i = 0; i < vecRecovered.size(); ++i)
        {
            const LabelValue current = document.GetLabel(static_cast<int>(i));
            if (current != vecRecovered[i])
            {
                edit.m_vecIndices.push_back(static_cast<std::uint32_t>(i));
                edit.m_vecBefore.push_back(current);
                edit.m_vecAfter.push_back(vecRecovered[i]);
            }
        }
        for (const auto& it : recovery.GetTeeth())
        {
            m_spCase->m_document.SetTooth(it.first, it.second);
            m_bMetaDirty = true;
        }
        if (!edit.m_vecIndices.empty())
        {
            m_spCase->m_document.Assign(edit.m_vecIndices, edit.m_vecAfter);
            m_upHistory->Push(std::move(edit));
        }
        emit LabelsChanged();
        emit LabelStateChanged();
        emit HistoryChanged();
        emit Message(QStringLiteral("자동 저장본을 복구했다. 저장하면 라벨 파일에 반영된다."));
    }
    else
    {
        QFile::remove(m_spCase->m_strAutosavePath);
        emit Message(QStringLiteral("자동 저장본을 버렸다."));
    }

    m_spCase->m_upRecovery.reset();
    NotifyDirty();
}

void WorkspacePresenter::Undo()
{
    if (!m_spCase || m_bStrokeActive)
    {
        return;
    }
    const LabelEdit* kpEdit = m_upHistory->Undo(m_spCase->m_document);
    if (kpEdit == nullptr)
    {
        return;
    }
    emit LabelsChanged();
    emit HistoryChanged();
    emit Message(QStringLiteral("실행 취소: %1").arg(kpEdit->m_strName));
    NotifyDirty();
}

void WorkspacePresenter::Redo()
{
    if (!m_spCase || m_bStrokeActive)
    {
        return;
    }
    const LabelEdit* kpEdit = m_upHistory->Redo(m_spCase->m_document);
    if (kpEdit == nullptr)
    {
        return;
    }
    emit LabelsChanged();
    emit HistoryChanged();
    emit Message(QStringLiteral("다시 실행: %1").arg(kpEdit->m_strName));
    NotifyDirty();
}

void WorkspacePresenter::JumpToHistory(int appliedCount)
{
    if (!m_spCase || m_bStrokeActive)
    {
        return;
    }
    bool bChanged = false;
    while (m_upHistory->GetAppliedCount() > appliedCount && m_upHistory->Undo(m_spCase->m_document) != nullptr)
    {
        bChanged = true;
    }
    while (m_upHistory->GetAppliedCount() < appliedCount && m_upHistory->Redo(m_spCase->m_document) != nullptr)
    {
        bChanged = true;
    }
    if (bChanged)
    {
        emit LabelsChanged();
        emit HistoryChanged();
        NotifyDirty();
    }
}

PickResult WorkspacePresenter::Pick(const QVector3D& origin, const QVector3D& dir, float pixelAngle) const
{
    PickResult result;
    if (!m_spCase)
    {
        return result;
    }

    const Mesh& mesh = m_spCase->m_mesh;
    const LabelDocument& document = m_spCase->m_document;
    if (mesh.HasFaces())
    {
        RayHit hit;
        if (!m_spCase->m_bvh.Raycast(origin, dir, hit))
        {
            return result;
        }
        result.m_iFace = hit.m_iFace;
        result.m_v3Point = origin + dir * hit.m_fT;
        const std::uint32_t* kpFace = mesh.GetFace(hit.m_iFace);
        float fBest = std::numeric_limits<float>::max();
        for (int i = 0; i < 3; ++i)
        {
            const float fDistance = (mesh.GetPosition(static_cast<int>(kpFace[i])) - result.m_v3Point).lengthSquared();
            if (fDistance < fBest)
            {
                fBest = fDistance;
                result.m_iVertex = static_cast<int>(kpFace[i]);
            }
        }
    }
    else
    {
        // 포인트 클라우드: 광선에서 4픽셀 안에 있는 점 중 가장 앞의 점
        const float fTolerance = std::tan(std::max(pixelAngle, 1e-5f) * 4.0f);
        const std::set<int>& setHidden = document.GetHiddenLabels();
        float fBestT = std::numeric_limits<float>::max();
        const int iVertexCount = mesh.GetVertexCount();
        for (int iVertex = 0; iVertex < iVertexCount; ++iVertex)
        {
            const QVector3D v3ToPoint = mesh.GetPosition(iVertex) - origin;
            const float fT = QVector3D::dotProduct(v3ToPoint, dir);
            if (fT <= 0.0f || fT >= fBestT)
            {
                continue;
            }
            const float fRadius = fT * fTolerance;
            if (v3ToPoint.lengthSquared() - fT * fT > fRadius * fRadius)
            {
                continue;
            }
            if (!setHidden.empty() && setHidden.count(document.GetLabel(iVertex)) > 0)
            {
                continue;
            }
            fBestT = fT;
            result.m_iVertex = iVertex;
        }
        if (result.m_iVertex < 0)
        {
            return result;
        }
        result.m_v3Point = mesh.GetPosition(result.m_iVertex);
    }

    result.m_bHit = true;
    result.m_label = document.GetLabel(result.m_iVertex);
    if (static_cast<int>(m_spCase->m_vecCurvature.size()) == mesh.GetVertexCount())
    {
        result.m_fCurvature = m_spCase->m_vecCurvature[static_cast<size_t>(result.m_iVertex)];
    }
    return result;
}

void WorkspacePresenter::BeginStroke(bool erase)
{
    if (!m_spCase)
    {
        return;
    }
    m_bStrokeActive = true;
    m_bStrokeErase = erase;
    m_hashStrokeBefore.clear();
}

void WorkspacePresenter::PaintDab(const PickResult& pick, const QVector3D& camera)
{
    if (!m_bStrokeActive || !m_spCase || !pick.m_bHit)
    {
        return;
    }

    PaintInput input = MakePaintInput(ETool::BRUSH, m_bStrokeErase, camera);
    input.m_iSeedFace = pick.m_iFace;
    input.m_iSeedVertex = pick.m_iVertex;
    input.m_v3SeedPoint = pick.m_v3Point;

    PaintOutput output;
    m_upPaint->SetStrategy(EPaintType::BRUSH);
    m_upPaint->Execute(input, output);
    if (output.m_vecIndices.empty())
    {
        return;
    }

    LabelDocument& document = m_spCase->m_document;
    for (std::uint32_t uVertex : output.m_vecIndices)
    {
        // 한 스트로크에서 처음 바뀌기 전 값만 기억한다
        m_hashStrokeBefore.emplace(uVertex, document.GetLabel(static_cast<int>(uVertex)));
    }
    document.Assign(output.m_vecIndices, input.m_target);
    emit LabelsChanged();
}

void WorkspacePresenter::EndStroke()
{
    if (!m_bStrokeActive)
    {
        return;
    }
    m_bStrokeActive = false;
    if (!m_spCase || m_hashStrokeBefore.empty())
    {
        m_hashStrokeBefore.clear();
        return;
    }

    const LabelDocument& document = m_spCase->m_document;
    LabelEdit edit;
    edit.m_strName = MakeEditName(QStringLiteral("브러시"), m_bStrokeErase);
    edit.m_vecIndices.reserve(m_hashStrokeBefore.size());
    for (const auto& it : m_hashStrokeBefore)
    {
        const LabelValue after = document.GetLabel(static_cast<int>(it.first));
        if (after != it.second)
        {
            edit.m_vecIndices.push_back(it.first);
            edit.m_vecBefore.push_back(it.second);
            edit.m_vecAfter.push_back(after);
        }
    }
    m_hashStrokeBefore.clear();
    if (edit.m_vecIndices.empty())
    {
        return;
    }
    m_upHistory->Push(std::move(edit));
    emit HistoryChanged();
    NotifyDirty();
}

void WorkspacePresenter::ApplyFill(const PickResult& pick, const QVector3D& camera, bool erase)
{
    if (!m_spCase || !pick.m_bHit)
    {
        return;
    }
    if (m_spCase->m_topology.IsEmpty())
    {
        emit Message(QStringLiteral("포인트 클라우드에는 아직 채우기를 쓸 수 없다."));
        return;
    }

    PaintInput input = MakePaintInput(ETool::FILL, erase, camera);
    input.m_iSeedVertex = pick.m_iVertex;
    PaintOutput output;
    m_upPaint->SetStrategy(EPaintType::FILL);
    m_upPaint->Execute(input, output);
    if (output.m_vecIndices.empty())
    {
        emit Message(QStringLiteral("바꿀 정점이 없다. 덮어쓰기 규칙이나 잠금을 확인한다."));
        return;
    }
    ApplyEdit(output.m_vecIndices, input.m_target, MakeEditName(QStringLiteral("채우기"), erase));
}

void WorkspacePresenter::ApplyLasso(const std::vector<QPointF>& vecPolygon, const QMatrix4x4& viewProjection,
                                    const QSize& viewport, const QVector3D& camera, bool erase)
{
    if (!m_spCase || vecPolygon.size() < 3)
    {
        return;
    }

    PaintInput input = MakePaintInput(ETool::LASSO, erase, camera);
    input.m_vecPolygon = vecPolygon;
    input.m_matViewProjection = viewProjection;
    input.m_viewportSize = viewport;
    PaintOutput output;
    m_upPaint->SetStrategy(EPaintType::LASSO);
    m_upPaint->Execute(input, output);
    if (output.m_vecIndices.empty())
    {
        return;
    }
    ApplyEdit(output.m_vecIndices, input.m_target, MakeEditName(QStringLiteral("올가미"), erase));
}

void WorkspacePresenter::SelectLabel(int label)
{
    if (!LabelTable::IsValidCode(label) || label == kUnlabeled)
    {
        emit Message(QStringLiteral("쓸 수 없는 라벨 값이다: %1").arg(label));
        return;
    }
    const int iFdi = LabelTable::GetFdiOf(label);
    const EJaw eJaw = m_kpSystem->GetJaw();
    if (LabelTable::IsFdi(iFdi) && eJaw != EJaw::UNKNOWN && LabelTable::GetJawOfFdi(iFdi) != eJaw)
    {
        emit Message(QStringLiteral("%1 케이스에서는 %2 을 고를 수 없다.")
                         .arg(LabelTable::GetJawName(eJaw))
                         .arg(iFdi));
        return;
    }
    m_kpSystem->SetActiveLabel(label);
}

void WorkspacePresenter::SelectFdi(int fdi)
{
    if (fdi == 0)
    {
        SelectLabel(kGingiva);
        return;
    }
    if (!LabelTable::IsFdi(fdi))
    {
        emit Message(QStringLiteral("%1 은 없는 치아 번호다.").arg(fdi));
        return;
    }
    SelectLabel(LabelTable::MakeCode(m_kpSystem->GetObjectClass(), fdi));
}

void WorkspacePresenter::SelectClass(int classValue)
{
    const LabelTable::ClassInfo* kpClass = LabelTable::FindClass(classValue);
    if (kpClass == nullptr)
    {
        return;
    }
    if (kpClass->m_eFdiUse == LabelTable::EFdiUse::NONE)
    {
        SelectLabel(LabelTable::MakeCode(classValue, 0));
        return;
    }

    // 지금 고른 자리를 새 클래스로 옮긴다. 자리가 없으면 다음 번호 입력부터 그 클래스로 고른다
    m_kpSystem->SetObjectClass(classValue);
    const int iFdi = LabelTable::GetFdiOf(m_kpSystem->GetActiveLabel());
    if (LabelTable::IsFdi(iFdi))
    {
        SelectLabel(LabelTable::MakeCode(classValue, iFdi));
    }
    else if (kpClass->m_eFdiUse == LabelTable::EFdiUse::ALLOWED)
    {
        SelectLabel(LabelTable::MakeCode(classValue, 0));
    }
    emit Message(QStringLiteral("클래스: %1. 숫자 두 개로 자리를 고른다.").arg(LabelTable::GetClassName(classValue)));
}

void WorkspacePresenter::SelectAdjacentTooth(int step)
{
    const std::vector<int>& vecOrder = LabelTable::GetArchOrder(m_kpSystem->GetJaw());
    if (vecOrder.empty())
    {
        return;
    }
    const int iClass = m_kpSystem->GetObjectClass();
    const int iCount = static_cast<int>(vecOrder.size());
    auto it = std::find(vecOrder.begin(), vecOrder.end(), LabelTable::GetFdiOf(m_kpSystem->GetActiveLabel()));
    int iIndex = it == vecOrder.end() ? (step > 0 ? -1 : iCount) : static_cast<int>(it - vecOrder.begin());
    for (int iTry = 0; iTry < iCount; ++iTry)
    {
        iIndex = (iIndex + step + iCount) % iCount;
        const int iFdi = vecOrder[static_cast<size_t>(iIndex)];
        // 치아는 결손 자리를 건너뛴다
        const bool bMissing = iClass == LabelTable::kClassTooth && m_spCase
                              && m_spCase->m_document.GetTooth(iFdi).m_eState == EToothState::MISSING;
        if (!bMissing)
        {
            m_kpSystem->SetActiveLabel(LabelTable::MakeCode(iClass, iFdi));
            return;
        }
    }
}

void WorkspacePresenter::PickLabelFromVertex(int vertex)
{
    if (!m_spCase || vertex < 0 || vertex >= m_spCase->m_document.GetVertexCount())
    {
        return;
    }
    const LabelValue label = m_spCase->m_document.GetLabel(vertex);
    if (label == kUnlabeled)
    {
        emit Message(QStringLiteral("미분류 정점이다. 활성 라벨을 바꾸지 않았다."));
        return;
    }
    SelectLabel(label);
}

void WorkspacePresenter::SetToothState(int fdi, EToothState state)
{
    if (!m_spCase || !LabelTable::IsFdi(fdi))
    {
        return;
    }
    ToothInfo info = m_spCase->m_document.GetTooth(fdi);
    if (info.m_eState == state)
    {
        return;
    }
    info.m_eState = state;
    m_spCase->m_document.SetTooth(fdi, info);
    m_bMetaDirty = true;
    emit LabelStateChanged();
    NotifyDirty();
}

void WorkspacePresenter::SetToothNote(int fdi, const QString& note)
{
    if (!m_spCase || !LabelTable::IsFdi(fdi))
    {
        return;
    }
    ToothInfo info = m_spCase->m_document.GetTooth(fdi);
    if (info.m_strNote == note)
    {
        return;
    }
    info.m_strNote = note;
    m_spCase->m_document.SetTooth(fdi, info);
    m_bMetaDirty = true;
    NotifyDirty();
}

void WorkspacePresenter::ClearLabelVertices(int label)
{
    if (!m_spCase)
    {
        return;
    }
    std::vector<std::uint32_t> vecIndices;
    const std::vector<LabelValue>& vecLabels = m_spCase->m_document.GetLabels();
    for (size_t i = 0; i < vecLabels.size(); ++i)
    {
        if (vecLabels[i] == label)
        {
            vecIndices.push_back(static_cast<std::uint32_t>(i));
        }
    }
    if (!vecIndices.empty())
    {
        ApplyEdit(vecIndices, kUnlabeled, QStringLiteral("%1 정점 지우기").arg(GetLabelText(label)));
    }
}

void WorkspacePresenter::ToggleLocked(int label)
{
    if (!m_spCase)
    {
        return;
    }
    LabelDocument& document = m_spCase->m_document;
    const bool bLocked = !document.IsLocked(label);
    document.SetLocked(label, bLocked);
    emit LabelStateChanged();
    emit Message(bLocked ? QStringLiteral("%1 을 잠갔다. 어떤 도구로도 바뀌지 않는다.").arg(LabelTable::GetName(label))
                         : QStringLiteral("%1 잠금을 풀었다.").arg(LabelTable::GetName(label)));
}

void WorkspacePresenter::ToggleHidden(int label)
{
    if (!m_spCase)
    {
        return;
    }
    LabelDocument& document = m_spCase->m_document;
    document.SetHidden(label, !document.IsHidden(label));
    emit LabelStateChanged();
}

void WorkspacePresenter::ShowAllLabels()
{
    if (!m_spCase)
    {
        return;
    }
    m_spCase->m_document.ClearHidden();
    emit LabelStateChanged();
}

void WorkspacePresenter::IsolateLabel(int label)
{
    if (!m_spCase)
    {
        return;
    }
    LabelDocument& document = m_spCase->m_document;
    std::vector<int> vecOthers = document.GetPresentCodes();
    vecOthers.push_back(kUnlabeled);

    bool bAlreadyIsolated = !document.IsHidden(label);
    for (int iOther : vecOthers)
    {
        if (iOther != label && !document.IsHidden(iOther))
        {
            bAlreadyIsolated = false;
            break;
        }
    }
    document.ClearHidden();
    if (!bAlreadyIsolated)
    {
        for (int iOther : vecOthers)
        {
            if (iOther != label)
            {
                document.SetHidden(iOther, true);
            }
        }
    }
    emit LabelStateChanged();
}

bool WorkspacePresenter::HasCase() const
{
    return static_cast<bool>(m_spCase);
}

bool WorkspacePresenter::IsLoading() const
{
    return m_upLoadThread->IsRunning();
}

bool WorkspacePresenter::IsDirty() const
{
    return m_spCase && (m_upHistory->GetStateId() != m_llSavedStateId || m_bMetaDirty);
}

const CaseData* WorkspacePresenter::GetCase() const
{
    return m_spCase.get();
}

QString WorkspacePresenter::GetCaseId() const
{
    return m_spCase ? m_spCase->m_strCaseId : QString();
}

QVector<LabelRowData> WorkspacePresenter::GetLabelRows() const
{
    // 치은 · 미분류, 열린 턱의 치아 16개, 그 밖에 정점이 있거나 지금 고른 다른 클래스 대상 (스캔바디 …)
    QVector<LabelRowData> vecRows;
    vecRows.append(GetLabelRow(kGingiva));
    vecRows.append(GetLabelRow(kUnlabeled));
    for (int iFdi : LabelTable::GetArchOrder(m_kpSystem->GetJaw()))
    {
        vecRows.append(GetLabelRow(LabelTable::MakeCode(LabelTable::kClassTooth, iFdi)));
    }

    std::vector<int> vecOthers;
    if (m_spCase)
    {
        vecOthers = m_spCase->m_document.GetPresentCodes();
    }
    vecOthers.push_back(m_kpSystem->GetActiveLabel());
    std::sort(vecOthers.begin(), vecOthers.end());
    vecOthers.erase(std::unique(vecOthers.begin(), vecOthers.end()), vecOthers.end());
    for (int iCode : vecOthers)
    {
        if (LabelTable::GetClassOf(iCode) > LabelTable::kClassTooth)
        {
            vecRows.append(GetLabelRow(iCode));
        }
    }
    return vecRows;
}

LabelRowData WorkspacePresenter::GetLabelRow(int label) const
{
    LabelRowData row;
    row.m_iLabel = label;
    const int iClass = LabelTable::GetClassOf(label);
    const int iFdi = LabelTable::GetFdiOf(label);
    if (iClass == LabelTable::kClassTooth)
    {
        row.m_strName = LabelTable::GetFdiTypeName(iFdi);
    }
    else if (iClass > LabelTable::kClassTooth)
    {
        row.m_strName = LabelTable::GetClassName(iClass);
        if (LabelTable::IsFdi(iFdi))
        {
            row.m_strName += QStringLiteral(" · %1 자리").arg(iFdi);
        }
    }
    else
    {
        row.m_strName = LabelTable::GetName(label);
    }
    if (m_spCase)
    {
        const LabelDocument& document = m_spCase->m_document;
        row.m_iCount = document.GetCount(label);
        // 치아 상태는 FDI 자리마다 있다 (치아 · 스캔바디가 같은 자리면 같은 상태를 본다)
        const ToothInfo info = LabelTable::IsFdi(iFdi) ? document.GetTooth(iFdi) : ToothInfo();
        row.m_eState = info.m_eState;
        row.m_strNote = info.m_strNote;
        row.m_bLocked = document.IsLocked(label);
        row.m_bHidden = document.IsHidden(label);
    }
    return row;
}

QVector<HistoryRowData> WorkspacePresenter::GetHistoryRows() const
{
    QVector<HistoryRowData> vecRows;
    const std::deque<LabelEdit>& deqEdits = m_upHistory->GetEdits();
    const int iApplied = m_upHistory->GetAppliedCount();
    for (size_t i = 0; i < deqEdits.size(); ++i)
    {
        HistoryRowData row;
        row.m_strTime = deqEdits[i].m_dtTime.toString(QStringLiteral("HH:mm:ss"));
        row.m_strName = deqEdits[i].m_strName;
        row.m_iVertexCount = static_cast<int>(deqEdits[i].m_vecIndices.size());
        row.m_bApplied = static_cast<int>(i) < iApplied;
        vecRows.append(row);
    }
    return vecRows;
}

int WorkspacePresenter::GetHistoryAppliedCount() const
{
    return m_upHistory->GetAppliedCount();
}

bool WorkspacePresenter::CanUndo() const
{
    return m_spCase && m_upHistory->CanUndo();
}

bool WorkspacePresenter::CanRedo() const
{
    return m_spCase && m_upHistory->CanRedo();
}

bool WorkspacePresenter::GetLabelBounds(int label, QVector3D& v3Min, QVector3D& v3Max) const
{
    if (!m_spCase)
    {
        return false;
    }
    const Mesh& mesh = m_spCase->m_mesh;
    const std::vector<LabelValue>& vecLabels = m_spCase->m_document.GetLabels();
    bool bFound = false;
    for (size_t i = 0; i < vecLabels.size(); ++i)
    {
        if (vecLabels[i] != label)
        {
            continue;
        }
        const QVector3D v3Position = mesh.GetPosition(static_cast<int>(i));
        if (!bFound)
        {
            v3Min = v3Position;
            v3Max = v3Position;
            bFound = true;
            continue;
        }
        v3Min = QVector3D(std::min(v3Min.x(), v3Position.x()), std::min(v3Min.y(), v3Position.y()), std::min(v3Min.z(), v3Position.z()));
        v3Max = QVector3D(std::max(v3Max.x(), v3Position.x()), std::max(v3Max.y(), v3Position.y()), std::max(v3Max.z(), v3Position.z()));
    }
    return bFound;
}

QDateTime WorkspacePresenter::GetLastAutosave() const
{
    return m_dtLastAutosave;
}

void WorkspacePresenter::OnLoadFinished(bool success)
{
    const QString strPath = m_strLoadingPath;
    m_strLoadingPath.clear();
    if (!success)
    {
        emit LoadFailed(m_upLoadThread->GetError());
        return;
    }

    std::shared_ptr<CaseData> spCase = m_upLoadThread->TakeResult();
    if (!spCase)
    {
        emit LoadFailed(QStringLiteral("파일을 열지 못했다: %1").arg(strPath));
        return;
    }

    m_spCase = std::move(spCase);
    m_upHistory->Clear();
    m_llSavedStateId = m_upHistory->GetStateId();
    m_llAutosavedStateId = m_llSavedStateId;
    m_bMetaDirty = m_spCase->m_bNeedsSave;
    m_bStrokeActive = false;
    m_hashStrokeBefore.clear();
    m_dtLastAutosave = QDateTime();

    m_kpSystem->SetJaw(m_spCase->m_eJaw);
    const int iActive = m_kpSystem->GetActiveLabel();
    if (LabelTable::IsFdi(LabelTable::GetFdiOf(iActive)) && m_spCase->m_eJaw != EJaw::UNKNOWN && LabelTable::GetJaw(iActive) != m_spCase->m_eJaw)
    {
        m_kpSystem->SetActiveLabel(kGingiva);
    }

    emit CaseOpened();
    emit LabelsChanged();
    emit LabelStateChanged();
    emit HistoryChanged();
    NotifyDirty();

    if (!m_spCase->m_strLoadNotice.isEmpty())
    {
        emit Message(m_spCase->m_strLoadNotice);
    }
    if (m_spCase->m_upRecovery)
    {
        emit RecoveryAvailable(m_spCase->m_dtRecovery, m_spCase->m_iRecoveryEditCount);
    }
}

void WorkspacePresenter::OnAutosaveTimer()
{
    if (!m_spCase || !IsDirty() || m_watcherAutosave.isRunning() || m_bStrokeActive)
    {
        return;
    }
    if (m_upHistory->GetStateId() == m_llAutosavedStateId && !m_bMetaDirty)
    {
        return;
    }

    // 라벨 배열을 복사해 백그라운드에서 쓴다. 메시는 열린 뒤 바뀌지 않으므로 케이스를 붙잡아 두고 같이 읽는다
    LabelDocument snapshot = m_spCase->m_document;
    snapshot.SetUpdatedAt(QDateTime::currentDateTime());
    const std::shared_ptr<const CaseData> spCase = m_spCase;
    const int iEditCount = GetUnsavedEditCount();
    m_llAutosavedStateId = m_upHistory->GetStateId();
    m_watcherAutosave.setFuture(QtConcurrent::run([spCase, snapshot, iEditCount]() {
        QString strError;
        LabelFile::Save(spCase->m_strAutosavePath, spCase->m_mesh, spCase->m_lstSourceComments, snapshot, strError, iEditCount);
        return strError;
    }));
}

void WorkspacePresenter::OnAutosaveFinished()
{
    const QString strError = m_watcherAutosave.result();
    if (!strError.isEmpty())
    {
        emit Message(QStringLiteral("자동 저장 실패: %1").arg(strError));
        return;
    }
    m_dtLastAutosave = QDateTime::currentDateTime();
    emit AutosaveWritten(m_dtLastAutosave);
}

PaintInput WorkspacePresenter::MakePaintInput(ETool tool, bool erase, const QVector3D& camera) const
{
    const PaintSettings& settings = m_kpSystem->GetPaintSettings(tool);
    const LabelDocument& document = m_spCase->m_document;

    PaintInput input;
    input.m_kpMesh = &m_spCase->m_mesh;
    input.m_kpTopology = m_spCase->m_topology.IsEmpty() ? nullptr : &m_spCase->m_topology;
    input.m_kpBvh = m_spCase->m_bvh.IsEmpty() ? nullptr : &m_spCase->m_bvh;
    input.m_kpCurvature = &m_spCase->m_vecCurvature;
    input.m_kpLabels = &document.GetLabels();
    input.m_kpLockedLabels = &document.GetLockedLabels();
    input.m_kpHiddenLabels = &document.GetHiddenLabels();
    input.m_target = erase ? kUnlabeled : static_cast<LabelValue>(m_kpSystem->GetActiveLabel());
    input.m_eOverwrite = erase ? EOverwriteRule::ALL : settings.m_eOverwrite;
    input.m_eReach = settings.m_eReach;
    input.m_v3Camera = camera;
    input.m_fRadius = settings.m_fRadius;
    input.m_bCurvatureStop = settings.m_bCurvatureStop;
    input.m_fCurvatureThreshold = Curvature::GetStopThreshold(settings.m_fSensitivity);
    return input;
}

void WorkspacePresenter::ApplyEdit(const std::vector<std::uint32_t>& vecIndices, LabelValue target, const QString& name)
{
    LabelDocument& document = m_spCase->m_document;
    LabelEdit edit;
    edit.m_strName = name;
    edit.m_vecIndices.reserve(vecIndices.size());
    for (std::uint32_t uVertex : vecIndices)
    {
        const LabelValue before = document.GetLabel(static_cast<int>(uVertex));
        if (before != target)
        {
            edit.m_vecIndices.push_back(uVertex);
            edit.m_vecBefore.push_back(before);
        }
    }
    if (edit.m_vecIndices.empty())
    {
        return;
    }
    edit.m_vecAfter.assign(edit.m_vecIndices.size(), target);
    document.Assign(edit.m_vecIndices, target);
    m_upHistory->Push(std::move(edit));
    emit LabelsChanged();
    emit HistoryChanged();
    NotifyDirty();
}

QString WorkspacePresenter::MakeEditName(const QString& toolName, bool erase) const
{
    if (erase)
    {
        return toolName == QLatin1String("브러시") ? QStringLiteral("지우개") : QStringLiteral("%1 지우기").arg(toolName);
    }
    return QStringLiteral("%1 · %2").arg(toolName, GetLabelText(m_kpSystem->GetActiveLabel()));
}

void WorkspacePresenter::NotifyDirty()
{
    const bool bDirty = IsDirty();
    if (bDirty != m_bLastDirty)
    {
        m_bLastDirty = bDirty;
        emit DirtyChanged(bDirty);
    }
}

int WorkspacePresenter::GetUnsavedEditCount() const
{
    // 저장 시점 이후 적용된 편집 수 (저장 시점이 기록에서 밀려났으면 지금 적용된 전부)
    const std::deque<LabelEdit>& deqEdits = m_upHistory->GetEdits();
    const int iApplied = m_upHistory->GetAppliedCount();
    int iCount = 0;
    for (int i = iApplied - 1; i >= 0; --i)
    {
        if (deqEdits[static_cast<size_t>(i)].m_llId == m_llSavedStateId)
        {
            break;
        }
        ++iCount;
    }
    return iCount;
}
