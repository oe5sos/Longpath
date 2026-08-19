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
#include <QLabel>

#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestPanadapterHeader : public QObject
{
    Q_OBJECT

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
};

QTEST_MAIN(TestPanadapterHeader)
#include "tst_panadapter_header.moc"
