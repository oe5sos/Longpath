// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Dauerverbindung zum Begleitgeraet.
//
// Stufe 1 hatte den CAT-Client in der Einstellseite. Eine SetupPage
// wird bei jedem Oeffnen neu gebaut: die Verbindung war beim ersten
// Blick auf eine andere Seite weg. Stufe 2 gibt sie RadioModel.
//
// Geprueft wird hier, was an dieser Klasse ENTSCHEIDUNGEN sind, nicht
// Verdrahtung:
//
//   1. Sie verbindet nicht von selbst, wenn der Lauf das nicht darf.
//      Die Lehre vom 2026-09-17: eine Sonde warf mit dem Rufzeichen
//      des Betreibers dessen eigene Cluster-Sitzung aus dem Netz, weil
//      nur die DATEIEN isoliert waren. Ein rigctld, das wir starten,
//      belegt dazu einen seriellen Anschluss -- womoeglich den, an dem
//      das echte Geraet haengt.
//
//   2. Sie merkt, wenn die Adresse auf Longpaths EIGENEN rigctld-Server
//      zeigt. Beide stehen nach Vorgabe auf 127.0.0.1:4532. Ohne diese
//      Pruefung fragt Longpath sich selbst, die Marke klebt auf der
//      eigenen Scheibe, und nichts warnt.
//
//   3. Sie rechnet den Transverter-Versatz nur dort hinein, wo er
//      hingehoert -- und legt ihn beim Laden als prozessweite Fassung
//      ab, weil vier weit auseinanderliegende Stellen ihn lesen.
//
//   4. Abstimmen geht nur, wenn eine Verbindung steht. Ein Klick in
//      einen Panadapter ohne Begleitgeraet darf nichts ausloesen und
//      sich auch nicht beschweren.

#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/CompanionRigController.h"
#include "core/TransverterSetup.h"

using namespace Longpath;

namespace {

constexpr qint64 k28_300  = 28300000;
constexpr qint64 k144_300 = 144300000;

void setzeEin(const QString& schluessel, const QString& wert)
{
    AppSettings::instance().setValue(schluessel, wert);
}

TransverterSetup zweiMeter()
{
    TransverterSetup t;
    t.enabled  = true;
    t.ifLowHz  = 28000000;
    t.ifHighHz = 30000000;
    t.offsetHz = 116000000;
    return t;
}

} // namespace

