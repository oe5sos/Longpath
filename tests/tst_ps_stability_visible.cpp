// SPDX-License-Identifier: GPL-3.0-or-later
//
// Wenn die Stabilitaetsregel eingreift, SIEHT man es.
//
// 2026-08-23. Die Regel (PureSignalStabilityPolicy) haelt die Korrektur
// beim Kaltstart zurueck und friert sie bei Aussetzern ein. Ohne
// Anzeige waere das eine Behauptung: der Betreiber saehe weiter
// "Correcting" und wuesste nicht, dass gerade gar nicht korrigiert
// wird.
//
// Das wiegt hier schwerer als sonst, weil ich die Regel NICHT gegen
// Hardware pruefen konnte — ich habe keinen Sender. Eine Regel, der man
// nicht beim Arbeiten zusehen kann, waere in dieser Lage nicht
// vertretbar.
//
// Gemessen wird der TEXT der Marke, also das, was am Bildschirm steht.

#include <QtTest>

#include "gui/PsaIndicatorWidget.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstPsStabilityVisible : public QObject
{
    Q_OBJECT

private:
    // Die Marke zeigt PS-Zustaende nur waehrend MOX — davor steht
    // ueberall "Pure Signal2". Ohne dieses Vorspiel prueft der Rest
    // nichts.
    void arm(PsaIndicatorWidget& w)
    {
        w.setPsEnabled(true);
        w.setMox(true);
        w.setCorrectionsBeingApplied(true);
    }

private slots:
    void ohneEingriffStehtCorrecting()
    {
        RadioModel model;
        PsaIndicatorWidget w(&model);
        arm(w);
        w.setStabilityAction(PsCorrectionAction::Run);
        qInfo().noquote() << "Run →" << w.psText();
        QVERIFY2(w.psText().contains(QStringLiteral("orrect")),
                 qPrintable(w.psText()));
    }

    void beimKaltstartStehtWartet()
    {
        RadioModel model;
        PsaIndicatorWidget w(&model);
        arm(w);
        w.setStabilityAction(PsCorrectionAction::Withhold);
        qInfo().noquote() << "Withhold →" << w.psText();
        QVERIFY2(w.psText().contains(QStringLiteral("wartet"))
                     || w.psText().contains(QStringLiteral("Wartet")),
                 qPrintable(w.psText()));
        // Und ganz sicher NICHT "Correcting" — das waere die stille
        // Falschaussage, um die es geht.
        QVERIFY2(!w.psText().contains(QStringLiteral("orrect")),
                 qPrintable(w.psText()));
    }

    void beimAussetzerStehtEingefroren()
    {
        RadioModel model;
        PsaIndicatorWidget w(&model);
        arm(w);
        w.setStabilityAction(PsCorrectionAction::Hold);
        qInfo().noquote() << "Hold →" << w.psText();
        QVERIFY2(w.psText().contains(QStringLiteral("ingefroren"))
                     || w.psText().contains(QStringLiteral("Halt")),
                 qPrintable(w.psText()));
        QVERIFY2(!w.psText().contains(QStringLiteral("orrect")),
                 qPrintable(w.psText()));
    }

    void zurueckAufLaufenGehtAuch()
    {
        // Ein Zustand, aus dem man nicht herauskommt, waere schlimmer
        // als keiner.
        RadioModel model;
        PsaIndicatorWidget w(&model);
        arm(w);
        w.setStabilityAction(PsCorrectionAction::Hold);
        w.setStabilityAction(PsCorrectionAction::Run);
        qInfo().noquote() << "Hold → Run →" << w.psText();
        QVERIFY2(w.psText().contains(QStringLiteral("orrect")),
                 qPrintable(w.psText()));
    }
};

QTEST_MAIN(TstPsStabilityVisible)
#include "tst_ps_stability_visible.moc"
