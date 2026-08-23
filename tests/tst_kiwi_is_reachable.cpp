// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der KiwiSDR ist ueber die Oberflaeche ERREICHBAR.
//
// KiwiSDR Stufe 6b, 2026-08-23. Bis dahin war der KiwiSDR vollstaendig
// gebaut und vollstaendig unerreichbar: Protokoll, Verzeichnis,
// Verwaltung, Anzeige, Ton und Wasserfall standen, aber es gab in der
// Oberflaeche keinen Weg, einen Empfaenger auszuwaehlen.
//
// Das ist keine Kleinigkeit, sondern der teuerste Fehler dieser Art:
// gebaut und nicht erreichbar ist so gut wie nicht gebaut, nur dass
// niemand es merkt, weil alle Bausteine ihre eigenen Pruefungen
// bestehen. Genau dieses Muster hat in dieser Sitzung schon zweimal
// zugeschlagen — die Zoomknoepfe waren Signale ohne Empfaenger, und
// die Frequenzaenderung erreichte den Panadapter nicht, obwohl die
// Kommentarzeile darueber stand.
//
// Diese Pruefung geht darum durch das MENUE, nicht durch die API.

#include <QtTest>
#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include "gui/MainWindow.h"
#include "core/KiwiSdrManager.h"
#include "models/RadioModel.h"

using namespace Longpath;

namespace {

// Alle Eintraege eines Menuebaums, flach.
//
// Gesucht wird am FENSTER, nicht an der Menueleiste: auf macOS ist die
// Leiste nativ, und die QMenu-Objekte haengen als Kinder am Fenster,
// nicht an ihr. Ein findChildren auf menuBar() liefert dort eine leere
// Liste — und eine leere Liste bestuende jede Pruefung, die auf
// "enthaelt nicht" baut. Genau diesen Fehler hat der erste Lauf
// gezeigt.
QStringList allEntries(QWidget* root)
{
    QStringList out;
    for (QMenu* m : root->findChildren<QMenu*>()) {
        for (QAction* a : m->actions()) {
            if (!a->text().isEmpty()) { out << a->text(); }
        }
    }
    return out;
}

} // namespace

class TstKiwiIsReachable : public QObject
{
    Q_OBJECT

private slots:
    void dasMenueFuehrtZumKiwi()
    {
        // Auf dem Halden angelegt und ueber close() abgebaut: ein
        // MainWindow auf dem Stapel wird beim Verlassen des Rahmens
        // zerstoert, OHNE dass closeEvent lief — und dann bricht der
        // Lauf mit "QThread: Destroyed while thread 'SpectrumThread'
        // is still running" ab. Der erste Lauf dieser Pruefung hat
        // genau das gezeigt.
        auto* mw = new MainWindow();
        MainWindow& w = *mw;
        w.resize(1200, 800);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, 15000));

        const QStringList entries = allEntries(&w);
        QVERIFY2(!entries.isEmpty(),
                 "Es wurde ueberhaupt kein Menueeintrag gefunden — "
                 "dann prueft der Rest dieser Pruefung nichts");
        const QString all = entries.join(QStringLiteral(" | "));

        // Der Sammelpunkt.
        QVERIFY2(all.contains(QStringLiteral("KiwiSDR")), qPrintable(all));

        // Und die beiden Wege hinein. Ein Menue, das nur den Namen
        // traegt, ist dieselbe Sackgasse wie gar keines.
        QVERIFY2(all.contains(QStringLiteral("Öffentliche Empfänger")),
                 qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("von Hand hinzufügen")),
                 qPrintable(all));
        qInfo().noquote() << "gefunden:"
                          << entries.filter(QStringLiteral("Kiwi")).join(", ")
                          << "/"
                          << entries.filter(QStringLiteral("Empfänger"))
                                 .join(", ");
        mw->close();
    }

    void einEmpfaengerLandetInDerListe()
    {
        // Der ganze Weg am fernen Ende gemessen: nach dem Aufnehmen muss
        // der Empfaenger in der Verwaltung stehen. Ein Menuepunkt, der
        // nichts anlegt, waere derselbe Fehler eine Ebene tiefer.
        // Auf dem Halden angelegt und ueber close() abgebaut: ein
        // MainWindow auf dem Stapel wird beim Verlassen des Rahmens
        // zerstoert, OHNE dass closeEvent lief — und dann bricht der
        // Lauf mit "QThread: Destroyed while thread 'SpectrumThread'
        // is still running" ab. Der erste Lauf dieser Pruefung hat
        // genau das gezeigt.
        auto* mw = new MainWindow();
        MainWindow& w = *mw;
        w.resize(1200, 800);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, 15000));

        w.addKiwiSdrReceiverForTest(QStringLiteral("Gmunden"),
                             QStringLiteral("kiwi.example.at:8073"));

        const QVector<KiwiSdrAntennaProfile> profiles =
            w.kiwiSdrManagerForTest()->profiles();
        qInfo() << "Profile nach dem Aufnehmen:" << profiles.size();
        QVERIFY2(!profiles.isEmpty(),
                 "addKiwiSdrReceiver hat nichts angelegt");
        bool found = false;
        for (const auto& p : profiles) {
            if (p.endpoint.contains(QStringLiteral("kiwi.example.at"))) {
                found = true;
            }
        }
        QVERIFY2(found, "Der Empfaenger steht nicht unter seiner Adresse");
        mw->close();
    }
};

QTEST_MAIN(TstKiwiIsReachable)
#include "tst_kiwi_is_reachable.moc"
