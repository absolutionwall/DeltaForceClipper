#include "clip_library.hpp"

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

class ClipLibraryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qputenv("DFC_CLIPS_HOME", QDir::temp().absoluteFilePath("dfc_clip_library_test"));
        QDir(qgetenv("DFC_CLIPS_HOME")).removeRecursively();
    }

    void cleanupTestCase()
    {
        QDir(qgetenv("DFC_CLIPS_HOME")).removeRecursively();
    }

    void makeClipFillsRequiredFields()
    {
        ClipLibrary library;
        const QJsonObject clip = library.makeClip("F:/clips/a.mp4", "triple_kill", "Havoc", "Warfare", "CR-56 Amax");

        QCOMPARE(clip.value("game").toString(), QStringLiteral("Delta Force"));
        QCOMPARE(clip.value("event").toString(), QStringLiteral("triple_kill"));
        QCOMPARE(clip.value("kills").toInt(), 3);
        QCOMPARE(clip.value("player").toString(), QStringLiteral("Absolution"));
        QCOMPARE(clip.value("mode").toString(), QStringLiteral("Warfare"));
        QCOMPARE(clip.value("file").toString(), QStringLiteral("F:/clips/a.mp4"));
        QVERIFY(clip.value("id").toString().size() > 0);
        QVERIFY(clip.value("created_at").toString().size() > 0);
        QVERIFY(clip.value("tags").toArray().contains(QStringLiteral("triple_kill")));
    }

    void makeClipPreservesQuadraAndPentaCounts()
    {
        ClipLibrary library;
        QCOMPARE(library.makeClip("F:/clips/quadra.mp4", "quadra_kill").value("kills").toInt(), 4);
        QCOMPARE(library.makeClip("F:/clips/penta.mp4", "penta_kill").value("kills").toInt(), 5);
    }

    void makeClipIncludesDetectionTagsWithoutDuplicates()
    {
        ClipLibrary library;
        const QJsonObject clip = library.makeClip(
            "F:/clips/launcher.mp4",
            "kill",
            QString(),
            QStringLiteral("Warfare"),
            QString(),
            QJsonArray{QStringLiteral("launcher"), QStringLiteral("missile"), QStringLiteral("launcher")});
        const QJsonArray tags = clip.value("tags").toArray();
        QVERIFY(tags.contains(QStringLiteral("launcher")));
        QVERIFY(tags.contains(QStringLiteral("missile")));
        int launcherCount = 0;
        for (const QJsonValue &tag : tags) {
            if (tag.toString() == QStringLiteral("launcher")) {
                ++launcherCount;
            }
        }
        QCOMPARE(launcherCount, 1);
    }

    void addClipPersistsToLibrary()
    {
        ClipLibrary library;
        QString error;
        const QJsonObject clip = library.makeClip("F:/clips/b.mp4", "kill");

        QVERIFY2(library.addClip(clip, &error), qPrintable(error));

        QFile file(library.libraryPath());
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(file.errorString()));
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        QVERIFY(doc.isObject());
        QCOMPARE(doc.object().value("schema_version").toInt(), 1);
        const QJsonArray clips = doc.object().value("clips").toArray();
        QCOMPARE(clips.size(), 1);
        QCOMPARE(clips.first().toObject().value("file").toString(), QStringLiteral("F:/clips/b.mp4"));
    }

    void addClipAppendsWithoutLosingPrevious()
    {
        ClipLibrary library;
        QString error;
        QVERIFY(library.addClip(library.makeClip("F:/clips/1.mp4", "kill"), &error));
        QVERIFY(library.addClip(library.makeClip("F:/clips/2.mp4", "double_kill"), &error));

        QFile file(library.libraryPath());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonArray clips = QJsonDocument::fromJson(file.readAll()).object().value("clips").toArray();
        file.close();

        QCOMPARE(clips.size(), 2);
        QCOMPARE(clips.at(0).toObject().value("file").toString(), QStringLiteral("F:/clips/1.mp4"));
        QCOMPARE(clips.at(1).toObject().value("event").toString(), QStringLiteral("double_kill"));
    }

    void pruneMissingFilesRemovesOnlyDeletedClips()
    {
        const QString root = QString::fromLocal8Bit(qgetenv("DFC_CLIPS_HOME"));
        QDir(root).removeRecursively();
        ClipLibrary library;

        const QString existingPath = QDir(root).filePath(QStringLiteral("existing.mkv"));
        QFile existingFile(existingPath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        existingFile.close();

        QString error;
        QVERIFY2(library.addClip(library.makeClip(existingPath, "kill"), &error), qPrintable(error));
        QVERIFY2(library.addClip(
                     library.makeClip(QDir(root).filePath(QStringLiteral("deleted.mkv")), "kill"), &error),
                 qPrintable(error));
        QVERIFY2(library.pruneMissingFiles(&error), qPrintable(error));

        QFile libraryFile(library.libraryPath());
        QVERIFY2(libraryFile.open(QIODevice::ReadOnly), qPrintable(libraryFile.errorString()));
        const QJsonArray clips = QJsonDocument::fromJson(libraryFile.readAll())
                                     .object().value(QStringLiteral("clips")).toArray();
        QCOMPARE(clips.size(), 1);
        QCOMPARE(clips.first().toObject().value(QStringLiteral("file")).toString(), existingPath);
        QVERIFY(QFileInfo::exists(existingPath));
    }
};

QTEST_MAIN(ClipLibraryTest)
#include "test_clip_library.moc"
