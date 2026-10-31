// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_flex_wasserfall_ohne_klick.cpp  (Longpath)
// =================================================================
//
// Betreiber 2026-10-31: „warum der wasserfall dort nichts anzeigt
// weisz ich auch nicht" -- und der Satz, der es aufklaerte: „einmal
// klick in den panadapter und es funktioniert."
//
// `m_waterfall` entstand nur in `applyResizeSettled()`. Kam das
// Fenster in gespeicherter Groesse hoch, blieb es null, und beide
// Zeilenwege verwarfen jede Zeile. Im Protokoll desselben Laufs
// standen 10 285 angekommene Wasserfallzeilen -- der Strom lief, nur
// schrieb sie niemand.
//
// Der Lauf liest die Quelle: ein `SpectrumWidget` braucht Schirm und
// GPU, und der CI-Laeufer hat keine (2026-10-21).
// =================================================================

#include <QtTest/QtTest>

class TstFlexWasserfallOhneKlick : public QObject
{
    Q_OBJECT

private slots:
    void beideZeilenwegeLegenDasBildNotfallsAn()
    {
        const QByteArray root = qgetenv("LONGPATH_SOURCE_DIR");
        if (root.isEmpty()) { QSKIP("LONGPATH_SOURCE_DIR nicht gesetzt"); }
        QFile f(QString::fromLocal8Bit(root)
                + QStringLiteral("/src/gui/SpectrumWidget.cpp"));
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
        const QString src = QString::fromUtf8(f.readAll());

        QVERIFY2(src.contains(QStringLiteral("bool SpectrumWidget::stelleWasserfallbildSicher")),
                 "der Helfer ist weg");

        // Der eigene Weg (aus der FFT).
        const int a = src.indexOf(QStringLiteral("void SpectrumWidget::pushWaterfallRow"));
        QVERIFY(a > 0);
        const QString eigen = src.mid(a, 900);
        QVERIFY2(eigen.contains(QStringLiteral("stelleWasserfallbildSicher")),
                 "pushWaterfallRow verwirft wieder, solange kein Bild da ist");

        // Und der Weg der Maschine -- bei der Flex ist DAS der Weg.
        const int b = src.indexOf(QStringLiteral("void SpectrumWidget::zeigeFremdeWasserfallzeile"));
        QVERIFY(b > 0);
        const QString fremd = src.mid(b, 900);
        QVERIFY2(fremd.contains(QStringLiteral("stelleWasserfallbildSicher")),
                 "die Zeilen der Maschine fallen wieder ins Leere");
        QVERIFY2(!fremd.contains(QStringLiteral("if (m_waterfall.isNull()) { return; }")),
                 "das blinde Verwerfen steht wieder da");
    }
};

QTEST_MAIN(TstFlexWasserfallOhneKlick)
#include "tst_flex_wasserfall_ohne_klick.moc"
