// no-port-check: Longpath-original test. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Temperaturen der Geraete im Kopf (Betreiber, 2026-11-18).
//
// Was hier festgenagelt wird:
//   - ohne Messwert steht NICHTS da (und der Kopf sieht aus wie vorher),
//   - mit Messwert erscheint der Eintrag, ohne verschwindet er wieder,
//   - mehrere Geraete stehen nebeneinander, in fester Reihenfolge,
//   - eine Warnstufe gibt es nur, wenn das Geraet sie selbst meldet,
//   - die Verdrahtung nimmt die Werte von den Signalen, die es schon
//     gibt, und raeumt ab, wenn ein Geraet geht.
//
// Dazu ein Zeichenwerkzeug: mit LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1
// legt `blaetter` den Kopf mit Beispielwerten als PNG ab, je Fall ein
// Blatt. Ohne die beiden ueberspringt es sich.

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMap>
#include <QMenuBar>
#include <QPalette>
#include <QPixmap>

#include "core/AcomConnection.h"
#include "core/AcomProtocol.h"
#include "core/AudioEngine.h"
#include "core/ConnectionState.h"
#include "core/Kpa500Connection.h"
#include "core/PaTempUnit.h"
#include "core/PgxlConnection.h"
#include "core/RadioStatus.h"
#include "core/Rf2ksConnection.h"
#include "core/SpeConnection.h"
#include "core/SpeProtocol.h"
#include "gui/DeviceTemperatureFeed.h"
#include "gui/StyleConstants.h"
#include "gui/TitleBar.h"
#include "gui/widgets/DeviceTemperatureStrip.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

using Strip = DeviceTemperatureStrip;
using Feed  = DeviceTemperatureFeed;

QString id(const char* s) { return QLatin1String(s); }

void baueDreiPlaetze(Strip& strip)
{
    strip.addSlot(QStringLiteral("radio"), QStringLiteral("FLEX"),
                  QStringLiteral("radio"));
    strip.addSlot(QStringLiteral("pgxl"), QStringLiteral("PGXL"),
                  QStringLiteral("pgxl"));
    strip.addSlot(QStringLiteral("spe"), QStringLiteral("SPE"),
                  QStringLiteral("spe"));
}

RadioStatus& status(RadioModel& model)
{
    // RadioModel gibt den Zustand nur lesend heraus; gesetzt wird er im
    // Betrieb von innen. Der Pruefstand steht hier an der Stelle des
    // Geraets.
    return const_cast<RadioStatus&>(model.radioStatus());
}

} // namespace

class TstDeviceTemperatureStrip : public QObject {
    Q_OBJECT

private slots:

    void init()
    {
        // Jeder Fall beginnt in Celsius, gleich was der vorige tat.
        PaTempUnitNotifier::setUnit(PaTempUnit::Celsius);
    }

    // ── Die Leiste ───────────────────────────────────────────────────

    void ohneMesswertStehtNichtsDa()
    {
        Strip strip;
        baueDreiPlaetze(strip);
        QCOMPARE(strip.count(), 0);
        QVERIFY(strip.texts().isEmpty());
        // Unsichtbar heisst: im Kopf nimmt sie keinen Platz.
        QVERIFY(strip.isHidden());
    }

    void mitMesswertErscheintDerEintrag()
    {
        Strip strip;
        baueDreiPlaetze(strip);
        strip.setReading(QStringLiteral("radio"), 28.8);
        QVERIFY(!strip.isHidden());
        QVERIFY(strip.hasReading(QStringLiteral("radio")));
        QCOMPARE(strip.text(QStringLiteral("radio")), QStringLiteral("FLEX 29°C"));
        QCOMPARE(strip.count(), 1);

        auto* label = strip.findChild<QLabel*>(QStringLiteral("deviceTemp_radio"));
        QVERIFY(label);
        QVERIFY(!label->isHidden());
        // Die Plaetze ohne Wert bleiben unsichtbar.
        auto* leer = strip.findChild<QLabel*>(QStringLiteral("deviceTemp_spe"));
        QVERIFY(leer);
        QVERIFY(leer->isHidden());
    }

