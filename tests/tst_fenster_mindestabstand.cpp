// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original test, no Thetis logic.
//
// Mindestabstand zwischen schwebenden Fenstern (2026-10-15 / 2026-10-21).
//
// Betreiber 2026-10-15: „der pixelabstand automatisch zu allen fenstern
// ist nicht da", und zum Mass: „ein minimaler automatischer Abstand,
// vielleicht 2 pixel. x und y."
//
// Der Abstand stand an diesem Tag schon einmal im Baum und musste wieder
// heraus (5922f1b6, mit Martins Bild daneben: „warum ist das alles
// verstellt"): er lief bei JEDEM moveEvent, also auch waehrend das
// Layoutprofil seine Fenster setzt, und zerlegte die Anordnung.
//
// Darum pruefen hier ZWEI Faelle gegeneinander, und der zweite ist der
// wichtigere:
//
//   * beimZiehenRuecktEsAuf        -- der Abstand greift, wenn gezogen wird
//   * dasPlatzierenVerstelltNichts -- und NICHT, wenn das Programm platziert
//
// Gegenprobe gefahren (2026-10-21, beide Richtungen, offscreen):
//   - Abstand ganz ausgebaut           -> beimZiehenRuecktEsAuf FAIL
//   - Abstand ohne Tor (Fassung 10-15) -> dasPlatzierenVerstelltNichts FAIL
//   - mit Tor                          -> beide gruen

#include <QtTest>
#include <QWidget>

#include "gui/StyleConstants.h"
#include "gui/WindowPlacement.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {

// Alle Zahlen hier sind Vielfache von 16 (kSnapGridPx): das Raster
// allein bewegt diese Fenster also nicht, und was sich bewegt, hat der
// Mindestabstand bewegt. Klein genug, um auch auf der kleinen
// Offscreen-Flaeche der CI ganz hineinzupassen.
constexpr int kBreite = 160;
constexpr int kHoehe  = 96;
const QRect kLinks(96, 96, kBreite, kHoehe);        // rechte Kante 255
const QRect kBuendigRechts(256, 96, kBreite, kHoehe);

// ── Was das Raster aus einem Fenster macht (2026-10-31) ──────────────
//
// Seit der Abstand IM Raster steckt (`snappedFrameRect`), liegt die
// nahe Kante auf der Linie und die ferne um `kFensterLuftPx` davor.
// Ein Fenster, das vorher genau eine Rasterzelle fuellte, ist danach
// also um die Luft schmaler und niedriger -- und bleibt dann dort:
// 256 + 157 = 413, naechste Linie 416, 416 - 3 = 413. Die Rechnung ist
// also ortsfest, ein zweiter Durchgang verkleinert nicht noch einmal.
// Genau das haelt `dasRasternIstOrtsfest` fest -- und NUR der, denn er
// rechnet mit `snappedFrameRect` und braucht kein Fenster. Die Laeufe
// mit echten Fenstern vergleichen die Groesse bewusst nicht auf den
// Punkt: unter xvfb/offscreen bekommt ein `Qt::Tool`-Fenster nicht
// unbedingt die Groesse, die es verlangt (im Haus nachgewiesen:
// offscreen macht aus einer Breite 796). Eine Aussage ueber Geometrie
// gehoert in den reinen Lauf, eine ueber Verhalten in den mit
// Fenster.
const QSize kGerastert(kBreite - Style::kFensterLuftPx,
                       kHoehe  - Style::kFensterLuftPx);

// Was das moveEvent eines schwebenden Fensters tut: eine Zeile.
void meldeBewegung(QWidget* w) { snapToGridAfterSettle(w); }

} // namespace

class TstFensterMindestabstand : public QObject { Q_OBJECT
private slots:

    // ── Die Rechnung, am Schreibtisch ────────────────────────────────

