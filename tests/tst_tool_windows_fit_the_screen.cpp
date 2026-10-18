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
#include <QFile>
#include <QGuiApplication>
#include <QAction>
#include <QScrollArea>
#include <QCheckBox>
#include <QTabWidget>

#include "gui/ConnectionPanel.h"
#include "gui/LayoutProfiles.h"
#include <QPushButton>
#include "gui/MainWindow.h"
#include "gui/WindowPlacement.h"
#include "gui/applets/BroadcastOptimizerPanel.h"
#include "core/Sprache.h"
#include "core/strip/AetherPreset.h"
#include "core/strip/StripChain.h"
#include "gui/applets/StripWindow.h"
#include "models/RadioModel.h"
#include "FensterFrist.h"

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

    // „im channel strip das live hören nicht mehr zu sehen!"
    //
    // Martin, 2026-10-12 morgens. Die Ursache lag an jenem Morgen
    // NICHT im Fenster: in /Applications stand eine Fassung aus einem
    // fremden Zweig (feat/fm-relaisbetrieb@203cdc35, 03:14 Uhr), die
    // den Mithoerer gar nicht enthaelt -- 116 StripLiveMonitor-
    // Symbole in dieser Fassung, null in jener. Das kann kein
    // Prueftand finden.
    //
    // Was er finden kann, ist die zweite Haelfte derselben Gefahr:
    // die Mithoerreihe ist auf sechs Bedienteile gewachsen (Haekchen,
    // Latenz, Kanal, Eingang, Ausgang, Lautstaerke), und zwei davon
    // tragen Geraetenamen -- so lang, wie die Soundkarte heisst. Wird
    // die Reihe breiter als das Fenster, muss das Fenster mitwachsen,
    // und auf einem MacBook Air ragt es dann ueber den Schirm: die
    // hinteren Teile der Reihe sind „nicht mehr zu sehen", mit genau
    // derselben Meldung aus einem ganz anderen Grund.
    void theLiveMonitorRowStaysInTheWindow()
    {
        RadioModel radio;
        StripWindow w(&radio);

        const QStringList teile{ QStringLiteral("stripLiveBtn"),
                                 QStringLiteral("stripLiveLatency"),
                                 QStringLiteral("stripLiveChannel"),
                                 QStringLiteral("stripLiveIn"),
                                 QStringLiteral("stripLiveOut"),
                                 QStringLiteral("stripLiveVol") };
        int reihe = 0;
        for (const QString& n : teile) {
            auto* teil = w.findChild<QWidget*>(n);
            QVERIFY2(teil != nullptr, qPrintable(n));
            reihe += teil->minimumSizeHint().width();
            qInfo() << n << "min" << teil->minimumSizeHint().width()
                    << "hint" << teil->sizeHint().width();
        }
        qInfo() << "Reihe (nur Mithoerer) min" << reihe
                << "| Fenster min" << w.minimumSizeHint().width()
                << "| Fenster hint" << w.sizeHint().width();

        // Das Fenster oeffnet 980 px breit. Verlangt sein Inhalt mehr,
        // waechst es darueber hinaus und laeuft auf einem 1470er
        // Schirm an den Rand.
        QVERIFY2(w.minimumSizeHint().width() <= 980,
                 qPrintable(QStringLiteral("Mindestbreite %1 px -- die "
                                           "Mithoerreihe sprengt das Fenster")
                                .arg(w.minimumSizeHint().width())));
    }

    // Ein Bild der Stufenflaeche, zur Durchsicht. Nur mit
    // LONGPATH_RENDER_DIR; im CI also nicht.
    void rendersTheStripSurface()
    {
        const QByteArray dir = qgetenv("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) { QSKIP("LONGPATH_RENDER_DIR nicht gesetzt"); }

        RadioModel radio;
        StripWindow w(&radio);

        // Mit Martins Kette im Bild, nicht mit den Vorgaben: die Reihe
        // oben soll zeigen, dass die Röhre vorne steht.
        if (StripChain* c = radio.stripChain()) {
            const QString profil = QString::fromLatin1(LONGPATH_TEST_DATA_DIR)
                                   + QStringLiteral("/aether-ssb-30-07-2026.json");
            if (QFile::exists(profil)) {
                AetherPreset::applyFile(profil, *c);
                c->setEnabled(true);
            }
        }

        for (const int breite : { 1180, 980, 880 }) {
            w.resize(breite, 900);
            w.grab().save(QString::fromUtf8(dir)
                          + QStringLiteral("/kanalzug-%1.png").arg(breite));
        }
    }

    // Ein Klick auf eine Kachel der Kette oeffnet DIESE Stufe.
    //
    // Die Reihe meldet eine Stufennummer 0..7, die Reiterleiste traegt
    // seit dem Uebersichtsreiter aber vorne noch „Strip". Das Fenster
    // rechnet deshalb +1 -- eine Annahme, die beim naechsten
    // eingeschobenen Reiter still danebenliegt und sich als „der
    // Klick oeffnet die falsche Stufe" zeigt, was niemand einem
    // Indexversatz zuordnet. (Genau das war zwischen dem
    // Uebersichtsreiter und heute Abend der Fall.)
    void theTabAfterTheOverviewIsTheFirstStage()
    {
        RadioModel radio;
        StripWindow w(&radio);
        auto* tabs = w.findChild<QTabWidget*>();
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->tabText(0), QStringLiteral("Strip"));
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            const auto st = static_cast<StripChain::Stage>(i);
            QCOMPARE(tabs->tabText(i + 1),
                     Sprache::T(
                         QString::fromLatin1(StripChain::stageName(st))));
        }
    }

    // Jede Stufe hat ihren eigenen Schalter -- auch der Entzerrer.
    //
    // „den EQ kann man eigentlich nicht on und off schalten." Stimmte:
    // die anderen Stufenseiten tragen oben ihr Haekchen, die
    // Entzerrerseite ist als ganzes Panel von AetherSDR uebernommen
    // und hatte dort keines. Gerade beim Entzerren ist es der
    // wichtigste Knopf -- eine Kurve beurteilt man, indem man sie
    // wegnimmt.
    //
    // Geprueft wird die WIRKUNG, nicht die Beschriftung. Der erste
    // Anlauf suchte nach „<Stufe> on" und fiel ueber PUDU, dessen
    // Haekchen „Exciter on" heisst -- ein Prueftand, der Namen
    // vergleicht, prueft die Namen.
    void everyStagePageCarriesItsOwnSwitch()
    {
        RadioModel radio;
        StripWindow w(&radio);
        StripChain* c = radio.stripChain();
        QVERIFY(c != nullptr);
        auto* tabs = w.findChild<QTabWidget*>();
        QVERIFY(tabs != nullptr);

        for (int i = 0; i < StripChain::kStageCount; ++i) {
            const auto st = static_cast<StripChain::Stage>(i);
            // Die Oberfläche ist deutsch (core/Sprache.h) -- verglichen
            // wird mit dem übersetzten Namen, sonst prüft der Prüfstand
            // eine Sprache, die niemand sieht.
            const QString name = Sprache::T(
                QString::fromLatin1(StripChain::stageName(st)));
            QWidget* page = tabs->widget(i + 1);
            QVERIFY2(page != nullptr, qPrintable(name));

            bool works = false;
            for (QCheckBox* b : page->findChildren<QCheckBox*>()) {
                const bool before = c->stageEnabled(st);
                b->setChecked(!before);
                if (c->stageEnabled(st) == !before) {
                    b->setChecked(before);
                    works = c->stageEnabled(st) == before;
                    break;
                }
                b->setChecked(before);
            }
            QVERIFY2(works,
                     qPrintable(QStringLiteral("die Stufenseite %1 hat "
                                               "kein Haekchen, das diese "
                                               "Stufe schaltet").arg(name)));
        }
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
            if (tabs->tabText(i)
                == Sprache::T(QStringLiteral("Voice check"))) {
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
        // ── Dieser Fall kann auf dem Schirmlosen nicht laufen ────────
        //
        // Gemessen am 2026-10-13: das Hauptfenster traegt einen
        // QRhiWidget (den Zeigerkranz), und sobald der beim Einhaengen
        // in seinen Behaelter umgehaengt wird, meldet die schirmlose
        // Qt-Anbindung „This plugin does not support setParent!" und
        // greift danach in einen Zeiger 0x1 (EXC_BAD_ACCESS in
        // objc_msgSend). Das ist eine Grenze der Anbindung, nicht
        // unsere: der Absturz liegt in Cocoa-Code, den es nur auf dem
        // Mac gibt.
        //
        // Ueberspringen statt rot stehen lassen -- und mit Grund, denn
        // ein roter Prueftand, den niemand ausfuehren kann, ist ein
        // kaputtes Werkzeug. Auf einem echten Schirm laeuft der Fall;
        // Prueflaeufe mit Fenstern gehoeren hier aber nicht hin (der
        // Betreiber hielt am 2026-09-30 eine Sonde fuer Longpath
        // selbst).
#ifdef Q_OS_MAC
        if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
            QSKIP("QRhiWidget + setParent stuerzt in der schirmlosen "
                  "Qt-Anbindung des Macs ab");
        }
#endif
        auto* mw = new MainWindow();        // bewusst nicht abgeraeumt
        // Seit 2026-10-17 formatfuellend MIT Rahmen: randlos nimmt die
        // Titelleiste mit, und mit ihr die drei Knoepfe links oben, die
        // der Betreiber zweimal vermisst hat. showMaximized() fuellt den
        // Schirm genauso.
        QVERIFY2(!mw->windowFlags().testFlag(Qt::FramelessWindowHint),
                 "nach dem Start randlos -- dann fehlen die Fensterknoepfe");
        mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(mw, kFensterFrist));
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
        // Das Prinzip bleibt: ein Profil schaltet das Formatfuellende
        // nicht ab. Nur die FORM hat sich am 2026-10-17 geaendert --
        // maximiert statt randlos, damit die Fensterknoepfe bleiben.
        QVERIFY2(!mw->windowFlags().testFlag(Qt::FramelessWindowHint),
                 "ein Profil hat das Fenster randlos gemacht");
        QVERIFY2(mw->isMaximized(),
                 "ein Profil schaltet das Formatfuellende nicht ab");

        // Esc im Hauptfenster: ein Feld abbrechen, nicht das Vollbild.
        QTest::keyClick(mw, Qt::Key_Escape);
        QTest::qWait(50);
        QVERIFY2(!mw->windowFlags().testFlag(Qt::FramelessWindowHint),
                 "Esc hat das Fenster randlos gemacht");
        QVERIFY2(mw->isMaximized(),
                 "Esc soll ein Feld abbrechen, nicht das Formatfuellende");
    }
};

QTEST_MAIN(TstToolWindowsFitTheScreen)
#include "tst_tool_windows_fit_the_screen.moc"
