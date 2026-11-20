#include "clip_library.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <QtGlobal>

ClipLibrary::ClipLibrary() = default;

QString ClipLibrary::rootDirectory() const
{
    // Same shared folder the plugin and the external detector use, so the
    // library, config.json, events.jsonl and clips.json all live together in
    // %APPDATA%\DeltaForceClipper. DFC_CLIPS_HOME overrides it (mainly tests).
    const QString override = QFile::decodeName(qgetenv("DFC_CLIPS_HOME"));
    if (!override.isEmpty()) {
        return QDir::fromNativeSeparators(override);
    }
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return QDir(base).filePath(QStringLiteral("DeltaForceClipper"));
}

QString ClipLibrary::clipDirectory() const
{
    return QDir(rootDirectory()).filePath(QStringLiteral("clips"));
}

QString ClipLibrary::libraryPath() const
{
    return QDir(rootDirectory()).filePath(QStringLiteral("clips.json"));
}

bool ClipLibrary::load(QJsonObject *root, QString *error) const
{
    if (!root) {
        if (error) *error = QStringLiteral("Invalid root object.");
        return false;
    }

    QFile file(libraryPath());
    if (!file.exists()) {
        *root = QJsonObject{{QStringLiteral("schema_version"), 1},
                            {QStringLiteral("clips"), QJsonArray{}}};
        return true;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Could not open clips.json: %1").arg(file.errorString());
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) *error = QStringLiteral("Invalid clips.json: %1").arg(parseError.errorString());
        return false;
    }

    *root = doc.object();
    if (!root->contains(QStringLiteral("clips"))) {
        (*root)[QStringLiteral("clips")] = QJsonArray{};
    }
    return true;
}

bool ClipLibrary::save(const QJsonObject &root, QString *error) const
{
    QDir dir;
    if (!dir.mkpath(rootDirectory()) || !dir.mkpath(clipDirectory())) {
        if (error) *error = QStringLiteral("Could not create library directory.");
        return false;
    }

    QFile file(libraryPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("Could not write clips.json: %1").arg(file.errorString());
        return false;
    }

    const QJsonDocument doc(root);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

QJsonObject ClipLibrary::makeClip(const QString &filePath,
                                  const QString &eventType,
                                  const QString &map,
                                  const QString &mode,
                                  const QString &weapon,
                                  const QJsonArray &additionalTags) const
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QFileInfo info(filePath);

    QJsonArray tags;
    if (!eventType.isEmpty()) tags.append(eventType);
    if (!map.isEmpty()) tags.append(map.toLower().replace(' ', '-'));
    if (!mode.isEmpty()) tags.append(mode.toLower().replace(' ', '-').toLower());
    for (const QJsonValue &tagValue : additionalTags) {
        const QString tag = tagValue.toString().trimmed();
        if (!tag.isEmpty() && !tags.contains(tag)) {
            tags.append(tag);
        }
    }

    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("created_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {QStringLiteral("game"), QStringLiteral("Delta Force")},
        {QStringLiteral("mode"), mode.isEmpty() ? QStringLiteral("Warfare") : mode},
        {QStringLiteral("map"), map},
        {QStringLiteral("event"), eventType},
        {QStringLiteral("kills"), eventType == QStringLiteral("penta_kill") ? 5 :
                                    eventType == QStringLiteral("quadra_kill") ? 4 :
                                    eventType == QStringLiteral("triple_kill") ? 3 :
                                    eventType == QStringLiteral("double_kill") ? 2 :
                                    eventType == QStringLiteral("kill") ? 1 : 0},
        {QStringLiteral("weapon"), weapon},
        {QStringLiteral("player"), QStringLiteral("Absolution")},
        {QStringLiteral("source"), QStringLiteral("live")},
        {QStringLiteral("file"), info.absoluteFilePath()},
        {QStringLiteral("extension"), info.suffix()},
        {QStringLiteral("tags"), tags}
    };
}

bool ClipLibrary::addClip(const QJsonObject &clip, QString *error)
{
    QJsonObject root;
    if (!load(&root, error)) return false;

    QJsonArray clips = root.value(QStringLiteral("clips")).toArray();
    clips.append(clip);
    root[QStringLiteral("clips")] = clips;
    root[QStringLiteral("schema_version")] = 1;

    return save(root, error);
}

bool ClipLibrary::pruneMissingFiles(QString *error)
{
    QJsonObject root;
    if (!load(&root, error)) return false;

    const QJsonArray clips = root.value(QStringLiteral("clips")).toArray();
    QJsonArray existingClips;
    bool changed = false;
    for (const QJsonValue &value : clips) {
        const QString filePath = value.toObject().value(QStringLiteral("file")).toString();
        if (filePath.isEmpty() || !QFileInfo::exists(filePath)) {
            changed = true;
            continue;
        }
        existingClips.append(value);
    }

    if (!changed) return true;
    root[QStringLiteral("clips")] = existingClips;
    return save(root, error);
}
