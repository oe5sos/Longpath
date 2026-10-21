// =================================================================
// tests/tst_maske_sagt_was_dahinter_liegt.cpp  (Longpath)
// =================================================================
// Longpath-original — no Thetis source ported here; no attribution
// markers required.
// =================================================================
//
// Warum es diesen Prueflauf gibt
// ------------------------------
// Die Betreiberregel vom 2026-09-01 lautet „ALLE fliegenden Fenster
// gehoeren hinter die ConnectMaske". Sie ist richtig, und Longpath
// haelt sie: `hideFloatingWindowsBehindConnectMask()` versteckt
// Panadapter, Meter, Rotor und Logbuch und merkt sie sich, bis die
// Maske weg ist.
//
// Nur: sie SCHWIEG darueber. Am 2026-10-21 hat das eine Stunde
// gekostet. Die FLEX-8400 war verbunden, Bild und Ton liefen
// nachweislich — 2581 Bildrahmen und 1306 Wasserfallzeilen standen im
// Protokoll, das Zeichnen meldete „wird gezeichnet" — und der
// Betreiber sah eine schwarze Flaeche. Sein Panadapter ist ein
// schwebendes Fenster und lag damit hinter genau dem Dialog, in dem er
// „Connected" las. Die Elternkette sagte es schliesslich:
//
//     Elternkette 2: PanFloatingWindow sichtbar=false versteckt=true
//
// Gesucht wurde derweil im Funkgeraet. Ein Satz im Dialog haette
// gereicht, und dieser Prueflauf haelt ihn fest.
//
// Die Gegenprobe ist mitgefahren: ohne `setWindowsBehindMask()` bleibt
// das Schild leer und unsichtbar, und beide Faelle unten fallen um.

#include <QLabel>
#include <QObject>
#include <QtTest>

#include "gui/ConnectionPanel.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstMaskeSagtWasDahinterLiegt : public QObject
{
    Q_OBJECT

private slots:
    void ohneVersteckteFensterSchweigtDieMaske()
    {
        RadioModel model;
        ConnectionPanel panel(&model);
        QLabel* schild = panel.behindMaskLabelForTest();
        QVERIFY2(schild, "die Maske hat kein Schild fuer Verstecktes");

        panel.setWindowsBehindMask(0);
        QVERIFY2(!schild->isVisible(),
                 "ohne verstecktes Fenster darf kein Hinweis stehen");
        QVERIFY(schild->text().isEmpty());
    }

    void einVerstecktesFensterWirdGenannt()
    {
        RadioModel model;
        ConnectionPanel panel(&model);
        QLabel* schild = panel.behindMaskLabelForTest();
        QVERIFY(schild);

        panel.setWindowsBehindMask(1);

        // Der Faenger: der Text muss die ZAHL tragen und sagen, was zu
        // tun ist. Ein blosses „es ist etwas versteckt" haette den
        // Betreiber am 2026-10-21 genauso ratlos gelassen.
        QVERIFY2(!schild->text().isEmpty(),
                 "ein verstecktes Fenster, und die Maske sagt nichts");
        QVERIFY2(schild->text().contains(QStringLiteral("1")),
                 qPrintable(QStringLiteral("die Zahl fehlt im Hinweis: ")
                            + schild->text()));
        QVERIFY2(schild->text().contains(QStringLiteral("close"),
                                         Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("der Hinweis sagt nicht, was hilft: ")
                            + schild->text()));
        QVERIFY2(!schild->toolTip().isEmpty(),
                 "kein Tooltip, der die Regel erklaert");
    }

    void mehrereFensterWerdenGezaehlt()
    {
        RadioModel model;
        ConnectionPanel panel(&model);
        QLabel* schild = panel.behindMaskLabelForTest();
        QVERIFY(schild);

        panel.setWindowsBehindMask(7);
        QVERIFY2(schild->text().contains(QStringLiteral("7")),
                 qPrintable(QStringLiteral("falsche Zahl: ") + schild->text()));
    }

    void derHinweisVerschwindetWieder()
    {
        // Sonst steht er beim naechsten Oeffnen der Maske noch da,
        // obwohl nichts mehr versteckt ist — ein Schild, das luegt,
        // ist schlimmer als keines (HAUSSTIL Regel 7 in Geist: lieber
        // ein Strich als eine falsche Zahl).
        RadioModel model;
        ConnectionPanel panel(&model);
        QLabel* schild = panel.behindMaskLabelForTest();
        QVERIFY(schild);

        panel.setWindowsBehindMask(3);
        QVERIFY(!schild->text().isEmpty());

        panel.setWindowsBehindMask(0);
        QVERIFY2(schild->text().isEmpty(),
                 "der Hinweis blieb stehen, nachdem die Fenster zurueck sind");
        QVERIFY(!schild->isVisible());
        QVERIFY(schild->toolTip().isEmpty());
    }
};

QTEST_MAIN(TstMaskeSagtWasDahinterLiegt)
#include "tst_maske_sagt_was_dahinter_liegt.moc"
