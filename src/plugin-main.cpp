#include "plugin-main.hpp"
#include "clip_library.hpp"
#include "clip_window.hpp"
#include "event_coalescer.hpp"
#include "settings_dialog.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>
#include <QUuid>
#include <QtMath>
#include <cstdarg>
#include <cstdio>
#include <memory>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

void obs_log(int log_level, const char *format, ...)
{
    char message[2048];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    blog(log_level, "[%s] %s", PLUGIN_NAME, message);
}

static DeltaForceClipper *g_instance = nullptr;

// The plugin and the external killfeed detector share this folder. It is
// pinned to %APPDATA%\DeltaForceClipper so both processes agree on it
// regardless of where OBS host puts QStandardPaths::AppDataLocation.
static QString sharedDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return QDir(QDir(base).filePath(QStringLiteral("DeltaForceClipper"))).absolutePath();
}

static QString configPath()
{
    return QDir(sharedDirectory()).filePath(QStringLiteral("config.json"));
}

static QJsonArray defaultTriggerMessages()
{
    return QJsonArray{
        QJsonObject{{"phrase", "double kill"}, {"event", "double_kill"}},
        QJsonObject{{"phrase", "triple kill"}, {"event", "triple_kill"}},
        QJsonObject{{"phrase", "multikill"}, {"event", "multikill"}},
        QJsonObject{{"phrase", "quadra kill"}, {"event", "quadra_kill"}},
        QJsonObject{{"phrase", "stinger kill"}, {"event", "kill"}, {"tag", "launcher"}},
        QJsonObject{{"phrase", "penta kill"}, {"event", "penta_kill"}},
        QJsonObject{{"phrase", "guided missile kill"}, {"event", "kill"}, {"tag", "missile"}}
    };
}

static QJsonArray previousDefaultTriggerMessages()
{
    return QJsonArray{
        QJsonObject{{"phrase", "double kill"}, {"event", "double_kill"}},
        QJsonObject{{"phrase", "triple kill"}, {"event", "triple_kill"}},
        QJsonObject{{"phrase", "multikill"}, {"event", "multikill"}},
        QJsonObject{{"phrase", "quadra kill"}, {"event", "multikill"}},
        QJsonObject{{"phrase", "stinger kill"}, {"event", "kill"}},
        QJsonObject{{"phrase", "penta kill"}, {"event", "multikill"}},
        QJsonObject{{"phrase", "guided missile kill"}, {"event", "kill"}}
    };
}

static QJsonArray defaultDetectorOcrRegions()
{
    return QJsonArray{
        QJsonObject{{"x", 0.39}, {"y", 0.66}, {"width", 0.25}, {"height", 0.20}},
        QJsonObject{{"x", 0.80}, {"y", 0.05}, {"width", 0.20}, {"height", 0.22}}
    };
}

static bool isValidOcrRegion(const QJsonValue &value)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject region = value.toObject();
    const QStringList keys{QStringLiteral("x"), QStringLiteral("y"),
                           QStringLiteral("width"), QStringLiteral("height")};
    for (const QString &key : keys) {
        if (!region.value(key).isDouble()) {
            return false;
        }
    }
    const double x = region.value(QStringLiteral("x")).toDouble();
    const double y = region.value(QStringLiteral("y")).toDouble();
    const double width = region.value(QStringLiteral("width")).toDouble();
    const double height = region.value(QStringLiteral("height")).toDouble();
    return qIsFinite(x) && qIsFinite(y) && qIsFinite(width) && qIsFinite(height) &&
           x >= 0.0 && y >= 0.0 && width > 0.0 && height > 0.0 &&
           x + width <= 1.0 && y + height <= 1.0;
}

static qint64 requestedClipWindowMs(const QJsonArray &eventRecords, int preEventMs, int postEventMs)
{
    QDateTime firstEvent;
    QDateTime lastEvent;
    for (const QJsonValue &value : eventRecords) {
        const QDateTime receivedAt = QDateTime::fromString(
            value.toObject().value(QStringLiteral("received_at_utc")).toString(), Qt::ISODateWithMs);
        if (!receivedAt.isValid()) {
            return -1;
        }
        if (!firstEvent.isValid() || receivedAt < firstEvent) {
            firstEvent = receivedAt;
        }
        if (!lastEvent.isValid() || receivedAt > lastEvent) {
            lastEvent = receivedAt;
        }
    }
    if (!firstEvent.isValid() || !lastEvent.isValid()) {
        return -1;
    }
    return deltaforceclipper::requestedClipWindowMs(
        firstEvent.toMSecsSinceEpoch(), lastEvent.toMSecsSinceEpoch(),
        qMax(0, preEventMs), qMax(0, postEventMs));
}

QJsonArray DeltaForceClipper::defaultOcrRegions()
{
    return defaultDetectorOcrRegions();
}

