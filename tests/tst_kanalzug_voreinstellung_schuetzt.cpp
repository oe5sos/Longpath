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

#include <cmath>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

#include "core/AppSettings.h"
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

    // „kann ich die audioprofile nicht löschen" (2026-10-14).
    // Geprüft wird die Bedienung: Eintrag wählen, Knopf ansehen,
    // löschen, Liste ansehen.
    void eine_eigene_kette_laesst_sich_loeschen()
    {
        {
            StripChain vorbereiten;
            vorbereiten.prepare(48000.0);
            QVERIFY(StripSettings::saveUserPreset(QStringLiteral("Weg damit"),
                                                  vorbereiten));
            QVERIFY(StripSettings::saveUserPreset(QStringLiteral("Bleibt"),
                                                  vorbereiten));
        }

        RadioModel radio;
        StripWindow w(&radio);
        auto* box = w.findChild<QComboBox*>(QStringLiteral("stripPresetBox"));
        QVERIFY(box != nullptr);
        auto* knopf = w.findChild<QPushButton*>(
            QStringLiteral("stripPresetDelete"));
        QVERIFY2(knopf != nullptr, "der Löschknopf hat keinen Namen");

        const int at = box->findData(QStringLiteral("Weg damit"));
        QVERIFY2(at >= 0, "die eigene Kette steht nicht in der Liste");
        box->setCurrentIndex(at);
        QVERIFY2(knopf->isEnabled(),
                 "der Löschknopf ist aus, obwohl eine EIGENE Kette "
                 "gewählt ist");

        // Jetzt die Bedienung selbst: erster Druck fragt, zweiter
        // löscht. Seit dem 2026-10-14 steht kein modales Fenster mehr
        // dazwischen — eines, das nicht ankommt, lässt den Knopf tot
        // aussehen, und genau so war es am Gerät.
        knopf->click();
        QVERIFY2(StripSettings::userPresetNames()
                     .contains(QStringLiteral("Weg damit")),
                 "der erste Druck hat schon gelöscht — ohne Rückfrage");
        auto* zeile = w.findChild<QLabel*>(QStringLiteral("stripPresetNote"));
        QVERIFY2(zeile != nullptr, "die Hinweiszeile hat keinen Namen");
        QVERIFY2(zeile->text().contains(QStringLiteral("Weg damit")),
                 qPrintable(QStringLiteral("die Zeile sagt nichts: %1")
                                .arg(zeile->text())));

        knopf->click();
        QVERIFY2(!StripSettings::userPresetNames()
                      .contains(QStringLiteral("Weg damit")),
                 "der zweite Druck hat nicht gelöscht");
        QVERIFY2(StripSettings::userPresetNames()
                     .contains(QStringLiteral("Bleibt")),
                 "das Löschen der einen hat die andere mitgenommen");

        StripSettings::removeUserPreset(QStringLiteral("Bleibt"));
    }

    // ── Die Kette gehoert zum Mikrofon ──────────────────────────
    //
    // „wenn ich mit longpath arbeite, dann soll keine kette erfolgen.
    // wenn externes mikrofon, sprich teufel, dann kette"
    // (2026-10-14). Am Apollo steht die Bearbeitung schon in der
    // Console; eine zweite Kette danach macht sie zweimal.
    //
    // Geprüft wird über die BEDIENUNG: Gerät wählen, Haken setzen,
    // anderes Gerät wählen, zurückwechseln.
    void der_hauptschalter_gehoert_zum_mikrofon()
    {
        RadioModel radio;
        StripWindow w(&radio);
        auto* ein = w.findChild<QComboBox*>(QStringLiteral("stripLiveIn"));
        QVERIFY2(ein != nullptr, "das Eingangsfeld hat keinen Namen");
        auto* haken = w.findChild<QCheckBox*>(QStringLiteral("stripMaster"));
        QVERIFY2(haken != nullptr, "der Hauptschalter hat keinen Namen");
        if (ein->count() < 2) {
            QSKIP("diese Maschine hat nur eine Aufnahmequelle");
        }

        // Gerät A: Kette an.
        ein->setCurrentIndex(0);
        emit ein->activated(0);
        haken->setChecked(true);
        QVERIFY(radio.stripChain()->isEnabled());

        // Gerät B: Kette aus.
        ein->setCurrentIndex(1);
        emit ein->activated(1);
        haken->setChecked(false);
        QVERIFY(!radio.stripChain()->isEnabled());

        // Zurück auf A -- die Kette muss von selbst wiederkommen.
        ein->setCurrentIndex(0);
        emit ein->activated(0);
        QVERIFY2(haken->isChecked(),
                 "der Hauptschalter ist beim Zurückwechseln aus geblieben");
        QVERIFY2(radio.stripChain()->isEnabled(),
                 "die Kette läuft nicht, obwohl der Haken sitzt");

        // Und wieder B -- aus, ohne dass jemand etwas anfasst.
        ein->setCurrentIndex(1);
        emit ein->activated(1);
        QVERIFY2(!haken->isChecked(),
                 "der Hauptschalter ist beim Wechsel an geblieben");

        // Und ein Gerät, zu dem nichts gespeichert ist, bekommt KEINE
        // Kette -- es darf nicht den Stand des vorigen mitschleppen.
        if (ein->count() > 2) {
            ein->setCurrentIndex(0);
            emit ein->activated(0);          // A: Kette an
            QVERIFY(haken->isChecked());
            AppSettings::instance().remove(StripWindow::kettenSchluessel(
                ein->itemData(2).toByteArray()));
            ein->setCurrentIndex(2);
            emit ein->activated(2);
            QVERIFY2(!haken->isChecked(),
                     "ein unbekanntes Mikrofon hat die Kette des vorigen "
                     "geerbt");
        }

        // Aufräumen: alle Schlüssel wieder weg.
        for (int i = 0; i < ein->count(); ++i) {
            AppSettings::instance().remove(StripWindow::kettenSchluessel(
                ein->itemData(i).toByteArray()));
        }
    }

    // ── Die Eingangszeile lügt nicht mehr ───────────────────────
    //
    // Am 2026-10-14 stand dort „0,0 dBFS · 12 dB zu viel", während am
    // Gerät in Wahrheit -55 dBFS ankamen: bei abgeschaltetem
    // Hauptschalter misst die Kette nicht, und die Zeile behielt den
    // Zeigerstand aus einer früheren Sitzung mit einem anderen
    // Mikrofon. Das hat einen halben Abend gekostet.
    //
    // Geprüft wird über den echten Takt des Fensters, nicht durch
    // einen Aufruf von Hand: die Zeile wird von einem Zeitgeber
    // nachgezogen, und genau der soll laufen.
    void die_eingangszeile_behaelt_keinen_alten_zeigerstand()
    {
        RadioModel radio;
        StripChain* c = radio.stripChain();
        QVERIFY(c != nullptr);

        // ── Erst einen Zeigerstand erzeugen ─────────────────────
        //
        // Ohne diesen Schritt prüft der Fall nichts: in einem frischen
        // Vorgang steht die Zahl ohnehin auf „nichts gemessen", und
        // auch die ALTE Fassung hätte einen Strich gezeigt. Der
        // Fehler bestand darin, einen EINMAL gemessenen Wert über das
        // Abschalten hinaus stehen zu lassen.
        c->prepare(48000.0);
        c->setEnabled(true);
        constexpr int kBlock = 64;
        const int fenster = 48000 * 2 + kBlock;
        for (int n = 0; n < fenster; n += kBlock) {
            std::vector<float> blk(kBlock, 0.0f);
            if ((n / kBlock) % 10 == 0) {
                for (int i = 0; i < kBlock; ++i) {
                    const double t = double(n + i) / 48000.0;
                    blk[size_t(i)] = 0.45f * float(
                        std::sin(2.0 * M_PI * 220.0 * t));
                }
            }
            c->processMono(blk.data(), kBlock);
        }
        QVERIFY2(c->speechPeakDb() > -20.0f,
                 "der Prüfstand hat selbst keinen Pegel erzeugt");

        // Und jetzt aus -- ab hier misst nichts mehr.
        c->setEnabled(false);

        StripWindow w(&radio);
        auto* zeile = w.findChild<QLabel*>(QStringLiteral("stripEingangNote"));
        QVERIFY2(zeile != nullptr, "die Eingangszeile hat keinen Namen");

        QTest::qWait(700);        // zwei Takte der Anzeige
        QVERIFY2(!zeile->text().contains(QStringLiteral("dBFS"))
                     || zeile->text().contains(QStringLiteral("—")),
                 qPrintable(QStringLiteral("die Zeile behauptet einen Pegel, "
                                           "obwohl nichts misst: %1")
                                .arg(zeile->text())));

        auto* knopf = w.findChild<QPushButton*>(
            QStringLiteral("stripEingangBtn"));
        QVERIFY2(knopf != nullptr, "der Übernehmen-Knopf hat keinen Namen");
        QVERIFY2(!knopf->isEnabled(),
                 "„Übernehmen\" ist an, obwohl nichts gemessen wurde — "
                 "genau so sind am 2026-10-14 +36 dB gesetzt worden");
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
