// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_flex_rauschsperre.cpp  (Longpath)
// =================================================================
//
// Betreiber 2026-10-31: „wo kann ich den squelch einstellen" -- und
// auf die Antwort „RX-Applet, Knopf SQL": „der funktioniert nicht",
// „schieberegler ohne änderung".
//
// Unsere Rauschsperre schreibt in `RxChannel` (WDSP). Eine
// Flex-Scheibe ist aber eine Scheibe OHNE RxChannel --
// `MainWindow_Flex.cpp:45` sagt das selbst. Der Regler hatte kein
// Ziel.
//
// Die Maschine hat eine eigene. Direkt von ihr gelesen
// (`sub slice all`, nur lesend):  squelch=0 squelch_level=20
// =================================================================

#include <QtTest/QtTest>

#include "core/FlexRadioConnection.h"

using namespace Longpath;

class TstFlexRauschsperre : public QObject
{
    Q_OBJECT

private slots:
    // Die beiden Schluessel stehen so, wie die Maschine sie nennt.
    void dieBefehleTragenDieNamenDerMaschine()
    {
        QCOMPARE(FlexCommandWire::setSliceSquelchCommand(QStringLiteral("0"), true),
                 QStringLiteral("slice set 0 squelch=1"));
        QCOMPARE(FlexCommandWire::setSliceSquelchCommand(QStringLiteral("0"), false),
                 QStringLiteral("slice set 0 squelch=0"));
        QCOMPARE(FlexCommandWire::setSliceSquelchLevelCommand(QStringLiteral("0"), 20),
                 QStringLiteral("slice set 0 squelch_level=20"));
    }

    // Ohne Scheibenkennung geht nichts hinaus -- ein Befehl an
    // niemanden ist schlimmer als keiner.
    void ohneScheibeKeinBefehl()
    {
        QVERIFY(FlexCommandWire::setSliceSquelchCommand(QString(), true).isEmpty());
        QVERIFY(FlexCommandWire::setSliceSquelchLevelCommand(QString(), 20).isEmpty());
    }

    // Die Stufe laeuft von 0 bis 100, wie der Schieber im RX-Applet.
    void dieStufeBleibtImBereich()
    {
        QVERIFY(!FlexCommandWire::setSliceSquelchLevelCommand(
                    QStringLiteral("0"), 0).isEmpty());
        QVERIFY(!FlexCommandWire::setSliceSquelchLevelCommand(
                    QStringLiteral("0"), 100).isEmpty());
        QVERIFY(FlexCommandWire::setSliceSquelchLevelCommand(
                    QStringLiteral("0"), 101).isEmpty());
        QVERIFY(FlexCommandWire::setSliceSquelchLevelCommand(
                    QStringLiteral("0"), -1).isEmpty());
    }

    // ── Und der Regler erreicht den Weg wirklich ────────────────────
    //
    // Der eigentliche Fehler war nicht ein fehlender Befehl, sondern
    // ein Regler ohne Ziel. Dieser Fall haelt die Verdrahtung fest.
    void derReglerErreichtDieMaschine()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/models/RadioModel.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());

        // JEDEN Rumpf einzeln, bis zu seinem eigenen Ende. Ein Fenster
        // fester Groesse ab dem ersten Treffer reicht NICHT: der Name
        // steht mehrfach in der Datei, und das Fenster erwischte dann
        // den Nachbarn. Beim ersten Versuch am 2026-10-31 blieb die
        // Gegenprobe deswegen gruen, obwohl die Zeile entfernt war --
        // ein Waechter, der nicht faellt, waecht nicht.
        for (const QString& signal : {QStringLiteral("&SliceModel::ssqlEnabledChanged"),
                                      QStringLiteral("&SliceModel::ssqlThreshChanged")}) {
            const int a = src.indexOf(signal);
            QVERIFY2(a > 0, qPrintable(signal));
            const int e = src.indexOf(QStringLiteral("\n    });"), a);
            QVERIFY2(e > a, qPrintable(signal));
            QVERIFY2(src.mid(a, e - a).contains(QStringLiteral("sendeFlexRauschsperre")),
                     qPrintable(signal + QStringLiteral(
                         " geht nicht an die Maschine — bei der Flex "
                         "bewirkt der Regler dann wieder nichts")));
        }
    }
};

QTEST_MAIN(TstFlexRauschsperre)
#include "tst_flex_rauschsperre.moc"
