// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_kanalzug_voreinstellung_schuetzt.cpp  (Longpath)
// =================================================================
//
// Eine Voreinstellung darf keine Arbeit vernichten.
//
// Am 2026-10-13 um 21:05 hat am Gerät des Betreibers ein Griff in die
// Profilliste eine von Hand eingestellte Kette überschrieben --
// Kompressor und Entzerrer waren danach die der eingebauten
// Voreinstellung, und nichts hat darauf hingewiesen. Seine nächste
// Frage lautete: „ich habe änderungen gemacht, aber würde die gerne
// speichern. sehe aber keine speicherung der profile."
//
// Die Antwort darauf ist nicht nur ein besser beschrifteter Knopf.
// Eine Voreinstellung ersetzt JEDE Stufe; wer sie aus Neugier anfasst,
// soll zurückkönnen. Also wandert der bestehende Stand vorher unter
// einen festen Namen -- ohne Rückfrage, ohne Fenster, und gleich
// danach in derselben Liste sichtbar.
//
// Geprüft wird über die BEDIENUNG: das Auswahlfeld, wie seine Hand es
// benutzt. Ein Prüfstand, der stattdessen applyPreset() ruft, prüft
// eine Funktion, die der Betreiber nie anfasst.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-13 — Original für Longpath von Martin Fischer,
//                 KI-gestützt über Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>

#include <QComboBox>

#include "core/Sprache.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"
#include "gui/applets/StripWindow.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstKanalzugVoreinstellungSchuetzt : public QObject
{
    Q_OBJECT
private slots:

    void ein_griff_in_die_liste_vernichtet_keine_arbeit()
    {
        RadioModel radio;
        StripWindow w(&radio);
        StripChain* c = radio.stripChain();
        QVERIFY(c != nullptr);

        // Eine Stunde Handarbeit, zusammengefasst in drei Werten.
        c->setStageEnabled(StripChain::Stage::Tube, true);
        c->tube().setDriveDb(13.5f);
        c->comp().setMakeupDb(7.25f);
        c->eq().setMasterGain(1.44f);

        auto* box = w.findChild<QComboBox*>(QStringLiteral("stripPresetBox"));
        QVERIFY2(box != nullptr, "das Auswahlfeld hat keinen Namen");

        // Irgendeine eingebaute Voreinstellung aus der Liste greifen --
        // genau wie seine Hand es tut.
        int ziel = -1;
        for (int i = 0; i < box->count(); ++i) {
            const QString n = box->itemData(i).toString();
            if (!n.isEmpty() && !box->itemData(i, Qt::UserRole + 1).toBool()) {
                ziel = i;
                break;
            }
        }
        QVERIFY2(ziel >= 0, "keine eingebaute Voreinstellung in der Liste");
        const QString name = box->itemData(ziel).toString();
        box->setCurrentIndex(ziel);        // löst applyPreset aus

        // Die Kette ist jetzt die der Voreinstellung -- das ist gewollt.
        QVERIFY2(!qFuzzyCompare(c->tube().driveDb(), 13.5f)
                     || !qFuzzyCompare(c->comp().makeupDb(), 7.25f),
                 qPrintable(QStringLiteral("die Voreinstellung „%1\" hat "
                                           "gar nichts verändert")
                                .arg(name)));

        // Und der vorherige Stand liegt abrufbar daneben.
        const QString sicherung = StripWindow::kLetzterStand();
        QVERIFY2(StripSettings::userPresetNames().contains(sicherung),
                 qPrintable(QStringLiteral("„%1\" fehlt in der Liste: %2")
                                .arg(sicherung,
                                     StripSettings::userPresetNames()
                                         .join(QLatin1Char(',')))));

        StripChain zurueck;
        zurueck.prepare(48000.0);
        QVERIFY(StripSettings::applyUserPreset(sicherung, zurueck));
        QVERIFY(qFuzzyCompare(zurueck.tube().driveDb(), 13.5f));
        QVERIFY(qFuzzyCompare(zurueck.comp().makeupDb(), 7.25f));
        QVERIFY(qFuzzyCompare(zurueck.eq().masterGain(), 1.44f));

        // Und er steht auch im Auswahlfeld, nicht nur in den
        // Einstellungen -- sonst findet ihn niemand.
        QVERIFY2(box->findData(sicherung) >= 0,
                 "die Sicherung steht nicht in der Liste");
        // Ausgewählt bleibt aber die Voreinstellung, die er wollte.
        QCOMPARE(box->currentData().toString(), name);

        StripSettings::removeUserPreset(sicherung);
    }

    // Die Sicherung darf sich nicht selbst überschreiben: wer sie
    // auswählt, um zurückzukehren, würde sonst beim Klick genau den
    // Stand darüberschreiben, den er zurückholen will.
    void die_sicherung_sichert_sich_nicht_selbst()
    {
        // Die Sicherung muss VOR dem Fenster stehen: die Liste wird
        // beim Aufbau gefüllt.
        {
            StripChain vorher;
            vorher.prepare(48000.0);
            vorher.tube().setDriveDb(3.0f);
            QVERIFY(StripSettings::saveUserPreset(StripWindow::kLetzterStand(),
                                                  vorher));
        }

        RadioModel radio;
        StripWindow w(&radio);
        StripChain* c = radio.stripChain();
        QVERIFY(c != nullptr);

        c->tube().setDriveDb(11.0f);
        auto* box = w.findChild<QComboBox*>(QStringLiteral("stripPresetBox"));
        QVERIFY(box != nullptr);
        const int at = box->findData(StripWindow::kLetzterStand());
        QVERIFY2(at >= 0, "die Sicherung steht nicht in der Liste");
        box->setCurrentIndex(at);

        // Zurückgekehrt, nicht überschrieben.
        QVERIFY(qFuzzyCompare(c->tube().driveDb(), 3.0f));

        StripChain probe;
        probe.prepare(48000.0);
        QVERIFY(StripSettings::applyUserPreset(StripWindow::kLetzterStand(),
                                               probe));
        QVERIFY2(qFuzzyCompare(probe.tube().driveDb(), 3.0f),
                 "die Sicherung hat sich beim Zurückkehren selbst "
                 "überschrieben");

        StripSettings::removeUserPreset(StripWindow::kLetzterStand());
    }
};

QTEST_MAIN(TstKanalzugVoreinstellungSchuetzt)
#include "tst_kanalzug_voreinstellung_schuetzt.moc"
