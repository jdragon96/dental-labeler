#include "UI/View/Workspace/RenumberDialog.h"

#include "ui_RenumberDialog.h"

#include "Core/LabelTable.h"
#include "Core/SystemStatus.h"
#include "Presenter/Workspace/WorkspacePresenter.h"

#include <QPushButton>

#include <algorithm>
#include <vector>

namespace
{
QString MakeItemText(int code, int count)
{
    QString strText = QStringLiteral("%1 · %2").arg(LabelTable::GetShortText(code), LabelTable::GetName(code));
    if (count > 0)
    {
        strText += QStringLiteral("  (정점 %L1)").arg(count);
    }
    return strText;
}
}

RenumberDialog::RenumberDialog(SystemStatus* kpSystem, WorkspacePresenter* kpPresenter, QWidget* parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::RenumberDialog>())
    , m_kpSystem(kpSystem)
    , m_kpPresenter(kpPresenter)
{
    ui->setupUi(this);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(QStringLiteral("적용"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("취소"));
    FillCombos();

    auto updateAll = [this] {
        UpdateDirectionTexts();
        UpdatePreview();
    };
    connect(ui->radioShift, &QRadioButton::toggled, this, updateAll);
    connect(ui->comboFirst, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateAll);
    connect(ui->comboLast, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateAll);
    connect(ui->comboDirection, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateAll);
    connect(ui->spinSteps, QOverload<int>::of(&QSpinBox::valueChanged), this, updateAll);
    connect(ui->comboFrom, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateAll);
    connect(ui->comboTo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateAll);
    connect(ui->checkSwap, &QCheckBox::toggled, this, updateAll);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RenumberDialog::OnAccepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    updateAll();
}

RenumberDialog::~RenumberDialog() = default;

void RenumberDialog::StartWithSingle(int fromCode)
{
    const int iIndex = ui->comboFrom->findData(fromCode);
    if (iIndex < 0)
    {
        return;
    }
    ui->comboFrom->setCurrentIndex(iIndex);
    ui->radioSingle->setChecked(true);

    // 새 라벨은 치식 순서 다음 자리에서 시작한다 (한 칸 밀린 경우가 가장 흔하다)
    const int iFdi = LabelTable::GetFdiOf(fromCode);
    const std::vector<int>& vecOrder = LabelTable::GetArchOrder(m_kpSystem->GetJaw());
    const auto it = std::find(vecOrder.begin(), vecOrder.end(), iFdi);
    if (it != vecOrder.end())
    {
        const int iNextFdi = it + 1 != vecOrder.end() ? *(it + 1) : *(it - 1);
        const int iTo = ui->comboTo->findData(LabelTable::MakeCode(LabelTable::GetClassOf(fromCode), iNextFdi));
        if (iTo >= 0)
        {
            ui->comboTo->setCurrentIndex(iTo);
        }
    }
    ui->comboTo->setFocus();
}

void RenumberDialog::FillCombos()
{
    const std::vector<int>& vecOrder = LabelTable::GetArchOrder(m_kpSystem->GetJaw());
    auto countAt = [this](int fdi) {
        int iCount = 0;
        for (const LabelTable::ClassInfo& info : LabelTable::GetClasses())
        {
            if (info.m_eFdiUse != LabelTable::EFdiUse::NONE)
            {
                iCount += m_kpPresenter->GetLabelRow(LabelTable::MakeCode(info.m_iValue, fdi)).m_iCount;
            }
        }
        return iCount;
    };

    // 밀기: 자리마다. 정점이 있는 처음 · 끝 자리를 미리 고른다
    int iFirstPainted = -1;
    int iLastPainted = -1;
    for (size_t i = 0; i < vecOrder.size(); ++i)
    {
        const int iFdi = vecOrder[i];
        const int iCount = countAt(iFdi);
        QString strText = QStringLiteral("%1 · %2").arg(iFdi).arg(LabelTable::GetFdiName(iFdi));
        if (iCount > 0)
        {
            strText += QStringLiteral("  (정점 %L1)").arg(iCount);
            iLastPainted = static_cast<int>(i);
            if (iFirstPainted < 0)
            {
                iFirstPainted = static_cast<int>(i);
            }
        }
        ui->comboFirst->addItem(strText, iFdi);
        ui->comboLast->addItem(strText, iFdi);
    }
    ui->comboFirst->setCurrentIndex(std::max(0, iFirstPainted));
    ui->comboLast->setCurrentIndex(std::max(0, iLastPainted));
    ui->comboDirection->addItem(QString(), 1);
    ui->comboDirection->addItem(QString(), -1);

    // 하나 바꾸기: 바꿀 라벨은 정점이 있는 것, 새 라벨은 치은 · 이 턱의 치아 · 스캔바디 자리
    const int iActive = m_kpSystem->GetActiveLabel();
    for (const LabelRowData& row : m_kpPresenter->GetLabelRows())
    {
        if (row.m_iLabel != kUnlabeled && row.m_iCount > 0)
        {
            ui->comboFrom->addItem(MakeItemText(row.m_iLabel, row.m_iCount), row.m_iLabel);
            if (row.m_iLabel == iActive)
            {
                ui->comboFrom->setCurrentIndex(ui->comboFrom->count() - 1);
            }
        }
    }
    ui->comboTo->addItem(MakeItemText(kGingiva, 0), static_cast<int>(kGingiva));
    for (const LabelTable::ClassInfo& info : LabelTable::GetClasses())
    {
        if (info.m_eFdiUse == LabelTable::EFdiUse::NONE)
        {
            continue;
        }
        for (int iFdi : vecOrder)
        {
            const int iCode = LabelTable::MakeCode(info.m_iValue, iFdi);
            ui->comboTo->addItem(MakeItemText(iCode, 0), iCode);
        }
        if (info.m_eFdiUse == LabelTable::EFdiUse::ALLOWED)
        {
            const int iCode = LabelTable::MakeCode(info.m_iValue, 0);
            ui->comboTo->addItem(MakeItemText(iCode, 0), iCode);
        }
    }
    if (ui->comboFrom->count() == 0)
    {
        ui->radioSingle->setEnabled(false);
    }
}

void RenumberDialog::UpdateDirectionTexts()
{
    const std::vector<int>& vecOrder = LabelTable::GetArchOrder(m_kpSystem->GetJaw());
    const int iFdi = ui->comboFirst->currentData().toInt();
    const int iIndex = static_cast<int>(std::find(vecOrder.begin(), vecOrder.end(), iFdi) - vecOrder.begin());
    const int iSteps = ui->spinSteps->value();
    auto targetText = [&](int index) {
        return index >= 0 && index < static_cast<int>(vecOrder.size()) ? QString::number(vecOrder[static_cast<size_t>(index)])
                                                                        : QStringLiteral("자리 없음");
    };
    // 방향은 예시로 보인다: 처음 자리가 어디로 가는지
    ui->comboDirection->setItemText(0, QStringLiteral("%1 → %2 쪽").arg(iFdi).arg(targetText(iIndex + iSteps)));
    ui->comboDirection->setItemText(1, QStringLiteral("%1 → %2 쪽").arg(iFdi).arg(targetText(iIndex - iSteps)));
}

void RenumberDialog::UpdatePreview()
{
    const bool bShift = ui->radioShift->isChecked();
    ui->pageShift->setEnabled(bShift);
    ui->pageSingle->setEnabled(!bShift);

    const RenumberPlan plan = MakePlan();
    QString strHtml;
    if (!plan.m_strError.isEmpty())
    {
        strHtml = QStringLiteral("<span style='color:#FF8A8A'>%1</span>").arg(plan.m_strError.toHtmlEscaped());
    }
    else
    {
        for (const QString& warning : plan.m_lstWarnings)
        {
            strHtml += QStringLiteral("<span style='color:#F2C14E'>%1</span><br>").arg(warning.toHtmlEscaped());
        }
        strHtml += QStringLiteral("<b>바뀌는 라벨 %1개</b><br>").arg(static_cast<int>(plan.m_mapCodes.size()));
        for (const auto& it : plan.GetOrderedPairs())
        {
            strHtml += QStringLiteral("%1 → %2 · 정점 %L3<br>")
                           .arg(LabelTable::GetShortText(it.first), LabelTable::GetShortText(it.second))
                           .arg(m_kpPresenter->GetLabelRow(it.first).m_iCount);
        }
        strHtml += QStringLiteral("<span style='color:#8B96A1'>치아 상태 · 메모는 자리에 남는다. 적용 뒤 Ctrl+Z 로 한 번에 되돌린다.</span>");
    }
    ui->textPreview->setHtml(strHtml);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(plan.IsValid());
}

RenumberPlan RenumberDialog::MakePlan() const
{
    if (ui->radioShift->isChecked())
    {
        const int iSteps = ui->spinSteps->value() * ui->comboDirection->currentData().toInt();
        return m_kpPresenter->PlanArchShift(ui->comboFirst->currentData().toInt(), ui->comboLast->currentData().toInt(), iSteps);
    }
    if (ui->comboFrom->count() == 0)
    {
        RenumberPlan plan;
        plan.m_strError = QStringLiteral("칠한 라벨이 없다.");
        return plan;
    }
    return m_kpPresenter->PlanSingleRenumber(ui->comboFrom->currentData().toInt(), ui->comboTo->currentData().toInt(),
                                             ui->checkSwap->isChecked());
}

void RenumberDialog::OnAccepted()
{
    if (m_kpPresenter->ApplyRenumber(MakePlan()))
    {
        accept();
        return;
    }
    ui->textPreview->setHtml(QStringLiteral("<span style='color:#FF8A8A'>적용하지 못했다. 미리 보기를 확인한다.</span>"));
}
