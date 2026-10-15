// SPDX-License-Identifier: GPL-3.0-or-later
//
// Faerbt ein Palettenwechsel das WIRKLICHE Fenster um?
//
// ── Warum dieser Lauf noetig ist ────────────────────────────────────
//
// Es gibt schon tst_palette_im_menue. Der prueft ein frisch gebautes
// EINZELWIDGET, dem der Lauf selbst ein Stylesheet mit genau der
// Pruefffarbe gegeben hat — eine Laborbedingung. Er war gruen, als ich
// dem Betreiber zum zweiten Mal meldete, das Umschalten gehe; am
// 2026-10-15 kam zurueck: „theme funktioniert auch noch nicht."
//
// Zwei Wege faerben in diesem Programm (siehe gui/styles/ThemeQss.cpp
// und Style::role()): die Stylesheet-Tabelle UND der Malcode, der
// seine Farben erst beim Zeichnen holt. Ein Lauf, der nur
// `styleSheet()` liest, sieht den zweiten Weg gar nicht — und ein
// Widget, das seine Farbe beim Malen holt, behaelt sein altes Bild,
// solange niemand `update()` ruft.
//
// Darum misst dieser Lauf das, was der Betreiber sieht: BILDPUNKTE
// des echten Hauptfensters, vorher und nachher.
//
// Das Grundrauschen wird mitgemessen (zwei Bilder OHNE Wechsel):
// Wasserfall und Uhr laufen weiter, ein paar Punkte aendern sich
// immer. Der Wechsel muss deutlich darueber liegen, sonst ist er
// keiner.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QFile>
#include <QImage>
#include <QMenu>
#include <QMenuBar>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/styles/Theme.h"
#include "gui/styles/ThemeQss.h"

using namespace Longpath;

namespace {

/// Anteil der Bildpunkte, die sich unterscheiden (0..1).
double anteilAnders(const QImage& a, const QImage& b)
{
    if (a.size() != b.size() || a.isNull()) { return -1.0; }
    const QImage x = a.convertToFormat(QImage::Format_ARGB32);
    const QImage y = b.convertToFormat(QImage::Format_ARGB32);
    qint64 anders = 0;
    const qint64 gesamt = qint64(x.width()) * x.height();
    for (int zeile = 0; zeile < x.height(); ++zeile) {
        const QRgb* px = reinterpret_cast<const QRgb*>(x.constScanLine(zeile));
        const QRgb* py = reinterpret_cast<const QRgb*>(y.constScanLine(zeile));
        for (int s = 0; s < x.width(); ++s) {
            if (px[s] != py[s]) { ++anders; }
        }
    }
    return gesamt > 0 ? double(anders) / double(gesamt) : -1.0;
}

} // namespace

class TstPaletteFaerbtDasFenster : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_themeDir;

    /// Eine Palette, die mehr als eine Rolle umwirft — sonst misst der
    /// Lauf nur, ob EINE Farbe im Fenster vorkommt.
    void legeEinePaletteHin()
    {
        QVERIFY(m_themeDir.isValid());
        QFile f(m_themeDir.filePath(QStringLiteral("schrei.json")));
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        // ── Die ECHTEN Rollennamen ──────────────────────────────
        //
        // Der erste Entwurf dieses Laufs schrieb „bg" und
        // „panel-bg" — beides gibt es nicht. Sie heissen `app-bg`
        // und `panel` (gemessen: `grep -rho 'role("[a-z0-9-]*"'`
        // ueber src/ und die Rollenliste in resources/themes/
        // moos.json). Mit erfundenen Namen aenderte sich 4 % der
        // Flaeche, und ich haette daraus beinahe einen Befund ueber
        // das PROGRAMM gemacht.
        //
        // Genommen sind die Rollen mit den meisten Lesern plus die
        // grossen Flaechen.
        f.write(QStringLiteral(
            "{ \"name\": \"Pruefpalette Schrei\", \"colors\": {"
            " \"app-bg\": \"#203040\","
            " \"panel\": \"#304050\","
            " \"inset\": \"#405060\","
            " \"inset-bg\": \"#405060\","
            " \"statusbar-bg\": \"#283848\","
            " \"titlebar-top\": \"#304050\","
            " \"titlebar-mid\": \"#2a3a4a\","
            " \"titlebar-bottom\": \"#243444\","
            " \"panadapter-bg\": \"#182838\","
            " \"text\": \"#f0e0d0\","
            " \"text-secondary\": \"#e0d0c0\","
            " \"text-scale\": \"#d8c8b8\","
            " \"measured\": \"#f0c080\","
            " \"border\": \"#8090a0\","
            " \"button\": \"#405060\","
            " \"grid\": \"#405868\""
            " } }").toUtf8());
        f.close();
        qputenv("LONGPATH_THEME_DIR", m_themeDir.path().toLocal8Bit());
    }

