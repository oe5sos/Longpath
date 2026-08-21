// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_disconnect_label_transparent.cpp  (Longpath)
// =================================================================
// Der „DISCONNECTED"-Schriftzug darf das Bild nicht ausloeschen.
//
// Er ist ein Kindwidget ueber der GANZEN Flaeche des Panadapters
// (setGeometry(0, 0, width(), height())) und trug bis 2026-08-21
// `background-color: rgba(10, 12, 20, 200)` — eine zu 78 % deckende
// schwarze Decke.
//
// Der Betreiber hat den Fehler zweimal gemeldet, ohne ihn benennen zu
// koennen, weil er wie ein Zeitproblem aussah:
//
//   „beim start war das hintergrundbild kurz perfekt, hat sich dann
//    aber wieder abgedunkelt"
//   „die ersten 10 sekunden bild perfekt da und danach verschwindet es"
//
// Die Decke erscheint eben erst, wenn der Verbindungsversuch aufgibt.
// Sie verdeckte das Hintergrundbild UND die Einblendungen (Kompass,
// Stehwelle), die darunter ins Spektrum gemalt werden.
//
// Getrenntsein ist ein Zustand, keine Gefahr — das steht schon im
// Quelltext daneben. Dann darf es auch nicht das ganze Bild
// ausloeschen.
//
// Modification history (Longpath):
//   2026-08-21 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QLabel>

#include "gui/SpectrumWidget.h"

using namespace Longpath;

class TestDisconnectLabelTransparent : public QObject
{
    Q_OBJECT
private slots:
    void theLabelDoesNotBlanketTheSpectrum()
    {
        auto* w = new SpectrumWidget();
        w->resize(700, 400);
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w));

        QLabel* label = nullptr;
        for (QLabel* l : w->findChildren<QLabel*>()) {
            if (l->text() == QStringLiteral("DISCONNECTED")) { label = l; break; }
        }
        QVERIFY2(label, "der Schriftzug muss es geben");

        const QString qss = label->styleSheet();
        QVERIFY2(qss.contains(QStringLiteral("background-color: transparent")),
                 qPrintable(QStringLiteral(
                     "der Schriftzug MUSS durchsichtig sein — er liegt "
                     "ueber der ganzen Flaeche des Panadapters und "
                     "verdeckt sonst Hintergrundbild und Einblendungen. "
                     "Stylesheet: %1").arg(qss)));

        // Und er darf die Bedienung nicht blockieren: ein Schriftzug,
        // der das Abstimmen abfaengt, waere die Decke in anderer Form.
        QVERIFY2(label->testAttribute(Qt::WA_TransparentForMouseEvents),
                 "Klicks muessen hindurchgehen");
        w->hide();
        delete w;
    }
};
QTEST_MAIN(TestDisconnectLabelTransparent)
#include "tst_disconnect_label_transparent.moc"
