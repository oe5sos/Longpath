// no-port-check: Longpath-eigener Pruefstand.
// =================================================================
// tests/tst_ft8_contest_ablauf.cpp  (Longpath)
// =================================================================
//
// Der Zugablauf im EU-VHF-Contest.
//
// Das FORMAT kann Longpath seit heute in beide Richtungen
// (tst_ft8_eu_vhf). Hier geht es um das, was davor liegt: erzeugt der
// Ablauf die richtigen Saetze, und versteht er die, die zurueckkommen?
//
// ── Der Befund, der diesen Lauf ausgeloest hat ──────────────────────
//
// `Ft8Ablauf::anMich()` schaelte die spitzen Klammern nur vom
// ABSENDER (fuer den Fox), nicht vom Angerufenen. Im Contest stehen
// aber beide Rufzeichen nur als Hash im Paket, also auch meines:
//
//     <OE5SOS> <DL1ABC> 590001 JN78CT
//
// `f.at(0) != m_meinRuf` warf das weg. Ein Contest-Anruf an mich
// erreichte den Ablauf nie, und das QSO kam nicht zustande — von
// aussen ununterscheidbar von „er ruft mich nicht".
//
// =================================================================
// Modification history (Longpath):
//   2026-11-04 — Original for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest/QtTest>

#include "core/Ft8Ablauf.h"

using namespace Longpath;
using Contest = Ft8Ablauf::Contest;
using Zustand = Ft8Ablauf::Zustand;

namespace {

Contest guterContest(int nummer = 1)
{
    Contest c;
    c.an = true;
    c.nummer = nummer;
    c.meinLocator6 = QStringLiteral("JN78CT");
    return c;
}

} // namespace

class TstFt8ContestAblauf : public QObject
{
    Q_OBJECT

private slots:

    // ── Was „brauchbar" heisst ──────────────────────────────────────
    //
    // Der Feldkenner MUSS sechs Stellen haben: die 25 Bit in Typ 5
    // sind fuer sechs gerechnet. Wer nur „JN78" eingetragen hat, kann
    // am EU-VHF-Contest nicht teilnehmen — und das Programm muss das
    // sagen, statt stillschweigend etwas anderes zu senden.
    void nurEinSechsstelligerFeldkennerTaugt()
    {
        Contest c = guterContest();
        QVERIFY(c.brauchbar());

        c.meinLocator6 = QStringLiteral("JN78");     // vier Stellen
        QVERIFY(!c.brauchbar());
        c.meinLocator6 = QStringLiteral("JN78CTX");  // sieben
        QVERIFY(!c.brauchbar());
        c.meinLocator6 = QString();
        QVERIFY(!c.brauchbar());
        // Ausserhalb des Rasters: Feld nur A..R, Unterquadrat A..X.
        c.meinLocator6 = QStringLiteral("SN78CT");
        QVERIFY(!c.brauchbar());
        c.meinLocator6 = QStringLiteral("JN78CZ");
        QVERIFY(!c.brauchbar());
        // Und die Nummer in ihren Grenzen.
        c.meinLocator6 = QStringLiteral("JN78CT");
        c.nummer = 0;
        QVERIFY(!c.brauchbar());
        c.nummer = 4096;
        QVERIFY(!c.brauchbar());
        c.nummer = 4095;
        QVERIFY(c.brauchbar());
    }

    // Ausgeschaltet ist ausgeschaltet — auch mit gueltigem Feldkenner.
    void ohneSchalterKeinContest()
    {
        Contest c = guterContest();
        c.an = false;
        QVERIFY(!c.brauchbar());
    }

    // ── Die beiden Austauschzuege ───────────────────────────────────
    void derAustauschErsetztDenRapport()
    {
        const QString tx2 = Ft8Ablauf::nachrichtFuer(
            Zustand::SendeReport, QStringLiteral("DL1ABC"),
            QStringLiteral("OE5SOS"), QStringLiteral("JN78"), -12,
            guterContest(1));
        QCOMPARE(tx2, QStringLiteral("<DL1ABC> <OE5SOS> 590001 JN78CT"));

        const QString tx3 = Ft8Ablauf::nachrichtFuer(
            Zustand::SendeRReport, QStringLiteral("DL1ABC"),
            QStringLiteral("OE5SOS"), QStringLiteral("JN78"), -12,
            guterContest(1));
        QCOMPARE(tx3, QStringLiteral("<DL1ABC> <OE5SOS> R 590001 JN78CT"));
    }

    // Die Nummer behaelt ihre vier Stellen — „5901" statt „590001"
    // waere ein anderer Austausch.
    void dieNummerHatImmerVierStellen()
    {
        struct { int n; const char* erwartet; } faelle[] = {
            {    1, "590001" }, {   42, "590042" },
            {  999, "590999" }, { 4095, "594095" },
        };
        for (const auto& f : faelle) {
            const QString s = Ft8Ablauf::nachrichtFuer(
                Zustand::SendeReport, QStringLiteral("DL1ABC"),
                QStringLiteral("OE5SOS"), QStringLiteral("JN78"), -12,
                guterContest(f.n));
            QVERIFY2(s.contains(QLatin1String(f.erwartet)), qPrintable(s));
        }
    }