private slots:
    void initTestCase() { legeEinePaletteHin(); }
    void cleanupTestCase() { qunsetenv("LONGPATH_THEME_DIR"); }

    void einWechselAendertDasBild()
    {
        Style::Theme::instance().clear();
        Style::forgetSources();

        MainWindow mw;
        mw.resize(1470, 900);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw));
        QTest::qWait(600);           // Layout und erstes Zeichnen

        // ── Grundrauschen: zwei Bilder OHNE Wechsel ─────────────────
        const QImage ruhe1 = mw.grab().toImage();
        QTest::qWait(250);
        const QImage ruhe2 = mw.grab().toImage();
        const double rauschen = anteilAnders(ruhe1, ruhe2);
        QVERIFY2(rauschen >= 0.0, "Bilder nicht vergleichbar");
        qInfo() << "Grundrauschen:" << QString::number(rauschen * 100.0, 'f', 2) << "%";

        // ── Jetzt der Wechsel ───────────────────────────────────────
        QString fehler;
        QVERIFY2(Style::Theme::instance().activate(
                     QStringLiteral("Pruefpalette Schrei"), &fehler),
                 qPrintable(fehler));
        Style::ThemeFilter::reapplyAll();
        QTest::qWait(400);
        const QImage nachher = mw.grab().toImage();

        const double gewechselt = anteilAnders(ruhe2, nachher);
        qInfo() << "nach dem Wechsel anders:"
                << QString::number(gewechselt * 100.0, 'f', 2) << "%";

        // Die Bilder ZUERST ablegen, dann pruefen: ein Lauf, der an
        // seiner eigenen Behauptung abbricht, bevor er den Beleg
        // hinlegt, ist beim Suchen wertlos (2026-10-15 selbst
        // erlebt).
        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!dir.isEmpty()) {
            ruhe2.save(dir + QStringLiteral("/palette-vorher.png"));
            nachher.save(dir + QStringLiteral("/palette-nachher.png"));
        }

        // Die Decke ist bewusst niedrig: es geht nicht darum, WIE VIEL
        // sich faerbt, sondern dass ueberhaupt etwas geschieht und
        // zwar deutlich mehr als das Rauschen. Ein Fenster, in dem
        // nach einem Palettenwechsel weniger als ein Zwanzigstel der
        // Flaeche anders aussieht, hat fuer den Betreiber nicht
        // gewechselt.
        QVERIFY2(gewechselt > 0.05,
                 qPrintable(QStringLiteral(
                     "nur %1 %% der Bildpunkte haben sich geaendert "
                     "(Grundrauschen %2 %%) — fuer das Auge ist das "
                     "kein Palettenwechsel")
                         .arg(gewechselt * 100.0, 0, 'f', 2)
                         .arg(rauschen * 100.0, 0, 'f', 2)));
        QVERIFY2(gewechselt > rauschen * 3.0,
                 qPrintable(QStringLiteral("Wechsel %1 %% kaum ueber dem "
                                           "Rauschen %2 %%")
                                .arg(gewechselt * 100.0, 0, 'f', 2)
                                .arg(rauschen * 100.0, 0, 'f', 2)));

    }

    /// Und jetzt die Palette, die der Betreiber wirklich im Menue
    /// findet: resources/themes/moos.json, so wie sie ausgeliefert
    /// wird.
    ///
    /// Der Fall darueber beweist nur, dass der Weg traegt. Dieser
    /// beantwortet die Frage, die am 2026-10-15 gestellt wurde:
    /// „theme funktioniert auch noch nicht."
    void dieAusgelieferteMoosPaletteWirkt()
    {
        const QString quelle = QStringLiteral(LONGPATH_SRC_DIR)
                               + QStringLiteral("/resources/themes/moos.json");
        QVERIFY2(QFile::exists(quelle), qPrintable(quelle));
        QVERIFY(QFile::copy(quelle, m_themeDir.filePath(
                    QStringLiteral("moos.json")))
                || QFile::exists(m_themeDir.filePath(
                    QStringLiteral("moos.json"))));

        Style::Theme::instance().clear();
        Style::forgetSources();

        MainWindow mw;
        mw.resize(1470, 900);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw));
        QTest::qWait(600);

        const QImage vorher = mw.grab().toImage();

        QString fehler;
        QStringList namen;
        for (const auto& t : Style::Theme::available()) { namen << t.name; }
        qInfo().noquote() << "im Menue:" << namen.join(QLatin1String(", "));

        QVERIFY2(Style::Theme::instance().activate(
                     QStringLiteral("Moos"), &fehler),
                 qPrintable(QStringLiteral("Moos liess sich nicht "
                                           "aktivieren: %1 — verfuegbar: %2")
                                .arg(fehler, namen.join(QLatin1String(", ")))));
        Style::ThemeFilter::reapplyAll();
        QTest::qWait(400);
        const QImage nachher = mw.grab().toImage();

        const double anders = anteilAnders(vorher, nachher);
        qInfo() << "Moos aendert:"
                << QString::number(anders * 100.0, 'f', 2) << "%";

        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!dir.isEmpty()) {
            vorher.save(dir + QStringLiteral("/moos-vorher.png"));
            nachher.save(dir + QStringLiteral("/moos-nachher.png"));
        }

        QVERIFY2(anders > 0.05,
                 qPrintable(QStringLiteral(
                     "die ausgelieferte Palette „Moos\" aendert nur %1 %% "
                     "der Flaeche — fuer das Auge waechselt da nichts")
                         .arg(anders * 100.0, 0, 'f', 2)));
    }

    /// Der ganze Weg: KLICK im Menue -> Bild.
    ///
    /// Die beiden Faelle davor rufen `Theme::activate()` direkt. Was
    /// der Betreiber tut, ist ein Klick in View > Palette — und
    /// genau dort lag der Fehler vom 2026-10-14 („Dark Theme", ein
    /// Eintrag, der nur eine Zeile ins Protokoll schrieb). Ein Lauf,
    /// der die Bedienung ueberspringt, haette ihn nicht gefunden.
    void einKlickImMenueFaerbtDasFenster()
    {
        Style::Theme::instance().clear();
        Style::forgetSources();

        MainWindow mw;
        mw.resize(1470, 900);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw));
        QTest::qWait(600);

        // Das Menue so suchen wie tst_palette_im_menue: NICHT ueber
        // menuBar(), die ist nach setMenuWidget() leer.
        QMenu* palette = nullptr;
        for (QMenuBar* bar : mw.findChildren<QMenuBar*>()) {
            for (QAction* a : bar->actions()) {
                if (!a->menu() || !a->text().contains(QLatin1String("View"))) { continue; }
                for (QAction* b : a->menu()->actions()) {
                    if (b->menu() && b->text().contains(QLatin1String("Palette"))) {
                        palette = b->menu();
                    }
                }
            }
        }
        QVERIFY2(palette, "kein Palette-Untermenue im View-Menue");

        QStringList gefuehrt;
        QAction* moos = nullptr;
        for (QAction* a : palette->actions()) {
            if (a->isSeparator()) { continue; }
            gefuehrt << a->text();
            if (a->text().contains(QLatin1String("Moos"))) { moos = a; }
        }
        qInfo().noquote() << "Menue fuehrt:" << gefuehrt.join(QLatin1String(", "));
        QVERIFY2(moos, qPrintable(QStringLiteral(
            "„Moos\" steht nicht im Menue — gefuehrt: %1")
                .arg(gefuehrt.join(QLatin1String(", ")))));

        const QImage vorher = mw.grab().toImage();
        moos->trigger();
        QTest::qWait(400);
        const QImage nachher = mw.grab().toImage();

        const double anders = anteilAnders(vorher, nachher);
        qInfo() << "Klick aendert:"
                << QString::number(anders * 100.0, 'f', 2) << "%";

        const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!dir.isEmpty()) {
            nachher.save(dir + QStringLiteral("/klick-nachher.png"));
        }

        // Und der Merker, damit die Wahl den Neustart ueberlebt.
        QCOMPARE(AppSettings::instance().value("ActiveTheme", QString())
                     .toString(), QStringLiteral("Moos"));
        QVERIFY2(anders > 0.05,
                 qPrintable(QStringLiteral("ein Klick auf „Moos\" aendert "
                                           "nur %1 %% der Flaeche")
                                .arg(anders * 100.0, 0, 'f', 2)));
    }

    /// „Flach" und „Tief" tragen NULL Farben — und die Neustartfrage
    /// darf dann nicht behaupten, die Farben seien schon an.
    ///
    /// Gemessen am 2026-10-15 an den ausgelieferten Dateien:
    /// flach.json und tief.json haben den Schluessel `formen` und
    /// keinen einzigen `colors`-Eintrag. Wer eine davon waehlt, sieht
    /// im laufenden Programm nichts — las aber „The colours are on
    /// already". Das ist die wahrscheinlichste Quelle von „theme
    /// funktioniert auch noch nicht" (Betreiber, 2026-10-15).
    void formpalettenTragenKeineFarben()
    {
        const QString wurzel = QStringLiteral(LONGPATH_SRC_DIR)
                               + QStringLiteral("/resources/themes/");
        struct { const char* datei; bool farben; } f[] = {
            {"flach.json", false},
            {"tief.json",  false},
            {"moos.json",  true},
        };
        for (const auto& e : f) {
            const QString pfad = wurzel + QString::fromLatin1(e.datei);
            QVERIFY2(QFile::exists(pfad), qPrintable(pfad));
            Style::Theme::instance().clear();
            QString fehler;
            QVERIFY2(Style::Theme::instance().loadFile(pfad, &fehler),
                     qPrintable(fehler));
            qInfo().noquote()
                << QString::fromLatin1(e.datei) << "- Farben:"
                << (Style::Theme::instance().hasColours() ? "ja" : "nein")
                << " Form:"
                << (Style::Theme::instance().formOverrides().isEmpty()
                        ? "nein" : "ja");
            QCOMPARE(Style::Theme::instance().hasColours(), e.farben);
            // Jede der drei ist aktiv — auch die beiden ohne Farbe.
            QVERIFY(Style::Theme::instance().isActive());
        }
        Style::Theme::instance().clear();
    }
};

QTEST_MAIN(TstPaletteFaerbtDasFenster)
#include "tst_palette_faerbt_das_fenster.moc"