static QString defaultTriggerTag(const QString &phrase)
{
    const QString normalized = phrase.trimmed().toLower();
    if (normalized == QStringLiteral("guided missile kill")) {
        return QStringLiteral("missile");
    }
    if (normalized == QStringLiteral("stinger kill")) {
        return QStringLiteral("launcher");
    }
    return QString();
}

static void tool_menu_callback(void *)
{
    if (g_instance) g_instance->showSettings();
}

static void frontend_event_callback(enum obs_frontend_event event, void *private_data)
{
    auto *core = static_cast<DeltaForceClipper *>(private_data);
    if (!core) return;

    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        if (core->autoStartReplayBuffer()) {
            QTimer::singleShot(250, core, [core] { core->startReplayBuffer(); });
        }
        break;
    case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
        QTimer::singleShot(0, core, [core] { core->handleReplayBufferSaved(); });
        break;
    default:
        break;
    }
}

DeltaForceClipper::DeltaForceClipper()
    : QObject(nullptr)
{
    g_instance = this;
    loadConfig();

    obsReady_ = true;
    obs_frontend_add_event_callback(frontend_event_callback, this);
    obs_frontend_add_tools_menu_item("Delta Force Clipper", tool_menu_callback, nullptr);
    ensureSettingsDock();

    QObject::connect(&pollTimer_, &QTimer::timeout, this, &DeltaForceClipper::pollDetectorEvents);
    postEventTimer_.setSingleShot(true);
    QObject::connect(&postEventTimer_, &QTimer::timeout, this, &DeltaForceClipper::saveScheduledEvent);
    QObject::connect(&detectorProcess_, &QProcess::readyReadStandardOutput, this, [this] {
        const QString output = QString::fromUtf8(detectorProcess_.readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) {
            for (const QString &line : output.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                obs_log(LOG_INFO, "Detector: %s", line.toUtf8().constData());
            }
        }
    });
    QObject::connect(&detectorProcess_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        obs_log(LOG_WARNING, "Killfeed detector process error (%d): %s",
                static_cast<int>(error), detectorProcess_.errorString().toUtf8().constData());
    });
    QObject::connect(&detectorProcess_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                     this, [this](int exitCode, QProcess::ExitStatus status) {
        const bool expectedStop = detectorStopping_;
        detectorStopping_ = false;
        obs_log(expectedStop || (status == QProcess::NormalExit && exitCode == 0) ? LOG_INFO : LOG_WARNING,
                "Killfeed detector exited (code=%d, status=%d)",
                exitCode, static_cast<int>(status));
    });
    pollTimer_.start(1000);
    const QFileInfo eventsInfo(eventsFile_);
    eventsOffset_ = eventsInfo.exists() ? eventsInfo.size() : 0;
    if (detectorEnabled_) {
        startDetector();
    }

    obs_log(LOG_INFO, "Delta Force Clipper %s loaded", PLUGIN_VERSION);
}

DeltaForceClipper::~DeltaForceClipper()
{
    obs_frontend_remove_event_callback(frontend_event_callback, this);
    stopDetector();
    if (dialog_) {
        obs_frontend_remove_dock("deltaforceclipper.settings");
        delete dialog_;
        dialog_ = nullptr;
    }
    g_instance = nullptr;
}

DeltaForceClipper *delta_force_clipper_instance()
{
    return g_instance;
}

bool DeltaForceClipper::replayBufferActive() const
{
    return obs_frontend_replay_buffer_active();
}

QString DeltaForceClipper::defaultClipFolder() const
{
    return QStringLiteral("F:/OBS/DeltaForceClips");
}

QString DeltaForceClipper::defaultEventsFile()
{
    return QDir(sharedDirectory()).filePath(QStringLiteral("events.jsonl"));
}