    void ohneMesswertVerschwindetErWieder()
    {
        Strip strip;
        baueDreiPlaetze(strip);
        strip.setReading(QStringLiteral("radio"), 31.0);
        strip.setReading(QStringLiteral("spe"), 44.0, Strip::Unit::AsReported);
        QCOMPARE(strip.count(), 2);

        strip.clearReading(QStringLiteral("radio"));
        QVERIFY(!strip.hasReading(QStringLiteral("radio")));
        QCOMPARE(strip.text(QStringLiteral("radio")), QString());
        QCOMPARE(strip.texts(), QStringList{QStringLiteral("SPE 44°")});
        QVERIFY(!strip.isHidden());

        // Der letzte geht: die ganze Leiste ist wieder weg.
        strip.clearReading(QStringLiteral("spe"));
        QCOMPARE(strip.count(), 0);
        QVERIFY(strip.isHidden());
    }

    void mehrereGeraeteStehenNebeneinanderInFesterReihenfolge()
    {
        Strip strip;
        baueDreiPlaetze(strip);
        // Absichtlich verkehrt herum gemeldet: die Reihenfolge im Kopf
        // haengt an den Plaetzen, nicht daran, wer zuerst geantwortet hat.
        strip.setReading(QStringLiteral("spe"), 47.0, Strip::Unit::AsReported);
        strip.setReading(QStringLiteral("pgxl"), 41.2);
        strip.setReading(QStringLiteral("radio"), 29.0);
        QCOMPARE(strip.texts(),
                 (QStringList{QStringLiteral("FLEX 29°C"),
                              QStringLiteral("PGXL 41°C"),
                              QStringLiteral("SPE 47°")}));

        // Und sie stehen wirklich nebeneinander, nicht uebereinander.
        strip.show();
        QVERIFY(QTest::qWaitForWindowExposed(&strip));
        auto* a = strip.findChild<QLabel*>(QStringLiteral("deviceTemp_radio"));
        auto* b = strip.findChild<QLabel*>(QStringLiteral("deviceTemp_pgxl"));
        auto* c = strip.findChild<QLabel*>(QStringLiteral("deviceTemp_spe"));
        QVERIFY(a && b && c);
        QVERIFY2(a->geometry().right() < b->geometry().left(),
                 "FLEX muss links von PGXL stehen");
        QVERIFY2(b->geometry().right() < c->geometry().left(),
                 "PGXL muss links von SPE stehen");
        QCOMPARE(a->geometry().center().y(), c->geometry().center().y());
    }

    void unbekannterPlatzUndUnsinnWerdenUebergangen()
    {
        Strip strip;
        baueDreiPlaetze(strip);
        strip.setReading(QStringLiteral("gibtesnicht"), 50.0);
        strip.setReading(QStringLiteral("radio"), qQNaN());
        QCOMPARE(strip.count(), 0);
        QVERIFY(strip.isHidden());
    }

    void dieZahlZappeltNicht()
    {
        // Die Flex meldet in Vierundsechzigsteln. Pendelt die Messung um
        // eine halbe Stufe, darf die Ziffer im Kopf nicht mitpendeln.
        Strip strip;
        baueDreiPlaetze(strip);
        strip.setReading(QStringLiteral("radio"), 29.0);
        for (const double v : {29.4, 29.6, 29.45, 29.7, 28.4}) {
            strip.setReading(QStringLiteral("radio"), v);
            QCOMPARE(strip.text(QStringLiteral("radio")),
                     QStringLiteral("FLEX 29°C"));
        }
        // Echte Aenderung: sie folgt.
        strip.setReading(QStringLiteral("radio"), 30.2);
        QCOMPARE(strip.text(QStringLiteral("radio")), QStringLiteral("FLEX 30°C"));
        strip.setReading(QStringLiteral("radio"), 38.7);
        QCOMPARE(strip.text(QStringLiteral("radio")), QStringLiteral("FLEX 39°C"));
    }

