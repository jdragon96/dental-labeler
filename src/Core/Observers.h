#pragma once

#include "Core/Types.h"

#include <QObject>
#include <QString>

// 위젯끼리 주고받는 알림 (docs/Qt.md 2.1.1)
class Observers : public QObject
{
    Q_OBJECT

public:
    explicit Observers(QObject* parent = nullptr);

signals:
    void ActiveLabelChanged(int label);
    void ActiveToolChanged(ETool tool);
    void DisplayModeChanged(EDisplayMode mode);
    void LabelOpacityChanged(float opacity);
    void PaintSettingsChanged(ETool tool);
    void JawChanged(EJaw jaw);
    // 치아 번호를 고를 때 붙일 클래스 (치아 · 스캔바디 …)
    void ObjectClassChanged(int classValue);

    // 숫자 두 개 라벨 입력 중인 숫자 ("3"). 빈 문자열이면 입력 끝
    void PendingLabelDigitsChanged(const QString& digits);

    void ViewPresetRequested(EViewPreset preset);
    void FitViewRequested();
    void FocusLabelRequested(int label);
};