void DeltaForceClipper::loadConfig()
{
    triggerMessages_ = defaultTriggerMessages();
    ocrRegions_ = defaultDetectorOcrRegions();
    const QString path = configPath();
    QFile file(path);

    if (!file.exists()) {
        clipFolder_ = defaultClipFolder();
        eventsFile_ = defaultEventsFile();
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        clipFolder_ = defaultClipFolder();
        eventsFile_ = defaultEventsFile();
        return;
    }

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        clipFolder_ = defaultClipFolder();
        eventsFile_ = defaultEventsFile();
        return;
    }

    const auto obj = doc.object();
    const QJsonArray savedOcrRegions = obj.value(QStringLiteral("ocr_regions")).toArray();
    if (!savedOcrRegions.isEmpty()) {
        bool validRegions = true;
        for (const QJsonValue &region : savedOcrRegions) {
            if (!isValidOcrRegion(region)) {
                validRegions = false;
                break;
            }
        }
        if (validRegions) {
            ocrRegions_ = savedOcrRegions;
        } else {
            obs_log(LOG_WARNING, "Ignoring invalid OCR regions in configuration; using defaults.");
        }
    }
    bool migratedDefaultTriggers = false;
    bufferSeconds_ = obj.value("buffer_seconds").toInt(30);
    clipFolder_ = obj.value("clip_folder").toString(defaultClipFolder());
    autoStartReplayBuffer_ = obj.value("auto_start_replay_buffer").toBool(true);
    detectorEnabled_ = obj.value("detector_enabled").toBool(true);
    eventsFile_ = obj.value("events_file").toString(defaultEventsFile());
    if (eventsFile_.isEmpty()) {
        eventsFile_ = defaultEventsFile();
    }
    ocrLang_ = obj.value("ocr_lang").toString(QStringLiteral("en"));
    preTime_ = obj.value("pre_time").toInt(1000);
    postTime_ = obj.value("post_time").toInt(3000);
    playerNick_ = obj.value("player_nick").toString(QStringLiteral("Absolution"));
    if (obj.contains("trigger_messages") && obj.value("trigger_messages").isArray()) {
        triggerMessages_ = obj.value("trigger_messages").toArray();
        if (triggerMessages_ == previousDefaultTriggerMessages()) {
            triggerMessages_ = defaultTriggerMessages();
            migratedDefaultTriggers = true;
        }
        for (int i = 0; i < triggerMessages_.size(); ++i) {
            QJsonObject message = triggerMessages_.at(i).toObject();
            if (!message.contains(QStringLiteral("tag"))) {
                const QString tag = defaultTriggerTag(message.value(QStringLiteral("phrase")).toString());
                if (!tag.isEmpty()) {
                    message.insert(QStringLiteral("tag"), tag);
                    triggerMessages_[i] = message;
                    migratedDefaultTriggers = true;
                }
            }
        }
    }
    uiLanguage_ = obj.value("ui_language").toString(QStringLiteral("en"));
    if (uiLanguage_ != QStringLiteral("es") && uiLanguage_ != QStringLiteral("pt-BR")) {
        uiLanguage_ = QStringLiteral("en");
    }
    if (migratedDefaultTriggers) {
        saveConfig();
    }
}

void DeltaForceClipper::saveConfig()
{
    QDir dir(QFileInfo(configPath()).absolutePath());
    dir.mkpath(QStringLiteral("."));

    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        obs_log(LOG_WARNING, "Unable to save Delta Force Clipper configuration: %s", file.errorString().toUtf8().constData());
        return;
    }

    QJsonObject obj{
        {"schema_version", 1},
        {"buffer_seconds", bufferSeconds_},
        {"clip_folder", clipFolder_},
        {"auto_start_replay_buffer", autoStartReplayBuffer_},
        {"detector_enabled", detectorEnabled_},
        {"events_file", eventsFile_},
        {"ocr_lang", ocrLang_},
        {"pre_time", preTime_},
        {"post_time", postTime_},
        {"player_nick", playerNick_},
        {"ui_language", uiLanguage_},
        {"trigger_messages", triggerMessages_},
        {"ocr_regions", ocrRegions_}
    };

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void DeltaForceClipper::setBufferSeconds(int seconds)
{
    bufferSeconds_ = qBound(5, seconds, 180);
}

void DeltaForceClipper::setClipFolder(const QString &folder)
{
    clipFolder_ = QDir::fromNativeSeparators(folder.trimmed());
    if (clipFolder_.isEmpty()) clipFolder_ = defaultClipFolder();
}

void DeltaForceClipper::setAutoStartReplayBuffer(bool enabled)
{
    autoStartReplayBuffer_ = enabled;
}

void DeltaForceClipper::setDetectorEnabled(bool enabled)
{
    detectorEnabled_ = enabled;
}

void DeltaForceClipper::applyDetectorSettings()
{
    stopDetector();
    if (detectorEnabled_) {
        startDetector();
    }
}

void DeltaForceClipper::setEventsFile(const QString &path)
{
    eventsFile_ = QDir::fromNativeSeparators(path.trimmed());
    if (eventsFile_.isEmpty()) {
        eventsFile_ = defaultEventsFile();
    } else {
        eventsFile_ = QFileInfo(eventsFile_).absoluteFilePath();
    }
    const QFileInfo info(eventsFile_);
    eventsOffset_ = info.exists() ? info.size() : 0;
}

void DeltaForceClipper::setOcrLang(const QString &lang) { ocrLang_ = lang; }
void DeltaForceClipper::setPreTime(int ms) { preTime_ = ms; }
void DeltaForceClipper::setPostTime(int ms) { postTime_ = ms; }
void DeltaForceClipper::setPlayerNick(const QString &nick) { playerNick_ = nick; }
void DeltaForceClipper::setUiLanguage(const QString &language)
{
    uiLanguage_ = language == QStringLiteral("es") || language == QStringLiteral("pt-BR")
                      ? language
                      : QStringLiteral("en");
}
void DeltaForceClipper::setTriggerMessages(const QJsonArray &messages)
{
    triggerMessages_ = messages;
}

