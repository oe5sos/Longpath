// no-port-check: Longpath-eigener Pruefstand.
// =================================================================
// tests/tst_digi_frequenzen.cpp  (Longpath)
// =================================================================
//
// Die Tafel der Anrufstellen, und die eine Regel, die daran haengt:
// wer schon richtig steht, wird nicht verstellt.
//
// Der Anlass steht in DigiFrequenzen.h -- „ich will nicht die freq
// suchen". Ein Griff, der die falsche Frequenz einstellt, waere
// schlimmer als gar keiner: auf 14.080 statt 14.074 hoert man nichts
// und sucht dann den Fehler im Dekoder.
//
// Ohne Fenster und ohne Funkgeraet (QTEST_APPLESS_MAIN) -- die Tafel
// kennt beides nicht, und das soll so bleiben.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/DigiFrequenzen.h"

using namespace Longpath;
using Longpath::DigiFrequenzen::Digimodus;

class TstDigiFrequenzen : public QObject
{
    Q_OBJECT

private slots:

    void dieBaenderDieJederBenutztStimmen()
    {
        // Die Zahlen aus der Vorgabeliste von WSJT-X. Sie stehen hier
        // ein zweites Mal und ABSICHTLICH ausgeschrieben: eine Tafel,
        // die sich selbst prueft, prueft nichts.
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band40m, Digimodus::Ft8),
                 qint64(7'074'000));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band20m, Digimodus::Ft8),
                 qint64(14'074'000));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band15m, Digimodus::Ft8),
                 qint64(21'074'000));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band10m, Digimodus::Ft8),
                 qint64(28'074'000));

        // FT4 liegt woanders, und zwar nicht um einen festen Abstand --
        // auf 40 m darunter, auf 20 m darueber. Genau deshalb kann man
        // es nicht ausrechnen.
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band40m, Digimodus::Ft4),
                 qint64(7'047'500));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band20m, Digimodus::Ft4),
                 qint64(14'080'000));
    }

    void wasEsNichtGibtMeldetNullUndNichtIrgendwas()
    {
        // 60 m hat FT8, aber keine allgemein benutzte FT4-Stelle.
        QVERIFY(DigiFrequenzen::anruffrequenz(Band::Band60m, Digimodus::Ft8) > 0);
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band60m, Digimodus::Ft4),
                 qint64(0));

        // Und was gar kein Amateurband ist, hat keine. Null heisst
        // „ich weiss es nicht" -- der Aufrufer laesst die Frequenz
        // dann stehen, statt das Geraet irgendwohin zu stellen.
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::GEN, Digimodus::Ft8),
                 qint64(0));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::WWV, Digimodus::Ft8),
                 qint64(0));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band49m, Digimodus::Ft8),
                 qint64(0));
    }

    void werSchonDortStehtWirdNichtVerstellt()
    {
        QVERIFY(DigiFrequenzen::stehtSchonDort(14'074'000, Digimodus::Ft8));
        // Ein Geraet meldet nicht immer auf das Hertz genau, und wer
        // sich 300 Hz danebenstellt, meint es so.
        QVERIFY(DigiFrequenzen::stehtSchonDort(14'074'300, Digimodus::Ft8));
        QVERIFY(DigiFrequenzen::stehtSchonDort(14'073'700, Digimodus::Ft8));

        // Zwei Kilohertz daneben ist nicht mehr „dort".
        QVERIFY(!DigiFrequenzen::stehtSchonDort(14'076'000, Digimodus::Ft8));

        // Und die Betriebsart zaehlt mit: auf der FT4-Stelle steht man
        // fuer FT8 NICHT richtig, auch wenn es dasselbe Band ist.
        // Genau dieser Fall kostet sonst eine halbe Stunde Fehlersuche
        // im Dekoder.
        QVERIFY(DigiFrequenzen::stehtSchonDort(14'080'000, Digimodus::Ft4));
        QVERIFY(!DigiFrequenzen::stehtSchonDort(14'080'000, Digimodus::Ft8));
    }

    void einUngueltigerWertGiltNichtAlsRichtigStehend()
    {
        // Sonst liesse ein Geraet, das noch nichts gemeldet hat
        // (Frequenz 0), den Griff tatenlos zurueck -- und der Bediener
        // haette gedrueckt und nichts bekommen.
        QVERIFY(!DigiFrequenzen::stehtSchonDort(0, Digimodus::Ft8));
        QVERIFY(!DigiFrequenzen::stehtSchonDort(-1, Digimodus::Ft8));
    }
};

QTEST_APPLESS_MAIN(TstDigiFrequenzen)
#include "tst_digi_frequenzen.moc"
