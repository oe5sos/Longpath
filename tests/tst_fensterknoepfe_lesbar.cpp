// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/tst_fensterknoepfe_lesbar.cpp  (Longpath)
//
// Die drei Knoepfe in der Titelzeile eines schwebenden Fensters --
// Vollbild, Andocken, Schliessen -- sind 16x16 Pixel gross und tragen
// je ein Zeichen.
//
// Style::kButtonStyle gilt seit dem 2026-09-18 app-weit und bringt
// `padding: 4px 12px` mit. Das trifft JEDEN Knopf, dessen eigenes
// Stilblatt zum Innenabstand schweigt -- und diese drei schweigen.
// 16 Pixel minus 2x12 ist negativ: fuer das Zeichen bleibt nichts.
//
// Gefunden am 2026-10-07 bei der Arbeit an der Profilschiene, wo
// dieselbe Rechnung die Buchstaben der Abzeichen gekostet hat (4 Pixel
// Platz fuer ein „N"). Dieselbe Falle hat am 2026-09-25 die Zoomknoepfe
// erwischt (tst_zoom_buttons_legible). Das hier ist der dritte Fall --
// darum eine eigene Pruefung statt nur einer Behebung.
//
// Gemessen wird mit demselben Mass wie dort: die Textflaeche, die der
// Stil dem Knopf laesst, gegen das, was das Zeichen wirklich braucht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-07 — Original fuer Longpath, KI-gestuetzt (Anthropic
//                 Claude), Betreiber Martin Fischer.
// =================================================================

#include <QtTest>

#include "gui/WindowChrome.h"
#include "gui/styles/AppTheme.h"

#include <QApplication>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionButton>

using namespace Longpath;

class TestFensterknoepfeLesbar : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Ohne das app-weite Blatt prueft die Datei eine Titelzeile, die
        // es so nicht gibt -- genau daran ist der Fehler drei Wochen
        // lang vorbeigelaufen.
        applyAppBaselineQss(*qApp);
    }

    void jedesZeichenHatPlatz()
    {
        WindowTitleBar leiste(QStringLiteral("Prueffenster"));
        leiste.show();
        QVERIFY(QTest::qWaitForWindowExposed(&leiste));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        int geprueft = 0;
        for (QPushButton* b : leiste.findChildren<QPushButton*>()) {
            if (b->text().isEmpty()) { continue; }
            b->ensurePolished();

            QStyleOptionButton opt;
            opt.initFrom(b);
            opt.text = b->text();
            const QRect flaeche =
                b->style()->subElementRect(QStyle::SE_PushButtonContents, &opt, b);
            const QRect zeichen = QFontMetrics(b->font()).tightBoundingRect(b->text());

            QVERIFY2(flaeche.width() >= zeichen.width()
                         && flaeche.height() >= zeichen.height(),
                     qPrintable(QStringLiteral(
                         "„%1“ hat %2x%3 Platz, braucht aber %4x%5 -- "
                         "es wird weggeschnitten")
                             .arg(b->text())
                             .arg(flaeche.width()).arg(flaeche.height())
                             .arg(zeichen.width()).arg(zeichen.height())));
            ++geprueft;
        }
        QVERIFY2(geprueft >= 3,
                 "weniger als drei beschriftete Knoepfe gefunden -- dann "
                 "prueft diese Datei nichts");
    }
};

QTEST_MAIN(TestFensterknoepfeLesbar)
#include "tst_fensterknoepfe_lesbar.moc"