    void fahrenheitNurWoCelsiusFeststeht()
    {
        Strip strip;
        baueDreiPlaetze(strip);
        strip.setReading(QStringLiteral("radio"), 30.0);
        strip.setReading(QStringLiteral("spe"), 44.0, Strip::Unit::AsReported);

        PaTempUnitNotifier::setUnit(PaTempUnit::Fahrenheit);
        QCOMPARE(strip.text(QStringLiteral("radio")), QStringLiteral("FLEX 86°F"));
        // Der SPE sagt nicht, in welcher Einheit er meldet: seine Zahl
        // wird nicht umgerechnet und bekommt keinen Buchstaben.
        QCOMPARE(strip.text(QStringLiteral("spe")), QStringLiteral("SPE 44°"));

        PaTempUnitNotifier::setUnit(PaTempUnit::Celsius);
        QCOMPARE(strip.text(QStringLiteral("radio")), QStringLiteral("FLEX 30°C"));
    }

    void warnstufeNurAufMeldungUndSieGehtMitDemWert()
    {
        Strip strip;
        baueDreiPlaetze(strip);
        // Auch eine sehr hohe Zahl macht von sich aus keine Warnung:
        // die Leiste kennt keine Schwelle.
        strip.setReading(QStringLiteral("radio"), 95.0);
        QCOMPARE(strip.alert(QStringLiteral("radio")), Strip::Alert::None);

        strip.setReading(QStringLiteral("spe"), 71.0, Strip::Unit::AsReported);
        strip.setAlert(QStringLiteral("spe"), Strip::Alert::Warning,
                       QStringLiteral("Overheating"));
        QCOMPARE(strip.alert(QStringLiteral("spe")), Strip::Alert::Warning);
        auto* label = strip.findChild<QLabel*>(QStringLiteral("deviceTemp_spe"));
        QVERIFY(label);
        QVERIFY(label->toolTip().contains(QStringLiteral("Overheating")));
        // Die Zahl bleibt dieselbe: der Rahmen ist der Zustand.
        QCOMPARE(strip.text(QStringLiteral("spe")), QStringLiteral("SPE 71°"));

        // Geraet weg, Geraet wieder da: die alte Meldung steht nicht mehr.
        strip.clearReading(QStringLiteral("spe"));
        strip.setReading(QStringLiteral("spe"), 40.0, Strip::Unit::AsReported);
        QCOMPARE(strip.alert(QStringLiteral("spe")), Strip::Alert::None);
        QVERIFY(!label->toolTip().contains(QStringLiteral("Overheating")));
    }

    // ── Der Kopf ─────────────────────────────────────────────────────

    void imKopfUnsichtbarSolangeNichtsGemeldetIst()
    {
        AudioEngine engine;
        TitleBar bar(&engine);
        Strip* strip = bar.deviceTemperatures();
        QVERIFY(strip);
        QVERIFY(strip->isHidden());
    }

    void ohneMesswertIstDerKopfWieVorher()
    {
        AudioEngine engine;
        TitleBar bar(&engine);
        const QSize leer = bar.sizeHint();
        Strip* strip = bar.deviceTemperatures();
        // Plaetze allein aendern nichts: versteckt zaehlt die Leiste im
        // Layout nicht mit.
        strip->addSlot(QStringLiteral("radio"), QStringLiteral("FLEX"),
                       QStringLiteral("radio"));
        QCOMPARE(bar.sizeHint(), leer);
        strip->setReading(QStringLiteral("radio"), 29.0);
        QVERIFY(bar.sizeHint().width() > leer.width());
        strip->clearReading(QStringLiteral("radio"));
        QCOMPARE(bar.sizeHint(), leer);
    }

