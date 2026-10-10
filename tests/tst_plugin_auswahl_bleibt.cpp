// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_plugin_auswahl_bleibt.cpp  (Longpath)
// =================================================================
// „kann keine auswählen."
//
// Die beiden Auswahllisten der Einschübe wurden nur IM Suchlauf
// gefüllt, und der läuft genau einmal. Bei jedem Profilwechsel baut
// das Fenster seine Reiter neu — die Einschubseite bekommt dann zwei
// frische, LEERE Listen, während der Kachelkatalog darunter aus dem
// Zwischenspeicher wieder voll war. Unten standen alle Plugins, oben
// war nichts auszuwählen.
//
// Geprüft wird deshalb genau diese Reihenfolge: suchen, Reiter
// umbauen, und danach muss in den Listen noch etwas stehen.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QComboBox>

#include "core/strip/StripChain.h"
#include "gui/applets/StripWindow.h"

using namespace Longpath;

class TstPluginAuswahlBleibt : public QObject {
    Q_OBJECT

private:
    // Die Auswahlfelder der Einschübe erkennt man daran, dass ihr
    // erster Eintrag der leere Platz ist.
    static QList<QComboBox*> einschubFelder(QWidget* w)
    {
        QList<QComboBox*> l;
        for (QComboBox* c : w->findChildren<QComboBox*>()) {
            if (c->count() > 0 && c->itemData(0).toString().isEmpty()
                && c->itemText(0).contains(QStringLiteral("leer"))) {
                l << c;
            }
        }
        return l;
    }

private slots:
    void theListsSurviveARebuild()
    {
        StripWindow w(nullptr, nullptr);
        w.resize(1200, 800);

        // Suchen wie beim Aufschlagen des Reiters.
        w.scanPluginsForTest();
        const auto vorher = einschubFelder(&w);
        if (vorher.isEmpty() || vorher.first()->count() <= 1) {
            QSKIP("auf diesem Rechner wurde kein Plugin gefunden");
        }
        const int hatte = vorher.first()->count();

        // Ein Profilwechsel baut die Reiter neu.
        w.rebuildTabsForTest();

        const auto nachher = einschubFelder(&w);
        QVERIFY2(!nachher.isEmpty(),
                 "nach dem Umbau gibt es die Auswahlfelder nicht mehr");
        QVERIFY2(nachher.first()->count() == hatte,
                 qPrintable(QStringLiteral("vorher %1 Einträge, nachher %2 "
                                           "-- die Liste ist leergelaufen")
                                .arg(hatte).arg(nachher.first()->count())));
    }
};

QTEST_MAIN(TstPluginAuswahlBleibt)
#include "tst_plugin_auswahl_bleibt.moc"
