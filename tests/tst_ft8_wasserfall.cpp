// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der Streifen im FT8-Fenster rechnet an einer Stelle, die man NICHT
// ansehen kann: Hz <-> Bildpunkt. Genau dort entscheidet sich, ob ein
// Klick die Stelle trifft, die der Betreiber gemeint hat.
//
// Darum steht die Umrechnung als statische Funktion da und nicht im
// Malcode -- dieser Lauf braucht kein Fenster, keinen Schirm und keine
// Tonkarte.
//
// Die Falle, gegen die der zweite Fall steht: der Streifen zeigt 0 bis
// 3000 Hz, aber SENDEN darf man nur zwischen 200 und 2800. Ohne das
// Begrenzen ergaebe ein Klick an den linken Rand eine Sendestelle von
// 0 Hz -- eine Tonlage, die es nicht gibt, und der Sender wuerde sie
// klaglos nehmen.

#include <QtTest>

#include <cmath>

#include "gui/widgets/Ft8Wasserfall.h"

using namespace Longpath;

class TstFt8Wasserfall : public QObject
{
    Q_OBJECT

private slots:
    void dieSkalaLiegtRichtigHerum()
    {
        const int breite = 600;
        QCOMPARE(int(Ft8Wasserfall::hzZuX(0, breite)), 0);
        QCOMPARE(int(Ft8Wasserfall::hzZuX(3000, breite)), breite);
        // Die Mitte des Durchlassbereichs in der Mitte des Streifens.
        QCOMPARE(int(Ft8Wasserfall::hzZuX(1500, breite)), breite / 2);
    }

    void einKlickTrifftDieGemeinteStelle()
    {
        const int breite = 600;
        // In der Mitte geklickt heisst 1500 Hz -- auf ein paar Hertz
        // genau, mehr gibt ein Bildpunkt nicht her.
        const int mitte = Ft8Wasserfall::xZuSendeHz(breite / 2.0, breite);
        QVERIFY2(qAbs(mitte - 1500) <= 5, qPrintable(QString::number(mitte)));

        // Hin und zurueck: was gezeichnet wird, muss auch getroffen
        // werden.
        for (int hz : {400, 1000, 1500, 2000, 2500}) {
            const double x = Ft8Wasserfall::hzZuX(hz, breite);
            const int zurueck = Ft8Wasserfall::xZuSendeHz(x, breite);
            QVERIFY2(qAbs(zurueck - hz) <= 5,
                     qPrintable(QStringLiteral("%1 -> %2").arg(hz).arg(zurueck)));
        }
    }

    void dieRaenderGebenKeineUnmoeglicheStelle()
    {
        const int breite = 600;
        // Ganz links: NICHT 0 Hz, sondern die kleinste sendbare Stelle.
        QCOMPARE(Ft8Wasserfall::xZuSendeHz(0.0, breite),
                 Ft8Wasserfall::kSendeMinHz);
        // Ganz rechts: nicht 3000, sondern die groesste sendbare.
        QCOMPARE(Ft8Wasserfall::xZuSendeHz(double(breite), breite),
                 Ft8Wasserfall::kSendeMaxHz);
        // Und auch daneben geklickt bleibt es im Rahmen.
        QCOMPARE(Ft8Wasserfall::xZuSendeHz(-50.0, breite),
                 Ft8Wasserfall::kSendeMinHz);
        QCOMPARE(Ft8Wasserfall::xZuSendeHz(breite + 50.0, breite),
                 Ft8Wasserfall::kSendeMaxHz);
    }

