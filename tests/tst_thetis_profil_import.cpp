// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; die gepruefte Dateiform und
// jede Spaltenzuordnung sind in src/core/ThetisProfilImport.{h,cpp}
// zitiert.
//
// Thetis-TX-Profile einlesen (2026-10-13).
//
// Zeus-Punkt aus 2.0.50, von Martin hereingegeben: "Use the Import button
// on the TX Audio Profile bar to load either a Thetis XML profile export
// or a full Thetis database backup. Zeus recognizes the Thetis file on its
// own, even if it has been renamed."
//
// Longpath ist selbst ein Thetis-Port: 57 der 87 Profilfelder heissen auf
// beiden Seiten GLEICH, weil sie aus derselben Tabelle gezogen wurden.
// Darum ist das Einlesen hier keine Umdeutung, sondern fast eine
// Abschrift -- und genau deshalb gefaehrlich: wo die Namen NICHT
// uebereinstimmen, verstellt eine falsche Zuordnung den Sendeton, und die
// Anzeige sieht dabei richtig aus.
//
// WAS DIESER PRUEFFALL NICHT BELEGEN KANN:
// Auf diesem Rechner liegt KEINE echte Thetis-Ausfuhr -- Thetis laeuft
// unter Windows, und die Suche nach einer `database*.xml` hat nichts
// gefunden. Die Dateiform ist aus Thetis' QUELLTEXT abgeleitet
// (`DataSet("Data")` + `WriteXml(..., WriteSchema)`), nicht von einer
// echten Datei abgelesen. Die Prueffaelle nageln darum das fest, was aus
// dem Quelltext BELEGT ist, und der Leser ist dort nachsichtig, wo die
// genaue Schreibweise unbekannt ist (Wahrheitswerte). Der Beleg ist eine
// Datei von Martins Thetis.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "models/TransmitModel.h"
#include "core/ThetisProfilImport.h"

using namespace Longpath;

namespace {

/// Eine Profilzeile in der Form, die .NET's DataSet.WriteXml schreibt.
QByteArray zeile(const QString& name, const QStringList& spalten)
{
    QString s = QStringLiteral("  <TXProfile>\n    <Name>%1</Name>\n").arg(name);
    for (const QString& sp : spalten) { s += QStringLiteral("    ") + sp + QLatin1Char('\n'); }
    s += QStringLiteral("  </TXProfile>\n");
    return s.toUtf8();
}

QByteArray datei(const QByteArray& inneres, bool mitFremdenTabellen = false)
{
    QByteArray b =
        "<?xml version=\"1.0\" standalone=\"yes\"?>\n"
        "<Data>\n"
        "  <xs:schema id=\"Data\" xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
        "    <xs:element name=\"Data\"/>\n"
        "  </xs:schema>\n";
    b += inneres;
    if (mitFremdenTabellen) {
        b += "  <BandText>\n    <Low>1.8</Low>\n  </BandText>\n"
             "  <Memory>\n    <Name>M1</Name>\n  </Memory>\n";
    }
    b += "</Data>\n";
    return b;
}

QByteArray einProfil(const QString& name = QStringLiteral("ESSB"))
{
    return datei(zeile(name, {
        QStringLiteral("<FilterLow>200</FilterLow>"),
        QStringLiteral("<FilterHigh>3100</FilterHigh>"),
        QStringLiteral("<MicGain>14</MicGain>"),
        QStringLiteral("<TXEQEnabled>true</TXEQEnabled>"),
        QStringLiteral("<TXEQPreamp>-3</TXEQPreamp>"),
        QStringLiteral("<TXEQ1>2</TXEQ1>"),
        QStringLiteral("<TXEQ10>-4</TXEQ10>"),
        QStringLiteral("<TxEqFreq1>32</TxEqFreq1>"),
        QStringLiteral("<CESSB_On>true</CESSB_On>"),
        QStringLiteral("<AM_Carrier_Level>80</AM_Carrier_Level>"),
        QStringLiteral("<Dexp_On>true</Dexp_On>"),
        QStringLiteral("<Dexp_Attenuate>12</Dexp_Attenuate>"),
        QStringLiteral("<Dexp_Hysterisis>3</Dexp_Hysterisis>"),
        QStringLiteral("<Dexp_Tau>25</Dexp_Tau>"),
        QStringLiteral("<Dexp_SCF_Low>400</Dexp_SCF_Low>"),
        QStringLiteral("<Dexp_SCF_High>1800</Dexp_SCF_High>"),
    }));
}

} // namespace

