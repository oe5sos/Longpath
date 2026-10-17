// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_ft8_ablauf.cpp  (Longpath)
// =================================================================
// Der FT8-QSO-Ablauf, Zustand fuer Zustand.
//
// Ft8Ablauf sendet nicht, tastet nicht und kennt keine Uhr — sie sagt
// nur, was im naechsten Fenster zu senden waere. Genau darum laesst
// sich hier jeder Uebergang einzeln hinschreiben, ohne Fenster, ohne
// Funkgeraet und ohne zu warten.
//
// Geprueft werden beide Rollen vollstaendig durch, dazu die Faelle,
// in denen ein Ablauf NICHT weitergehen darf: ein Dritter, der
// dazwischenruft, eine Nachricht an jemand anderen, und das Aufgeben
// nach genug Wiederholungen.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>

#include "core/Ft8Ablauf.h"

using namespace Longpath;
using Z = Ft8Ablauf::Zustand;

class TstFt8Ablauf : public QObject
{
    Q_OBJECT

private slots:
    // ── Die ganze Strecke als Antworter ─────────────────────────────
    void alsAntworterDurch()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67VV"), -12);

        QCOMPARE(a.zustand(), Z::RufeAn);
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("DL1ABC OE5SOS JN67"));
        QVERIFY(a.laeuft());

        // Er gibt mir seinen Rapport.
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC -09"), -12));
        QCOMPARE(a.zustand(), Z::SendeRReport);
        QCOMPARE(a.empfangenerReport(), -9);
        QCOMPARE(a.naechsteNachricht(),
                 QStringLiteral("DL1ABC OE5SOS R-12"));

        // Er ist fertig.
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC RR73"), -12));
        QCOMPARE(a.zustand(), Z::Sende73);
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("DL1ABC OE5SOS 73"));

        // Mein 73 ist hinaus — damit ist das QSO vollstaendig, auch
        // ohne dass er es noch einmal bestaetigt.
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Z::Fertig);
        QVERIFY(!a.laeuft());
        QCOMPARE(a.gesendeterReport(), -12);
        QCOMPARE(a.empfangenerReport(), -9);
    }

    // ── Die ganze Strecke als Anrufer ───────────────────────────────
    void alsAnruferDurch()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"),
                           QStringLiteral("JN67VV"));

        QCOMPARE(a.zustand(), Z::RufeCq);
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("CQ OE5SOS JN67"));
        QVERIFY(a.partner().isEmpty());

        // Jemand antwortet — der Rapport ist das, was ich von ihm messe.
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -07));
        QCOMPARE(a.partner(), QStringLiteral("DL1ABC"));
        QCOMPARE(a.zustand(), Z::SendeReport);
        QCOMPARE(a.naechsteNachricht(),
                 QStringLiteral("DL1ABC OE5SOS -07"));

        // Er bestaetigt und gibt seinen.
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC R-15"), -7));
        QCOMPARE(a.zustand(), Z::SendeRR73);
        QCOMPARE(a.empfangenerReport(), -15);
        QCOMPARE(a.naechsteNachricht(),
                 QStringLiteral("DL1ABC OE5SOS RR73"));

        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Z::Fertig);
    }

    // Ein positiver Rapport wird mit + und zwei Stellen geschrieben,
    // wie in WSJT-X.
    void einPositiverRapportStehtMitPlus()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), 3));
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("DL1ABC OE5SOS +03"));
    }

    // ── Sein Feldkenner, fuer das Logbuch ───────────────────────────
    void derLocatorDesPartnersWirdGemerkt()
    {
        // Als Anrufer: er steht in der Antwort auf mein CQ.
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.partnerLocator().isEmpty());
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
        QCOMPARE(a.partnerLocator(), QStringLiteral("JO31"));

        // Als Antworter: er kommt aus der angeklickten CQ-Zeile.
        Ft8Ablauf b;
        b.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12,
                             QStringLiteral("JO31"));
        QCOMPARE(b.partnerLocator(), QStringLiteral("JO31"));
    }

    // Nicht jedes dritte Feld ist ein Feldkenner — „-12", „R-09",
    // „RR73" und „73" stehen an derselben Stelle. Ein geratener
    // Locator stuende sonst im Logbuch.
    void nurEchteFeldkennerZaehlen_data()
    {
        QTest::addColumn<QString>("feld");
        QTest::addColumn<bool>("gilt");
        QTest::newRow("JN67")   << QStringLiteral("JN67")   << true;
        QTest::newRow("JN67VV") << QStringLiteral("JN67VV") << true;
        QTest::newRow("jo31")   << QStringLiteral("jo31")   << true;
        QTest::newRow("-12")    << QStringLiteral("-12")    << false;
        QTest::newRow("R-09")   << QStringLiteral("R-09")   << false;
        QTest::newRow("RR73")   << QStringLiteral("RR73")   << false;
        QTest::newRow("73")     << QStringLiteral("73")     << false;
        QTest::newRow("Ruf")    << QStringLiteral("DL1ABC") << false;
        QTest::newRow("leer")   << QString()                << false;
    }

    void nurEchteFeldkennerZaehlen()
    {
        QFETCH(QString, feld);
        QFETCH(bool, gilt);
        QCOMPARE(Ft8Ablauf::wieLocator(feld), gilt);
    }

    // Ein Anrufer ohne Locator (es gibt CQ-Rufe ohne) darf keinen
    // erfinden.
    void ohneLocatorBleibtDasFeldLeer()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC"), -7));
        QVERIFY2(a.partnerLocator().isEmpty(),
                 "ohne Feldkenner darf keiner im Logbuch stehen");
    }

    // ── Was den Ablauf NICHT weiterbringen darf ─────────────────────

    void eineNachrichtAnJemandAnderenZaehltNicht()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        QVERIFY(!a.verarbeite(QStringLiteral("OE1XYZ DL1ABC -09"), -12));
        QCOMPARE(a.zustand(), Z::RufeAn);
    }

    // Laeuft das QSO schon mit DL1ABC, darf ein Dritter es nicht
    // uebernehmen — sonst wechselt der Ablauf mitten im Gespraech den
    // Partner.
    void einDritterUebernimmtDasQsoNicht()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        QVERIFY(!a.verarbeite(QStringLiteral("OE5SOS SP9XYZ -05"), -5));
        QCOMPARE(a.partner(), QStringLiteral("DL1ABC"));
        QCOMPARE(a.zustand(), Z::RufeAn);
    }

    // Auf ein CQ hin darf dagegen jeder der erste sein — aber nur der
    // erste.
    void beimCqGiltDerErsteAnrufer()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
        QCOMPARE(a.partner(), QStringLiteral("DL1ABC"));
        QVERIFY(!a.verarbeite(QStringLiteral("OE5SOS SP9XYZ KO02"), -3));
        QCOMPARE(a.partner(), QStringLiteral("DL1ABC"));
    }

    // ── Aufgeben statt ewig rufen ───────────────────────────────────
    void ohneAntwortWirdAufgegeben()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);

        for (int i = 1; i < Ft8Ablauf::kMaxWiederholungen; ++i) {
            a.fensterVerstrichen();
            QVERIFY2(a.laeuft(), qPrintable(
                QStringLiteral("nach %1 Fenstern schon aufgegeben").arg(i)));
            QCOMPARE(a.wiederholungen(), i);
        }
        a.fensterVerstrichen();
        QCOMPARE(a.zustand(), Z::Abgebrochen);
        QVERIFY(!a.laeuft());
        QVERIFY2(a.naechsteNachricht().isEmpty(),
                 "ein abgebrochener Ablauf darf nichts mehr zu senden haben");
    }

    // Eine passende Antwort setzt den Zaehler zurueck — sonst gibt ein
    // langes QSO mitten im Gespraech auf.
    void einePassendeAntwortSetztDenZaehlerZurueck()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        a.fensterVerstrichen();
        a.fensterVerstrichen();
        QCOMPARE(a.wiederholungen(), 2);
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC -09"), -12));
        QCOMPARE(a.wiederholungen(), 0);
    }

    void abbrechenHaeltSofortAn()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QStringLiteral("OE5SOS"), QStringLiteral("JN67"));
        QVERIFY(a.laeuft());
        a.abbrechen();
        QCOMPARE(a.zustand(), Z::Abgebrochen);
        QVERIFY(a.naechsteNachricht().isEmpty());
        // Und danach bringt ihn nichts mehr in Gang.
        QVERIFY(!a.verarbeite(QStringLiteral("OE5SOS DL1ABC JO31"), -7));
    }

    // Ohne eigenes Rufzeichen faengt gar nichts an — sonst stuenden
    // Nachrichten mit einer Luecke auf dem Band.
    void ohneRufzeichenFaengtNichtsAn()
    {
        Ft8Ablauf a;
        a.starteAlsAnrufer(QString(), QStringLiteral("JN67"));
        QCOMPARE(a.zustand(), Z::Ruht);
        QVERIFY(!a.laeuft());
        QVERIFY(a.naechsteNachricht().isEmpty());

        Ft8Ablauf b;
        b.starteAlsAntworter(QStringLiteral("DL1ABC"), QString(),
                             QStringLiteral("JN67"), -12);
        QCOMPARE(b.zustand(), Z::Ruht);
    }

    // Gibt er mir seinen Rapport schon bestaetigt (R-09), ist mein
    // eigener bei ihm angekommen — dann ist der Zwischenschritt
    // ueberfluessig.
    void einBestaetigterRapportSpartEinenSchritt()
    {
        Ft8Ablauf a;
        a.starteAlsAntworter(QStringLiteral("DL1ABC"),
                             QStringLiteral("OE5SOS"),
                             QStringLiteral("JN67"), -12);
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS DL1ABC R-09"), -12));
        QCOMPARE(a.zustand(), Z::SendeRR73);
        QCOMPARE(a.empfangenerReport(), -9);
    }
};

QTEST_APPLESS_MAIN(TstFt8Ablauf)
#include "tst_ft8_ablauf.moc"
