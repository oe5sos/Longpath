// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_fensterknoepfe.cpp  (Longpath)
// =================================================================
// „bei apple sind links oben 3 farbige symbole, rot, gelb und grün
//  … diese sollte auch in longpath zu sehen sein"
//
// Ein QDialog bekommt von Qt auf macOS nur den Schliessknopf. Die
// anderen beiden Punkte zeichnet das System dann grau und tot -- und
// ein Fenster, das man nicht ins Dock legen kann, muss man
// schliessen, womit die Einstellungsarbeit darin weg ist.
//
// Geprüft wird beides: dass die Knöpfe kommen, UND dass sie an den
// Stellen ausbleiben, an denen sie schaden würden.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QDialog>

#include "gui/FensterKnoepfe.h"

using namespace Longpath;

class TstFensterKnoepfe : public QObject {
    Q_OBJECT

private slots:
    // Der gewöhnliche Fall: ein nicht-modales Werkzeugfenster.
    void aPlainDialogGetsAllThree()
    {
        QDialog d;
        QVERIFY(!d.windowFlags().testFlag(Qt::WindowMinimizeButtonHint));

        QVERIFY(FensterKnoepfe::knoepfeNachruesten(&d));

        QVERIFY2(d.windowFlags().testFlag(Qt::WindowMinimizeButtonHint),
                 "kein Zuklappknopf — genau der fehlte");
        QVERIFY(d.windowFlags().testFlag(Qt::WindowMaximizeButtonHint));
        QVERIFY(d.windowFlags().testFlag(Qt::WindowCloseButtonHint));
    }

    // Zweimal ändert nichts mehr: der Filter läuft bei jedem Polish.
    void twiceChangesNothing()
    {
        QDialog d;
        QVERIFY(FensterKnoepfe::knoepfeNachruesten(&d));
        QVERIFY2(!FensterKnoepfe::knoepfeNachruesten(&d),
                 "zweiter Durchgang hat nochmals angefasst");
    }

    // Modal NICHT: ein modales Fenster im Dock sperrt das Programm
    // und ist von dort nicht mehr zu erreichen.
    void modalIsLeftAlone()
    {
        QDialog d;
        d.setWindowModality(Qt::ApplicationModal);
        QVERIFY2(!FensterKnoepfe::knoepfeNachruesten(&d),
                 "ein modales Fenster darf nicht ins Dock können");
        QVERIFY(!d.windowFlags().testFlag(Qt::WindowMinimizeButtonHint));
    }

    // Und die absichtlich rahmenlosen bleiben rahmenlos.
    void framelessAndPopupsAreLeftAlone()
    {
        QDialog rahmenlos;
        rahmenlos.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        QVERIFY(!FensterKnoepfe::knoepfeNachruesten(&rahmenlos));

        QWidget popup;
        popup.setWindowFlags(Qt::Popup);
        QVERIFY(!FensterKnoepfe::knoepfeNachruesten(&popup));

        QWidget werkzeug;
        werkzeug.setWindowFlags(Qt::Tool);
        QVERIFY(!FensterKnoepfe::knoepfeNachruesten(&werkzeug));
    }

    // Ein Kind-Widget ist kein Fenster und wird nicht angefasst.
    void aChildWidgetIsNotAWindow()
    {
        QWidget eltern;
        auto* kind = new QWidget(&eltern);
        QVERIFY(!FensterKnoepfe::knoepfeNachruesten(kind));
    }
};

QTEST_MAIN(TstFensterKnoepfe)
#include "tst_fensterknoepfe.moc"
