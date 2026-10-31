// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_ft8_frist_qso_endet.cpp  (Longpath)
// =================================================================
//
// Das QSO geht in der Dekoderfrist zu Ende -- was geht dann hinaus?
//
// ── Was am Geraet gemessen wurde (2026-10-31) ───────────────────────
//
// Aus dem Mitschnitt des Betreibers, 40 m, FT8:
//
//     054045      OE5SOS UA3IHJ KO67     er ruft mich
//     054100  Tx  R5AV OE5SOS +15
//     054115      OE5SOS R5AV RR73       R5AV ist FERTIG
//     054130  Tx  R5AV OE5SOS +15        <- trotzdem nochmal an R5AV
//     054200  Tx  UA3IHJ OE5SOS +15      <- der Wartende erst jetzt
//     054230  Tx  UA3IHJ OE5SOS +15
//     054300  Tx  UA3IHJ OE5SOS +15      ... fuenfmal, ohne Antwort
//
// Betreiber: „warum sendet er 5 mal das gleiche".
//
// Die Fuenf ist `Ft8Ablauf::kMaxWiederholungen` und richtig so. Der
// Fehler liegt davor: der Zug um 054130. Die Nachricht wird beim Ende
// der VORIGEN Sendung gepackt, also 13 s vorher; kurz vor dem Tasten
// sieht `sendeNachricht` nach, ob sich der Zug geaendert hat. Diese
// Nachpruefung stand aber hinter `m_ablauf.laeuft()` -- und das ist
// bei `Fertig` und `Abgebrochen` falsch. Beendet die Gegenstation das
// QSO in genau dieser Frist, fiel die Pruefung ganz aus und die alte
// Nachricht ging hinaus.
//
// Der Preis ist nicht die eine Sendung, sondern das Fenster: UA3IHJ
// haette um 054130 drankommen koennen, 45 s nach seinem Ruf statt 75.
//
// Modification history (Longpath):
//   2026-10-31 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>
#include <QFile>

#include "core/Ft8Ablauf.h"

using namespace Longpath;

namespace {

QString liesGanz(const QString& relativ)
{
    const QString wurzel = qEnvironmentVariable("LONGPATH_SOURCE_DIR");
    if (wurzel.isEmpty()) { return {}; }
    QFile f(wurzel + QLatin1Char('/') + relativ);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return {}; }
    return QString::fromUtf8(f.readAll());
}

}  // namespace

class TstFt8FristQsoEndet : public QObject
{
    Q_OBJECT

private slots:
    // Erst die Messung selbst nachstellen: ein RR73 in der Frist
    // beendet den Ablauf, und `laeuft()` ist danach falsch. Genau
    // daran scheiterte die Nachpruefung.
    void einRr73InDerFristBeendetDenAblauf()
    {
        Ft8Ablauf a;
        a.starteMitAnrufer(QStringLiteral("R5AV"), QStringLiteral("OE5SOS"),
                           QStringLiteral("JN67"), 15, QString());
        QCOMPARE(a.zustand(), Ft8Ablauf::Zustand::SendeReport);
        QCOMPARE(a.naechsteNachricht(), QStringLiteral("R5AV OE5SOS +15"));
        QVERIFY(a.laeuft());

        // 054115: „OE5SOS R5AV RR73"
        QVERIFY(a.verarbeite(QStringLiteral("OE5SOS R5AV RR73"), 13));
        QCOMPARE(a.zustand(), Ft8Ablauf::Zustand::Fertig);

        // DAS ist der Punkt: ab hier faellt jede Pruefung aus, die an
        // `laeuft()` haengt -- und die alte Nachricht steht noch
        // gepackt in der Tonquelle.
        QVERIFY(!a.laeuft());
    }

    // Und die Grenze selbst, damit die Fuenf belegt ist und nicht
    // beim naechsten Eingriff still zu einer anderen Zahl wird.
    void einZugMittenImQsoWirdFuenfmalWiederholt()
    {
        Ft8Ablauf a;
        a.starteMitAnrufer(QStringLiteral("UA3IHJ"), QStringLiteral("OE5SOS"),
                           QStringLiteral("JN67"), 15, QStringLiteral("KO67"));
        const QString zug = a.naechsteNachricht();
        QCOMPARE(zug, QStringLiteral("UA3IHJ OE5SOS +15"));

        int gesendet = 0;
        while (a.laeuft() && gesendet < 20) {
            ++gesendet;                 // dieses Fenster geht hinaus
            QCOMPARE(a.naechsteNachricht(), zug);
            a.fensterVerstrichen();     // keine Antwort
        }
        QCOMPARE(gesendet, Ft8Ablauf::kMaxWiederholungen);
        QCOMPARE(a.zustand(), Ft8Ablauf::Zustand::Abgebrochen);
    }

    // Die Verdrahtung: die Nachpruefung muss den Fall „kein Zug mehr"
    // kennen, den fertigen QSO ins Logbuch geben und den Wartenden in
    // DIESEM Fenster anfangen -- nicht erst im naechsten.
    void dieFristKenntDenFallDassDasQsoEndet()
    {
        const QString text = liesGanz(
            QStringLiteral("src/gui/applets/Ft8Applet.cpp"));
        if (text.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }

        QVERIFY2(text.contains(
                     QStringLiteral("if (ausAblauf && !m_ablauf.laeuft()")),
                 "die Dekoderfrist kennt den Fall nicht, dass das QSO in "
                 "ihr zu Ende geht — die alte Nachricht geht hinaus");
        QVERIFY2(text.contains(QStringLiteral("beginneNaechstenWartenden()")),
                 "der Wartende bekommt dieses Fenster nicht");
        QVERIFY2(text.contains(QStringLiteral("meldeFertigesQso();")),
                 "das fertige QSO wuerde verlorengehen, wenn der naechste "
                 "Ablauf den Platz uebernimmt");
    }

    // Und der zweite Aufrufer darf nicht mitten im laufenden Fenster
    // ein zweites anfangen: `beginneNaechstenWartenden` sendet nicht.
    void dasAnfangenSendetNichtVonSelbst()
    {
        const QString text = liesGanz(
            QStringLiteral("src/gui/applets/Ft8Applet.cpp"));
        if (text.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }

        const int a = text.indexOf(
            QStringLiteral("bool Ft8Applet::beginneNaechstenWartenden()"));
        QVERIFY2(a >= 0, "beginneNaechstenWartenden fehlt");
        const int b = text.indexOf(QStringLiteral("\n}\n"), a);
        const QString rumpf = text.mid(a, b - a);
        QVERIFY2(!rumpf.contains(QStringLiteral("ablaufSchritt()")),
                 "das blosse Anfangen loest eine Sendung aus — mitten im "
                 "laufenden Fenster waere das ein zweites");
    }
};

QTEST_MAIN(TstFt8FristQsoEndet)
#include "tst_ft8_frist_qso_endet.moc"
