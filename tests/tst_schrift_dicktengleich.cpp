// Prüfstand: wo eine dicktengleiche Schrift gemeint ist, muss auch eine
// ankommen — auf jeder Plattform.
//
// Warum es das gibt: vier Stellen bauten ihre Schrift selbst zusammen und
// nannten dabei je EINEN Familiennamen, den es auf der laufenden Plattform
// nicht gibt:
//
//   TitleBar.cpp:350          QFont("SF Mono", 10, DemiBold)
//   SupportDialog.cpp:137     QFont("Consolas", 9)        <- Windows-Schrift!
//   SpectrumStatusOverlay     QFont("monospace", 9, Bold) <- CSS-Gattung
//   LayoutThumbnail           QFont("sans-serif", 14)     <- CSS-Gattung
//
// `SF Mono` ist auf macOS nicht als Familie angemeldet (Apple liefert sie nur
// mit Xcode/Terminal aus), `Consolas` gibt es dort gar nicht, und die
// CSS-Gattungsnamen kennt Qt nicht als Familie. In all diesen Fällen sucht Qt
// selbst etwas aus — und das ist nicht zwangsläufig dicktengleich. Damit kam
// genau das zurück, was `Style::monoFont` 2026-09-27 schon einmal abgestellt
// hatte: tanzende Stellen in einer Zahl.
//
// Belegt im Betriebslog vom 2026-10-03, bei jedem Start:
//   WRN: Populating font family aliases took 35 ms. Replace uses of missing
//        font family "SF Mono" with one that exists to avoid this cost.
//
// Geprüft wird darum nicht der Familienname, sondern das Ergebnis: was Qt
// nach der Auswahl tatsächlich hergibt (QFontInfo). Ein Prüfstand auf den
// Namen würde genau den Fehler wiederholen, um den es geht — der Name sagt
// nichts darüber, ob die Schrift existiert.

#include <QtTest>
#include <QFontInfo>

#include "gui/StyleConstants.h"

using namespace Longpath;

class TestSchriftDicktengleich : public QObject {
    Q_OBJECT

private slots:

    /** Was monoFontPt liefert, muss nach Qts Auswahl dicktengleich sein. */
    void monoFontPtIstWirklichDicktengleich()
    {
        const QFont f = Style::monoFontPt(10);
        const QFontInfo info(f);
        QVERIFY2(info.fixedPitch(),
                 qPrintable(QStringLiteral("Qt waehlte '%1' — nicht dicktengleich")
                                .arg(info.family())));
        QCOMPARE(f.pointSize(), 10);
    }

    /** Auch halbfett und in anderen Groessen — das Gewicht darf die Auswahl
     *  nicht kippen (eine Familie kann einzelne Schnitte vermissen lassen). */
    void auchHalbfettBleibtDicktengleich()
    {
        for (const int pt : {8, 9, 10, 12, 14}) {
            const QFont f = Style::monoFontPt(pt, QFont::DemiBold);
            QVERIFY2(QFontInfo(f).fixedPitch(),
                     qPrintable(QStringLiteral("bei %1 pt nicht dicktengleich")
                                    .arg(pt)));
        }
    }

    /** Die Schwesterfunktion mit Pixelgroesse muss dieselbe Zusage halten —
     *  sie ist die aeltere und wird oefter benutzt. */
    void monoFontMitPixelgroesseEbenso()
    {
        const QFont f = Style::monoFont(QApplication::font(), 12);
        QVERIFY2(QFontInfo(f).fixedPitch(),
                 qPrintable(QStringLiteral("Qt waehlte '%1' — nicht dicktengleich")
                                .arg(QFontInfo(f).family())));
    }

    /** Die Gegenprobe, und sie ist der eigentliche Beleg: ein Name, den es
     *  sicher nirgends gibt, liefert KEINE dicktengleiche Schrift. Ohne das
     *  koennte der Prüfstand oben auch dann gruen sein, wenn QFontInfo auf
     *  dieser Maschine alles fuer dicktengleich hielte. */
    void erfundeneFamilieIstNichtDicktengleich()
    {
        QFont erfunden(QStringLiteral("Diese Schrift Gibt Es Nicht XYZ"), 10);
        // Ohne styleHint/fixedPitch: genau die Lage der vier alten Stellen.
        const QFontInfo info(erfunden);
        QVERIFY2(!info.fixedPitch(),
                 "Auf dieser Maschine gilt sogar eine erfundene Familie als "
                 "dicktengleich — dann sagt der Prüfstand oben nichts aus");
    }
};

QTEST_MAIN(TestSchriftDicktengleich)
#include "tst_schrift_dicktengleich.moc"