    void imKopfRechtsNebenNameUndFassung()
    {
        AudioEngine engine;
        TitleBar bar(&engine);
        bar.resize(1440, bar.sizeHint().height());
        Strip* strip = bar.deviceTemperatures();
        strip->addSlot(QStringLiteral("radio"), QStringLiteral("FLEX"),
                       QStringLiteral("radio"));
        strip->setReading(QStringLiteral("radio"), 29.0);
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar));
        QTest::qWait(50);

        auto* fassung = bar.findChild<QLabel*>(QStringLiteral("titleBarVersion"));
        auto* stand = bar.findChild<QLabel*>(QStringLiteral("titleBarUpdateState"));
        QVERIFY(fassung && stand);
        QVERIFY(strip->isVisible());
        QCOMPARE(strip->visibleCount(), 1);
        QVERIFY2(strip->geometry().left() > stand->geometry().right(),
                 "die Temperaturen stehen rechts vom Stand der Aktualisierung");
        QVERIFY(strip->geometry().left() > fassung->geometry().right());
        // … und gleich daneben, nicht drueben bei der Uhr.
        QVERIFY(strip->geometry().left() - stand->geometry().right() < 10);
        QVERIFY(strip->geometry().width() > 30);
        // Senkrecht in der Mitte des Kopfes.
        auto* eintrag = strip->findChild<QLabel*>(QStringLiteral("deviceTemp_radio"));
        QVERIFY(eintrag);
        const int mitte = eintrag->mapTo(&bar, eintrag->rect().center()).y();
        QVERIFY2(qAbs(mitte - bar.height() / 2) <= 1,
                 qPrintable(QStringLiteral("Mitte %1 von %2").arg(mitte).arg(bar.height())));

        // Wechselt der Text links davon seine Breite, rueckt sie nach.
        bar.setUpdateState(QStringLiteral("· a much longer state text"),
                           TitleBar::AktualisierungsStand::Unbekannt);
        QTest::qWait(50);
        QVERIFY(strip->geometry().left() > stand->geometry().right());
        QVERIFY(strip->geometry().left() - stand->geometry().right() < 10);
    }

    void dieLeisteNimmtNurFreienPlatzUndDerErsteBleibt()
    {
        // Am echten Kopf gemessen (2026-11-18): mit den neun Menues
        // braucht er 1389 Punkte, das Notebook des Betreibers hat 1470.
        // Die Leiste darf dem Menue nichts wegnehmen — bis auf den
        // einen Eintrag, ohne den man sie dort nie saehe.
        //
        // Der Kopf steht hier in einem Rahmen ohne Layout, so wie er im
        // Programm im Hauptfenster steht: seine Breite bekommt er
        // gesagt. Als eigenes Fenster liesse er sich nicht schmaler
        // ziehen, als sein Layout mag, und der volle Kopf kaeme nie vor.
        AudioEngine engine;
        QWidget rahmen;
        TitleBar bar(&engine, &rahmen);
        auto* menue = new QMenuBar;
        for (const char* m : {"File", "Radio", "View", "DSP", "Band", "Mode",
                              "Containers", "Tools", "Help"}) {
            menue->addMenu(QLatin1String(m));
        }
        bar.setMenuBar(menue);
        const int ohne = bar.sizeHint().width();

        Strip* strip = bar.deviceTemperatures();
        const QVector<const char*> plaetze =
            {"radio", "pgxl", "spe", "kpa500", "acom"};
        for (const char* p : plaetze) {
            strip->addSlot(QLatin1String(p), QStringLiteral("GERAET"),
                           QString());
            strip->setReading(QLatin1String(p), 40.0);
        }
        QLabel* uhr = nullptr;
        for (QLabel* l : bar.findChildren<QLabel*>()) {
            if (l->text().endsWith(QStringLiteral("UTC"))) { uhr = l; }
        }
        QVERIFY(uhr);

        // Reichlich Platz: alle fuenf.
        rahmen.resize(ohne + 1000, bar.sizeHint().height());
        bar.setGeometry(0, 0, ohne + 900, bar.sizeHint().height());
        rahmen.show();
        QVERIFY(QTest::qWaitForWindowExposed(&rahmen));
        QTest::qWait(50);
        QCOMPARE(strip->visibleCount(), 5);
        QVERIFY(strip->geometry().right() < uhr->geometry().left());
        QCOMPARE(menue->width(), menue->sizeHint().width());
        const int einer = strip->firstEntryWidth();
        QVERIFY(einer > 30);

        // Schmaler: es werden weniger, nie mehr; gemeldet bleiben alle
        // fuenf; und solange mehr als einer steht, hat das Menue seine
        // volle Breite — die Leiste lebt dann nur von freiem Platz.
        int vorher = 5;
        for (const int luft : {400, 300, 200, 120, 40, 0, -60}) {
            bar.setGeometry(0, 0, ohne + luft, bar.height());
            QTest::qWait(50);
            const int n = strip->visibleCount();
            QVERIFY2(n <= vorher && n >= 1,
                     qPrintable(QStringLiteral("Luft %1: %2 Eintraege")
                                    .arg(luft).arg(n)));
            vorher = n;
            QCOMPARE(strip->count(), 5);
            if (n > 1) {
                QVERIFY2(menue->width() >= menue->sizeHint().width(),
                         qPrintable(QStringLiteral("Luft %1: Menue %2 von %3")
                                        .arg(luft).arg(menue->width())
                                        .arg(menue->sizeHint().width())));
            }
        }
        // Voller Kopf: genau einer, und zwar der erste Platz.
        QCOMPARE(vorher, 1);
        auto* erster = strip->findChild<QLabel*>(QStringLiteral("deviceTemp_radio"));
        QVERIFY(erster && !erster->isHidden());

        // Und wieder breit: sie kommen zurueck.
        bar.setGeometry(0, 0, ohne + 900, bar.height());
        QTest::qWait(50);
        QCOMPARE(strip->visibleCount(), 5);

        // Faellt der erste weg, rueckt der naechste auf den festen Platz.
        bar.setGeometry(0, 0, ohne, bar.height());
        QTest::qWait(50);
        QCOMPARE(strip->visibleCount(), 1);
        strip->clearReading(QStringLiteral("radio"));
        QTest::qWait(50);
        QCOMPARE(strip->visibleCount(), 1);
        auto* zweiter = strip->findChild<QLabel*>(QStringLiteral("deviceTemp_pgxl"));
        QVERIFY(zweiter && !zweiter->isHidden());
    }

    // ── Die Verdrahtung ──────────────────────────────────────────────

    void kuerzelDesFunkgeraets()
    {
        QCOMPARE(Feed::radioLabel(true, HPSDRModel::HERMES),
                 QStringLiteral("FLEX"));
        QCOMPARE(Feed::radioLabel(false, HPSDRModel::HERMESLITE),
                 QStringLiteral("HL2"));
        QCOMPARE(Feed::radioLabel(false, HPSDRModel::HERMES),
                 QStringLiteral("RADIO"));
        // Die HL2 meldet ihr FPGA, nicht die Endstufe -- das muss im
        // Tooltip stehen.
        QVERIFY(Feed::radioDescription(false, HPSDRModel::HERMESLITE)
                    .contains(QStringLiteral("FPGA")));
    }

    void ohneGeraetBleibtDerKopfLeer()
    {
        RadioModel model;
        Strip strip;
        Feed feed(&model, &strip);
        QCOMPARE(strip.count(), 0);
        QVERIFY(strip.isHidden());

        // Ein Wert in RadioStatus OHNE verbundenes Geraet ist kein
        // Messwert (RadioStatus behaelt den letzten ueber das Trennen).
        status(model).setPaTemperature(33.0);
        QCOMPARE(strip.count(), 0);
        QVERIFY(strip.isHidden());
    }

    void funkgeraetErscheintMitWertUndGehtMitDerVerbindung()
    {
        RadioModel model;
        Strip strip;
        Feed feed(&model, &strip);

        emit model.connectionStateChanged(ConnectionState::Connected);
        // Verbunden, aber noch nichts gemeldet: weiter leer. Eine ANAN
        // bleibt so, sie meldet keine Temperatur.
        QCOMPARE(strip.count(), 0);

        status(model).setPaTemperature(28.8);
        QCOMPARE(strip.texts(), QStringList{QStringLiteral("RADIO 29°C")});

        // Null ist der Ruhewert, keine Messung.
        status(model).setPaTemperature(0.0);
        QCOMPARE(strip.texts(), QStringList{QStringLiteral("RADIO 29°C")});

        emit model.connectionStateChanged(ConnectionState::LinkLost);
        QCOMPARE(strip.count(), 0);
        QVERIFY(strip.isHidden());

        // Getrennt gemeldete Werte zaehlen nicht.
        status(model).setPaTemperature(31.0);
        QCOMPARE(strip.count(), 0);
    }

    void pgxl()
    {
        RadioModel model;
        PgxlConnection* pgxl = model.pgxlConnection();
        if (!pgxl) { QSKIP("kein PgxlConnection in diesem Bau"); }
        Strip strip;
        Feed feed(&model, &strip);

        // Ein Zustand ohne Temperatur ist keine Temperatur.
        emit pgxl->statusUpdated({{QStringLiteral("state"), QStringLiteral("IDLE")}});
        QCOMPARE(strip.count(), 0);

        emit pgxl->statusUpdated({{QStringLiteral("temp"), QStringLiteral("41.2")}});
        QCOMPARE(strip.texts(), QStringList{QStringLiteral("PGXL 41°C")});

        emit pgxl->disconnected();
        QCOMPARE(strip.count(), 0);
    }

    void speMitEigenerWarnung()
    {
        RadioModel model;
        SpeConnection* spe = model.speConnection();
        if (!spe) { QSKIP("kein SpeConnection in diesem Bau"); }
        Strip strip;
        Feed feed(&model, &strip);

        Spe::Status s;
        s.tempUpper = 47;
        emit spe->statusUpdated(s);
        QCOMPARE(strip.texts(), QStringList{QStringLiteral("SPE 47°")});
        QCOMPARE(strip.alert(id(Feed::kSpe)), Strip::Alert::None);

        // Eine Warnung, die nicht von der Temperatur handelt (Stehwelle),
        // gehoert nicht an die Temperaturzahl.
        s.warning = QLatin1Char('S');
        emit spe->statusUpdated(s);
        QCOMPARE(strip.alert(id(Feed::kSpe)), Strip::Alert::None);

        // Das Geraet meldet Uebertemperatur.
        s.warning = QLatin1Char('O');
        s.tempUpper = 72;
        emit spe->statusUpdated(s);
        QCOMPARE(strip.alert(id(Feed::kSpe)), Strip::Alert::Warning);
        QCOMPARE(strip.text(id(Feed::kSpe)), QStringLiteral("SPE 72°"));

        s.warning = QLatin1Char('N');
        s.alarm   = QLatin1Char('H');
        emit spe->statusUpdated(s);
        QCOMPARE(strip.alert(id(Feed::kSpe)), Strip::Alert::Alarm);

        // Meldung zurueckgenommen.
        s.alarm = QLatin1Char('N');
        emit spe->statusUpdated(s);
        QCOMPARE(strip.alert(id(Feed::kSpe)), Strip::Alert::None);

        // Ueber ser2net bleibt die Strecke stehen, wenn die Endstufe
        // ausgeht: „antwortet nicht mehr" raeumt ab.
        emit spe->respondingChanged(false);
        QCOMPARE(strip.count(), 0);
    }

    void acomMitModellversatz()
    {
        RadioModel model;
        AcomConnection* acom = model.acomConnection();
        if (!acom) { QSKIP("kein AcomConnection in diesem Bau"); }
        Strip strip;
        Feed feed(&model, &strip);

        const int versatz =
            Acom::modelSpec(acom->currentModel()).temperatureOffset;
        Acom::Telemetry t;
        // Roh null: nichts gemeldet, kein Eintrag.
        emit acom->telemetryUpdated(t);
        QCOMPARE(strip.count(), 0);

        t.paTempRaw = static_cast<quint16>(versatz + 52);
        emit acom->telemetryUpdated(t);
        QCOMPARE(strip.texts(), QStringList{QStringLiteral("ACOM 52°C")});

        emit acom->disconnected();
        QCOMPARE(strip.count(), 0);
    }

    void rf2ks()
    {
        RadioModel model;
        Rf2ksConnection* rf2k = model.rfKitConnection();
        if (!rf2k) { QSKIP("kein Rf2ksConnection in diesem Bau"); }
        Strip strip;
        Feed feed(&model, &strip);

        RfKitPowerSnapshot snap;
        emit rf2k->powerUpdated(snap);
        QCOMPARE(strip.count(), 0);

        snap.temperatureC = 38.0f;
        emit rf2k->powerUpdated(snap);
        QCOMPARE(strip.texts(), QStringList{QStringLiteral("RF2K 38°C")});
        // Im Baum stehen fuer den RF2K-S geratene Schwellen (55/80).
        // Der Kopf benutzt sie nicht.
        snap.temperatureC = 90.0f;
        emit rf2k->powerUpdated(snap);
        QCOMPARE(strip.alert(id(Feed::kRf2ks)), Strip::Alert::None);

        emit rf2k->disconnected();
        QCOMPARE(strip.count(), 0);
    }

    void kpa500OhneLaufendeEndstufeKeinEintrag()
    {
        RadioModel model;
        Kpa500Connection* kpa = model.kpa500Connection();
        if (!kpa) { QSKIP("kein Kpa500Connection in diesem Bau"); }
        Strip strip;
        Feed feed(&model, &strip);

        // Kein Geraet am Draht: lastTemperature() ist 0, der Zustand
        // nicht „laeuft". Beides allein reicht, um nichts zu zeigen.
        emit kpa->telemetryUpdated();
        QCOMPARE(strip.count(), 0);
        emit kpa->livenessChanged(Kpa500Connection::Liveness::Running);
        emit kpa->telemetryUpdated();
        QCOMPARE(strip.count(), 0);
    }

    void funkgeraetUndEndstufenNebeneinander()
    {
        RadioModel model;
        PgxlConnection* pgxl = model.pgxlConnection();
        SpeConnection*  spe  = model.speConnection();
        if (!pgxl || !spe) { QSKIP("Endstufen-Verbindungen fehlen"); }
        Strip strip;
        Feed feed(&model, &strip);

        Spe::Status s;
        s.tempUpper = 44;
        emit spe->statusUpdated(s);
        emit pgxl->statusUpdated({{QStringLiteral("temp"), QStringLiteral("39")}});
        emit model.connectionStateChanged(ConnectionState::Connected);
        status(model).setPaTemperature(27.3);

        QCOMPARE(strip.texts(),
                 (QStringList{QStringLiteral("RADIO 27°C"),
                              QStringLiteral("PGXL 39°C"),
                              QStringLiteral("SPE 44°")}));

        // Das Funkgeraet geht, die Endstufen bleiben.
        emit model.connectionStateChanged(ConnectionState::Disconnected);
        QCOMPARE(strip.texts(),
                 (QStringList{QStringLiteral("PGXL 39°C"),
                              QStringLiteral("SPE 44°")}));
    }

    // ── Zeichenwerkzeug ──────────────────────────────────────────────

    void blaetter()
    {
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (dir.isEmpty()
            || qEnvironmentVariable("LONGPATH_ENTWURF") != QStringLiteral("1")) {
            QSKIP("braucht LONGPATH_GRAB_DIR und LONGPATH_ENTWURF=1");
        }
        QVERIFY(QDir().mkpath(dir));

        struct Eintrag {
            const char* platz;
            const char* kuerzel;
            double      grad;
            Strip::Unit einheit;
            Strip::Alert meldung;
            const char* text;
        };
        struct Blatt {
            const char*      datei;
            int              breite;
            QVector<Eintrag> eintraege;
        };
        const Strip::Unit C = Strip::Unit::Celsius;
        const Strip::Unit R = Strip::Unit::AsReported;
        const Strip::Alert keine = Strip::Alert::None;
        // 1470 ist das Notebook, an dem entschieden wird; dort ist der
        // Kopf fast voll. 1920 und 2560 zeigen, was bei Platz dasteht.
        const QVector<Blatt> faelle = {
            {"kopf-1470-1-ohne-geraet.png", 1470, {}},
            {"kopf-1470-2-flex-allein.png", 1470,
             {{"radio", "FLEX", 28.8, C, keine, ""}}},
            {"kopf-1470-3-flex-und-zwei-endstufen.png", 1470,
             {{"radio", "FLEX", 34.9, C, keine, ""},
              {"pgxl", "PGXL", 41.0, C, keine, ""},
              {"spe", "SPE", 47.0, R, keine, ""}}},
            {"kopf-1920-1-flex-und-zwei-endstufen.png", 1920,
             {{"radio", "FLEX", 34.9, C, keine, ""},
              {"pgxl", "PGXL", 41.0, C, keine, ""},
              {"spe", "SPE", 47.0, R, keine, ""}}},
            {"kopf-1920-2-spe-warnt.png", 1920,
             {{"radio", "FLEX", 38.7, C, keine, ""},
              {"spe", "SPE", 72.0, R, Strip::Alert::Warning, "Overheating"}}},
            {"kopf-1920-3-spe-alarm.png", 1920,
             {{"radio", "FLEX", 38.7, C, keine, ""},
              {"spe", "SPE", 81.0, R, Strip::Alert::Alarm,
               "Excess overheating"}}},
            {"kopf-2560-alle-fuenf.png", 2560,
             {{"radio", "HL2", 44.0, C, keine, ""},
              {"pgxl", "PGXL", 41.0, C, keine, ""},
              {"spe", "SPE", 47.0, R, keine, ""},
              {"kpa500", "KPA", 36.0, C, keine, ""},
              {"acom", "ACOM", 52.0, C, keine, ""}}},
        };

        for (const Blatt& b : faelle) {
            AudioEngine engine;
            QWidget rahmen;
            TitleBar bar(&engine, &rahmen);
            // Die neun Menues des Programms: ohne sie saehe der Kopf
            // leerer aus, als er ist.
            auto* menue = new QMenuBar;
            for (const char* m : {"File", "Radio", "View", "DSP", "Band",
                                  "Mode", "Containers", "Tools", "Help"}) {
                menue->addMenu(QLatin1String(m));
            }
            bar.setMenuBar(menue);
            bar.setUpdateState(QStringLiteral("· up to date"),
                               TitleBar::AktualisierungsStand::Aktuell);
            Strip* strip = bar.deviceTemperatures();
            for (const char* platz : {"radio", "pgxl", "spe", "kpa500", "acom"}) {
                strip->addSlot(QLatin1String(platz), QLatin1String(platz),
                               QString());
            }
            for (const Eintrag& e : b.eintraege) {
                strip->setSlotLabel(QLatin1String(e.platz),
                                    QLatin1String(e.kuerzel), QString());
                strip->setReading(QLatin1String(e.platz), e.grad, e.einheit);
                strip->setAlert(QLatin1String(e.platz), e.meldung,
                                QLatin1String(e.text));
            }
            // Der dunkle Grund kommt im Programm vom Hauptfenster; ein
            // alleinstehender Kopf stuende sonst auf dem hellen Grau der
            // Systempalette, und darauf laesst sich keine Farbe beurteilen.
            QPalette grund = bar.palette();
            grund.setColor(QPalette::Window, QColor(Style::kStatusBarBg));
            bar.setPalette(grund);
            bar.setAutoFillBackground(true);
            rahmen.resize(b.breite, bar.sizeHint().height());
            bar.setGeometry(0, 0, b.breite, bar.sizeHint().height());
            rahmen.show();
            QVERIFY(QTest::qWaitForWindowExposed(&rahmen));
            QTest::qWait(150);

            // Zweifach gerastert, damit das Blatt in der Vorschau bei
            // 144 dpi in wirklicher Groesse steht.
            QPixmap bild(bar.size() * 2);
            bild.setDevicePixelRatio(2.0);
            bild.fill(Qt::transparent);
            bar.render(&bild);
            const QString pfad = QDir(dir).filePath(QLatin1String(b.datei));
            // Nie einen frueheren Entwurf ueberschreiben.
            QVERIFY2(!QFile::exists(pfad),
                     qPrintable(QStringLiteral("%1 gibt es schon").arg(pfad)));
            QVERIFY2(bild.save(pfad), qPrintable(pfad));
            qInfo() << "Blatt:" << b.datei << "gemeldet" << strip->count()
                    << "zu sehen" << strip->visibleCount()
                    << "Menue" << menue->width() << "von"
                    << menue->sizeHint().width();
        }
    }
};

QTEST_MAIN(TstDeviceTemperatureStrip)
#include "tst_device_temperature_strip.moc"
