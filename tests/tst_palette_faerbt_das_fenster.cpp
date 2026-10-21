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
#include <QColor>
#include <QImage>
#include <QMenu>
#include <QMenuBar>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/styles/Theme.h"
#include "gui/styles/ThemeQss.h"

// Ein MainWindow braucht hier allein rund 5 s, bis es auf dem Schirm
// steht — genau die Vorgabefrist von QTest::qWaitForWindowExposed.
// Im vollen Durchgang vom 2026-10-16 fielen 18 Laeufe darueber, dieser
// mit. PR #250 stellt den ganzen Pruefstand auf tests/FensterFrist.h
// um; bis der drin ist, steht die Zahl hier.
static constexpr int kFensterFrist = 20000;

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

/// Wie `anteilAnders`, aber NUR ueber die Bildpunkte, die in zwei
/// Ruhebildern gleich geblieben sind.
///
/// Warum das noetig wurde (2026-10-21): auf dem CI-Laeufer gibt es
/// keine GPU. Die Panadapter sind QRhiWidgets, Qt meldet dort
/// „QRhiWidget: No QRhi", und ihre Flaechen sehen bei jedem grab()
/// anders aus. Das Grundrauschen zwischen zwei RUHEBILDERN lag damit
/// bei 19,58 %, und die Schranke „Wechsel > Rauschen x 3" verlangte
/// ueber 58 % -- mehr, als ein Palettenwechsel ueberhaupt faerbt. Der
/// Lauf fiel also an seiner eigenen Voraussetzung um, nicht an der
/// Palette.
///
/// Statt den Lauf zu ueberspringen (ein uebersprungener Lauf sieht wie
/// ein Erfolg aus) messen wir jetzt dort, wo etwas zu messen IST: die
/// zappelnden Punkte werden ausgeschlossen, die ruhigen verglichen.
/// Ohne GPU ist das die Bedienflaeche, und genau um die geht es bei
/// einer Palette.
double anteilAndersOhneZappler(const QImage& ruhe1, const QImage& ruhe2,
                               const QImage& nachher, qint64* ruhigeOut)
{
    if (ruhe1.size() != ruhe2.size() || ruhe1.size() != nachher.size()
        || ruhe1.isNull()) {
        return -1.0;
    }
    const QImage r1 = ruhe1.convertToFormat(QImage::Format_ARGB32);
    const QImage r2 = ruhe2.convertToFormat(QImage::Format_ARGB32);
    const QImage nz = nachher.convertToFormat(QImage::Format_ARGB32);

    qint64 ruhige = 0;
    qint64 anders = 0;
    for (int zeile = 0; zeile < r1.height(); ++zeile) {
        const QRgb* a = reinterpret_cast<const QRgb*>(r1.constScanLine(zeile));
        const QRgb* b = reinterpret_cast<const QRgb*>(r2.constScanLine(zeile));
        const QRgb* c = reinterpret_cast<const QRgb*>(nz.constScanLine(zeile));
        for (int s = 0; s < r1.width(); ++s) {
            if (a[s] != b[s]) { continue; }   // zappelt — nichts zu messen
            ++ruhige;
            if (b[s] != c[s]) { ++anders; }
        }
    }
    if (ruhigeOut) { *ruhigeOut = ruhige; }
    return ruhige > 0 ? double(anders) / double(ruhige) : -1.0;
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

    /// Die Rechnung selbst, mit gebauten Bildern -- damit der Fall
    /// ohne GPU nicht erst auf dem Laeufer auffaellt.
    ///
    /// Aufbau: ein Fuenftel der Flaeche zappelt (wie die Panadapter
    /// ohne QRhi), vier Fuenftel sind ruhig. Von den RUHIGEN aendert
    /// der Wechsel genau ein Viertel.
    void dieZapplerWerdenAusgeschlossen()
    {
        const int breite = 400, hoehe = 100;   // 40 000 Punkte
        QImage r1(breite, hoehe, QImage::Format_ARGB32);
        r1.fill(QColor(20, 20, 24));
        QImage r2 = r1.copy();
        QImage nz = r1.copy();

        // Oberes Fuenftel: zappelt zwischen den beiden Ruhebildern.
        // Drei Farben, die sich paarweise ALLE unterscheiden -- sonst
        // treffen sich zwei Zappelwerte zufaellig und die Rechnung
        // unten geht nicht mehr auf (beim ersten Anlauf 0,397 statt
        // 0,40, weil zwei Formeln an manchen Punkten denselben Wert
        // ergaben).
        for (int y = 0; y < hoehe / 5; ++y) {
            for (int x = 0; x < breite; ++x) {
                r2.setPixelColor(x, y, QColor(255, 0, 0));
                nz.setPixelColor(x, y, QColor(0, 255, 0));
            }
        }
        // Von den ruhigen Zeilen aendert der Wechsel ein Viertel.
        const int ruhigeZeilen = hoehe - hoehe / 5;            // 80
        for (int y = hoehe / 5; y < hoehe / 5 + ruhigeZeilen / 4; ++y) {
            for (int x = 0; x < breite; ++x) {
                nz.setPixelColor(x, y, QColor(200, 180, 120));
            }
        }

        qint64 ruhige = 0;
        const double anteil = anteilAndersOhneZappler(r1, r2, nz, &ruhige);

        QCOMPARE(ruhige, qint64(breite) * ruhigeZeilen);
        QVERIFY2(qAbs(anteil - 0.25) < 0.001,
                 qPrintable(QStringLiteral("ueber den ruhigen Punkten "
                                           "%1 statt 0.25")
                                .arg(anteil, 0, 'f', 4)));

        // Und die Gegenprobe, exakt nachrechenbar: der alte Weg ueber
        // ALLE Punkte meldet ein Fuenftel Zappeln (0,20) plus ein
        // Viertel der vier Fuenftel ruhigen (0,20) = 0,40. Genau diese
        // Vermischung hat den Lauf auf dem Laeufer umgeworfen: dort
        // war das Zappeln so gross, dass die Schranke
        // „Wechsel > Rauschen x 3" unerreichbar wurde.
        const double alleZusammen = anteilAnders(r2, nz);
        QVERIFY2(qAbs(alleZusammen - 0.40) < 0.001,
                 qPrintable(QStringLiteral("die alte Messung sollte hier "
                                           "genau 0.40 ergeben, ist aber "
                                           "%1").arg(alleZusammen, 0, 'f', 4)));
        QVERIFY2(alleZusammen > anteil + 0.10,
                 "die beiden Messungen unterscheiden sich kaum — dann "
                 "belegt dieser Lauf nichts");
    }

    void einWechselAendertDasBild()
    {
        Style::Theme::instance().clear();
        Style::forgetSources();

        MainWindow mw;
        mw.resize(1470, 900);
        mw.show();
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));
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

        // Nur die ruhigen Bildpunkte zaehlen. Auf einer Maschine MIT
        // GPU sind das praktisch alle, das Ergebnis aendert sich dort
        // also nicht; ohne GPU bleibt die Bedienflaeche uebrig.
        qint64 ruhige = 0;
        const double gewechselt =
            anteilAndersOhneZappler(ruhe1, ruhe2, nachher, &ruhige);
        const qint64 gesamt = qint64(ruhe2.width()) * ruhe2.height();
        qInfo() << "ruhige Bildpunkte:" << ruhige << "von" << gesamt
                << QStringLiteral("(%1 %)")
                       .arg(gesamt > 0 ? 100.0 * double(ruhige)
                                             / double(gesamt)
                                       : 0.0, 0, 'f', 1);
        qInfo() << "davon nach dem Wechsel anders:"
                << QString::number(gewechselt * 100.0, 'f', 2) << "%";

        // Die Voraussetzung mitpruefen: bleibt fast nichts ruhig, ist
        // nichts zu messen, und ein Ergebnis waere erfunden.
        QVERIFY2(gesamt > 0 && ruhige * 10 >= gesamt,
                 qPrintable(QStringLiteral(
                     "nur %1 von %2 Bildpunkten sind ruhig — in diesem "
                     "Fenster ist kein Palettenwechsel messbar")
                         .arg(ruhige).arg(gesamt)));

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
        // Die alte Schranke „Wechsel > Rauschen x 3" ist weg: sie
        // verglich den Wechsel mit einem Rauschen, das jetzt gar nicht
        // mehr mitgemessen wird. Unter den RUHIGEN Punkten ist das
        // Rauschen per Bauart null — die Schwelle von einem
        // Zwanzigstel oben traegt den Nachweis allein.
        qInfo() << "(Grundrauschen im ganzen Bild war"
                << QString::number(rauschen * 100.0, 'f', 2)
                << "% — ausgeschlossen)";

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
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));
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
        QVERIFY(QTest::qWaitForWindowExposed(&mw, kFensterFrist));
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
