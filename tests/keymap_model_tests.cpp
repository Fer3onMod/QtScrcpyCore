#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTextStream>
#include <cmath>

#include "schemestore.h"

namespace {
int failures = 0;

void check(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAIL: " << message << Qt::endl;
        ++failures;
    }
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QByteArray();
    return file.readAll();
}

void testKeyBinding()
{
    KeyBinding binding;
    check(KeyBinding::fromLegacyCode(QStringLiteral("Key_F"), &binding),
          QStringLiteral("legacy alphabetic key conversion"));
    check(binding.code == QStringLiteral("KeyF"),
          QStringLiteral("legacy key has stable code spelling"));

    QJsonObject json = binding.toJson();
    json.insert(QStringLiteral("modifiers"),
                QJsonArray{QStringLiteral("Control"), QStringLiteral("Alt")});
    KeyBinding parsed;
    check(KeyBinding::fromJson(json, &parsed), QStringLiteral("modifier round trip"));
    check(parsed.code == QStringLiteral("KeyF") && parsed.modifiers.size() == 2,
          QStringLiteral("key binding fields survive round trip"));

    json.insert(QStringLiteral("code"), QStringLiteral("Key_ArabicLetter"));
    check(!KeyBinding::fromJson(json, &parsed), QStringLiteral("unknown code is rejected"));
}

void testVersionTwoReadWrite()
{
    const QByteArray document = R"JSON({
      "version": 2,
      "schemes": [{
        "id": "scheme-1",
        "name": "Local Key (1)",
        "type": "local",
        "widgets": [
          {"id":"key-1","type":"key","code":"KeyF","modifiers":["Control"],"x":0.52,"y":0.28,"size":40,"z":3,"enabled":true},
          {"id":"move-1","type":"joystick","keys":{"up":"KeyW","left":"KeyA","down":"KeyS","right":"KeyD"},"x":0.12,"y":0.78,"radius":60,"speed":8,"enabled":true},
          {"id":"aim-1","type":"aim","toggleKey":"Backquote","x":0.37,"y":0.35,"sensX":25,"sensY":25,"enabled":true},
          {"id":"fire-1","type":"fire","code":"MouseLeft","x":0.8,"y":0.8,"radius":24},
          {"id":"slide-1","type":"slide","code":"KeyG","x":0.4,"y":0.6,"endX":0.4,"endY":0.2},
          {"id":"macro-1","type":"macro","code":"KeyM","x":0.5,"y":0.5,"steps":[
            {"action":"tap","x":0.3,"y":0.4,"delayMs":120},
            {"action":"keyDown","code":"KeyA","delayMs":20},
            {"action":"keyUp","code":"KeyA","delayMs":30}]},
          {"id":"repeat-1","type":"clickRepeat","code":"KeyR","x":0.7,"y":0.7,"intervalMs":100},
          {"id":"observed-1","type":"observed","code":"KeyO","x":0.1,"y":0.1},
          {"id":"angle-1","type":"viewAngle","code":"KeyV","x":0.5,"y":0.5,"sensX":20,"sensY":30},
          {"id":"right-move-1","type":"rightMouseMove","code":"MouseRight","x":0.5,"y":0.5,"sensX":25,"sensY":25},
          {"id":"cast-1","type":"intelligentCast","code":"KeyC","x":0.6,"y":0.6},
          {"id":"cancel-cast-1","type":"cancelCast","code":"Escape","x":0.6,"y":0.6},
          {"id":"vision-1","type":"visionExtension","code":"KeyX","x":0.6,"y":0.6}
        ]
      }]
    })JSON";

    SchemeStore store;
    QString error;
    const bool loaded = store.loadJson(document, &error);
    check(loaded, QStringLiteral("read v2 sample: %1").arg(error));
    check(store.schemes.size() == 1 && store.schemes.first().widgets.size() == 13,
          QStringLiteral("every v2 widget type is constructed"));
    const QByteArray saved = store.toJson(&error);
    SchemeStore reloaded;
    check(!saved.isEmpty() && reloaded.loadJson(saved, &error),
          QStringLiteral("v2 write then read: %1").arg(error));
    check(reloaded.toJson() == saved, QStringLiteral("v2 serialization is stable"));
}

