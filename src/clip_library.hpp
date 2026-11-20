#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class ClipLibrary {
public:
    ClipLibrary();

    QString libraryPath() const;
    bool addClip(const QJsonObject &clip, QString *error = nullptr);
    bool pruneMissingFiles(QString *error = nullptr);
    QJsonObject makeClip(const QString &filePath,
                         const QString &eventType,
                         const QString &map = QString(),
                         const QString &mode = QStringLiteral("Warfare"),
                         const QString &weapon = QString(),
                         const QJsonArray &additionalTags = QJsonArray{}) const;

private:
    bool load(QJsonObject *root, QString *error) const;
    bool save(const QJsonObject &root, QString *error) const;
    QString rootDirectory() const;
    QString clipDirectory() const;
};