    // Buendig heisst: auf den Abstand ruecken, in der Achse, in der sie
    // sich beruehren -- und nur dort.
    void buendigWirdAufgeschoben()
    {
        const QRect r = mitMindestabstand(kBuendigRechts, {kLinks},
                                          Style::kPanelGapPx);
        QCOMPARE(r.left(), kLinks.right() + 1 + Style::kPanelGapPx);
        QCOMPARE(r.top(), kBuendigRechts.top());        // y unberuehrt
        QCOMPARE(r.size(), kBuendigRechts.size());      // Groesse unberuehrt
    }

    // Dasselbe nach oben: ein Fenster, das unten anstoesst, geht auf.
    void buendigDarunterGehtAuf()
    {
        const QRect unten(96, 192, kBreite, kHoehe);    // kLinks endet bei 191
        const QRect r = mitMindestabstand(unten, {kLinks}, Style::kPanelGapPx);
        QCOMPARE(r.top(), kLinks.bottom() + 1 + Style::kPanelGapPx);
        QCOMPARE(r.left(), unten.left());
    }

    // Wer schon Luft hat, bleibt liegen.
    void weitWegBleibtLiegen()
    {
        const QRect weit(512, 96, kBreite, kHoehe);
        QCOMPARE(mitMindestabstand(weit, {kLinks}, Style::kPanelGapPx), weit);
    }

    // Schraeg darueber ist kein Nachbar: die Fenster beruehren sich in
    // der x-Achse, ueberlappen aber in y nicht -- sie stehen nicht
    // nebeneinander, und nichts muss aufgehen.
    void schraegIstKeinNachbar()
    {
        const QRect schraeg(256, 320, kBreite, kHoehe);
        QCOMPARE(mitMindestabstand(schraeg, {kLinks}, Style::kPanelGapPx),
                 schraeg);
    }

    // Was mitten darueber liegt, hat der Bediener dorthin gelegt. Und
    // derselbe Fall traegt das Hauptfenster: ein schwebendes Fenster IN
    // ihm wird nicht hinausgeschoben.
    void mittenDarueberBleibtLiegen()
    {
        const QRect mitten(160, 128, kBreite, kHoehe);   // tief in kLinks
        QCOMPARE(mitMindestabstand(mitten, {kLinks}, Style::kPanelGapPx),
                 mitten);

        const QRect hauptfenster(0, 0, 1200, 800);
        const QRect drin(400, 300, kBreite, kHoehe);
        QCOMPARE(mitMindestabstand(drin, {hauptfenster}, Style::kPanelGapPx),
                 drin);
    }

    // Eine ganze Rasterzelle uebereinander ist gewollt und bleibt --
    // gemessen an Martins eigener Anordnung vom 2026-10-21: „Asr"
    // (576 + 272 = bis 847) und „Frequency" (ab 832) liegen dort um
    // genau 16 Punkte uebereinander. Eine Schwelle von 16 haette dieses
    // Paar beim ersten Zug um 18 Punkte auseinandergerissen.
    void eineGanzeRasterzelleUeberlappungBleibt()
    {
        const QRect asr(576, 624, 272, 176);
        const QRect frequency(832, 624, 352, 176);
        QCOMPARE(asr.right(), 847);
        QCOMPARE(frequency.left() - asr.right() - 1, -16);
        QCOMPARE(mitMindestabstand(frequency, {asr}, Style::kPanelGapPx),
                 frequency);

        // Die kleinen Reste darunter aber nicht: zwei Punkte, wie sie
        // entstehen, wenn ein Nachbar selbst schon aufgeruckt wurde.
        const QRect knapp(asr.right() - 1, 624, 352, 176);
        QCOMPARE(mitMindestabstand(knapp, {asr}, Style::kPanelGapPx).left(),
                 asr.right() + 1 + Style::kPanelGapPx);
    }

    // Ein Abstand von null ist keine Regel.
    void ohneAbstandKeineBewegung()
    {
        QCOMPARE(mitMindestabstand(kBuendigRechts, {kLinks}, 0),
                 kBuendigRechts);
    }