void testInvalidDocuments()
{
    SchemeStore store;
    QString error;
    const QByteArray outOfRange = R"JSON({
      "version":2,"schemes":[{"id":"s","name":"invalid","type":"local","widgets":[
        {"id":"w","type":"key","code":"KeyF","x":1.01,"y":0.5}
      ]}]
    })JSON";
    check(!store.loadJson(outOfRange, &error) && !error.isEmpty(),
          QStringLiteral("out-of-range normalized coordinates are rejected"));

    error.clear();
    const QByteArray badJoystick = R"JSON({
      "version":2,"schemes":[{"id":"s","name":"invalid","type":"local","widgets":[
        {"id":"w","type":"joystick","keys":{"up":"KeyW","left":"KeyA","down":"KeyS","right":"KeyD"},
         "x":0.5,"y":0.5,"radius":0,"speed":8}
      ]}]
    })JSON";
    check(!store.loadJson(badJoystick, &error) && !error.isEmpty(),
          QStringLiteral("invalid joystick radius is rejected"));

    error.clear();
    check(!store.loadJson(QByteArrayLiteral("{\"version\":1,\"schemes\":[]}"), &error),
          QStringLiteral("unsupported document version is rejected"));
}

void testLegacyFixtures()
{
    const QStringList fixtures = {
        QStringLiteral("FRAG.json"),
        QStringLiteral("gameforpeace.json"),
        QStringLiteral("identityv.json"),
        QStringLiteral("test.json"),
        QStringLiteral("tiktok.json")
    };
    for (const QString &fixture : fixtures) {
        const QByteArray legacy = readFile(
            QDir(QString::fromUtf8(QSC_KEYMAP_FIXTURE_DIR)).filePath(fixture));
        SchemeStore migrated;
        QString error;
        check(!legacy.isEmpty(), QStringLiteral("fixture exists: %1").arg(fixture));
        check(!legacy.isEmpty()
                  && SchemeStore::migrateLegacyJson(legacy, fixture, &migrated, &error),
              QStringLiteral("legacy migration %1: %2").arg(fixture, error));
        check(migrated.schemes.size() == 1,
              QStringLiteral("legacy migration emits one local scheme: %1").arg(fixture));
        check(!migrated.toJson().isEmpty(),
              QStringLiteral("migrated legacy fixture validates: %1").arg(fixture));
        if (fixture == QStringLiteral("FRAG.json") && migrated.schemes.size() == 1) {
            bool foundAim = false;
            for (const std::shared_ptr<Widget> &widget : migrated.schemes.first().widgets) {
                const std::shared_ptr<AimWidget> aim = std::dynamic_pointer_cast<AimWidget>(widget);
                if (aim) {
                    foundAim = true;
                    check(std::fabs(aim->sensX - (1.0 / 3.25)) < 1e-9
                              && std::fabs(aim->sensY - (1.0 / 1.25)) < 1e-9,
                          QStringLiteral("legacy mouse sensitivity divisors preserve movement scale"));
                    break;
                }
            }
            check(foundAim, QStringLiteral("FRAG migration retains the aim mapping"));
        }
    }
}

void testAtomicSaveAndBackupMigration()
{
    QTemporaryDir temporary;
    check(temporary.isValid(), QStringLiteral("temporary directory creation"));
    if (!temporary.isValid()) return;

    const QString path = temporary.filePath(QStringLiteral("local.json"));
    const QByteArray legacy = R"JSON({
      "switchKey":"Key_QuoteLeft",
      "keyMapNodes":[{"type":"KMT_CLICK","key":"Key_F","pos":{"x":0.5,"y":0.5}}]
    })JSON";
    QFile source(path);
    check(source.open(QIODevice::WriteOnly) && source.write(legacy) == legacy.size(),
          QStringLiteral("write legacy test input"));
    source.close();

    QString backupPath;
    QString error;
    check(SchemeStore::migrateLegacyFileInPlace(path, &backupPath, &error),
          QStringLiteral("migrate file in place: %1").arg(error));
    check(QFileInfo::exists(backupPath), QStringLiteral("legacy backup exists"));
    check(readFile(backupPath) == legacy, QStringLiteral("legacy backup is byte-for-byte"));

    SchemeStore loaded;
    error.clear();
    check(loaded.loadFile(path, &error) && loaded.schemes.size() == 1,
          QStringLiteral("migrated file loads as v2: %1").arg(error));

    const QString savedPath = temporary.filePath(QStringLiteral("roundtrip.json"));
    error.clear();
    check(loaded.saveFile(savedPath, &error), QStringLiteral("atomic save: %1").arg(error));
    SchemeStore roundTrip;
    error.clear();
    check(roundTrip.loadFile(savedPath, &error) && roundTrip.toJson() == loaded.toJson(),
          QStringLiteral("atomic file write/read round trip: %1").arg(error));

    error.clear();
    check(!SchemeStore::migrateLegacyFileInPlace(path, nullptr, &error),
          QStringLiteral("existing backup is never overwritten"));
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    testKeyBinding();
    testVersionTwoReadWrite();
    testInvalidDocuments();
    testLegacyFixtures();
    testAtomicSaveAndBackupMigration();
    return failures == 0 ? 0 : 1;
}
