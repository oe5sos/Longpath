// SPDX-License-Identifier: GPL-3.0-or-later
//
// „theme wechsel funktioniert nicht." (Betreiber, 2026-10-15)
//
// Er hatte in zwei Punkten recht, und beide sind hier festgehalten.
//
// ── Punkt 1: im Menue gab es keine Palette ───────────────────────────
//
// Im Menue stand bis zum 2026-10-14 „Dark Theme" — ein Eintrag, der
// nur `qDebug("Dark Theme toggle NYI")` schrieb. Wer ihn benutzte, hat
// NIE etwas umgestellt. Die einzige echte Auswahl lag in
// Setup > Appearance > Palette, acht Klicks tief; in seinen
// Einstellungen stand nach Wochen kein einziges ActiveTheme.
//
// Das ist wieder die Fehlerklasse dieser Woche: vorhanden, verdrahtet,
// richtig — und unerreichbar.
//
// ── Punkt 2: der Klick muss WIRKEN, sofort ───────────────────────────
//
// „das theme sollte man danach auch per knopfdruck ändern können."
//
// themed() ersetzt in EINE Richtung: hat eine Palette #00b4d8 durch
// ihren Ton ersetzt, findet die naechste das #00b4d8 nicht mehr. Darum
// merkt ThemeQss zu jedem Ergebnis die Vorlage, und
// ThemeFilter::reapplyAll() rechnet von DER aus. Dieser Lauf prueft
// genau das an einem Widget, das schon steht: Palette A, Palette B,
// zurueck auf eingebaut — und jedes Mal die richtige Farbe.
//
// Gegenprobe (2026-10-15): ohne das Untermenue scheitert
// dasMenueFuehrtDiePaletten(); ohne das Vorbild-Register bleibt das
// Widget in dasUmschaltenWirktSofort() nach dem zweiten Wechsel auf
// der Farbe der ERSTEN Palette stehen.

#include <QtTest>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/styles/Theme.h"
#include "gui/styles/ThemeQss.h"

using namespace Longpath;

class TstPaletteImMenue : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_themeDir;

    /// Zwei Paletten hinlegen, die es wirklich gibt, und sie dem
    /// Programm unterschieben. LONGPATH_THEME_DIR schlaegt in
    /// Theme::searchPaths() alles andere — so haengt der Lauf nicht
    /// daran, was gerade im Programmpaket liegt.
    void legeZweiPalettenHin()
    {
        QVERIFY(m_themeDir.isValid());
        const struct { const char* datei; const char* name; const char* text; } p[] = {
            {"eins.json", "Pruefpalette Eins", "#112233"},
            {"zwei.json", "Pruefpalette Zwei", "#445566"},
        };
        for (const auto& e : p) {
            QFile f(m_themeDir.filePath(QString::fromLatin1(e.datei)));
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            // „text" ist die Rolle der Quelltextfarbe #c8d8e8 — die
            // steht in 47 Dateien und ist damit der sicherste Griff,
            // um einen Wechsel nachzuweisen.
            f.write(QStringLiteral(
                        "{ \"name\": \"%1\", \"colors\": "
                        "{ \"text\": \"%2\" } }")
                        .arg(QString::fromLatin1(e.name),
                             QString::fromLatin1(e.text))
                        .toUtf8());
            f.close();
        }
        qputenv("LONGPATH_THEME_DIR", m_themeDir.path().toLocal8Bit());
    }

    /// NICHT mw->menuBar() nehmen.
    ///
    /// MainWindow haengt seine Menueleiste per setMenuWidget() in eine
    /// eigene TitleBar um (Phase 3O, 2026-09-02 „Kopfleiste ist
    /// verschwunden"). Danach legt QMainWindow::menuBar() eine NEUE,
    /// LEERE Leiste an — der erste Entwurf dieses Laufs suchte genau
    /// darin und fand folgerichtig nichts, auch als das Untermenue
    /// schon da war.
    static QMenu* viewMenue(MainWindow* mw)
    {
        for (QMenuBar* bar : mw->findChildren<QMenuBar*>()) {
            for (QAction* a : bar->actions()) {
                if (!a->menu()) { continue; }
                if (a->text().contains(QLatin1String("View"))) {
                    return a->menu();
                }
            }
        }
        return nullptr;
    }

    static QMenu* palettenMenue(QMenu* view)
    {
        if (!view) { return nullptr; }
        for (QAction* a : view->actions()) {
            if (!a->menu()) { continue; }
            if (a->text().contains(QLatin1String("Palette"))) { return a->menu(); }
        }
        return nullptr;
    }

