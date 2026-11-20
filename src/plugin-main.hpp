#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

class SettingsDialog;
class QJsonObject;

class DeltaForceClipper final : public QObject {
    Q_OBJECT
public:
    DeltaForceClipper();
    ~DeltaForceClipper() override;

    bool obsReady() const { return obsReady_; }
    bool replayBufferActive() const;
    int bufferSeconds() const { return bufferSeconds_; }
    const QString &clipFolder() const { return clipFolder_; }
    bool autoStartReplayBuffer() const { return autoStartReplayBuffer_; }
    QString lastClipPath() const { return lastClipPath_; }
    QString lastEvent() const { return lastEvent_; }
    bool detectorEnabled() const { return detectorEnabled_; }
    const QString &eventsFile() const { return eventsFile_; }
    QString ocrLang() const { return ocrLang_; }
    int preTime() const { return preTime_; }
    int postTime() const { return postTime_; }
    QString playerNick() const { return playerNick_; }
    QString uiLanguage() const { return uiLanguage_; }
    bool detectorRunning() const;
    const QJsonArray &triggerMessages() const { return triggerMessages_; }
    const QJsonArray &ocrRegions() const { return ocrRegions_; }
    static QJsonArray defaultOcrRegions();

    void setBufferSeconds(int seconds);
    void setClipFolder(const QString &folder);
    void setAutoStartReplayBuffer(bool enabled);
    void setDetectorEnabled(bool enabled);
    void setEventsFile(const QString &path);
    void setOcrLang(const QString &lang);
    void setPreTime(int ms);
    void setPostTime(int ms);
    void setPlayerNick(const QString &nick);
    void setUiLanguage(const QString &language);
    void setTriggerMessages(const QJsonArray &messages);
    bool setOcrRegions(const QJsonArray &regions, QString *error = nullptr);

    void saveConfig();
    void applyDetectorSettings();
    void applyReplayBufferDuration();
    void startReplayBuffer();
    void stopReplayBuffer();
    void saveReplayBufferAs(const QString &eventType);
    void handleReplayBufferSaved();

    void showSettings();

private slots:
    void pollDetectorEvents();

private:
    void loadConfig();
    void ensureSettingsDock();
    void handleDetectorEvent(const QJsonObject &event);
    QString sanitizeEventType(const QString &type);
    void renameAndRegisterLastReplay(const QString &eventType);
    void registerClipFile(const QString &filePath,
                          const QString &eventType,
                          const QJsonArray &eventRecords,
                          bool boundariesTrimmed,
                          qint64 requestedWindowMs,
                          int bufferDurationSeconds,
                          const QString &boundaryStatus);
    void startDetector();
    void stopDetector();
    bool writeDetectorConfig();
    void saveScheduledEvent();
    QString defaultClipFolder() const;
    static QString defaultEventsFile();
    static QString configDirectory();

    bool obsReady_ = false;
    bool autoStartReplayBuffer_ = true;
    int bufferSeconds_ = 30;
    QString clipFolder_;
    QString pendingEventType_;
    bool pendingSave_ = false;
    QString lastClipPath_;
    SettingsDialog *dialog_ = nullptr;

    QTimer pollTimer_;
    QTimer postEventTimer_;
    QProcess detectorProcess_;
    bool detectorEnabled_ = true;
    bool detectorStopping_ = false;
    QString eventsFile_;
    qint64 eventsOffset_ = 0;
    QString lastEvent_;
    QDateTime lastSaveAttempt_;
    QMetaObject::Connection replaySavedConnection_;
    QString ocrLang_ = QStringLiteral("en");
    int preTime_ = 1000;
    int postTime_ = 3000;
    QString playerNick_ = QStringLiteral("Absolution");
    QString uiLanguage_ = QStringLiteral("en");
    QString scheduledEventType_;
    QJsonArray scheduledEventRecords_;
    QJsonArray pendingEventRecords_;
    QJsonArray triggerMessages_;
    QJsonArray ocrRegions_;
};

DeltaForceClipper *delta_force_clipper_instance();
