#include "UI/View/Workspace/LabelMenu.h"

#include "Core/LabelTable.h"
#include "Core/SystemStatus.h"
#include "Presenter/Workspace/WorkspacePresenter.h"
#include "UI/View/Workspace/RenumberDialog.h"

#include <QColorDialog>
#include <QLabel>
#include <QMenu>
#include <QPixmap>
#include <QWidgetAction>

#include <vector>

namespace
{
constexpr int kQuadrantSize = 8;

QIcon MakeSwatch(int code)
{
    QPixmap pixmap(12, 12);
    pixmap.fill(LabelTable::GetColor(code));
    return QIcon(pixmap);
}

LabelTable::EFdiUse GetFdiUse(int code)
{
    const LabelTable::ClassInfo* kpInfo = LabelTable::FindClass(LabelTable::GetClassOf(code));
    return kpInfo != nullptr ? kpInfo->m_eFdiUse : LabelTable::EFdiUse::NONE;
}
}

namespace LabelMenu
{
void AddTitle(QMenu& menu, int label, WorkspacePresenter* kpPresenter)
{
    QString strText = QStringLiteral("%1 · %2").arg(LabelTable::GetShortText(label), LabelTable::GetName(label));
    const int iCount = kpPresenter->GetLabelRow(label).m_iCount;
    if (iCount > 0)
    {
        strText += QStringLiteral(" · 정점 %L1").arg(iCount);
    }
    // QMenu::addSection 은 Fusion 에서 글자를 그리지 않아 라벨을 넣는다
    auto* pTitle = new QLabel(strText, &menu);
    pTitle->setObjectName(QStringLiteral("menuTitle"));
    auto* pAction = new QWidgetAction(&menu);
    pAction->setDefaultWidget(pTitle);
    menu.addAction(pAction);
    menu.addSeparator();
}

void AddRenumberActions(QMenu& menu, int label, SystemStatus* kpSystem, WorkspacePresenter* kpPresenter, QWidget* pParent)
{
    if (!kpPresenter->HasCase() || GetFdiUse(label) == LabelTable::EFdiUse::NONE || kpPresenter->GetLabelRow(label).m_iCount <= 0)
    {
        return;
    }

    const int iClass = LabelTable::GetClassOf(label);
    const int iFdi = LabelTable::GetFdiOf(label);
    EJaw eJaw = kpSystem->GetJaw();
    if (eJaw == EJaw::UNKNOWN && LabelTable::IsFdi(iFdi))
    {
        eJaw = LabelTable::GetJawOfFdi(iFdi);
    }

    QMenu* pSub = menu.addMenu(MakeSwatch(label), QStringLiteral("%1 번호 바꾸기").arg(LabelTable::GetShortText(label)));
    auto addTarget = [&](int target) {
        // 치은은 번호가 없으므로 맞바꾸지 않고 합친다
        const bool bSwap = GetFdiUse(target) != LabelTable::EFdiUse::NONE;
        QString strText = QStringLiteral("%1 · %2").arg(LabelTable::GetShortText(target), LabelTable::GetName(target));
        if (target != label && kpPresenter->GetLabelRow(target).m_iCount > 0)
        {
            // 탭 뒤 글자는 단축키 칸에 오른쪽 정렬로 보인다
            strText += bSwap ? QStringLiteral("\t⇄ 맞바꿈") : QStringLiteral("\t+ 합침");
        }
        QAction* pAction = pSub->addAction(MakeSwatch(target), strText, pParent, [kpPresenter, label, target, bSwap] {
            kpPresenter->ApplyRenumber(kpPresenter->PlanSingleRenumber(label, target, bSwap));
        });
        if (target == label)
        {
            pAction->setCheckable(true);
            pAction->setChecked(true);
            pAction->setEnabled(false);
        }
    };

    // 같은 클래스의 자리들, 치식 순서. 사분면마다 구분선
    const std::vector<int>& vecOrder = LabelTable::GetArchOrder(eJaw);
    for (size_t i = 0; i < vecOrder.size(); ++i)
    {
        if (i > 0 && i % kQuadrantSize == 0)
        {
            pSub->addSeparator();
        }
        addTarget(LabelTable::MakeCode(iClass, vecOrder[i]));
    }
    if (GetFdiUse(label) == LabelTable::EFdiUse::ALLOWED)
    {
        addTarget(LabelTable::MakeCode(iClass, 0));
    }

    // 같은 자리의 다른 클래스(치아 ↔ 스캔바디)와 치은
    pSub->addSeparator();
    for (const LabelTable::ClassInfo& info : LabelTable::GetClasses())
    {
        if (info.m_iValue == iClass || info.m_eFdiUse == LabelTable::EFdiUse::NONE)
        {
            continue;
        }
        if (LabelTable::IsFdi(iFdi))
        {
            addTarget(LabelTable::MakeCode(info.m_iValue, iFdi));
        }
        else if (info.m_eFdiUse == LabelTable::EFdiUse::ALLOWED)
        {
            addTarget(LabelTable::MakeCode(info.m_iValue, 0));
        }
    }
    addTarget(kGingiva);

    pSub->addSeparator();
    pSub->addAction(QStringLiteral("여러 자리 밀기 · 자세히…"), pParent, [kpSystem, kpPresenter, pParent, label] {
        RenumberDialog dialog(kpSystem, kpPresenter, pParent);
        dialog.StartWithSingle(label);
        dialog.exec();
    });
}

void AddColorActions(QMenu& menu, int label, WorkspacePresenter* kpPresenter, QWidget* pParent)
{
    menu.addAction(MakeSwatch(label), QStringLiteral("%1 색 바꾸기…").arg(LabelTable::GetShortText(label)), pParent,
                   [kpPresenter, pParent, label] { PickColor(label, kpPresenter, pParent); });
    QAction* pReset = menu.addAction(QStringLiteral("기본 색으로"), pParent, [kpPresenter, label] { kpPresenter->SetLabelColor(label, QColor()); });
    pReset->setEnabled(LabelTable::HasColorOverride(label));
}

void PickColor(int label, WorkspacePresenter* kpPresenter, QWidget* pParent)
{
    if (label == kUnlabeled || !LabelTable::IsValidCode(label))
    {
        return;
    }
    const QColor color = QColorDialog::getColor(LabelTable::GetColor(label), pParent,
                                                QStringLiteral("%1 · %2 색").arg(LabelTable::GetShortText(label), LabelTable::GetName(label)));
    if (color.isValid())
    {
        kpPresenter->SetLabelColor(label, color);
    }
}
}
