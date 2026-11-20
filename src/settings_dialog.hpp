#pragma once

#include <QDockWidget>
#include <QJsonArray>
#include <QTimer>
#include <QString>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;

class DeltaForceClipper;

class SettingsDialog final : public QDockWidget {
    Q_OBJECT
public:
    explicit SettingsDialog(DeltaForceClipper *core, QWidget *parent = nullptr);

    void refreshStatus();
    void refreshHistory();

private slots:
    void saveSettings();
    void saveTestClip();
    void startReplayBuffer();
    void stopReplayBuffer();

private:
    void buildUi();
    void clearUi();
    void addTriggerMessageRow(const QString &phrase, const QString &eventType, const QString &tag);
    void selectOcrRegion();
    void updateOcrRegionStatus();

    DeltaForceClipper *core_ = nullptr;
    QWidget *contentWidget_ = nullptr;
    QSpinBox *bufferSeconds_ = nullptr;
    QLineEdit *clipFolder_ = nullptr;
    QLineEdit *eventsFile_ = nullptr;
    QComboBox *uiLanguage_ = nullptr;
    QSpinBox *preTime_ = nullptr;
    QSpinBox *postTime_ = nullptr;
    QLineEdit *playerNick_ = nullptr;
    QCheckBox *autoStart_ = nullptr;
    QCheckBox *detectorEnabled_ = nullptr;
    QLabel *obsStatus_ = nullptr;
    QLabel *replayStatus_ = nullptr;
    QLabel *detectorStatus_ = nullptr;
    QLabel *lastClip_ = nullptr;
    QLabel *historyStatus_ = nullptr;
    QLabel *ocrRegionStatus_ = nullptr;
    QTableWidget *historyTable_ = nullptr;
    QTableWidget *triggerMessagesTable_ = nullptr;
    QJsonArray ocrRegions_;
    QTimer refreshTimer_;
    QTimer historyRefreshTimer_;
};
