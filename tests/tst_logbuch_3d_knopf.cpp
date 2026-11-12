// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_logbuch_3d_knopf.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der „3D"-Knopf in der EINGEBETTETEN Karte des Logbuchs (2026-11-10).
// Live-Befund: ohne Schluessel ein Klick, und es geschah sichtbar nichts
// -- kein Einrichtungsdialog, der Knopf blieb oben. Dieser Lauf baut das
// echte Logbuchfenster mit der Kartenspalte, meldet eine Attrappe als
// 3D-Flaeche an (wie main.cpp die WebEngine-Fassung) und klickt mit der
// Maus auf den Knopf.

#include <QtTest>

#include "core/AppSettings.h"
#include "core/GoogleMapTiles.h"
#include "gui/Google3dSetupDialog.h"
#include "gui/LogbookWindow.h"
#include "gui/MainWindow.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/GibsTileLayer.h"
#include "gui/widgets/Station3dSurface.h"

#include <QAccessible>
#include <QAccessibleActionInterface>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include "FensterFrist.h"

using namespace Longpath;

namespace {

QVector<Station3dFlight> g_fluege;   // was die Attrappe zu fliegen bekam

class FakeSurface : public Station3dSurface {
public:
    using Station3dSurface::Station3dSurface;
    void fly(const Station3dFlight& f) override { g_fluege << f; }
};

QString writeAdif(const QDir& dir)
{
    const QString path = dir.filePath(QStringLiteral("log.adi"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    f.write("Longpath test\n<EOH>\n"
            "<CALL:5>K1ABC <QSO_DATE:8>20260902 <TIME_ON:6>143710 <BAND:3>20m <MODE:3>SSB "
            "<GRIDSQUARE:4>FN42 <MY_GRIDSQUARE:6>JN67UT <EOR>\n");
    f.close();
    return path;
}

Google3dSetupDialog* visibleSetup()
{
    for (QWidget* t : QApplication::topLevelWidgets()) {
        auto* d = qobject_cast<Google3dSetupDialog*>(t);
        if (d && d->isVisible()) { return d; }
    }
    return nullptr;
}

} // namespace

class TstLogbuch3dKnopf : public QObject { Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookShowMap"), QStringLiteral("True"));
        AppSettings::instance().setValue(QStringLiteral("QsoMap3dFlight"), true);
        GoogleMapsKey::erase();
        GoogleMapsKey::forgetCachedForTest();
        CesiumIonToken::erase();
        CesiumIonToken::forgetCachedForTest();
        Map3dAccess::setChosenSource(Map3dAccess::Source::CesiumIon);
        Station3dSurface::setFactory([](QWidget* p) -> Station3dSurface* {
            return new FakeSurface(p);
        });
        g_fluege.clear();
    }
    void cleanup()
    {
        if (Google3dSetupDialog* d = visibleSetup()) { d->close(); }
        Station3dSurface::setFactory({});
    }

    // ── Oben neben „Map" (2026-11-12) ────────────────────────────────
    // Betreiber: „3d war nicht zu finden". Die Pille in der Kartenspalte
    // stand klein unter „Azimuthal"; ohne nachgeschlagene Station tat sie
    // nichts. Jetzt steht „3D" in der oberen Leiste direkt hinter „Map",
    // holt die Karte mit und zeigt SOFORT ein Bild.
    void oben_direkt_neben_map()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookShowMap"), QStringLiteral("False"));
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QPushButton* map = w.mapToggleForTest();
        QPushButton* b   = w.threeDToggleForTest();
        QVERIFY2(b, "kein 3D-Knopf in der Logbuchleiste");
        QVERIFY(b->isVisible());
        QCOMPARE(b->text(), QStringLiteral("3D"));
        // Dieselbe Reihe, unmittelbar rechts von „Map".
        QCOMPARE(b->y(), map->y());
        QVERIFY2(b->x() > map->x() + map->width()
                     && b->x() - (map->x() + map->width()) < 20,
                 qPrintable(QStringLiteral("Map %1..%2, 3D ab %3")
                                .arg(map->x()).arg(map->x() + map->width()).arg(b->x())));
        // Und kein anderes Widget liegt darueber.
        QWidget* hit = w.childAt(b->mapTo(&w, b->rect().center()));
        QVERIFY2(hit == b, "ueber dem 3D-Knopf liegt etwas");
    }

