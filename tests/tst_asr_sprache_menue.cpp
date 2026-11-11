// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-original (2026-11-11). Die Sprache der Mitschrift hinter dem
// Zahnrad im Panelkopf.
//
// Anlass: gemessen am selben Tag an Contest-Mitschnitten — mit der
// Vorgabe "de" hoert Whisper englischen Contestbetrieb als Deutsch und
// findet nur halb so viele Rufzeichen wie mit "en". Die Sprache stand
// bisher nur unter Setup > Transcription.
//
// Gemessen wird am fernen Ende: das Zahnrad wird ueberhaupt gezeichnet
// (hasExtendedSettings), die Wahl landet unter dem Schluessel, den
// MainWindow_Asr.cpp liest (AsrLanguage), und das Signal, an dem
// MainWindow die laufende Erkennung neu aufsetzt, kommt genau einmal.
//
// Modification history (Longpath):
//   2026-11-11 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude.

#include <QtTest>

#include <QAction>
#include <QMenu>
#include <QSignalSpy>
#include <QStandardPaths>

#include <memory>

#include "core/AppSettings.h"
#include "gui/applets/AsrApplet.h"

using namespace Longpath;

namespace {

QAction* aktionFuer(QMenu* menu, const QString& code)
{
    for (QAction* a : menu->actions()) {
        if (a->isCheckable() && a->data().toString() == code) { return a; }
    }
    return nullptr;
}

} // namespace

class TstAsrSpracheMenue : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Nie in die echten Einstellungen schreiben.
        QStandardPaths::setTestModeEnabled(true);
    }

    void zahnradIstDaUndBietetDreiSprachen()
    {
        AppSettings::instance().setValue(QStringLiteral("AsrLanguage"), QStringLiteral("de"));
        AsrApplet applet(nullptr);
        QVERIFY2(applet.hasExtendedSettings(),
                 "Ohne hasExtendedSettings() zeichnet der Panelkopf kein Zahnrad");

        std::unique_ptr<QMenu> menu(applet.baueOptionsMenue(nullptr));
        QVERIFY(aktionFuer(menu.get(), QStringLiteral("de")));
        QVERIFY(aktionFuer(menu.get(), QStringLiteral("en")));
        QVERIFY(aktionFuer(menu.get(), QStringLiteral("auto")));
        QVERIFY2(aktionFuer(menu.get(), QStringLiteral("de"))->isChecked(),
                 "Die gespeicherte Sprache muss angehakt sein");
        QVERIFY(!aktionFuer(menu.get(), QStringLiteral("en"))->isChecked());
    }

    void englischLandetUnterAsrLanguageUndMeldetSichEinmal()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("AsrLanguage"), QStringLiteral("de"));
        AsrApplet applet(nullptr);
        QSignalSpy gewechselt(&applet, &AsrApplet::languageChanged);

        std::unique_ptr<QMenu> menu(applet.baueOptionsMenue(nullptr));
        aktionFuer(menu.get(), QStringLiteral("en"))->trigger();
        QCOMPARE(s.value(QStringLiteral("AsrLanguage")).toString(), QStringLiteral("en"));
        QCOMPARE(gewechselt.count(), 1);
        QCOMPARE(gewechselt.at(0).at(0).toString(), QStringLiteral("en"));

        // Dieselbe Sprache noch einmal gewaehlt: kein Neuaufsetzen.
        std::unique_ptr<QMenu> nochmal(applet.baueOptionsMenue(nullptr));
        QVERIFY(aktionFuer(nochmal.get(), QStringLiteral("en"))->isChecked());
        aktionFuer(nochmal.get(), QStringLiteral("en"))->trigger();
        QCOMPARE(gewechselt.count(), 1);
    }
};

QTEST_MAIN(TstAsrSpracheMenue)
#include "tst_asr_sprache_menue.moc"
