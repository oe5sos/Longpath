// no-port-check: Longpath-original regression test.
//
// Phase 3J-1 closeout Item 7 (2026-05-12): pins the contract that
// AppSettings does NOT purge top-level keys on save/reload, regardless
// of whether the key matches any "known schema".  The user's memory
// note "Longpath purges unknown keys on save" turned out to be
// inaccurate -- audit of AppSettings::load / save / setValue / remove
// found zero code paths that filter top-level keys.  This test prevents
// any future change from accidentally introducing one.
//
// Coverage:
//   - User-set arbitrary key persists across save → reload
//   - User-set TciEmulate* keys persist (the canonical TCI bench gripe)
//   - A key with "False" string value round-trips (catches "empty-string
//     means absent" parser quirks)
//   - A key set to "" round-trips as ""
//
// If this test ever fails, the regression is almost certainly a new
// filter inside AppSettings::save() or load() -- start the audit at
// src/core/AppSettings.cpp.

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QXmlStreamReader>
#include <QFile>

#include "core/AppSettings.h"

using namespace Longpath;

class TstAppSettingsArbitraryKeyPersistence : public QObject {
    Q_OBJECT
private slots:
    // ── Zeichen, die XML in einem Elementnamen nicht erlaubt ────────
    //
    // Der Name einer eigenen Kette wird Teil des SCHLÜSSELS, und der
    // Schlüssel wird ein Elementname. Bis zum 2026-10-13 fing die
    // Tabelle nur ASCII-Sonderzeichen ab; ein Gedankenstrich (U+2014,
    // im gesperrten Bereich #x2000–#x206F) machte die Datei unlesbar.
    // Am Gerät des Betreibers ist das einmal passiert.
    //
    // Umlaute SIND in XML-Namen erlaubt und müssen unverändert
    // stehenbleiben -- sonst zeigen alte Einstellungsdateien plötzlich
    // auf Schlüssel, die es nicht mehr gibt.
    void keys_with_awkward_characters_keep_the_file_readable() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("probe.settings"));

        const QStringList schluessel{
            QStringLiteral("Kette/SSB \u2014 Glanz im Band/TubeDriveDb"),
            QStringLiteral("Kette/Grüße & Umlaute/Wert"),
            QStringLiteral("Kette/Pfeil \u2192 Ziel/Wert"),
            QStringLiteral("Kette/Smiley \U0001F600/Wert"),
        };
        {
            AppSettings s(path);
            for (int i = 0; i < schluessel.size(); ++i) {
                s.setValue(schluessel.at(i), QString::number(i + 1));
            }
            s.save();
        }

        // Ein fremder Leser muss die Datei öffnen können.
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray roh = f.readAll();
        QXmlStreamReader xml(roh);
        while (!xml.atEnd()) { xml.readNext(); }
        QVERIFY2(!xml.hasError(),
                 qPrintable(QStringLiteral("kein gültiges XML: %1")
                                .arg(xml.errorString())));

        // Umlaute bleiben, wie sie waren -- sonst finden alte Dateien
        // ihre eigenen Schlüssel nicht mehr.
        QVERIFY2(QString::fromUtf8(roh).contains(QStringLiteral("Grüße")),
                 "der Umlaut wurde unnötig umgeschrieben");

        // Und jeder Wert kommt unter seinem Namen zurück.
        {
            AppSettings s(path);
            s.load();   // der Prüf-Konstruktor lädt NICHT von selbst
            for (int i = 0; i < schluessel.size(); ++i) {
                QCOMPARE(s.value(schluessel.at(i)).toString(),
                         QString::number(i + 1));
            }
        }
    }

    // Eine Liste mit MEHR als einem Eintrag überlebte val.toString()
    // nicht: QVariant liefert dort eine leere Zeichenkette, und die
    // Liste war weg, noch bevor sie auf der Platte stand.
    void a_string_list_with_several_entries_survives() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("probe.settings"));

        const QStringList liste{ QStringLiteral("Eins"),
                                 QStringLiteral("Zwei, mit Komma"),
                                 QStringLiteral("Drei = mit Gleich"),
                                 QStringLiteral("Vier \u2014 mit Strich") };
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("Kette/Liste"), liste);
            s.save();
        }
        {
            AppSettings s(path);
            s.load();
            QCOMPARE(s.value(QStringLiteral("Kette/Liste")).toStringList(),
                     liste);
        }

        // Und eine Datei von FRÜHER, die denselben Schlüssel als
        // schlichten Text trägt, ergibt weiterhin einen Eintrag.
        const QString alt = tmp.filePath(QStringLiteral("alt.settings"));
        {
            QFile f(alt);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("<?xml version=\"1.0\"?>\n<Longpath>\n"
                    "  <Kette__s__Liste>NurEiner</Kette__s__Liste>\n"
                    "</Longpath>\n");
        }
        AppSettings s(alt);
        s.load();
        QCOMPARE(s.value(QStringLiteral("Kette/Liste")).toStringList(),
                 QStringList{ QStringLiteral("NurEiner") });
    }

    // Eine Datei, die eine ÄLTERE Fassung mit einem Gedankenstrich im
    // Schlüssel geschrieben hat, ist für jeden XML-Leser kaputt. Sie
    // wird jetzt beim Laden repariert, statt als verloren zu gelten --
    // genau diese Datei liegt am Gerät des Betreibers als
    // Longpath.settings.corrupt-20261013-214358.
    void an_old_file_with_a_dash_in_a_key_is_repaired_on_load() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("alt.settings"));
        {
            QFile f(path);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(QStringLiteral(
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Longpath>\n"
                        "  <Kette__s__SSB__sp__\u2014__sp__Glanz__s__Wert>42"
                        "</Kette__s__SSB__sp__\u2014__sp__Glanz__s__Wert>\n"
                        "  <Harmlos>7</Harmlos>\n"
                        "</Longpath>\n").toUtf8());
        }

        AppSettings s(path);
        s.load();
        // Der harmlose Schlüssel darf durch die Reparatur nicht
        // verlorengehen -- vorher war die GANZE Datei unlesbar.
        QCOMPARE(s.value(QStringLiteral("Harmlos")).toString(),
                 QStringLiteral("7"));
        QVERIFY2(!s.allKeys().isEmpty(), "nichts geladen");
        // Und der Schlüssel mit dem Gedankenstrich kommt unter seinem
        // richtigen Namen zurück.
        QCOMPARE(s.value(QStringLiteral("Kette/SSB \u2014 Glanz/Wert"))
                     .toString(),
                 QStringLiteral("42"));
    }

    void arbitrary_top_level_key_round_trips() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        // Cycle 1: set a key that no Longpath code path knows about.
        {
            AppSettings s(path);
            s.setValue(QStringLiteral("ArbitraryUserKey_42"),
                       QStringLiteral("hello"));
            s.save();
        }
        // Cycle 2: reload, key must still be present + correct value.
        {
            AppSettings s(path);
            s.load();
            QCOMPARE(s.value(QStringLiteral("ArbitraryUserKey_42"))
                         .toString(),
                     QStringLiteral("hello"));
        }
    }

    // The exact scenario that triggered Item 7: the operator pre-edits
    // TciEmulate* keys to True (or False) outside the running app, and
    // expects them to survive a save cycle.
    void tci_emulate_keys_round_trip() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        {
            AppSettings s(path);
            s.setValue(QStringLiteral("TciEmulateSunSDR2Pro"),
                       QStringLiteral("True"));
            s.setValue(QStringLiteral("TciEmulateExpertSDR3Protocol"),
                       QStringLiteral("False"));
            s.save();
        }
        {
            AppSettings s(path);
            s.load();
            QCOMPARE(s.value(QStringLiteral("TciEmulateSunSDR2Pro"))
                         .toString(),
                     QStringLiteral("True"));
            QCOMPARE(s.value(QStringLiteral("TciEmulateExpertSDR3Protocol"))
                         .toString(),
                     QStringLiteral("False"));
            QVERIFY(s.contains(QStringLiteral("TciEmulateSunSDR2Pro")));
            QVERIFY(s.contains(QStringLiteral("TciEmulateExpertSDR3Protocol")));
        }
    }

    // A key with empty-string value is NOT the same as an absent key.
    // QHash::insert("", "") + save must round-trip as a present key
    // with empty value, not as removal.
    void empty_string_value_round_trips() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        {
            AppSettings s(path);
            s.setValue(QStringLiteral("EmptyValueKey"), QString());
            s.save();
        }
        {
            AppSettings s(path);
            s.load();
            QVERIFY(s.contains(QStringLiteral("EmptyValueKey")));
            QCOMPARE(s.value(QStringLiteral("EmptyValueKey")).toString(),
                     QString());
        }
    }

    // Pin that the only paths that remove top-level keys are explicit:
    // remove(key), forgetRadio(mac), clearSavedRadios(), clearHardware
    // Values(mac).  None of those should touch an unrelated arbitrary
    // key.
    void scoped_removal_paths_do_not_touch_arbitrary_keys() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("NereusSDR.settings"));

        AppSettings s(path);
        s.setValue(QStringLiteral("ArbitraryUserKey_42"),
                   QStringLiteral("survive"));
        s.setValue(QStringLiteral("radios/AA:BB:CC:DD:EE:FF/name"),
                   QStringLiteral("test radio"));
        s.setValue(QStringLiteral("hardware/AA:BB:CC:DD:EE:FF/sampleRate"),
                   QStringLiteral("192000"));

        // forgetRadio + clearHardwareValues remove their scoped prefixes
        // but must leave the arbitrary key alone.
        s.forgetRadio(QStringLiteral("AA:BB:CC:DD:EE:FF"));
        s.clearHardwareValues(QStringLiteral("AA:BB:CC:DD:EE:FF"));

        QCOMPARE(s.value(QStringLiteral("ArbitraryUserKey_42")).toString(),
                 QStringLiteral("survive"));
        QVERIFY(!s.contains(
            QStringLiteral("radios/AA:BB:CC:DD:EE:FF/name")));
        QVERIFY(!s.contains(
            QStringLiteral("hardware/AA:BB:CC:DD:EE:FF/sampleRate")));
    }
};

QTEST_MAIN(TstAppSettingsArbitraryKeyPersistence)
#include "tst_app_settings_arbitrary_key_persistence.moc"