    // ── Und was NICHT Contest wird ──────────────────────────────────
    //
    // Anruf, RR73, 73 und CQ sind gewoehnliche Saetze: sie passen
    // nicht in Typ 5 (der traegt keinen Anruf und kein Schlusswort).
    void anrufUndSchlusswortBleibenGewoehnlich()
    {
        const Contest c = guterContest();
        QCOMPARE(Ft8Ablauf::nachrichtFuer(
                     Zustand::RufeAn, QStringLiteral("DL1ABC"),
                     QStringLiteral("OE5SOS"), QStringLiteral("JN78"), -12, c),
                 QStringLiteral("DL1ABC OE5SOS JN78"));
        QCOMPARE(Ft8Ablauf::nachrichtFuer(
                     Zustand::SendeRR73, QStringLiteral("DL1ABC"),
                     QStringLiteral("OE5SOS"), QStringLiteral("JN78"), -12, c),
                 QStringLiteral("DL1ABC OE5SOS RR73"));
        QCOMPARE(Ft8Ablauf::nachrichtFuer(
                     Zustand::Sende73, QStringLiteral("DL1ABC"),
                     QStringLiteral("OE5SOS"), QStringLiteral("JN78"), -12, c),
                 QStringLiteral("DL1ABC OE5SOS 73"));
        QCOMPARE(Ft8Ablauf::nachrichtFuer(
                     Zustand::RufeCq, QString(),
                     QStringLiteral("OE5SOS"), QStringLiteral("JN78"), -12, c),
                 QStringLiteral("CQ OE5SOS JN78"));
    }

    // ── Ein halbfertiger Contest legt den Betrieb NICHT lahm ────────
    //
    // Wer den Feldkenner noch nicht sechsstellig eingetragen hat,
    // funkt gewoehnlich weiter — statt gar nicht.
    void ohneBrauchbarenAustauschDerGewoehnlicheWeg()
    {
        Contest c = guterContest();
        c.meinLocator6 = QStringLiteral("JN78");   // zu kurz
        QCOMPARE(Ft8Ablauf::nachrichtFuer(
                     Zustand::SendeReport, QStringLiteral("DL1ABC"),
                     QStringLiteral("OE5SOS"), QStringLiteral("JN78"), -12, c),
                 QStringLiteral("DL1ABC OE5SOS -12"));
    }

    // ═══ DER BEFUND: ein Contest-Anruf an mich kam nie an ═══════════
    //
    // Gegenprobe: ohne das Schaelen des ANGERUFENEN in
    // `Ft8Ablauf::anMich()` bleibt der Ablauf hier auf `RufeCq`
    // stehen, und das QSO kommt nicht zustande.
    void einContestAnrufAnMichKommtAn()
    {
        Ft8Ablauf a;
        a.setzeContest(guterContest(1));
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN78"));
        QCOMPARE(a.zustand(), Zustand::RufeCq);

        QVERIFY2(a.verarbeite(QStringLiteral("<OE5SOS> <DL1ABC> 590007 JO31AB"),
                              -11),
                 "der Contest-Anruf wurde verworfen");
        QCOMPARE(a.zustand(), Zustand::SendeReport);
        QCOMPARE(a.partner(), QStringLiteral("DL1ABC"));
    }

    // Seine Nummer und sein Feldkenner landen, wo das Logbuch sie
    // sucht — nicht nur im Text.
    void seineNummerUndSeinFeldkennerWerdenGelesen()
    {
        Ft8Ablauf a;
        a.setzeContest(guterContest(1));
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN78"));
        a.verarbeite(QStringLiteral("<OE5SOS> <DL1ABC> 590007 JO31AB"), -11);

        QCOMPARE(a.empfangeneNummer(), 7);
        QCOMPARE(a.partnerLocator6(), QStringLiteral("JO31AB"));
        // Und im gewoehnlichen Feld, wo Logbuch und Anzeige suchen.
        QCOMPARE(a.partnerLocator(), QStringLiteral("JO31AB"));
    }

    // Ein bestaetigter Austausch („R …") bringt den Ablauf einen Zug
    // weiter als ein unbestaetigter — sonst faellt das QSO zurueck.
    void einBestaetigterAustauschSpringtZumRR73()
    {
        Ft8Ablauf a;
        a.setzeContest(guterContest(1));
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN78"));
        QVERIFY(a.verarbeite(
            QStringLiteral("<OE5SOS> <DL1ABC> R 590007 JO31AB"), -11));
        QCOMPARE(a.zustand(), Zustand::SendeRR73);
        QCOMPARE(a.empfangeneNummer(), 7);
    }

    // ── Und was KEIN Austausch ist ──────────────────────────────────
    //
    // Der Erkenner darf gewoehnliche Saetze nicht an sich ziehen.
    void gewoehnlicheSaetzeWerdenNichtAlsAustauschGelesen()
    {
        Ft8Ablauf a;
        a.setzeContest(guterContest(1));
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN78"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -11));
        // Kein Austausch -> keine Nummer, und der VIERstellige
        // Feldkenner bleibt, wo er hingehoert.
        QCOMPARE(a.empfangeneNummer(), 0);
        QCOMPARE(a.partnerLocator6(), QString());
        QCOMPARE(a.partnerLocator(), QStringLiteral("JO31"));
    }
};

QTEST_MAIN(TstFt8ContestAblauf)
#include "tst_ft8_contest_ablauf.moc"