bool DeltaForceClipper::setOcrRegions(const QJsonArray &regions, QString *error)
{
    if (regions.isEmpty()) {
        if (error) {
            *error = QStringLiteral("At least one OCR region is required.");
        }
        return false;
    }
    for (const QJsonValue &region : regions) {
        if (!isValidOcrRegion(region)) {
            if (error) {
                *error = QStringLiteral("OCR regions must be normalized rectangles within the screen.");
            }
            return false;
        }
    }
    ocrRegions_ = regions;
    return true;
}

bool DeltaForceClipper::detectorRunning() const
{
    return detectorProcess_.state() == QProcess::Running;
}

void DeltaForceClipper::pollDetectorEvents()
{
    if (!detectorEnabled_ || eventsFile_.isEmpty()) {
        return;
    }

    QFile file(eventsFile_);
    if (!file.exists()) {
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        obs_log(LOG_DEBUG, "Delta Force Clipper: cannot read events file %s", eventsFile_.toUtf8().constData());
        return;
    }

    const qint64 size = file.size();
    if (size < eventsOffset_) {
        // Detector truncated/rotated the log; restart from the beginning.
        eventsOffset_ = 0;
    }
    if (size == eventsOffset_) {
        return;
    }

    file.seek(eventsOffset_);
    const QByteArray block = file.readAll();
    eventsOffset_ = file.pos();

    for (const QByteArray &raw : block.split('\n')) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject()) {
            continue;
        }
        handleDetectorEvent(doc.object());
    }
}

