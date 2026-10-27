#pragma once

class QMenu;
class QWidget;
class SystemStatus;
class WorkspacePresenter;

// 라벨 오른쪽 클릭 메뉴의 공용 항목 (라벨 목록 · 치식도 · 3D 뷰포트)
namespace LabelMenu
{
// 맨 위 제목 줄 "46 · 하악 우측 제1대구치 · 정점 6,709" 와 구분선
void AddTitle(QMenu& menu, int label, WorkspacePresenter* kpPresenter);
// "번호 바꾸기" 하위 메뉴: 같은 클래스의 이 턱 자리, 같은 자리의 다른 클래스, 치은, 자세히(대화상자).
// 고른 번호에 이미 정점이 있으면 맞바꾸고(치은은 합친다) 기록 한 줄로 남긴다. 정점이 없거나 번호가 없는 라벨이면 넣지 않는다
void AddRenumberActions(QMenu& menu, int label, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter, QWidget* pParent);
// "색 바꾸기…" · "기본 색으로"
void AddColorActions(QMenu& menu, int label, WorkspacePresenter* kpPresenter, QWidget* pParent);
// 색 고르기 대화상자를 띄워 라벨 색을 바꾼다
void PickColor(int label, WorkspacePresenter* kpPresenter, QWidget* pParent);
}