    // ── Die Marke steht, wo die Regel sagt ──────────────────────────
    //
    // Die Umrechnung oben kann stimmen und der Malcode sie trotzdem
    // anders benutzen. Dieser Fall schliesst die Luecke: das Widget
    // wird wirklich gezeichnet, und an der ausgerechneten Spalte muss
    // die gruene Marke stehen.
    //
    // Mit LONGPATH_WASSERFALL_PNG=<pfad> legt der Lauf das Bild ab --
    // fuer die Beurteilung der Gestaltung, ohne dass eine Antenne
    // dranhaengen muss.
    void dieMarkeStehtWoDieRegelSagt()
    {
        Ft8Wasserfall w;
        w.resize(600, 120);
        w.setzeSendeTon(1700);

        // Ein erfundener Ton bei 1200 Hz UND Rauschen -- eine Sekunde,
        // verschraenktes Stereo wie vom Abgriff.
        //
        // Das Rauschen ist nicht Beiwerk, sondern noetig (2026-10-18):
        // ein mathematisch reiner Sinus hat einen Rauschboden bei
        // -110 dB, und dann saettigt selbst die ferne Flanke des
        // Fensters die Anzeige. So klingt kein Band. Mit Rauschen
        // zeigt der Streifen das, was der Betreiber wirklich sieht.
        const int rate = 48000;
        const int rahmen = rate;
        QVector<float> ton(rahmen * 2);
        quint32 zufall = 12345;   // fest, damit der Lauf wiederholbar ist
        for (int i = 0; i < rahmen; ++i) {
            zufall = zufall * 1664525u + 1013904223u;
            const float r = (float(zufall >> 8) / float(1 << 24) - 0.5f) * 0.02f;
            const float s =
                0.25f * float(std::sin(2.0 * M_PI * 1200.0 * i / rate)) + r;
            ton[i * 2] = s;
            ton[i * 2 + 1] = s;
        }
        // In Haeppchen einspeisen, nicht in einem Stueck: so faehrt der
        // Lauf auch das WEITERSCHIEBEN des Wasserfalls, und das Bild
        // zeigt, was der Betreiber nach ein paar Sekunden sieht.
        const int haeppchen = 4096;
        for (int ab = 0; ab + haeppchen <= rahmen; ab += haeppchen) {
            w.speiseStereo(ton.constData() + ab * 2, haeppchen, rate);
        }

        qInfo().noquote()
            << "Breite:" << w.width()
            << " Sammler:" << w.sammlerForTest()
            << " Spalten:" << w.spektrumSpaltenForTest()
            << " Spitze:" << w.spektrumMaxForTest();

        const QImage bild = w.grab().toImage();
        QVERIFY2(!bild.isNull(), "nichts gezeichnet");

        const QByteArray pfad = qgetenv("LONGPATH_WASSERFALL_PNG");
        if (!pfad.isEmpty()) {
            QVERIFY(bild.save(QString::fromLocal8Bit(pfad)));
            qInfo().noquote() << "Bild abgelegt:" << QString::fromLocal8Bit(pfad);
        }

        // Die Marke: an der ausgerechneten Spalte muss Gruen stehen.
        const double mx = Ft8Wasserfall::hzZuX(1700, bild.width());
        bool gruenGefunden = false;
        for (int y = 0; y < bild.height(); ++y) {
            // Zwei Bildpunkte Spielraum -- der Strich ist eins breit,
            // die Geraetepixel koennen doppelt zaehlen (Retina).
            for (int dx = -2; dx <= 2 && !gruenGefunden; ++dx) {
                const int x = int(mx) + dx;
                if (x < 0 || x >= bild.width()) { continue; }
                const QColor c = bild.pixelColor(x, y);
                if (c.green() > c.red() + 20 && c.green() > 60) {
                    gruenGefunden = true;
                }
            }
        }
        QVERIFY2(gruenGefunden,
                 "die Marke der Sendestelle steht nicht an der Spalte, "
                 "die hzZuX() ausrechnet");
    }

    void eineBreiteVonNullStuerztNicht()
    {
        // Beim Aufbau hat das Widget noch keine Breite. Ein Klick kann
        // dann zwar nicht kommen, aber gemalt wird trotzdem.
        QCOMPARE(int(Ft8Wasserfall::hzZuX(1500, 0)), 0);
        QCOMPARE(Ft8Wasserfall::xZuSendeHz(10.0, 0),
                 Ft8Wasserfall::kSendeMinHz);
    }
};

QTEST_MAIN(TstFt8Wasserfall)
#include "tst_ft8_wasserfall.moc"