void DeltaForceClipper::handleDetectorEvent(const QJsonObject &event)
{
    const QString type = sanitizeEventType(event.value("event").toString());
    if (type.isEmpty()) {
        return;
    }

    lastEvent_ = type;
    if (!replayBufferActive()) {
        obs_log(LOG_INFO, "Delta Force Clipper: detector event '%s' ignored, Replay Buffer is off", type.toUtf8().constData());
        return;
    }
    QJsonObject eventRecord = event;
    eventRecord.insert(QStringLiteral("received_at_utc"),
                       QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (postEventTimer_.isActive()) {
        scheduledEventRecords_.append(eventRecord);
        if (deltaforceclipper::promotesEvent(scheduledEventType_.toStdString(), type.toStdString())) {
            scheduledEventType_ = type;
        }
        postEventTimer_.start(qMax(0, postTime_));
        obs_log(LOG_INFO,
                "Delta Force Clipper: grouped '%s'; clip label is '%s', saving after %d ms without another event",
                type.toUtf8().constData(), scheduledEventType_.toUtf8().constData(), qMax(0, postTime_));
        return;
    }
    if (pendingSave_) {
        obs_log(LOG_DEBUG, "Delta Force Clipper: detector event '%s' skipped, save already pending", type.toUtf8().constData());
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    if (lastSaveAttempt_.isValid() && lastSaveAttempt_.msecsTo(now) < 1000) {
        obs_log(LOG_DEBUG, "Delta Force Clipper: detector event '%s' skipped, within save debounce", type.toUtf8().constData());
        return;
    }

    obs_log(LOG_INFO, "Delta Force Clipper: detector event '%s' received", type.toUtf8().constData());
    lastSaveAttempt_ = now;
    scheduledEventType_ = type;
    scheduledEventRecords_ = QJsonArray{eventRecord};
    postEventTimer_.start(qMax(0, postTime_));
}

void DeltaForceClipper::saveScheduledEvent()
{
    const QString event = scheduledEventType_;
    scheduledEventType_.clear();
    if (!event.isEmpty()) {
        pendingEventRecords_ = scheduledEventRecords_;
        scheduledEventRecords_ = QJsonArray{};
        obs_log(LOG_INFO, "Delta Force Clipper: saving '%s' after post-event delay", event.toUtf8().constData());
        saveReplayBufferAs(event);
    }
}

QString DeltaForceClipper::sanitizeEventType(const QString &type)
{
    static const QStringList allowed = {QStringLiteral("kill"),
                                        QStringLiteral("double_kill"),
                                        QStringLiteral("triple_kill"),
                                        QStringLiteral("multikill"),
                                        QStringLiteral("quadra_kill"),
                                        QStringLiteral("penta_kill")};
    const QString clean = type.trimmed().toLower();
    return allowed.contains(clean) ? clean : QString();
}

void DeltaForceClipper::applyReplayBufferDuration()
{
    obs_output_t *output = obs_frontend_get_replay_buffer_output();
    if (!output) {
        obs_log(LOG_WARNING, "Replay Buffer output is not available yet; duration will be applied when OBS initializes it.");
        return;
    }

    obs_data_t *settings = obs_output_get_settings(output);
    if (settings) {
        const int eventWindowSeconds = qCeil((preTime_ + postTime_) / 1000.0);
        const int durationSeconds = qBound(5, qMax(bufferSeconds_, eventWindowSeconds), 180);
        obs_data_set_int(settings, "max_time_sec", durationSeconds);
        obs_output_update(output, settings);
        obs_data_release(settings);
    }

    obs_output_release(output);
}

void DeltaForceClipper::startReplayBuffer()
{
    applyReplayBufferDuration();
    if (!obs_frontend_replay_buffer_active()) {
        obs_frontend_replay_buffer_start();
    }
}

void DeltaForceClipper::stopReplayBuffer()
{
    if (obs_frontend_replay_buffer_active()) {
        obs_frontend_replay_buffer_stop();
    }
}

void DeltaForceClipper::saveReplayBufferAs(const QString &eventType)
{
    if (!obs_frontend_replay_buffer_active()) {
        obs_log(LOG_WARNING, "Cannot save clip: Replay Buffer is not active.");
        return;
    }

    postEventTimer_.stop();
    scheduledEventType_.clear();
    if (eventType == QStringLiteral("manual")) {
        pendingEventRecords_ = QJsonArray{};
        scheduledEventRecords_ = QJsonArray{};
    }
    pendingEventType_ = eventType;
    pendingSave_ = true;
    obs_frontend_replay_buffer_save();
}

void DeltaForceClipper::handleReplayBufferSaved()
{
    if (!pendingSave_) {
        obs_log(LOG_DEBUG, "Replay Buffer saved outside Delta Force Clipper; leaving the file untouched.");
        return;
    }

    const QString event = pendingEventType_.isEmpty() ? QStringLiteral("manual") : pendingEventType_;
    renameAndRegisterLastReplay(event);
    pendingEventType_.clear();
    pendingEventRecords_ = QJsonArray{};
    pendingSave_ = false;

    if (dialog_) {
        dialog_->refreshStatus();
        dialog_->refreshHistory();
    }
}

void DeltaForceClipper::renameAndRegisterLastReplay(const QString &eventType)
{
    const QJsonArray eventRecords = pendingEventRecords_;
    char *lastReplay = obs_frontend_get_last_replay();
    if (!lastReplay || !*lastReplay) {
        if (lastReplay) bfree(lastReplay);
        obs_log(LOG_WARNING, "Replay Buffer saved event arrived, but OBS returned no replay path.");
        return;
    }

    const QString sourcePath = QString::fromUtf8(lastReplay);
    bfree(lastReplay);

    QDir destinationDir(clipFolder_);
    if (!destinationDir.mkpath(QStringLiteral("."))) {
        obs_log(LOG_WARNING, "Could not create clip folder: %s", clipFolder_.toUtf8().constData());
        return;
    }

    const QString dateFolder = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    destinationDir.mkpath(dateFolder);
    destinationDir.cd(dateFolder);

    const QFileInfo sourceInfo(sourcePath);
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    QString filename = QStringLiteral("DeltaForce_Warfare_%1_%2.%3")
                           .arg(eventType, stamp, sourceInfo.suffix().isEmpty() ? QStringLiteral("mp4") : sourceInfo.suffix());

    QString targetPath = destinationDir.filePath(filename);
    int suffix = 2;
    while (QFile::exists(targetPath)) {
        filename = QStringLiteral("DeltaForce_Warfare_%1_%2_%3.%4")
                       .arg(eventType, stamp)
                       .arg(suffix++)
                       .arg(sourceInfo.suffix().isEmpty() ? QStringLiteral("mp4") : sourceInfo.suffix());
        targetPath = destinationDir.filePath(filename);
    }

    bool moved = QFile::rename(sourcePath, targetPath);
    if (!moved) {
        // Cross-volume fallback: preserve the OBS file and copy a library copy.
        moved = QFile::copy(sourcePath, targetPath);
    }

    if (!moved) {
        obs_log(LOG_WARNING, "Could not organize replay file: %s", sourcePath.toUtf8().constData());
        return;
    }

    const int bufferDurationSeconds = qBound(
        5, qMax(bufferSeconds_, qCeil((qMax(0, preTime_) + qMax(0, postTime_)) / 1000.0)), 180);
    if (eventRecords.isEmpty()) {
        registerClipFile(targetPath, eventType, eventRecords, false, 0,
                         bufferDurationSeconds, QStringLiteral("manual replay; no trim requested"));
        return;
    }

    const qint64 windowDurationMs = requestedClipWindowMs(eventRecords, preTime_, postTime_);
    if (windowDurationMs < 1000) {
        obs_log(LOG_WARNING,
                "Requested event window is unavailable or shorter than one second; keeping the full replay.");
        registerClipFile(targetPath, eventType, eventRecords, false, windowDurationMs,
                         bufferDurationSeconds, QStringLiteral("invalid or too-short event window"));
        return;
    }
    if (windowDurationMs > static_cast<qint64>(bufferDurationSeconds) * 1000) {
        obs_log(LOG_WARNING,
                "Requested event window (%lld ms) exceeds the configured Replay Buffer (%d s); keeping the full replay.",
                static_cast<long long>(windowDurationMs), bufferDurationSeconds);
        registerClipFile(targetPath, eventType, eventRecords, false, windowDurationMs,
                         bufferDurationSeconds, QStringLiteral("requested window exceeds replay buffer"));
        return;
    }

    const QString ffmpegPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpegPath.isEmpty()) {
        obs_log(LOG_WARNING, "ffmpeg.exe was not found on PATH; keeping the full Replay Buffer clip.");
        registerClipFile(targetPath, eventType, eventRecords, false, windowDurationMs,
                         bufferDurationSeconds, QStringLiteral("ffmpeg.exe not found on PATH"));
        return;
    }

    const QString extension = sourceInfo.suffix().isEmpty() ? QStringLiteral("mp4") : sourceInfo.suffix();
    const QString tempPath = destinationDir.filePath(
        QStringLiteral("%1.trim-%2.%3")
            .arg(QFileInfo(targetPath).completeBaseName(),
                 QUuid::createUuid().toString(QUuid::WithoutBraces), extension));
    const QString duration = QString::number(windowDurationMs / 1000.0, 'f', 3);
    QStringList arguments{
        QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-nostdin"), QStringLiteral("-y"),
        QStringLiteral("-nostats"), QStringLiteral("-progress"), QStringLiteral("pipe:1"),
        QStringLiteral("-sseof"), QStringLiteral("-%1").arg(duration),
        QStringLiteral("-i"), targetPath,
        QStringLiteral("-t"), duration,
        QStringLiteral("-map"), QStringLiteral("0:v?"),
        QStringLiteral("-map"), QStringLiteral("0:a?"),
        QStringLiteral("-c"), QStringLiteral("copy"),
        QStringLiteral("-avoid_negative_ts"), QStringLiteral("make_zero"),
        tempPath
    };

    auto *process = new QProcess(this);
    auto finalized = std::make_shared<bool>(false);
    const auto completeTrim = [this, process, targetPath, tempPath, eventType, eventRecords,
                               windowDurationMs, bufferDurationSeconds, finalized]
                              (const QString &finalPath, bool trimmed, const QString &status) {
        if (*finalized) {
            return;
        }
        *finalized = true;
        if (!trimmed) {
            QFile::remove(tempPath);
            obs_log(LOG_WARNING, "Event-relative trimming was not applied (%s); keeping the Replay Buffer clip.",
                    status.toUtf8().constData());
        } else {
            obs_log(LOG_INFO,
                    "Applied a %lld ms event-relative window using FFmpeg stream copy; boundaries are keyframe-aligned.",
                    static_cast<long long>(windowDurationMs));
        }
        const QString finalStatus = trimmed && status.isEmpty()
                                         ? QStringLiteral("stream-copy trim applied")
                                         : status;
        registerClipFile(finalPath, eventType, eventRecords, trimmed, windowDurationMs,
                         bufferDurationSeconds, finalStatus);
        process->deleteLater();
    };

    connect(process, &QProcess::errorOccurred, this,
            [completeTrim, process, targetPath](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            completeTrim(targetPath, false, process->errorString());
        }
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [completeTrim, process, targetPath, tempPath, extension, windowDurationMs]
            (int exitCode, QProcess::ExitStatus status) {
        bool trimmed = exitCode == 0 && status == QProcess::NormalExit &&
                       QFileInfo::exists(tempPath) && QFileInfo(tempPath).size() > 0;
        QString finalPath = targetPath;
        QString reason;
        if (trimmed) {
            qint64 outputDurationUs = -1;
            const QStringList progressLines = QString::fromUtf8(process->readAllStandardOutput()).split(
                QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : progressLines) {
                if (line.startsWith(QStringLiteral("out_time_us="))) {
                    bool ok = false;
                    const qint64 value = line.mid(QStringLiteral("out_time_us=").size()).toLongLong(&ok);
                    if (ok) {
                        outputDurationUs = value;
                    }
                }
            }
            if (outputDurationUs < 0 ||
                outputDurationUs + 500000 < windowDurationMs * 1000) {
                trimmed = false;
                reason = QStringLiteral("Replay Buffer file was shorter than the requested event window");
            }
        }
        if (trimmed) {
            const QString backupPath = QStringLiteral("%1.full-%2.%3")
                                           .arg(QFileInfo(targetPath).completeBaseName(),
                                                QUuid::createUuid().toString(QUuid::WithoutBraces), extension);
            if (!QFile::rename(targetPath, backupPath)) {
                trimmed = false;
                reason = QStringLiteral("could not preserve the original replay during replacement");
            } else if (!QFile::rename(tempPath, targetPath)) {
                trimmed = false;
                reason = QStringLiteral("could not install the trimmed replay");
                if (!QFile::rename(backupPath, targetPath)) {
                    finalPath = backupPath;
                    reason += QStringLiteral("; original replay remains at the backup path");
                }
            } else if (!QFile::remove(backupPath)) {
                obs_log(LOG_WARNING, "Trimmed clip saved, but full-buffer backup could not be removed: %s",
                        backupPath.toUtf8().constData());
            }
        } else {
            if (reason.isEmpty()) {
                const QString stderrText = QString::fromUtf8(process->readAllStandardError()).simplified();
                reason = exitCode == 0 && status == QProcess::NormalExit
                             ? QStringLiteral("FFmpeg produced no output file")
                             : QStringLiteral("FFmpeg exited with code %1: %2").arg(exitCode).arg(stderrText.left(500));
            }
        }
        completeTrim(finalPath, trimmed, reason);
    });

    process->start(ffmpegPath, arguments);
}

void DeltaForceClipper::registerClipFile(const QString &filePath,
                                         const QString &eventType,
                                         const QJsonArray &eventRecords,
                                         bool boundariesTrimmed,
                                         qint64 requestedWindowMs,
                                         int bufferDurationSeconds,
                                         const QString &boundaryStatus)
{
    lastClipPath_ = QDir::toNativeSeparators(filePath);

    ClipLibrary library;
    QJsonArray detectionTags;
    for (const QJsonValue &detection : eventRecords) {
        const QString tag = detection.toObject().value(QStringLiteral("tag")).toString();
        if (!tag.isEmpty() && !detectionTags.contains(tag)) {
            detectionTags.append(tag);
        }
    }
    QJsonObject clip = library.makeClip(lastClipPath_,
                                        eventType,
                                        QString(),
                                        QStringLiteral("Warfare"),
                                        QString(),
                                        detectionTags);
    if (!eventRecords.isEmpty()) {
        const QJsonObject metadata{
            {QStringLiteral("record_type"), QStringLiteral("clip_metadata")},
            {QStringLiteral("clip_file"), QFileInfo(filePath).fileName()},
            {QStringLiteral("event_label"), eventType},
            {QStringLiteral("created_at_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {QStringLiteral("window_configuration"),
             QJsonObject{
                 {QStringLiteral("requested_pre_event_ms"), preTime_},
                 {QStringLiteral("grouping_quiet_ms"), postTime_},
                 {QStringLiteral("requested_clip_window_ms"), requestedWindowMs},
                 {QStringLiteral("replay_buffer_duration_seconds"), bufferDurationSeconds},
                 {QStringLiteral("boundaries_trimmed"), boundariesTrimmed},
                 {QStringLiteral("boundary_mode"), boundariesTrimmed
                      ? QStringLiteral("ffmpeg_stream_copy_keyframe_aligned")
                      : QStringLiteral("untrimmed_replay_buffer")},
                 {QStringLiteral("boundary_status"), boundaryStatus},
                 {QStringLiteral("exact_media_boundaries"), false}}},
            {QStringLiteral("tags"), clip.value(QStringLiteral("tags")).toArray()}
        };
        const QByteArray metadataJson = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
        obs_log(LOG_INFO, "Clip metadata: %s", metadataJson.constData());

        for (const QJsonValue &detectionValue : eventRecords) {
            QJsonObject detection = detectionValue.toObject();
            const QString raw = detection.value(QStringLiteral("raw")).toString().simplified();
            detection.insert(QStringLiteral("raw"), raw.left(256));
            detection.insert(QStringLiteral("raw_truncated"), raw.size() > 256);
            detection.insert(QStringLiteral("record_type"), QStringLiteral("clip_detection"));
            detection.insert(QStringLiteral("clip_file"), QFileInfo(filePath).fileName());
            const QByteArray detectionJson = QJsonDocument(detection).toJson(QJsonDocument::Compact);
            obs_log(LOG_INFO, "Clip detection: %s", detectionJson.constData());
        }
    }
    QString error;
    if (!library.addClip(clip, &error)) {
        obs_log(LOG_WARNING, "Clip saved but library update failed: %s", error.toUtf8().constData());
    }

    obs_log(LOG_INFO, "Delta Force clip saved: %s", lastClipPath_.toUtf8().constData());
    if (dialog_) {
        dialog_->refreshStatus();
        dialog_->refreshHistory();
    }
}

void DeltaForceClipper::ensureSettingsDock()
{
    if (dialog_) {
        return;
    }

    auto *mainWindow = static_cast<QWidget *>(obs_frontend_get_main_window());
    if (!mainWindow) {
        obs_log(LOG_WARNING, "OBS main window is unavailable; the Delta Force Clipper dock will be created when opened.");
        return;
    }

    dialog_ = new SettingsDialog(this, mainWindow);
    dialog_->setAttribute(Qt::WA_DeleteOnClose, false);
    if (!obs_frontend_add_custom_qdock("deltaforceclipper.settings", dialog_)) {
        obs_log(LOG_WARNING, "Could not register the Delta Force Clipper OBS dock.");
        delete dialog_;
        dialog_ = nullptr;
        return;
    }
    dialog_->hide();
}

void DeltaForceClipper::showSettings()
{
    ensureSettingsDock();
    if (!dialog_) {
        return;
    }
    dialog_->refreshStatus();
    dialog_->refreshHistory();
    dialog_->show();
    dialog_->raise();
    dialog_->activateWindow();
}

bool DeltaForceClipper::writeDetectorConfig()
{
    const QString path = QDir(sharedDirectory()).filePath(QStringLiteral("detector.json"));
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        obs_log(LOG_WARNING, "Unable to create the detector configuration directory.");
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        obs_log(LOG_WARNING, "Unable to write detector configuration: %s",
                file.errorString().toUtf8().constData());
        return false;
    }

    const QJsonObject config{
        {"events_file", eventsFile_},
        {"log_ocr_text", true},
        {"ocr_log_interval_seconds", 10},
        {"ocr_language", "en"},
        {"player_nick", playerNick_},
        {"pre_time", preTime_},
        {"post_time", postTime_},
        {"trigger_messages", triggerMessages_},
        {"rois", ocrRegions_}
    };
    const QByteArray json = QJsonDocument(config).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        obs_log(LOG_WARNING, "Unable to write the complete detector configuration: %s",
                file.errorString().toUtf8().constData());
        return false;
    }
    if (!file.flush()) {
        obs_log(LOG_WARNING, "Unable to flush detector configuration: %s",
                file.errorString().toUtf8().constData());
        return false;
    }
    return true;
}