    // ── Und an echten Fenstern ───────────────────────────────────────

    // Der Fall des Betreibers: zwei eingerastete Fenster stossen
    // buendig aneinander, er zieht das rechte dorthin -- und es rueckt
    // auf zwei Punkte auf.
    void beimZiehenRuecktEsAuf()
    {
        QWidget a(nullptr, Qt::Tool | Qt::FramelessWindowHint);
        QWidget b(nullptr, Qt::Tool | Qt::FramelessWindowHint);
        a.setGeometry(kLinks);
        b.setGeometry(QRect(432, 96, kBreite, kHoehe));
        a.show();
        b.show();
        QVERIFY(QTest::qWaitForWindowExposed(&a, kFensterFrist));
        QVERIFY(QTest::qWaitForWindowExposed(&b, kFensterFrist));
        a.setGeometry(kLinks);
        // Ein Zug schickt Dutzende Meldungen; vier reichen, um ihn von
        // einem Platzieren zu unterscheiden.
        for (const int x : {400, 336, 288, kBuendigRechts.left()}) {
            b.setGeometry(QRect(x, 96, kBreite, kHoehe));
            meldeBewegung(&b);
        }
        QVERIFY(b.frameGeometry().left() == kBuendigRechts.left());

        const int erwartet = a.frameGeometry().right() + 1 + Style::kFensterLuftPx;
        QTRY_VERIFY_WITH_TIMEOUT(b.frameGeometry().left() == erwartet, 3000);
        // Die andere Achse bleibt, wo sie war. Die GROESSE wird hier
        // bewusst nicht auf den Punkt verglichen: unter xvfb/offscreen
        // bekommt ein `Qt::Tool`-Fenster nicht unbedingt die Groesse,
        // die es verlangt (im Haus nachgewiesen: offscreen macht aus
        // einer Breite 796). Die genaue Rechnung steht platformfrei in
        // `dasRasternIstOrtsfest`; hier zaehlt, dasz das Fenster
        // aufgerueckt ist und nicht in der anderen Achse verrutscht.
        QCOMPARE(b.frameGeometry().top(), kBuendigRechts.top());

        // Und es bleibt dort. Raster 16 gegen Abstand 2: die naechste
        // Rasterlinie IST die buendige Kante, von der wir gerade
        // weggeruckt sind -- ohne eine Rechnung, die den Abstand auch im
        // zweiten Durchgang liefert, zoege das Raster das Fenster hier
        // wieder zurueck.
        meldeBewegung(&b);
        QTest::qWait(600);
        QCOMPARE(b.frameGeometry().left(), erwartet);
    }