class TstThetisProfilImport : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    // ── Die Dateiart ─────────────────────────────────────────────────────

    void eineEinzelausfuhrWirdErkannt()
    {
        const auto e = ThetisProfilImport::lies(einProfil());
        QVERIFY2(e.fehler.isEmpty(), qPrintable(e.fehler.join(QStringLiteral(" | "))));
        QCOMPARE(e.art, ThetisProfilImport::Art::Profilausfuhr);
        QCOMPARE(e.profile.size(), 1);
        QCOMPARE(e.profile.first().name, QStringLiteral("ESSB"));
    }

    void eineGanzeSicherungWirdErkannt()
    {
        // Der Unterschied ist NUR die Zahl der Tabellen: die
        // Einzelausfuhr wirft alle ausser TXProfile weg
        // (setup.cs:12486-12497), die Sicherung behaelt alle. Die
        // Endung ist bei beiden .xml.
        const QByteArray b = datei(
            zeile(QStringLiteral("DX"), {QStringLiteral("<MicGain>8</MicGain>")})
            + zeile(QStringLiteral("Rund"), {QStringLiteral("<MicGain>2</MicGain>")}),
            /*mitFremdenTabellen=*/true);
        const auto e = ThetisProfilImport::lies(b);
        QCOMPARE(e.art, ThetisProfilImport::Art::Datenbanksicherung);
        QCOMPARE(e.profile.size(), 2);
        QCOMPARE(e.profile.at(0).name, QStringLiteral("DX"));
        QCOMPARE(e.profile.at(1).name, QStringLiteral("Rund"));
    }

    void dieArtHaengtAnDenTABELLENUndNichtAnDerProfilzahl()
    {
        // Die Gegenprobe hat diesen Fall erzwungen. Mit den zwei Faellen
        // darueber allein war NICHT pruefbar, woran die Art entschieden
        // wird: die Einzelausfuhr hatte EIN Profil und keine fremden
        // Tabellen, die Sicherung ZWEI Profile und fremde Tabellen --
        // beide Regeln ("zaehle Tabellen" und "zaehle Profile") gaben
        // dasselbe Ergebnis, und eine falsche blieb gruen.
        //
        // Hier stehen sie darum gekreuzt. Beides kommt vor: eine
        // Sicherung einer Station mit genau einem eigenen Profil, und
        // eine von Hand zusammengesetzte Ausfuhr mit zweien.
        const auto sicherungMitEinemProfil = ThetisProfilImport::lies(datei(
            zeile(QStringLiteral("Allein"), {QStringLiteral("<MicGain>1</MicGain>")}),
            /*mitFremdenTabellen=*/true));
        QCOMPARE(sicherungMitEinemProfil.art,
                 ThetisProfilImport::Art::Datenbanksicherung);
        QCOMPARE(sicherungMitEinemProfil.profile.size(), 1);

        const auto ausfuhrMitZweiProfilen = ThetisProfilImport::lies(datei(
            zeile(QStringLiteral("A"), {QStringLiteral("<MicGain>1</MicGain>")})
            + zeile(QStringLiteral("B"), {QStringLiteral("<MicGain>2</MicGain>")}),
            /*mitFremdenTabellen=*/false));
        QCOMPARE(ausfuhrMitZweiProfilen.art,
                 ThetisProfilImport::Art::Profilausfuhr);
        QCOMPARE(ausfuhrMitZweiProfilen.profile.size(), 2);
    }

    void derDateinameSpieltKeineRolle()
    {
        // Zeus: "even if it has been renamed". Bei uns ist das keine
        // Erkennungslogik, sondern die SCHNITTSTELLE: `lies()` nimmt
        // Bytes und hat keinen Dateinamen. Dieser Fall nagelt das fest,
        // damit niemand spaeter eine Endungspruefung einbaut.
        const auto e = ThetisProfilImport::lies(einProfil());
        QCOMPARE(e.art, ThetisProfilImport::Art::Profilausfuhr);
    }

    void eineFremdeDateiGibtEinenGrundUndKeineHalbeAusbeute()
    {
        const auto leer = ThetisProfilImport::lies(QByteArray());
        QCOMPARE(leer.art, ThetisProfilImport::Art::Unbekannt);
        QVERIFY(leer.profile.isEmpty());
        QVERIFY(!leer.fehler.isEmpty());

        const auto fremd = ThetisProfilImport::lies(
            QByteArray("<?xml version=\"1.0\"?>\n<Einstellungen><A>1</A></Einstellungen>"));
        QCOMPARE(fremd.art, ThetisProfilImport::Art::Unbekannt);
        QVERIFY(fremd.profile.isEmpty());
        QVERIFY2(fremd.fehler.first().contains(QStringLiteral("Einstellungen")),
                 qPrintable(fremd.fehler.join(QStringLiteral(" | "))));

        const auto kaputt = ThetisProfilImport::lies(
            QByteArray("<Data><TXProfile><Name>A</Name>"));
        QVERIFY(!kaputt.fehler.isEmpty());

        const auto ohneProfil = ThetisProfilImport::lies(datei(QByteArray(), true));
        QCOMPARE(ohneProfil.art, ThetisProfilImport::Art::Unbekannt);
        QVERIFY(!ohneProfil.fehler.isEmpty());
    }

    // ── Die Zuordnung ────────────────────────────────────────────────────

    void namensgleicheSpaltenGehenDurch()
    {
        const auto e = ThetisProfilImport::lies(einProfil());
        QCOMPARE(e.profile.size(), 1);
        const QHash<QString, QVariant> w = e.profile.first().werte;
        QCOMPARE(w.value(QStringLiteral("FilterLow")).toString(),  QStringLiteral("200"));
        QCOMPARE(w.value(QStringLiteral("FilterHigh")).toString(), QStringLiteral("3100"));
        QCOMPARE(w.value(QStringLiteral("MicGain")).toString(),    QStringLiteral("14"));
        QCOMPARE(w.value(QStringLiteral("TXEQPreamp")).toString(), QStringLiteral("-3"));
        QCOMPARE(w.value(QStringLiteral("TXEQ1")).toString(),      QStringLiteral("2"));
        QCOMPARE(w.value(QStringLiteral("TXEQ10")).toString(),     QStringLiteral("-4"));
        QCOMPARE(w.value(QStringLiteral("TxEqFreq1")).toString(),  QStringLiteral("32"));
        QCOMPARE(w.value(QStringLiteral("AM_Carrier_Level")).toString(),
                 QStringLiteral("80"));
    }

    void dieIrrefuehrendeSpalteIstDasExpansionsverhaeltnis()
    {
        // DER Fall, um den es bei der Zuordnung geht. "Dexp_Attenuate"
        // klingt nach einer Daempfung und traegt das
        // EXPANSIONSVERHAELTNIS in dB -- belegt in Thetis' eigenem Code
        // (setup.cs:3074 vergleicht die Spalte mit
        // udDEXPExpansionRatio.Value). Wer nach Namensaehnlichkeit
        // abbildet, verstellt den Sendeton, und die Anzeige sieht
        // richtig aus.
        const auto e = ThetisProfilImport::lies(einProfil());
        const QHash<QString, QVariant> w = e.profile.first().werte;
        QCOMPARE(w.value(QStringLiteral("DEXP_ExpansionRatioDb")).toString(),
                 QStringLiteral("12"));
        QCOMPARE(w.value(QStringLiteral("DEXP_HysteresisRatioDb")).toString(),
                 QStringLiteral("3"));
        QCOMPARE(w.value(QStringLiteral("DEXP_DetectorTauMs")).toString(),
                 QStringLiteral("25"));
        QCOMPARE(w.value(QStringLiteral("DEXP_LowCutHz")).toString(),
                 QStringLiteral("400"));
        QCOMPARE(w.value(QStringLiteral("DEXP_HighCutHz")).toString(),
                 QStringLiteral("1800"));
        QCOMPARE(w.value(QStringLiteral("DEXP_Enabled")).toString(),
                 QStringLiteral("True"));
        // Und die Thetis-Namen tauchen NICHT auf -- sonst stuende das
        // Verhaeltnis unter einem Schluessel, den niemand liest. Das ist
        // derselbe Fehler, der bei Rufzeichen und Locator vier Dinge
        // ins Leere laufen liess (2026-10-13).
        QVERIFY(!w.contains(QStringLiteral("Dexp_Attenuate")));
        QVERIFY(!w.contains(QStringLiteral("Dexp_On")));
    }

    void dieZuordnungGehtNurInEineRichtungUndNurFuerBekannteFelder()
    {
        QCOMPARE(ThetisProfilImport::longpathFeld(QStringLiteral("MicGain")),
                 QStringLiteral("MicGain"));
        QCOMPARE(ThetisProfilImport::longpathFeld(QStringLiteral("Dexp_Attenuate")),
                 QStringLiteral("DEXP_ExpansionRatioDb"));
        // Thetis hat 208 Spalten, Longpath fuehrt 87. Die uebrigen gehen
        // NICHT stillschweigend durch -- sonst lagen Werte unter
        // Schluesseln, die kein Leser kennt.
        QVERIFY(ThetisProfilImport::longpathFeld(QStringLiteral("VAC1_RX_Gain")).isEmpty());
        QVERIFY(ThetisProfilImport::longpathFeld(QStringLiteral("RXEQ1")).isEmpty());
        QVERIFY(ThetisProfilImport::longpathFeld(QStringLiteral("Power")).isEmpty());
        QVERIFY(ThetisProfilImport::longpathFeld(QStringLiteral("Quatsch")).isEmpty());
    }

    void dieFeldlisteDecktSichMitDemProfilverwalter()
    {
        // Der Leser kennt den Profilverwalter absichtlich NICHT (er soll
        // ohne AppSettings pruefbar sein) und fuehrt darum eine eigene
        // Liste der Longpath-Felder. Zwei Listen laufen auseinander --
        // also hier festgenagelt, in BEIDE Richtungen.
        const QHash<QString, QVariant> echt =
            MicProfileManager::defaultProfileValues();
        QStringList fehltImLeser;
        for (auto it = echt.constBegin(); it != echt.constEnd(); ++it) {
            if (ThetisProfilImport::longpathFeld(it.key()).isEmpty()
                && it.key() != QStringLiteral("Mic_Source")) {
                // Mic_Source ist Longpath-eigen (Pc/Radio/Vax) und hat
                // in Thetis keine Entsprechung -- Thetis' Mic_Source ist
                // die BUCHSENWAHL am ANAN. Siehe 2026-10-13.
                fehltImLeser << it.key();
            }
        }
        QVERIFY2(fehltImLeser.isEmpty(),
                 qPrintable(QStringLiteral("Der Leser kennt diese "
                                           "Longpath-Felder nicht: %1")
                                .arg(fehltImLeser.join(QStringLiteral(", ")))));
    }

    // ── Wahrheitswerte ───────────────────────────────────────────────────

    void wahrheitswerteKommenInJederSchreibweiseAn()
    {
        // .NET schreibt klein ("true"), Longpaths Einstellungen fuehren
        // "True". Die GENAUE Schreibweise einer echten Thetis-Ausfuhr
        // ist hier NICHT nachgemessen -- es liegt keine auf diesem
        // Rechner. Darum nachsichtig, und das ist eine Entscheidung,
        // kein Versehen.
        bool ok = false;
        QCOMPARE(ThetisProfilImport::wahrheitswert(QStringLiteral("true"), &ok),
                 QStringLiteral("True"));
        QVERIFY(ok);
        QCOMPARE(ThetisProfilImport::wahrheitswert(QStringLiteral("True")),
                 QStringLiteral("True"));
        QCOMPARE(ThetisProfilImport::wahrheitswert(QStringLiteral("1")),
                 QStringLiteral("True"));
        QCOMPARE(ThetisProfilImport::wahrheitswert(QStringLiteral("false")),
                 QStringLiteral("False"));
        QCOMPARE(ThetisProfilImport::wahrheitswert(QStringLiteral("0")),
                 QStringLiteral("False"));
        ThetisProfilImport::wahrheitswert(QStringLiteral("vielleicht"), &ok);
        QVERIFY(!ok);
    }

    void einUnlesbarerWahrheitswertWirdGEMELDETUndNichtGERATEN()
    {
        const auto e = ThetisProfilImport::lies(datei(zeile(
            QStringLiteral("Krumm"),
            {QStringLiteral("<MicGain>5</MicGain>"),
             QStringLiteral("<CESSB_On>vielleicht</CESSB_On>")})));
        QCOMPARE(e.profile.size(), 1);
        const ThetisProfil& p = e.profile.first();
        QVERIFY(!p.werte.contains(QStringLiteral("CESSB_On")));
        QVERIFY2(!p.hinweise.filter(QStringLiteral("CESSB_On")).isEmpty(),
                 qPrintable(p.hinweise.join(QStringLiteral(" | "))));
        // Der Rest des Profils kommt trotzdem an.
        QCOMPARE(p.werte.value(QStringLiteral("MicGain")).toString(),
                 QStringLiteral("5"));
    }

    // ── Thetis' eigene Regeln ────────────────────────────────────────────

    void dasFremdeDefaultProfilWirdUebergangen()
    {
        // From database.cs:11287 -- `if ("Default" != (string)(newRow
        // ["Name"]))`. Thetis nimmt die fremde "Default"-Zeile beim
        // eigenen Einlesen ausdruecklich nicht herein: sie traegt den
        // Anlegezustand jener Fassung, nicht eine Einstellung des
        // Betreibers -- und wuerde hier ueber ein gleichnamiges
        // Longpath-Profil fallen.
        const auto e = ThetisProfilImport::lies(datei(
            zeile(QStringLiteral("Default"), {QStringLiteral("<MicGain>0</MicGain>")})
            + zeile(QStringLiteral("Meins"), {QStringLiteral("<MicGain>9</MicGain>")})));
        QCOMPARE(e.profile.size(), 1);
        QCOMPARE(e.profile.first().name, QStringLiteral("Meins"));
        QVERIFY2(!e.hinweise.filter(QStringLiteral("Default")).isEmpty(),
                 qPrintable(e.hinweise.join(QStringLiteral(" | "))));
    }

    void eineZeileOhneNamenWirdGezaehltUndNichtAngelegt()
    {
        const auto e = ThetisProfilImport::lies(
            datei(QByteArray("  <TXProfile>\n    <MicGain>3</MicGain>\n  </TXProfile>\n")
                  + zeile(QStringLiteral("Gut"), {QStringLiteral("<MicGain>4</MicGain>")})));
        QCOMPARE(e.profile.size(), 1);
        QVERIFY2(!e.hinweise.filter(QStringLiteral("ohne Namen")).isEmpty(),
                 qPrintable(e.hinweise.join(QStringLiteral(" | "))));
    }

    // ── Die Hinweise ─────────────────────────────────────────────────────

    void wasLiegenbleibtStehtInDenHinweisen()
    {
        // Zeus: "you get a note for that profile telling you what was
        // adjusted". Ohne das waere ein Einlesen eine BEHAUPTUNG: der
        // Betreiber sieht einen Profilnamen und weiss nicht, dass die
        // halbe Kette fehlt.
        const auto e = ThetisProfilImport::lies(datei(zeile(
            QStringLiteral("Voll"),
            {QStringLiteral("<MicGain>6</MicGain>"),
             QStringLiteral("<Power>100</Power>"),
             QStringLiteral("<CompanderOn>true</CompanderOn>"),
             QStringLiteral("<VAC1_RX_Gain>-10</VAC1_RX_Gain>")})));
        QCOMPARE(e.profile.size(), 1);
        const QString alles = e.hinweise.join(QStringLiteral("\n"));
        QVERIFY2(alles.contains(QStringLiteral("3 Thetis-Spalten")), qPrintable(alles));
        // Und die, die ein Betreiber im Sendeweg vermissen wuerde,
        // stehen MIT Grund da.
        QVERIFY2(alles.contains(QStringLiteral("CompanderOn")), qPrintable(alles));
        QVERIFY2(alles.contains(QStringLiteral("Power")), qPrintable(alles));
    }

    void jederNichtUebernommeneGrundIstEinSatzUndKeineAbkuerzung()
    {
        const QHash<QString, QString> g = ThetisProfilImport::nichtUebernommen();
        QVERIFY(!g.isEmpty());
        for (auto it = g.constBegin(); it != g.constEnd(); ++it) {
            QVERIFY2(it.value().size() > 20,
                     qPrintable(QStringLiteral("%1: Grund zu kurz").arg(it.key())));
            // Und keine davon darf gleichzeitig abgebildet sein.
            QVERIFY2(ThetisProfilImport::longpathFeld(it.key()).isEmpty(),
                     qPrintable(QStringLiteral("%1 steht als 'nicht uebernommen' "
                                               "UND in der Abbildung").arg(it.key())));
        }
    }

    void eineLeereSpalteLaesstDieVorgabeStehen()
    {
        // DataSet laesst eine DBNull-Spalte GANZ weg; eine LEERE Spalte
        // ist also etwas anderes und darf nicht als "0" ankommen.
        const auto e = ThetisProfilImport::lies(datei(zeile(
            QStringLiteral("Luecke"),
            {QStringLiteral("<MicGain>7</MicGain>"),
             QStringLiteral("<FilterLow></FilterLow>")})));
        QCOMPARE(e.profile.size(), 1);
        const ThetisProfil& p = e.profile.first();
        QVERIFY(!p.werte.contains(QStringLiteral("FilterLow")));
        QVERIFY2(!p.hinweise.filter(QStringLiteral("FilterLow")).isEmpty(),
                 qPrintable(p.hinweise.join(QStringLiteral(" | "))));
    }

    // ── Zweiter Gang: anlegen ────────────────────────────────────────────

    void dieProfileWerdenAngelegtAberNICHTS_WIRD_ANGEWANDT()
    {
        MicProfileManager m;
        m.setMacAddress(QStringLiteral("00:11:22:33:44:55"));
        m.load();
        const QString vorherAktiv = m.activeProfileName();
        const int vorherAnzahl = m.profileNames().size();

        const auto gelesen = ThetisProfilImport::lies(einProfil(
            QStringLiteral("MeinESSB")));
        QCOMPARE(gelesen.profile.size(), 1);

        const auto erg = m.einleseThetisProfile(gelesen.profile);
        QCOMPARE(erg.anzahl(), 1);
        QCOMPARE(erg.angelegt.value(QStringLiteral("MeinESSB")),
                 QStringLiteral("MeinESSB"));
        QVERIFY(m.profileNames().contains(QStringLiteral("MeinESSB")));
        QCOMPARE(m.profileNames().size(), vorherAnzahl + 1);

        // DIE Zusage: der Sendeton bleibt, wie er war. Ein Einlesen,
        // das sofort umschaltet, verstellt ihn mitten im Gespraech.
        QCOMPARE(m.activeProfileName(), vorherAktiv);
    }

    void einVorhandenerNameWirdNICHT_UEBERSCHRIEBEN()
    {
        MicProfileManager m;
        m.setMacAddress(QStringLiteral("00:11:22:33:44:66"));
        m.load();
        // "Default" legt load() an; es ist also mit Sicherheit da.
        QVERIFY(m.profileNames().contains(QStringLiteral("Default")));

        // Eine Datei mit einem Namen, den es hier schon gibt. (Nicht
        // "Default" selbst -- das uebergeht schon der Leser.)
        const QString schon = m.profileNames().first();
        const auto gelesen = ThetisProfilImport::lies(einProfil(schon));
        QCOMPARE(gelesen.profile.size(), 1);

        const auto erg = m.einleseThetisProfile(gelesen.profile);
        const QString neu = erg.angelegt.value(schon);
        QCOMPARE(neu, QStringLiteral("%1 (Thetis)").arg(schon));
        QVERIFY(m.profileNames().contains(neu));
        // Und das eigene steht unberuehrt daneben.
        QVERIFY(m.profileNames().contains(schon));
        QVERIFY2(!erg.hinweise.value(neu).filter(QStringLiteral("unberuehrt")).isEmpty(),
                 qPrintable(erg.hinweise.value(neu).join(QStringLiteral(" | "))));

        // Noch einmal dieselbe Datei: dann mit Zahl, immer noch ohne
        // etwas zu ueberschreiben.
        const auto erg2 = m.einleseThetisProfile(gelesen.profile);
        QCOMPARE(erg2.angelegt.value(schon),
                 QStringLiteral("%1 (Thetis 2)").arg(schon));
    }

    void lueckenWerdenAusDenVorgabenGefuelltUndDasStehtDabei()
    {
        // From database.cs:11263-11292 -- Thetis fuellt beim eigenen
        // Einlesen fehlende Spalten aus "Default". Ohne das stuende beim
        // Umschalten auf einem fehlenden Feld der Wert des VORIGEN
        // Profils.
        MicProfileManager m;
        m.setMacAddress(QStringLiteral("00:11:22:33:44:77"));
        m.load();
        const auto gelesen = ThetisProfilImport::lies(datei(zeile(
            QStringLiteral("Karg"), {QStringLiteral("<MicGain>11</MicGain>")})));
        const auto erg = m.einleseThetisProfile(gelesen.profile);
        QCOMPARE(erg.anzahl(), 1);
        const QStringList h = erg.hinweise.value(QStringLiteral("Karg"));
        QVERIFY2(!h.filter(QStringLiteral("Vorgabe")).isEmpty(),
                 qPrintable(h.join(QStringLiteral(" | "))));

        // Und das Profil traegt wirklich einen vollen Satz Felder, nicht
        // nur das eine aus der Datei.
        TransmitModel tx;
        QVERIFY(m.setActiveProfile(QStringLiteral("Karg"), &tx));
        // micGainDb, NICHT micGain: das sind zwei verschiedene
        // Eigenschaften (micGain ist der lineare Vorverstaerker,
        // micGainDb der Regler in dB), und das Profilfeld "MicGain"
        // geht an setMicGainDb (MicProfileManager.cpp:1510). Hier stand
        // zuerst der falsche Leser, und der Fall war rot -- gut so.
        QCOMPARE(tx.micGainDb(), 11);
        // ── Das MUSS in den EINSTELLUNGEN stehen, nicht am Modell ───────
        //
        // Hier standen zuerst `tx.filterLow()`/`filterHigh()` -- und die
        // Gegenprobe (Auffuellen ausgebaut) blieb GRUEN. Der Grund:
        // `setActiveProfile` hat fuer jedes Feld einen eigenen
        // Rueckfallwert in seinem `take()`, und der ist fuer die
        // Filterkanten derselbe wie die Vorgabe. Das Modell sah also
        // richtig aus, waehrend in der Einstellungsdatei NICHTS stand.
        //
        // Geprueft wird darum die Datei selbst: jedes Feld muss unter
        // dem Profilschluessel LIEGEN. Sonst stuende beim naechsten
        // Umschalten der Wert, den `take()` gerade raet -- und der kann
        // sich aendern.
        auto& st = AppSettings::instance();
        const QString praefix =
            QStringLiteral("hardware/00:11:22:33:44:77/tx/profile/Karg/");
        QCOMPARE(st.value(praefix + QStringLiteral("MicGain")).toString(),
                 QStringLiteral("11"));
        QCOMPARE(st.value(praefix + QStringLiteral("TwoToneFreq1")).toString(),
                 QStringLiteral("700"));
        QCOMPARE(st.value(praefix + QStringLiteral("FilterLow")).toString(),
                 QStringLiteral("100"));
        QCOMPARE(st.value(praefix + QStringLiteral("AM_Carrier_Level")).toString(),
                 QStringLiteral("100"));
        // Und zwar ALLE Felder, nicht eine Auswahl.
        const QHash<QString, QVariant> vorgaben =
            MicProfileManager::defaultProfileValues();
        QStringList fehlend;
        for (auto it = vorgaben.constBegin(); it != vorgaben.constEnd(); ++it) {
            if (st.value(praefix + it.key()).toString().isEmpty()
                && it.key() != QStringLiteral("TXParaEQData")
                && it.key() != QStringLiteral("CFCParaEQData")) {
                // Die zwei Blobs sind absichtlich leer.
                fehlend << it.key();
            }
        }
        QVERIFY2(fehlend.isEmpty(),
                 qPrintable(QStringLiteral("Diese Felder wurden nicht "
                                           "geschrieben: %1")
                                .arg(fehlend.join(QStringLiteral(", ")))));
    }

    void ohneVerbundenesGeraetWirdNichtsAngelegtUndDasWirdGESAGT()
    {
        // Die Profile haengen an der Geraete-MAC. Ohne Verbindung gibt
        // es keinen Ort dafuer -- und ein stilles "nichts passiert"
        // waere genau die Art Fehlschlag, die niemand merkt.
        MicProfileManager m;              // ohne setMacAddress
        const auto gelesen = ThetisProfilImport::lies(einProfil());
        const auto erg = m.einleseThetisProfile(gelesen.profile);
        QCOMPARE(erg.anzahl(), 0);
        QCOMPARE(erg.ausgelassen.size(), 1);
        QVERIFY2(erg.ausgelassen.constBegin().value().contains(
                     QStringLiteral("verbinden")),
                 qPrintable(erg.ausgelassen.constBegin().value()));
    }

    void einWertAusserhalbUnseresBereichsKommtTROTZDEMAn()
    {
        // Zeus ausdruecklich: "Mic gain keeps its Thetis value even when
        // it is outside the range Zeus used to allow." Der Leser
        // klammert NICHT.
        //
        // 90 dB ist mit Bedacht gewaehlt: Longpaths Bereich ist
        // [-50, +70] (TransmitModel::kMicGainDbMin/Max). Der Wert liegt
        // also WIRKLICH draussen -- hier stand zuerst 70, und das ist
        // der Rand, nicht draussen; der Fall haette eine Klammerung
        // nicht gefangen.
        const auto e = ThetisProfilImport::lies(datei(zeile(
            QStringLiteral("Laut"), {QStringLiteral("<MicGain>90</MicGain>")})));
        QCOMPARE(e.profile.first().werte.value(QStringLiteral("MicGain")).toString(),
                 QStringLiteral("90"));

        // Geklammert wird erst beim ANWENDEN, im Setter von
        // TransmitModel -- und dort steht dann der naechstmoegliche
        // Wert, nicht eine stille Null.
        MicProfileManager m;
        m.setMacAddress(QStringLiteral("00:11:22:33:44:88"));
        m.load();
        m.einleseThetisProfile(e.profile);
        TransmitModel tx;
        QVERIFY(m.setActiveProfile(QStringLiteral("Laut"), &tx));
        QCOMPARE(tx.micGainDb(), TransmitModel::kMicGainDbMax);
    }
};

QTEST_MAIN(TstThetisProfilImport)
#include "tst_thetis_profil_import.moc"