void DeltaForceClipper::stopDetector()
{
    if (detectorProcess_.state() == QProcess::NotRunning) {
        return;
    }
    detectorStopping_ = true;
    detectorProcess_.terminate();
    if (!detectorProcess_.waitForFinished(1500)) {
        detectorProcess_.kill();
        detectorProcess_.waitForFinished(1500);
    }
}

void DeltaForceClipper::startDetector()
{
    if (!detectorEnabled_ || detectorProcess_.state() != QProcess::NotRunning) {
        return;
    }

    detectorStopping_ = false;
    if (!writeDetectorConfig()) {
        return;
    }
    char *toolsPathRaw = obs_module_file("tools");
    const QString toolsPath = toolsPathRaw ? QString::fromUtf8(toolsPathRaw) : QString();
    if (toolsPathRaw) {
        bfree(toolsPathRaw);
    }
    const QString entryPoint = QDir(QDir(toolsPath).filePath(QStringLiteral("killfeed_detector")))
                                   .filePath(QStringLiteral("__main__.py"));
    if (toolsPath.isEmpty() || !QFileInfo::exists(entryPoint)) {
        obs_log(LOG_WARNING, "Killfeed detector files are unavailable in the OBS plugin data directory.");
        return;
    }

    QString python = QStandardPaths::findExecutable(QStringLiteral("python.exe"));
    QStringList arguments;
    if (python.isEmpty()) {
        python = QStandardPaths::findExecutable(QStringLiteral("py.exe"));
        if (!python.isEmpty()) {
            arguments << QStringLiteral("-3");
        }
    }
    if (python.isEmpty()) {
        obs_log(LOG_WARNING, "Python 3 was not found; install Python and detector requirements to enable live detection.");
        return;
    }

    arguments << QStringLiteral("-m") << QStringLiteral("killfeed_detector")
              << QStringLiteral("--config")
              << QDir(sharedDirectory()).filePath(QStringLiteral("detector.json"));
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    const QString currentPythonPath = environment.value(QStringLiteral("PYTHONPATH"));
    environment.insert(QStringLiteral("PYTHONPATH"),
                       currentPythonPath.isEmpty() ? toolsPath
                                                   : toolsPath + QDir::listSeparator() + currentPythonPath);
    detectorProcess_.setProcessEnvironment(environment);
    detectorProcess_.setWorkingDirectory(toolsPath);
    detectorProcess_.setProgram(python);
    detectorProcess_.setArguments(arguments);
    detectorProcess_.setProcessChannelMode(QProcess::MergedChannels);
    detectorProcess_.start();
    if (!detectorProcess_.waitForStarted(3000)) {
        obs_log(LOG_WARNING, "Could not start killfeed detector: %s",
                detectorProcess_.errorString().toUtf8().constData());
    } else {
        obs_log(LOG_INFO, "Killfeed detector started using %s", python.toUtf8().constData());
    }
}

bool obs_module_load(void)
{
    if (!g_instance) {
        new DeltaForceClipper();
    }
    return true;
}

void obs_module_unload(void)
{
    delete g_instance;
}
