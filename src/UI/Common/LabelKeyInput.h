#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

// 숫자 두 개로 라벨 고르기 (docs/ScreenPlan.html 3.3). 3 다음 0.8초 안에 6 → 36. 0 은 바로 치은.
// 앱 전체 이벤트 필터로 동작하고, 글자 입력칸에 포커스가 있으면 끼어들지 않는다
class LabelKeyInput : public QObject
{
    Q_OBJECT

public:
    explicit LabelKeyInput(QObject* parent = nullptr);

public:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void LabelEntered(int label);
    void PendingChanged(const QString& digits);

private:
    void Clear();

private:
    QString m_strPending;
    QTimer m_timerTimeout;
};
