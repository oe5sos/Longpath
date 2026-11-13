// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_rotorlog_ordnung.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Das Rotor/Log-Feld nach Entwurf R (2026-11-12), mit grossem Foto
// (Betreiber: „Ja, mit großem Foto"). Weltbild, Rotor-Einrichtung,
// Rose/Kugel, eigener Locator und „Logbook…" hinter dem Zahnrad; Kurz-
// und Langweg, Entfernung und Locator als Ablesegitter; Rotorband; die
// Drehknoepfe sagen, wohin -- und „blocked", wenn der Weg hinter dem
// Berg liegt.
//
// Mit LONGPATH_GRAB_DIR legt der Pruefstand Bilder des echten Felds ab.

#include <QtTest>

#include "core/AppSettings.h"
#include "core/RotorBereich.h"
#include "gui/widgets/RotorLogbookPanel.h"
#include "FensterFrist.h"

#include <QAction>
#include <QDir>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

using namespace Longpath;

namespace {

QLineEdit* feld(QWidget& w, const QString& platzhalter)
{
    for (QLineEdit* e : w.findChildren<QLineEdit*>()) {
        if (e->placeholderText() == platzhalter) { return e; }
    }
    return nullptr;
}

QPushButton* knopf(QWidget& w, const QString& anfang)
{
    for (QPushButton* b : w.findChildren<QPushButton*>()) {
        if (b->text().startsWith(anfang)) { return b; }
    }
    return nullptr;
}

bool zeigt(QWidget& w, const QString& text)
{
    for (QLabel* l : w.findChildren<QLabel*>()) {
        if (l->isVisible() && l->text() == text) { return true; }
    }
    return false;
}

void bild(QWidget& w, const QString& name)
{
    const QString dir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
    if (dir.isEmpty()) { return; }
    QDir().mkpath(dir);
    QTest::qWait(300);
    w.grab().save(dir + QLatin1Char('/') + name);
}

} // namespace

class TstRotorlogOrdnung : public QObject { Q_OBJECT
private slots:
    void init()
    {
        RotorBereich frei;
        frei.save();
        // Fuer die Bilder: das echte Weltbild (ohne zeichnet Longpath die
        // schematische Notkugel) und die Kugel statt der Rose.
        const QString welt = qEnvironmentVariable("LONGPATH_GRAB_WELT");
        if (!welt.isEmpty()) {
            AppSettings::instance().setValue(QStringLiteral("GlobeWorldImagePath"), welt);
            AppSettings::instance().setValue(QStringLiteral("GlobeWorldImageStyle"), QStringLiteral("Crisp"));
        }
    }
    void kugelZeigen(RotorLogbookPanel& p)
    {
        if (qEnvironmentVariable("LONGPATH_GRAB_WELT").isEmpty()) { return; }
        if (auto* kugel = p.findChild<QAction*>(QStringLiteral("rotorLogGlobeAction"))) {
            kugel->setChecked(true);
        }
    }

    // Was eine Einstellung ist, steht nicht mehr als Knopf im Feld.
    void einstellungen_hinter_dem_zahnrad()
    {
        RotorLogbookPanel p(nullptr, nullptr, nullptr);
        p.resize(317, 733);
        p.show();
        QVERIFY(QTest::qWaitForWindowExposed(&p, kFensterFrist));
        auto* zahnrad = p.findChild<QPushButton*>(QStringLiteral("rotorLogOptions"));
        QVERIFY(zahnrad && zahnrad->isVisible());
        for (const QString& alt : {QStringLiteral("World image…"), QStringLiteral("Rotor…"),
                                   QStringLiteral("Globe"), QStringLiteral("Logbook…")}) {
            QPushButton* b = knopf(p, alt);
            QVERIFY2(!b || !b->isVisible(), qPrintable(alt + QStringLiteral(" steht noch im Feld")));
        }
        QLineEdit* mein = feld(p, QStringLiteral("your locator"));
        QVERIFY2(mein && !mein->isVisible(), "der eigene Locator steht noch im Feld");
        // Der schnelle LOG-Knopf bleibt (Betreiber 2026-09-02: Tab -> LOG).
        QPushButton* log = nullptr;
        for (QPushButton* b : p.findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("LOG")) { log = b; }
        }
        QVERIFY(log && log->isVisible());
    }

    // Weg frei: Ablesegitter und Drehknoepfe mit Grad.
    void weg_frei()
    {
        RotorLogbookPanel p(nullptr, nullptr, nullptr);
        p.resize(317, 733);
        p.show();
        QVERIFY(QTest::qWaitForWindowExposed(&p, kFensterFrist));
        feld(p, QStringLiteral("your locator"))->setText(QStringLiteral("JN67VV"));
        feld(p, QStringLiteral("callsign"))->setText(QStringLiteral("I0QMN"));
        feld(p, QStringLiteral("locator"))->setText(QStringLiteral("JN62KS"));
        QTRY_VERIFY(zeigt(p, QStringLiteral("187°")));
        QVERIFY(zeigt(p, QStringLiteral("7°")));
        QVERIFY(zeigt(p, QStringLiteral("574 km")));
        QPushButton* drehen = knopf(p, QStringLiteral("Rotate 187°"));
        QVERIFY2(drehen && drehen->isEnabled(), "kein „Rotate 187°\"");
        QPushButton* lp = p.findChild<QPushButton*>(QStringLiteral("rotorLogLongPath"));
        QVERIFY(lp && lp->text() == QStringLiteral("LP 7°") && lp->isEnabled());
        kugelZeigen(p);
        bild(p, QStringLiteral("R1-umgesetzt.png"));
    }

    // Kurzer Weg hinter dem Berg (erlaubt 180 -> 360): der Knopf sagt es.
    void kurzer_weg_hinter_dem_berg()
    {
        RotorBereich berg;
        berg.an = true;
        berg.vonGrad = 180.0;
        berg.bisGrad = 360.0;
        berg.save();
        RotorLogbookPanel p(nullptr, nullptr, nullptr);
        p.resize(317, 733);
        p.show();
        QVERIFY(QTest::qWaitForWindowExposed(&p, kFensterFrist));
        feld(p, QStringLiteral("your locator"))->setText(QStringLiteral("JN67VV"));
        feld(p, QStringLiteral("callsign"))->setText(QStringLiteral("OG3B"));
        feld(p, QStringLiteral("locator"))->setText(QStringLiteral("KP10AQ"));
        QTRY_VERIFY(zeigt(p, QStringLiteral("17°")));
        QPushButton* gesperrt = knopf(p, QStringLiteral("17° — blocked"));
        QVERIFY2(gesperrt && !gesperrt->isEnabled(), "der gesperrte Weg sieht aus wie ein freier");
        QPushButton* lp = p.findChild<QPushButton*>(QStringLiteral("rotorLogLongPath"));
        QVERIFY(lp && lp->text() == QStringLiteral("LP 197°") && lp->isEnabled());
        kugelZeigen(p);
        bild(p, QStringLiteral("R2-umgesetzt.png"));
        RotorBereich frei;
        frei.save();
    }
};

QTEST_MAIN(TstRotorlogOrdnung)
#include "tst_rotorlog_ordnung.moc"
