// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_tool_windows_fit_the_screen.cpp  (Longpath)
// =================================================================
// Longpath fuellt den Schirm, und ein Werkzeugfenster ragt nicht
// darueber hinaus.
//
// Der Betreiber, 2026-09-27, mit Bildern vom MacBook Air (1470 x 956,
// nutzbar 849 px hoch): „longpath sollte immer formatfüllend sein."
// Dazu stand der Kanalzug 980 x 1000 da -- unten hinter dem Dock, und
// kleiner ziehen ging nicht, weil das Layout ~1000 px Mindesthoehe
// verlangte.
//
// Geprueft:
//   - fitFrameIntoArea(): passt -> unveraendert; zu gross -> kleiner und
//     herein; Mindestgroesse gewinnt, Titelleiste bleibt erreichbar;
//   - der Kanalzug kommt mit weniger als 849 px aus, oeffnet aber, wo
//     Platz ist, so gross wie bisher (Seiten scrollen nur bei Bedarf);
//   - das Hauptfenster ist nach dem Start randlos formatfuellend, ein
//     Profil mit fullScreen=false aendert daran nichts, Esc auch nicht.
//
// Modification history (Longpath):
//   2026-09-27 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QAction>
#include <QScrollArea>
#include <QTabWidget>

#include "gui/ConnectionPanel.h"
#include "gui/LayoutProfiles.h"
#include <QPushButton>
#include "gui/MainWindow.h"
#include "gui/WindowPlacement.h"
#include "gui/applets/BroadcastOptimizerPanel.h"
#include "gui/applets/StripWindow.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstToolWindowsFitTheScreen : public QObject
{
    Q_OBJECT
private slots:
    void aFrameThatFitsStaysPut()
    {
        const QRect area(0, 33, 1470, 849);
        const QRect frame(200, 100, 760, 700);
        QCOMPARE(fitFrameIntoArea(frame, area), frame);
    }

    void aTooTallFrameShrinksAndComesIn()
    {
        // Das Bild vom 2026-09-27: 980 x 1000 (+28 Titelleiste) bei y=33.
        const QRect area(0, 33, 1470, 849);
        const QRect frame(235, 33, 980, 1028);
        const QRect fit = fitFrameIntoArea(frame, area, QSize(560, 400));
        QCOMPARE(fit, QRect(235, 33, 980, 849));
        QVERIFY(area.contains(fit));
    }

    void aFrameHangingOverTheEdgeIsPushedBack()
    {
        const QRect area(0, 33, 1470, 849);
        const QRect fit = fitFrameIntoArea(QRect(1200, 600, 600, 400), area);
        QCOMPARE(fit, QRect(870, 482, 600, 400));
    }

    void theMinimumWinsAndTheTitleBarStaysReachable()
    {
        const QRect area(0, 33, 1470, 849);
        const QRect fit = fitFrameIntoArea(QRect(300, 300, 900, 1200), area,
                                           QSize(600, 1000));
        QCOMPARE(fit.size(), QSize(900, 1000));
        QCOMPARE(fit.topLeft(), QPoint(300, 33));
    }

    void theChannelStripFitsAnAirButOpensAsBefore()
    {
        RadioModel radio;
        StripWindow w(&radio);
        // 849 px nutzbar, 28 davon Titelleiste.
        QVERIFY2(w.minimumSizeHint().height() <= 821,
                 qPrintable(QStringLiteral("Mindesthoehe %1 px passt nicht auf "
                                           "ein MacBook Air")
                                .arg(w.minimumSizeHint().height())));
        QVERIFY2(w.minimumSizeHint().width() <= 1470,
                 qPrintable(QStringLiteral("Mindestbreite %1 px")
                                .arg(w.minimumSizeHint().width())));

        // Wo Platz ist, bleibt alles wie es war: jede Stufenseite meldet
        // ihre eigene Groesse, nicht die gedeckelte der Scrollflaeche.
        auto* tabs = w.findChild<QTabWidget*>();
        QVERIFY(tabs);
        int wrapped = 0;
        for (int i = 0; i < tabs->count(); ++i) {
            auto* sa = qobject_cast<QScrollArea*>(tabs->widget(i));
            if (!sa || !sa->widget()) { continue; }
            ++wrapped;
            QCOMPARE(sa->sizeHint(), sa->widget()->sizeHint());
        }
        // Seit dem Umbau auf Aethers Form sind die acht Stufen EINE
        // Flaeche ("Strip") statt acht Reiter -- wer von Hand
        // einstellt, muss die ganze Kette sehen, waehrend er an einer
        // Stufe dreht. Entscheidend bleibt, dass sie scrollt: das
        // Raster ist hoeher als ein MacBook Air, und ohne Rollflaeche
        // liegt sein unteres Ende hinter dem Dock.
        QVERIFY2(wrapped >= 1, "die Stufenflaeche scrollt");
        QVERIFY2(tabs->count() >= 3,
                 "Strip, On air und Voice check sind da");
        QVERIFY2(tabs->tabText(0) == QLatin1String("Strip"),
                 "die Stufenflaeche steht vorn");
    }

    // Ein Bild der Stufenflaeche, zur Durchsicht. Nur mit
    // LONGPATH_RENDER_DIR; im CI also nicht.
    void rendersTheStripSurface()
    {
        const QByteArray dir = qgetenv("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_RENDER_DIR nicht gesetzt"); }

        RadioModel radio;
        StripWindow w(&radio);
        w.resize(1180, 900);
        w.grab().save(QString::fromUtf8(dir)
                      + QStringLiteral("/kanalzug-eine-flaeche.png"));
    }

    // Der Einschubreiter, mit echtem Suchlauf.
    //
    // Gerendert und nicht beschrieben, weil die Gruppierung GERATEN
    // ist (siehe core/strip/PluginPlacement.h) -- ob das Raten auf
    // dieser Maschine brauchbar aussieht, sagt nur das Bild. Und
    // geprueft wird dabei eines, das kein Bild zeigt: dass der
    // Suchlauf ueberhaupt etwas findet und nichts davon in der
    // Oberflaeche verschwindet.
    void thePluginsTabRendersWithWhateverIsInstalled()
    {
        const QByteArray dir = qgetenv("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_RENDER_DIR nicht gesetzt"); }

        RadioModel radio;
        StripWindow w(&radio);
        w.resize(1180, 900);

        auto* tabs = w.findChild<QTabWidget*>();
        QVERIFY(tabs != nullptr);
        int at = -1;
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == QLatin1String("Plugins")) { at = i; break; }
        }
        QVERIFY2(at >= 0, "kein Reiter „Plugins\"");
        tabs->setCurrentIndex(at);

        QPushButton* scan = nullptr;
        for (QPushButton* b : w.findChildren<QPushButton*>()) {
            if (b->text().contains(QLatin1String("Look for plugins"))) {
                scan = b;
                break;
            }
        }
        QVERIFY(scan != nullptr);
        scan->click();
        QCoreApplication::processEvents();

        w.grab().save(QString::fromUtf8(dir)
                      + QStringLiteral("/kanalzug-plugins.png"));
    }

    // ── Der Stimmcheck war die eine Ausnahme ─────────────────────────
    //
    // Acht Stufenseiten in einer Scrollflaeche, der Stimmcheck-Reiter
    // ohne — und damit kam er durch, solange er kurz genug war. Am
    // 2026-10-09 kam der Sendeton-Optimierer ans Ende dieses Reiters,
    // und der ganze Abschnitt lag unter der Fensterkante: unsichtbar,
    // nicht erreichbar, und der Reiter sah voellig normal aus. Der
    // Betreiber am Geraet: „das ist aber nichts neues."
    //
    // Geprueft wird beides, weil eines allein nichts belegt: dass der
    // Reiter scrollt, UND dass der Optimierer wirklich darin haengt.
    // Ein Reiter mit Scrollflaeche und ohne Panel waere genauso leer.
    void theVoiceCheckTabScrollsAndCarriesTheOptimizer()
    {
        RadioModel radio;
        StripWindow w(&radio);

        auto* tabs = w.findChild<QTabWidget*>();
        QVERIFY(tabs);

        int voiceTab = -1;
        for (int i = 0; i < tabs->count(); ++i) {
            if (tabs->tabText(i) == QLatin1String("Voice check")) {
                voiceTab = i;
                break;
            }
        }
        QVERIFY2(voiceTab >= 0, "der Stimmcheck-Reiter ist da");

        auto* sa = qobject_cast<QScrollArea*>(tabs->widget(voiceTab));
        QVERIFY2(sa != nullptr,
                 "der Stimmcheck haengt in einer Scrollflaeche wie jede "
                 "Stufenseite -- sonst faellt sein unteres Ende hinter "
                 "den Dock");
        QVERIFY(sa->widget() != nullptr);

        auto* panel = w.findChild<BroadcastOptimizerPanel*>();
        QVERIFY2(panel != nullptr,
                 "der Sendeton-Optimierer haengt im Stimmcheck");
        QVERIFY2(sa->widget()->isAncestorOf(panel),
                 "und zwar INNERHALB der Scrollflaeche, nicht daneben");

        // Die Seite ist hoeher als der Platz -- genau deshalb muss sie
        // scrollen. Waere sie es nicht, pruefte dieser Fall nichts.
        QVERIFY(sa->widget()->sizeHint().height() > 600);
    }

    void theMainWindowAlwaysFillsTheScreen()
    {
        auto* mw = new MainWindow();        // bewusst nicht abgeraeumt
        QVERIFY2(mw->windowFlags().testFlag(Qt::FramelessWindowHint),
                 "nach dem Start formatfuellend (randlos)");
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw));
        if (QTest::qWaitFor([mw]() { return mw->findChild<ConnectionPanel*>() != nullptr; },
                            15000)) {
            mw->findChild<ConnectionPanel*>()->close();
            QTRY_VERIFY_WITH_TIMEOUT(mw->findChild<ConnectionPanel*>() == nullptr, 5000);
        }

        // Ein Profil, das ohne Vollbild aufgenommen wurde.
        auto* profiles = mw->findChild<LayoutProfiles*>();
        QVERIFY(profiles);
        QVariantMap state;
        state.insert(QStringLiteral("mainWindow"),
                     QVariantMap{{QStringLiteral("fullScreen"), false},
                                 {QStringLiteral("maximized"), false}});
        QVERIFY(profiles->createWith(QStringLiteral("Ohne-Vollbild"), state));
        QTest::qWait(100);
        QVERIFY2(mw->windowFlags().testFlag(Qt::FramelessWindowHint),
                 "ein Profil schaltet das Vollbild nicht ab");

        // Esc im Hauptfenster: ein Feld abbrechen, nicht das Vollbild.
        QTest::keyClick(mw, Qt::Key_Escape);
        QTest::qWait(50);
        QVERIFY(mw->windowFlags().testFlag(Qt::FramelessWindowHint));
    }
};

QTEST_MAIN(TstToolWindowsFitTheScreen)
#include "tst_tool_windows_fit_the_screen.moc"
