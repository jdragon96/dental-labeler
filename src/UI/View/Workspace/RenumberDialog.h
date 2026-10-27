#pragma once

#include "Document/LabelRenumber.h"

#include <QDialog>

#include <memory>

namespace Ui
{
class RenumberDialog;
}

class SystemStatus;
class WorkspacePresenter;

// 번호 바꾸기 (DL-21): 치식 순서로 여러 자리를 밀거나 라벨 하나를 바꾼다. 미리 보기를 보고 적용한다
class RenumberDialog : public QDialog
{
    Q_OBJECT

public:
    RenumberDialog(SystemStatus* kpSystem, WorkspacePresenter* kpPresenter, QWidget* parent = nullptr);
    ~RenumberDialog() override;

public:
    // 하나 바꾸기로 시작하고 바꿀 라벨을 고른다
    void StartWithSingle(int fromCode);

private:
    void FillCombos();
    void UpdateDirectionTexts();
    void UpdatePreview();
    RenumberPlan MakePlan() const;
    void OnAccepted();

private:
    std::unique_ptr<Ui::RenumberDialog> ui;
    SystemStatus* m_kpSystem;
    WorkspacePresenter* m_kpPresenter;
};