class TstBegleitgeraetRegler : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        // ── Diesen Lauf erst zur Sonde machen ────────────────────────
        //
        // AppSettings::isSandbox() haengt ALLEIN an LONGPATH_CONFIG_DIR.
        // TestSandboxInit setzt es nicht (es schaltet
        // QStandardPaths::setTestModeEnabled ein), und
        // tools/run_tests.sh setzt es auch nicht. In einem gewoehnlichen
        // ctest-Lauf ist darfVonSelbstVerbinden() daher WAHR -- jedes
        // Pruefprogramm darf ins Netz.
        //
        // Gefunden hat das die Voraussetzungspruefung weiter unten:
        // ohne sie waere inDerSondeVerbindetNichtVonSelbst() gruen
        // gewesen, WEIL die Voraussetzung ausfiel. Der Fall haette dann
        // nichts geprueft und das jahrelang stillschweigend getan.
        //
        // Hier wird die Bedingung also selbst hergestellt. Dass sie im
        // ganzen Baum fehlt, ist ein eigener Befund und eine eigene
        // Aenderung -- sie betrifft 539 Pruefstaende und gehoert nicht
        // in diesen.
        QVERIFY(m_konfig.isValid());
        qputenv("LONGPATH_CONFIG_DIR", m_konfig.path().toUtf8());
        QVERIFY(AppSettings::isSandbox());
    }

    void cleanupTestCase()
    {
        qunsetenv("LONGPATH_CONFIG_DIR");
    }

    void init()
    {
        // Jeder Fall faengt ohne Transverter an. Sonst traegt ein Fall
        // den Zustand des vorigen -- und genau diese Art Fehler sucht
        // man am laengsten, weil der Pruefstand allein gruen ist.
        TransverterSetup::setCurrent(TransverterSetup{});

        setzeEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("False"));
        setzeEin(QStringLiteral("CompanionRigStartDaemon"), QStringLiteral("False"));
        setzeEin(QStringLiteral("RigctldEnabled"), QStringLiteral("False"));
        AppSettings::instance().setValue(QStringLiteral("Transverter/Enabled"),
                                        QStringLiteral("False"));
    }

    // ── 1. Nicht von selbst ─────────────────────────────────────────

    void ausgeschaltetVerbindetNicht()
    {
        CompanionRigController r;
        QVERIFY(!r.aktiv());
        r.starteWennGewuenscht();
        QVERIFY(!r.verbunden());
        // Kein Client angelegt: es gab nichts zu verbinden.
        QVERIFY(r.client() == nullptr);
    }

    void inDerSondeVerbindetNichtVonSelbst()
    {
        // Der Pruefstand IST die Sonde: TestSandboxInit schaltet den
        // Sandkasten ein, also ist darfVonSelbstVerbinden() hier falsch.
        // Faellt diese Zusicherung um, ist nicht dieser Fall kaputt --
        // dann greift die Isolierung nicht mehr, und das ist der
        // groessere Befund.
        QVERIFY2(!AppSettings::darfVonSelbstVerbinden(),
                 "Der Pruefstand laeuft nicht isoliert -- dann prueft dieser "
                 "Fall nichts.");

        setzeEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("CompanionRigHost"), QStringLiteral("127.0.0.1"));
        setzeEin(QStringLiteral("CompanionRigPort"), QStringLiteral("4599"));

        CompanionRigController r;
        QVERIFY(r.aktiv());

        r.starteWennGewuenscht();

        // Eingeschaltet, und trotzdem kein Socket: der Lauf darf nicht.
        QVERIFY(!r.verbunden());
        QVERIFY(r.client() == nullptr);
    }

    void vonHandGehtAuchInDerSonde()
    {
        // „Was der Bediener in diesem Lauf von Hand anklickt, laeuft
        // unveraendert" -- AppSettings.h. Die Sonde ist nicht gelaehmt,
        // sie draengt sich nur nicht auf.
        setzeEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("CompanionRigHost"), QStringLiteral("127.0.0.1"));
        setzeEin(QStringLiteral("CompanionRigPort"), QStringLiteral("4599"));

        CompanionRigController r;
        r.verbinde();

        // Es wird VERSUCHT. Ob dort etwas lauscht, ist eine andere
        // Frage -- auf 4599 lauscht nichts, der Versuch scheitert, und
        // genau deshalb steht hier kein isConnected().
        QVERIFY(r.client() != nullptr);
    }

    // ── 2. Nicht sich selbst ────────────────────────────────────────

    void eigenerServerWirdErkannt()
    {
        setzeEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("CompanionRigHost"), QStringLiteral("127.0.0.1"));
        setzeEin(QStringLiteral("CompanionRigPort"), QStringLiteral("4532"));
        setzeEin(QStringLiteral("RigctldEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("RigctldPort"), QStringLiteral("4532"));

        CompanionRigController r;
        QVERIFY(r.zieltAufUnsSelbst());

        QSignalSpy klagen(&r, &CompanionRigController::problem);
        r.verbinde();

        // Nicht verbunden, und mit Grund: ein stummes Nichtstun waere
        // hier das Schlimmste -- der Betreiber wartet dann auf eine
        // Frequenz, die nie kommt.
        QVERIFY(!r.verbunden());
        QCOMPARE(klagen.count(), 1);
        QVERIFY(klagen.at(0).at(0).toString().contains(
            QStringLiteral("own rigctld")));
    }

    void andererPortIstKeinKreis()
    {
        setzeEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("CompanionRigHost"), QStringLiteral("127.0.0.1"));
        setzeEin(QStringLiteral("CompanionRigPort"), QStringLiteral("4533"));
        setzeEin(QStringLiteral("RigctldEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("RigctldPort"), QStringLiteral("4532"));

        CompanionRigController r;
        QVERIFY(!r.zieltAufUnsSelbst());
    }

    void fremderRechnerIstKeinKreis()
    {
        // Derselbe Port, aber ein anderer Rechner: der Mac mini, auf
        // dem rigctld fuer das Geraet laeuft. Kein Kreis.
        setzeEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("CompanionRigHost"),
                 QStringLiteral("172.30.30.16"));
        setzeEin(QStringLiteral("CompanionRigPort"), QStringLiteral("4532"));
        setzeEin(QStringLiteral("RigctldEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("RigctldPort"), QStringLiteral("4532"));

        CompanionRigController r;
        QVERIFY(!r.zieltAufUnsSelbst());
    }

    void eigenerServerAusIstKeinKreis()
    {
        setzeEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("CompanionRigHost"), QStringLiteral("127.0.0.1"));
        setzeEin(QStringLiteral("CompanionRigPort"), QStringLiteral("4532"));
        setzeEin(QStringLiteral("RigctldEnabled"), QStringLiteral("False"));
        setzeEin(QStringLiteral("RigctldPort"), QStringLiteral("4532"));

        CompanionRigController r;
        QVERIFY(!r.zieltAufUnsSelbst());
    }

    void localhostZaehltAuch()
    {
        setzeEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("CompanionRigHost"), QStringLiteral("localhost"));
        setzeEin(QStringLiteral("CompanionRigPort"), QStringLiteral("4532"));
        setzeEin(QStringLiteral("RigctldEnabled"), QStringLiteral("True"));
        setzeEin(QStringLiteral("RigctldPort"), QStringLiteral("4532"));

        CompanionRigController r;
        QVERIFY(r.zieltAufUnsSelbst());
    }

    // ── 3. Der Versatz ──────────────────────────────────────────────

    void ladenLegtDenVersatzAb()
    {
        zweiMeter().save();

        CompanionRigController r;

        // Der Ctor hat geladen und abgelegt. Daran haengen die VFO-Zahl,
        // die Frequenzachse, der Bandplan und das Logbuch.
        QVERIFY(TransverterSetup::current().active());
        QCOMPARE(TransverterSetup::displayHz(k28_300), k144_300);
    }

    void ohneTransverterBleibtAlleswieEsWar()
    {
        CompanionRigController r;

        QVERIFY(!TransverterSetup::current().active());
        QCOMPARE(TransverterSetup::displayHz(k28_300), k28_300);
        QCOMPARE(TransverterSetup::panHz(k28_300), k28_300);

        double  mhz  = 28.3;
        QString band = QStringLiteral("10m");
        QVERIFY(!TransverterSetup::logbuchFrequenz(k28_300, &mhz, &band));
        // Unberuehrt: der Aufrufer behaelt seine eigene Rechnung.
        QCOMPARE(mhz, 28.3);
        QCOMPARE(band, QStringLiteral("10m"));
    }

    void versatzMeldetSichBeimLaden()
    {
        zweiMeter().save();
        CompanionRigController r;

        QSignalSpy versatz(&r, &CompanionRigController::versatzGeaendert);
        r.ladeEinstellungen();
        QCOMPARE(versatz.count(), 1);
    }

    void ohneFrischeFrequenzKeineZahl()
    {
        zweiMeter().save();
        CompanionRigController r;

        // Nichts verbunden, also nichts zu melden -- und vor allem
        // nicht 116 MHz, was aus einer 0 und dem Versatz entstuende.
        QCOMPARE(r.zfFrequenzHz(), qint64(0));
        QCOMPARE(r.rfFrequenzHz(), qint64(0));
    }

    // ── 4. Abstimmen ────────────────────────────────────────────────

    void ohneVerbindungWirdNichtAbgestimmt()
    {
        CompanionRigController r;
        QVERIFY(!r.darfAbstimmen());

        QSignalSpy klagen(&r, &CompanionRigController::problem);
        r.stimmeAbZf(k28_300);

        // Stillschweigend. Ein Klick in einen Panadapter ohne
        // Begleitgeraet ist kein Fehler des Bedieners.
        QCOMPARE(klagen.count(), 0);
    }

    void eingeschaltetAberUnverbundenStimmtNichtAb()
    {
        setzeEin(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
        CompanionRigController r;
        QVERIFY(r.aktiv());
        QVERIFY(!r.darfAbstimmen());
    }

    // ── Die Beschreibung fuer die Statuszeile ───────────────────────

    void beschreibungSagtWarumNichtsDaIst()
    {
        CompanionRigController r;
        // Keine leere Zeile. Ein Feld, das nichts sagt, sieht aus wie
        // ein Fehler der Oberflaeche.
        QVERIFY(!r.beschreibung().isEmpty());
    }

private:
    QTemporaryDir m_konfig;
};

QTEST_MAIN(TstBegleitgeraetRegler)
#include "tst_begleitgeraet_regler.moc"