    // ── Der Fall, an dem es am 2026-10-15 gescheitert ist ────────────
    //
    // Das Layoutprofil setzt seine Fenster. Fuer Longpath ist das
    // Platzieren, fuer Qt sieht es aus wie Ziehen. `EigeneBewegung` ist
    // die Marke, mit der LayoutProfiles::runApply() den Unterschied
    // sagt -- und der Mindestabstand darf hier NICHT greifen, auch nicht
    // bei mehreren Meldungen hintereinander.
    void dasPlatzierenVerstelltNichts()
    {
        QWidget a(nullptr, Qt::Tool | Qt::FramelessWindowHint);
        QWidget b(nullptr, Qt::Tool | Qt::FramelessWindowHint);
        a.setGeometry(kLinks);
        b.setGeometry(QRect(432, 96, kBreite, kHoehe));
        a.show();
        b.show();
        QVERIFY(QTest::qWaitForWindowExposed(&a, kFensterFrist));
        QVERIFY(QTest::qWaitForWindowExposed(&b, kFensterFrist));

        {
            // Genau, was runApply() tut: Marke halten, Geometrien
            // setzen, und jedes Fenster meldet dabei aus seinem
            // moveEvent.
            const EigeneBewegung wirSelbst;
            a.setGeometry(kLinks);
            meldeBewegung(&a);
            for (const int x : {400, 336, 288, kBuendigRechts.left()}) {
                b.setGeometry(QRect(x, 96, kBreite, kHoehe));
                meldeBewegung(&b);
            }
        }
        const QRect vorher = b.frameGeometry();
        QCOMPARE(vorher.left(), kBuendigRechts.left());

        // Lange genug warten, dass der gedaempfte Rasterschritt (180 ms)
        // samt Rueckstoss durch waere.
        QTest::qWait(800);
        // Die LAGE bleibt -- darum geht es hier: ein Platzieren schiebt
        // nicht. Die Groesse darf sich aendern (der gedaempfte
        // Rasterschritt laeuft auch beim Platzieren; er war bis zum
        // 2026-10-31 nur ein Nichts, weil 160 x 96 schon auf dem Raster
        // lagen, und zieht jetzt die ferne Kante um die Luft herein) --
        // auf den Punkt verglichen wird sie hier NICHT, siehe den
        // xvfb-Hinweis oben.
        QCOMPARE(b.frameGeometry().topLeft(), vorher.topLeft());
        QCOMPARE(a.frameGeometry().topLeft(), kLinks.topLeft());
    }

    // Eine EINZELNE fremde Meldung ist kein Zug. Das ist das Loch, das
    // die Marke allein laesst: nach einem `setGeometry()` liefert Qt auf
    // manchen Fensterarten noch ein spaetes Lageereignis, und das kommt
    // an, wenn die `EigeneBewegung` schon gestorben ist. Erst ab der
    // dritten Meldung mit wirklich veraenderter Lage gilt es als Ziehen.
    void eineEinzelneMeldungIstKeinZug()
    {
        QWidget a(nullptr, Qt::Tool | Qt::FramelessWindowHint);
        QWidget b(nullptr, Qt::Tool | Qt::FramelessWindowHint);
        a.setGeometry(kLinks);
        b.setGeometry(QRect(432, 96, kBreite, kHoehe));
        a.show();
        b.show();
        QVERIFY(QTest::qWaitForWindowExposed(&a, kFensterFrist));
        QVERIFY(QTest::qWaitForWindowExposed(&b, kFensterFrist));

        // Ohne jede Marke, aber nur einmal -- so sieht ein Nachhall aus,
        // nicht ein Zug.
        b.setGeometry(kBuendigRechts);
        meldeBewegung(&b);
        QTest::qWait(800);
        // Nicht WEGGESCHOBEN -- das ist der Punkt, und nur der. Die
        // Groesse bleibt hier ungeprueft, siehe den xvfb-Hinweis oben.
        QCOMPARE(b.frameGeometry().topLeft(), kBuendigRechts.topLeft());
    }

    // ── Das Rastern ist ortsfest (2026-10-31) ────────────────────────
    //
    // Die ferne Kante um die Luft hereinzuziehen waere gefaehrlich,
    // wenn jeder weitere Durchgang noch einmal haerte: ein Layout, das
    // beim Starten wiederhergestellt wird, schruempfte dann bei jedem
    // Start um drei Punkte. Tut es nicht -- gerechnet: 256 + 157 = 413,
    // naechste Linie 416, 416 - 3 = 413.
    void dasRasternIstOrtsfest()
    {
        const QRect einmal  = snappedFrameRect(kBuendigRechts);
        const QRect zweimal = snappedFrameRect(einmal);
        const QRect dreimal = snappedFrameRect(zweimal);
        QCOMPARE(zweimal, einmal);
        QCOMPARE(dreimal, einmal);
        QCOMPARE(einmal.size(), kGerastert);
    }
};

QTEST_MAIN(TstFensterMindestabstand)
#include "tst_fenster_mindestabstand.moc"
