// =================================================================
// tests/tst_panadapter_header.cpp  (NereusSDR)
// =================================================================
//
// Die Kopfleiste des Panadapters.
//
// Der Panadapter war die einzige Flaeche ohne Kopf, obwohl jede Applet
// daneben einen hat — im Zeus-Bild steht dort „PANADAPTER · 13.139312
// MHz". Dieser Test haelt zwei Dinge fest, die beim naechsten Umbau
// leicht verlorengehen:
//
//   1. dass es die Zeile ueberhaupt gibt (Erreichbarkeit, wie bei den
//      Modusgruppen)
//   2. dass sie der Mitte FOLGT und sie nicht einmalig zeigt
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QPushButton>
#include <QSignalSpy>
#include <QLabel>

#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TestPanadapterHeader : public QObject
{
    Q_OBJECT

private:

    // Der Ablöse-Schalter ist der einzige Knopf im Kopf; ueber den
    // Hinweistext gefunden, damit die Suche haelt, wenn spaeter ein
    // zweiter dazukommt.
    // Gesucht wird ueber das ZEICHEN, nicht ueber den Hinweistext.
    //
    // Vorher stand hier eine Suche nach „window" oder „layout" im
    // Hinweis. Am 2026-08-21 bekam der Knopf einen deutschen Hinweis
    // (die Sperr-Begruendung), und die Suche fand ihn nicht mehr —
    // drei Faelle fielen aus, ohne dass am Knopf etwas kaputt war.
    // Eine Pruefung, die an einer Formulierung haengt, bricht bei
    // jeder Umformulierung.
    static QPushButton* floatButtonOf(PanadapterApplet& pan)
    {
        for (QPushButton* b : pan.findChildren<QPushButton*>()) {
            const QString t = b->text();
            if (t == QString::fromUtf8("\u2197")
                || t == QString::fromUtf8("\u2199")) {
                return b;
            }
        }
        return nullptr;
    }

private slots:

    void theHeaderExistsAndNamesItself()
    {
        PanadapterApplet pan(QStringLiteral("pan-0"));
        QVERIFY2(pan.titleLabel(), "der Panadapter hat keine Kopfleiste");
        QVERIFY2(pan.titleLabel()->text().contains(QStringLiteral("PANADAPTER")),
                 "die Kopfzeile nennt die Flaeche nicht");
    }

    // Ohne Radio steht NUR der Name da. Eine Frequenz ohne Verbindung
    // waere eine Behauptung — dieselbe Regel, nach der der Zeiger ohne
    // Messwert wegbleibt.
    void withoutARadioItShowsNoFrequency()
    {
        PanadapterApplet pan(QStringLiteral("pan-0"));
        QCOMPARE(pan.titleLabel()->text(), QStringLiteral("PANADAPTER"));
    }

    void theHeaderFollowsTheCentreFrequency()
    {
        PanadapterApplet pan(QStringLiteral("pan-0"));
        QVERIFY(pan.spectrumWidget());

        pan.spectrumWidget()->setFrequencyRange(7.131300e6, 192000.0);
        QVERIFY2(pan.titleLabel()->text().contains(QStringLiteral("7.131300")),
                 qPrintable(QStringLiteral("Kopfzeile zeigt: %1")
                                .arg(pan.titleLabel()->text())));

        // Und sie bleibt nicht stehen: ein zweiter Wert muss ankommen.
        pan.spectrumWidget()->setFrequencyRange(14.225000e6, 192000.0);
        QVERIFY2(pan.titleLabel()->text().contains(QStringLiteral("14.225000")),
                 "die Kopfzeile ist beim ersten Wert stehengeblieben");
    }

    // ── Der Schalter im Kopf ─────────────────────────────────────────
    //
    // Entwurf 4 (2026-08-19): der Kopf traegt EINEN Schalter fuer beide
    // Richtungen. Bisher lag „Float this pan" nur im Rechtsklick-Menue
    // und der Rueckweg nirgends — man musste das Fenster schliessen und
    // wissen, dass genau das zurueckdockt.
    void theHeaderHasAFloatSwitch()
    {
        PanadapterApplet pan(QStringLiteral("pan-0"));
        QVERIFY2(floatButtonOf(pan) != nullptr,
                 "ohne Schalter bleibt das Ablösen im Rechtsklick-Menue "
                 "vergraben");
    }

    // Der Schalter HAT abgeloest — bis zum 2026-08-21. Seitdem ist das
    // Abloesen gesperrt, weil es beim Beenden abstuerzt (Stapel und
    // drei verworfene Kuren: cd6e83f5), und weil der Grund weggefallen
    // ist, aus dem der Betreiber es benutzte: er zog damit den
    // Panadapter groesser, was seit eaeec343 direkt am Rand geht.
    //
    // Der Fall bleibt stehen und dreht sich um: er bewacht jetzt, dass
    // die Sperre haelt UND sich erklaert. Die ausfuehrliche Fassung
    // steht in tst_pan_float_locked.
    // Die Sperre vom 2026-08-21 ist Geschichte: die vier verlorenen
    // AetherSDR-Schutzmethoden sind rueckportiert, der Absturz-Repro
    // ueberlebt (tst_real_pan_float_state), der Schalter loest wieder
    // ab. Dieser Fall hiess einen Tag lang theSwitchIsLockedAndSaysWhy.
    void theSwitchAsksToDetach()
    {
        PanadapterApplet pan(QStringLiteral("pan-0"));
        QSignalSpy spy(&pan, &PanadapterApplet::floatRequested);

        QPushButton* b = floatButtonOf(pan);
        QVERIFY(b);
        QVERIFY2(b->isEnabled(), "Die Sperre ist zurueckgekommen");
        b->click();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("pan-0"));
    }

    // DER FALL, DER VORHER FEHLTE: derselbe Schalter muss zurueckfuehren.
    void andTheSameSwitchAsksToComeBack()
    {
        PanadapterApplet pan(QStringLiteral("pan-0"));
        pan.setFloatingIndicator(true);

        QSignalSpy back(&pan, &PanadapterApplet::dockRequested);
        QSignalSpy away(&pan, &PanadapterApplet::floatRequested);

        QPushButton* b = floatButtonOf(pan);
        QVERIFY(b);
        b->click();

        QCOMPARE(back.count(), 1);
        QVERIFY2(away.count() == 0,
                 "ein abgeloester Panadapter darf nicht noch einmal "
                 "ablösen wollen");
    }

    // Das Zeichen muss dem Zustand folgen, auch wenn der Umzug woanders
    // ausgeloest wurde — Rechtsklick, Fenster schliessen, Anordnung
    // wechseln. Ein Zeichen, das nur beim eigenen Klick nachzieht,
    // luegt beim naechsten Weg.
    void theSwitchSaysWhichWayItPoints()
    {
        PanadapterApplet pan(QStringLiteral("pan-0"));
        QPushButton* b = floatButtonOf(pan);
        QVERIFY(b);

        const QString docked = b->text();
        pan.setFloatingIndicator(true);
        QVERIFY2(b->text() != docked,
                 "abgeloest und eingefuegt muessen sich am Zeichen "
                 "unterscheiden");

        pan.setFloatingIndicator(false);
        QCOMPARE(b->text(), docked);
    }
};

QTEST_MAIN(TestPanadapterHeader)
#include "tst_panadapter_header.moc"
