// SPDX-License-Identifier: GPL-3.0-or-later
//
// Griffleisten muss man treffen koennen.
//
// Der Betreiber, 2026-08-21: „der pandapter muss jetzt endlich mal
// funktionieren, das ist das kernstueck." Davor hatte er gesagt, er
// loese ihn nur ab, weil das „der einzige Weg" sei, die Groesse zu
// aendern — und das Abloesen ist die Handlung, die abstuerzt.
//
// Es war nie der einzige Weg. Die Splitter waren immer da, ihre Griffe
// aber DREI PIXEL breit; die im Panadapter-Stapel setzten gar nichts
// und blieben auf Qts Vorgabe. Drei Pixel trifft man auf einem
// Retina-Schirm mit der Maus nicht.
//
// Das ist dieselbe Fehlerklasse wie im Erreichbarkeits-Durchgang, nur
// in Pixeln statt in Signalen: vorhanden, verdrahtet, richtig — und
// unerreichbar. Ein Ziehgriff, den niemand fassen kann, ist kein
// Ziehgriff.

#include <QtTest>
#include <QSplitter>

#include "gui/MainWindow.h"
#include "gui/StyleConstants.h"

using namespace Longpath;

class TstSplitterHandlesGrabbable : public QObject
{
    Q_OBJECT

private slots:
    void everySplitterInTheMainWindowCanBeGrabbed()
    {
        auto* mw = new MainWindow();
        mw->resize(1700, 1000);
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(250);

        const auto splitters = mw->findChildren<QSplitter*>();
        QVERIFY2(!splitters.isEmpty(),
                 "Keine Splitter gefunden — dann prueft das hier nichts");

        QStringList tooThin;
        for (QSplitter* sp : splitters) {
            if (sp->handleWidth() < Style::kSplitterHandlePx) {
                tooThin << QStringLiteral("%1 (%2 px)")
                               .arg(QString::fromLatin1(
                                        sp->metaObject()->className()))
                               .arg(sp->handleWidth());
            }
        }

        qInfo() << "Splitter im Hauptfenster:" << splitters.size();

        QVERIFY2(tooThin.isEmpty(),
                 qPrintable(QStringLiteral(
                     "Diese Griffe sind schmaler als %1 px und damit mit "
                     "der Maus nicht zu fassen:\n  %2")
                     .arg(Style::kSplitterHandlePx)
                     .arg(tooThin.join(QStringLiteral("\n  ")))));

        mw->close();
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
    }

    /// Und der Panadapter-Stapel gehoert ausdruecklich dazu.
    ///
    /// Er ist der Grund, warum das hier steht: seine Splitter setzten
    /// die Breite ueberhaupt nicht.
    void theStackAroundThePanadapterIsIncluded()
    {
        QVERIFY2(Style::kSplitterHandlePx >= 6,
                 "Unter sechs Pixeln wird der Griff wieder unfassbar — "
                 "genau der Zustand, der das Abloesen erzwungen hat");
    }
};

QTEST_MAIN(TstSplitterHandlesGrabbable)
#include "tst_splitter_handles_grabbable.moc"
