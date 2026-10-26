#pragma once

#include "Core/Types.h"
#include "Interface/IPaintStrategy.h"
#include "Presenter/Workspace/WorkspaceTypes.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QMatrix4x4>
#include <QObject>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class Observers;
class SystemStatus;
class LabelPaint;
class EditHistory;
class MeshLoadThread;
struct CaseData;

// 라벨링 작업 화면(DL-20)의 로직. 위젯을 모른다 (docs/Qt.md 3.2)
class WorkspacePresenter : public QObject
{
    Q_OBJECT

public:
    WorkspacePresenter(Observers* kpObservers, SystemStatus* kpSystem, QObject* parent = nullptr);
    ~WorkspacePresenter() override;

public:
    void OpenFile(const QString& path);
    void CancelLoading();
    bool Save();
    void ResolveRecovery(bool restore);

    void Undo();
    void Redo();
    void JumpToHistory(int appliedCount);

    // pixelAngle: 화면 1픽셀이 차지하는 시야각 (포인트 클라우드 고르기 허용 범위)
    PickResult Pick(const QVector3D& origin, const QVector3D& dir, float pixelAngle) const;
    void BeginStroke(bool erase);
    void PaintDab(const PickResult& pick, const QVector3D& camera);
    void EndStroke();
    void ApplyFill(const PickResult& pick, const QVector3D& camera, bool erase);
    void ApplyLasso(const std::vector<QPointF>& vecPolygon, const QMatrix4x4& viewProjection, const QSize& viewport,
                    const QVector3D& camera, bool erase);

    // 라벨 코드로 고른다 (목록 · 최근 라벨 · 스포이드)
    void SelectLabel(int label);
    // FDI 자리로 고른다 (숫자 두 개 · 치식도). 지금 클래스(치아 · 스캔바디 …)를 붙인다. 0 은 치은
    void SelectFdi(int fdi);
    // 클래스를 바꾼다. 치은처럼 자리가 없는 클래스는 바로 그 라벨을 고른다
    void SelectClass(int classValue);
    void SelectAdjacentTooth(int step);
    void PickLabelFromVertex(int vertex);

    void SetToothState(int fdi, EToothState state);
    void SetToothNote(int fdi, const QString& note);
    void ClearLabelVertices(int label);
    void ToggleLocked(int label);
    void ToggleHidden(int label);
    void ShowAllLabels();
    void IsolateLabel(int label);

public:
    bool HasCase() const;
    bool IsLoading() const;
    bool IsDirty() const;
    const CaseData* GetCase() const;
    QString GetCaseId() const;
    QVector<LabelRowData> GetLabelRows() const;
    LabelRowData GetLabelRow(int label) const;
    QVector<HistoryRowData> GetHistoryRows() const;
    int GetHistoryAppliedCount() const;
    bool CanUndo() const;
    bool CanRedo() const;
    bool GetLabelBounds(int label, QVector3D& v3Min, QVector3D& v3Max) const;
    QDateTime GetLastAutosave() const;

signals:
    void LoadStarted(const QString& fileName);
    void LoadProgress(const QString& step, int percent);
    void LoadFailed(const QString& message);
    void CaseOpened();
    void RecoveryAvailable(const QDateTime& time, int editCount);
    void LabelsChanged();
    void LabelStateChanged();
    void HistoryChanged();
    void DirtyChanged(bool dirty);
    void Saved(const QString& path);
    void SaveFailed(const QString& message);
    void AutosaveWritten(const QDateTime& time);
    void Message(const QString& text);

private:
    void OnLoadFinished(bool success);
    void OnAutosaveTimer();
    void OnAutosaveFinished();
    PaintInput MakePaintInput(ETool tool, bool erase, const QVector3D& camera) const;
    void ApplyEdit(const std::vector<std::uint32_t>& vecIndices, LabelValue target, const QString& name);
    QString MakeEditName(const QString& toolName, bool erase) const;
    void NotifyDirty();
    int GetUnsavedEditCount() const;

private:
    Observers* m_kpObservers;
    SystemStatus* m_kpSystem;
    std::unique_ptr<MeshLoadThread> m_upLoadThread;
    std::unique_ptr<LabelPaint> m_upPaint;
    std::unique_ptr<EditHistory> m_upHistory;
    std::shared_ptr<CaseData> m_spCase;
    QTimer m_timerAutosave;
    QFutureWatcher<QString> m_watcherAutosave;
    long long m_llSavedStateId;
    long long m_llAutosavedStateId;
    bool m_bMetaDirty;
    bool m_bLastDirty;
    bool m_bStrokeActive;
    bool m_bStrokeErase;
    std::unordered_map<std::uint32_t, LabelValue> m_hashStrokeBefore;
    QDateTime m_dtLastAutosave;
    QString m_strLoadingPath;
};