private slots:
    void initTestCase() { legeZweiPalettenHin(); }

    void cleanupTestCase() { qunsetenv("LONGPATH_THEME_DIR"); }

    /// Das Menue fuehrt die eingebaute Palette UND jede gefundene.
    void dasMenueFuehrtDiePaletten()
    {
        auto* mw = new MainWindow();
        QMenu* pal = palettenMenue(viewMenue(mw));
        QVERIFY2(pal, "kein Palette-Untermenue im View-Menue");

        QStringList gefuehrt;
        for (QAction* a : pal->actions()) {
            if (a->isSeparator()) { continue; }
            gefuehrt << a->text();
            QVERIFY2(a->isEnabled(),
                     qPrintable(QStringLiteral("ausgegraut: %1").arg(a->text())));
            QVERIFY2(a->isCheckable(),
                     qPrintable(QStringLiteral("nicht ankreuzbar: %1")
                                    .arg(a->text())));
        }
        qInfo() << "Palette-Untermenue:" << gefuehrt;

        QVERIFY2(gefuehrt.contains(Style::Theme::builtInName()),
                 "die eingebaute Palette fehlt — dann kommt niemand zurueck");
        QVERIFY2(gefuehrt.contains(QStringLiteral("Pruefpalette Eins")),
                 qPrintable(QStringLiteral("Palette fehlt — gefuehrt: %1")
                                .arg(gefuehrt.join(QLatin1String(", ")))));
        QVERIFY2(gefuehrt.contains(QStringLiteral("Pruefpalette Zwei")),
                 "die zweite gefundene Palette fehlt");

        delete mw;
    }

    /// Und ein Klick MERKT sie auch — sonst ist sie nach dem Neustart
    /// wieder weg.
    void einKlickSchreibtDenMerker()
    {
        auto& s = AppSettings::instance();
        s.setValue("ActiveTheme", QString());

        auto* mw = new MainWindow();
        QMenu* pal = palettenMenue(viewMenue(mw));
        QVERIFY(pal);

        QAction* ziel = nullptr;
        for (QAction* a : pal->actions()) {
            if (a->text() == QStringLiteral("Pruefpalette Zwei")) { ziel = a; }
        }
        QVERIFY2(ziel, "Pruefpalette Zwei nicht im Menue");

        ziel->trigger();

        QCOMPARE(s.value("ActiveTheme", QString()).toString(),
                 QStringLiteral("Pruefpalette Zwei"));

        // Und zurueck auf die eingebaute: ein LEERER Merker, nicht der
        // Anzeigename. Andersherum suchte applyStoredChoice() beim
        // Start eine Datei namens „Night (built-in)".
        QAction* eingebaut = nullptr;
        for (QAction* a : pal->actions()) {
            if (a->text() == Style::Theme::builtInName()) { eingebaut = a; }
        }
        QVERIFY(eingebaut);
        eingebaut->trigger();
        QVERIFY2(s.value("ActiveTheme", QString()).toString().isEmpty(),
                 "der Weg zurueck auf die eingebaute Palette fehlt");

        delete mw;
    }

    /// Der Knopfdruck faerbt ein Widget um, das SCHON STEHT — und
    /// zwar in jede Richtung.
    void dasUmschaltenWirktSofort()
    {
        Style::Theme::instance().clear();
        Style::forgetSources();

        auto* filter = new Style::ThemeFilter(qApp);
        qApp->installEventFilter(filter);

        // Ein Widget wie jedes andere im Programm: Quelltextfarbe im
        // Stylesheet, und es ist schon gezeichnet, als die Palette
        // wechselt.
        auto* w = new QWidget;
        w->setStyleSheet(QStringLiteral("QWidget { color: #c8d8e8; }"));
        w->show();
        QVERIFY(QTest::qWaitForWindowExposed(w));

        QString err;
        QVERIFY2(Style::Theme::instance().activate(
                     QStringLiteral("Pruefpalette Eins"), &err),
                 qPrintable(err));
        Style::ThemeFilter::reapplyAll();
        QVERIFY2(w->styleSheet().contains(QLatin1String("#112233")),
                 qPrintable(QStringLiteral("nach Palette Eins: %1")
                                .arg(w->styleSheet())));

        // Und jetzt der Punkt, an dem es bis zum 2026-10-15 scheiterte:
        // im Stylesheet steht kein #c8d8e8 mehr, das die zweite Palette
        // finden koennte.
        QVERIFY2(Style::Theme::instance().activate(
                     QStringLiteral("Pruefpalette Zwei"), &err),
                 qPrintable(err));
        Style::ThemeFilter::reapplyAll();
        QVERIFY2(w->styleSheet().contains(QLatin1String("#445566")),
                 qPrintable(QStringLiteral("nach Palette Zwei: %1")
                                .arg(w->styleSheet())));

        // Zurueck auf eingebaut. Erwartet wird NICHT #c8d8e8, sondern
        // der heutige Wert der Rolle „text" aus StyleConstants.h.
        //
        // Das ist kein Mangel des Umschaltens, sondern der Befund vom
        // 2026-10-14: #c8d8e8 ist die ALTE Textfarbe und steht noch in
        // 47 Dateien; die Tabelle in ThemeQss bildet sie auch ohne
        // jede Palette auf kTextPrimary ab. „Eingebaut" heisst also
        // „die Tabelle ohne Datei", nicht „der Buchstabe im Quelltext".
        Style::Theme::instance().clear();
        Style::ThemeFilter::reapplyAll();
        const QString eingebauterText = Style::hexRole("#c8d8e8");
        QVERIFY2(w->styleSheet().contains(eingebauterText),
                 qPrintable(QStringLiteral("zurueck auf eingebaut: %1 "
                                           "(erwartet %2)")
                                .arg(w->styleSheet(), eingebauterText)));
        QVERIFY2(!w->styleSheet().contains(QLatin1String("#445566")),
                 "die Farbe der zweiten Palette klebt noch");

        qApp->removeEventFilter(filter);
        delete filter;
        delete w;
        Style::Theme::instance().clear();
    }

    /// Ein Name, den es nicht gibt, aendert nichts.
    void einFalscherNameAendertNichts()
    {
        auto& s = AppSettings::instance();
        s.setValue("ActiveTheme", QStringLiteral("Pruefpalette Eins"));

        QString err;
        QVERIFY(!Style::Theme::instance().activate(
            QStringLiteral("Gibt Es Nicht"), &err));
        QVERIFY(!err.isEmpty());
        QCOMPARE(s.value("ActiveTheme", QString()).toString(),
                 QStringLiteral("Pruefpalette Eins"));
    }
};

QTEST_MAIN(TstPaletteImMenue)
#include "tst_palette_im_menue.moc"