    // Nichts markiert: ein Druck holt die Karte und fliegt im 3D zum
    // eigenen QTH -- nicht „nichts".
    void ohne_auswahl_fliegt_er_zum_eigenen_qth()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookShowMap"), QStringLiteral("False"));
        QVERIFY(CesiumIonToken::save(QStringLiteral(
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiJwcnVlZnN0YW5kIn0.UHJ1ZWZzdGFuZE5pY2h0RWNodA")));
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        if (QTableWidget* t = w.findChild<QTableWidget*>()) { t->clearSelection(); }

        QPushButton* b = w.threeDToggleForTest();
        QTest::mouseClick(b, Qt::LeftButton, Qt::NoModifier, b->rect().center());

        QsoMapWindow* m = w.mapPanelForTest();
        QVERIFY2(m && m->isVisible(), "die Karte kam nicht mit");
        QVERIFY2(m->zeigt3d(), "kein 3D-Bild");
        QVERIFY(b->isChecked());
        QVERIFY(w.mapToggleForTest()->isChecked());
        QCOMPARE(g_fluege.size(), 1);
        QCOMPARE(g_fluege.last().call, QStringLiteral("JN67UT"));
        QVERIFY2(!g_fluege.last().hasHome, "Grosskreis vom QTH zu sich selbst");

        // Noch ein Druck: zurueck auf die flache Karte, der Knopf rastet aus.
        QTest::mouseClick(b, Qt::LeftButton, Qt::NoModifier, b->rect().center());
        QVERIFY(!m->zeigt3d());
        QVERIFY(!b->isChecked());
        QVERIFY(m->isVisible());   // die Karte bleibt, nur flach
    }

    // Eine markierte Zeile ist das Ziel.
    void mit_auswahl_fliegt_er_zur_markierten_station()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookShowMap"), QStringLiteral("False"));
        QVERIFY(CesiumIonToken::save(QStringLiteral(
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiJwcnVlZnN0YW5kIn0.UHJ1ZWZzdGFuZE5pY2h0RWNodA")));
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QTableWidget* t = w.findChild<QTableWidget*>();
        QVERIFY(t && t->rowCount() == 1);
        t->selectRow(0);

        QPushButton* b = w.threeDToggleForTest();
        QTest::mouseClick(b, Qt::LeftButton, Qt::NoModifier, b->rect().center());

        QVERIFY(w.mapPanelForTest()->zeigt3d());
        QVERIFY(b->isChecked());
        QVERIFY(!g_fluege.isEmpty());
        QCOMPARE(g_fluege.last().call, QStringLiteral("K1ABC"));
        QVERIFY(g_fluege.last().hasHome);   // mit Grosskreis von daheim
    }

    // Ohne Zugang: derselbe Weg wie die Pille -- die Einrichtung, und
    // der Knopf rastet nicht ein.
    void ohne_zugang_oeffnet_er_die_einrichtung()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookShowMap"), QStringLiteral("False"));
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QPushButton* b = w.threeDToggleForTest();
        QTest::mouseClick(b, Qt::LeftButton, Qt::NoModifier, b->rect().center());
        QTRY_VERIFY2_WITH_TIMEOUT(visibleSetup() != nullptr, "keine Einrichtung", 3000);
        QVERIFY(!b->isChecked());
        QVERIFY(g_fluege.isEmpty());
    }

    void click_without_key_opens_the_setup()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QsoMapWindow* map = w.mapPanelForTest();
        QVERIFY2(map && map->isEmbedded(), "keine eingebettete Karte");
        map->imagery()->setNetworkEnabled(false);
        QPushButton* b = map->threeDButtonForTest();
        QVERIFY(b);
        QVERIFY2(b->isVisible(), "3D-Knopf nicht sichtbar");
        QVERIFY2(b->isEnabled(), qPrintable(b->toolTip()));

        // Wirklich mit der Maus, in die Mitte des Knopfes -- liegt dort
        // etwas anderes darueber, faengt das der Klick.
        QWidget* hit = map->window()->childAt(b->mapTo(map->window(), b->rect().center()));
        QVERIFY2(hit == b, qPrintable(QStringLiteral("ueber dem Knopf liegt %1")
                                          .arg(hit ? QString::fromLatin1(hit->metaObject()->className())
                                                   : QStringLiteral("nichts"))));
        QTest::mouseClick(b, Qt::LeftButton, Qt::NoModifier, b->rect().center());

        QTRY_VERIFY2_WITH_TIMEOUT(visibleSetup() != nullptr, "kein Einrichtungsdialog", 3000);
        QVERIFY(!b->isChecked());   // ohne Schluessel rastet nichts ein

        // Schluessel eintragen und speichern: der Knopf der Spalte rastet ein.
        Google3dSetupDialog* d = visibleSetup();
        // Vorgabe Cesium ion: ein (erfundener) ion-Zugang reicht.
        QCOMPARE(d->shownSource(), Map3dAccess::Source::CesiumIon);
        d->keyEditForTest()->setText(QStringLiteral(
            "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJqdGkiOiJwcnVlZnN0YW5kIn0.UHJ1ZWZzdGFuZE5pY2h0RWNodA"));
        d->saveButtonForTest()->click();
        QVERIFY(b->isChecked());
    }

    // Die Bedienhilfe (AXPress, VoiceOver, Automatisierung) drueckt einen
    // ANKREUZBAREN Knopf ueber toggle() -- das sendet toggled, aber kein
    // clicked. Jede angebotene Handlung muss den Dialog oeffnen.
    void accessibility_press_opens_the_setup()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QPushButton* b = w.mapPanelForTest()->threeDButtonForTest();
        QAccessibleInterface* iface = QAccessible::queryAccessibleInterface(b);
        QVERIFY(iface && iface->actionInterface());
        const QStringList actions = iface->actionInterface()->actionNames();
        QVERIFY2(!actions.isEmpty(), "keine Handlung angeboten");
        for (const QString& act : actions) {
            if (act == QAccessibleActionInterface::setFocusAction()) { continue; }
            iface->actionInterface()->doAction(act);
            const QByteArray why = (act + QStringLiteral(" oeffnet nichts")).toUtf8();
            QTRY_VERIFY2_WITH_TIMEOUT(visibleSetup() != nullptr, why.constData(), 3000);
            QVERIFY2(!b->isChecked(), qPrintable(act));
            visibleSetup()->close();
            QTRY_VERIFY(visibleSetup() == nullptr);
        }
    }

    // Schmale Spalte: die Leiste bricht um. Der Knopf muss dann noch
    // oben liegen und nicht unter der Kartenflaeche.
    void button_is_reachable_at_every_width_data()
    {
        QTest::addColumn<int>("width");
        for (int w : {640, 800, 960, 1120, 1400, 1800}) {
            QTest::newRow(qPrintable(QString::number(w))) << w;
        }
    }
    void button_is_reachable_at_every_width()
    {
        QFETCH(int, width);
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(width, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QCoreApplication::processEvents();
        QPushButton* b = w.mapPanelForTest()->threeDButtonForTest();
        if (!b->isVisible()) { QSKIP("Kartenspalte bei dieser Breite zu"); }
        QWidget* hit = w.childAt(b->mapTo(&w, b->rect().center()));
        QVERIFY2(hit == b, qPrintable(QStringLiteral("ueber dem Knopf liegt %1 (Knopf %2)")
                                          .arg(hit ? QString::fromLatin1(hit->metaObject()->className())
                                                   : QStringLiteral("nichts"))
                                          .arg(QStringLiteral("%1,%2 %3x%4").arg(b->x()).arg(b->y())
                                               .arg(b->width()).arg(b->height()))));
    }

    // Der Weg wie im Programm: Hauptfenster, Tools -> Logbook (das
    // Logbuch gehoert dem Rotor/Log-Panel), dort die Kartenspalte.
    void through_the_main_window_like_live()
    {
        MainWindow mw;
        mw.resize(1280, 800);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));

        QAction* logAct = nullptr;
        for (QAction* a : mw.findChildren<QAction*>()) {
            if (a->objectName() == QLatin1String("actLogbook")) { logAct = a; break; }
        }
        QVERIFY(logAct);
        logAct->trigger();
        LogbookWindow* lw = nullptr;
        QTRY_VERIFY((lw = mw.findChild<LogbookWindow*>()) != nullptr && lw->isVisible());
        QVERIFY(QTest::qWaitForWindowExposed(lw, kFensterFrist));

        QsoMapWindow* map = lw->mapPanelForTest();
        QVERIFY2(map && map->isVisible(), "keine sichtbare Kartenspalte im Logbuch");
        map->imagery()->setNetworkEnabled(false);
        QPushButton* b = map->threeDButtonForTest();
        QVERIFY2(b && b->isVisible(), "3D-Knopf nicht sichtbar");
        QVERIFY2(b->isEnabled(), qPrintable(b->toolTip()));
        QWidget* hit = b->window()->childAt(b->mapTo(b->window(), b->rect().center()));
        QVERIFY2(hit == b, qPrintable(QStringLiteral("ueber dem Knopf liegt %1")
                                          .arg(hit ? QString::fromLatin1(hit->metaObject()->className())
                                                   : QStringLiteral("nichts"))));
        QTest::mouseClick(b, Qt::LeftButton, Qt::NoModifier, b->rect().center());
        QTRY_VERIFY2_WITH_TIMEOUT(visibleSetup() != nullptr, "kein Einrichtungsdialog", 3000);
        visibleSetup()->close();
    }
};

QTEST_MAIN(TstLogbuch3dKnopf)
#include "tst_logbuch_3d_knopf.moc"
