// tests/tst_widget_picker.cpp  (NereusSDR)
//
// NereusSDR-original. No Thetis port.
//
// ── Das Plus ─────────────────────────────────────────────────────────
//
// OE5SOS, 2026-08-15: „Ich möchte mit einem PLUS Widget hinzufügen
// können und entfernen, um so mein eigenes Profil selbst zu gestalten."
//
// Vier Eigenschaften, an denen so eine Auswahl scheitert, und alle vier
// sind pruefbar:
//
//   · Sie zeigt ALLE angemeldeten Widgets. Eine von Hand gepflegte
//     Kopie der Liste waere die Stelle, an der das naechste Widget
//     fehlt — deshalb liest sie registeredIds(), und deshalb prueft
//     der erste Test genau das.
//
//   · Ein Haken aendert wirklich etwas. Eine Auswahl, die nur sich
//     selbst umschaltet, sieht richtig aus und tut nichts.
//
//   · Sie folgt Aenderungen von ausserhalb. Das Menue „Containers" kann
//     dasselbe, und zwei Bedienwege, die sich gegenseitig nicht
//     mitbekommen, ergeben ein Fenster, das nicht zu seinen Haken passt.
//
//   · Nicht verfuegbar ist etwas anderes als ausgeblendet. Ist die
//     4O3A-Hauptschaltung aus, gibt es Verstaerker und Tuner nicht —
//     und ein Plus, das sie anbietet und dann nichts tut, ist schlimmer
//     als eines, das sie grau zeigt.

#include <QtTest>

#include "gui/applets/AppletVisibilityController.h"
#include "gui/widgets/WidgetPicker.h"

using namespace NereusSDR;

namespace {

/// Drei angemeldete Widgets, zwei davon sichtbar.
///
/// ── Warum hier zweimal gesetzt wird ──────────────────────────────────
///
/// registerApplet() nimmt den Vorgabewert NUR, wenn in den Einstellungen
/// noch nichts steht — sonst gewinnt das Gespeicherte. Das ist im Betrieb
/// richtig und in Tests eine Falle: alle Testmethoden laufen in einem
/// Prozess und teilen sich dieselbe Einstellungsdatei.
///
/// Gefunden am 2026-08-15: tickingReachesTheController schaltet Tuner
/// sichtbar, das wird geschrieben, und der naechste Test findet den
/// Haken bereits gesetzt. Sein click() nahm ihn dann weg, und das
/// Signal meldete voellig korrekt false — der Test behauptete true.
///
/// Also nach dem Anmelden ausdruecklich setzen. Damit steht jeder Test
/// auf demselben Boden, egal was vorher lief oder was das laufende
/// Programm zuletzt gespeichert hat.
void registerThree(AppletVisibilityController& vis)
{
    vis.registerApplet(QStringLiteral("Rx"),    QStringLiteral("RX"),    true);
    vis.registerApplet(QStringLiteral("Tx"),    QStringLiteral("TX"),    true);
    vis.registerApplet(QStringLiteral("Tuner"), QStringLiteral("Tuner"), false);

    vis.setVisible(QStringLiteral("Rx"),    true);
    vis.setVisible(QStringLiteral("Tx"),    true);
    vis.setVisible(QStringLiteral("Tuner"), false);
    vis.setAvailable(QStringLiteral("Rx"),    true);
    vis.setAvailable(QStringLiteral("Tx"),    true);
    vis.setAvailable(QStringLiteral("Tuner"), true);

    vis.describeApplet(QStringLiteral("Rx"), QStringLiteral("Empfang"),
                       {QStringLiteral("rx"), QStringLiteral("filter")});
    vis.describeApplet(QStringLiteral("Tx"), QStringLiteral("Senden"),
                       {QStringLiteral("tx"), QStringLiteral("swr")});
    vis.describeApplet(QStringLiteral("Tuner"), QStringLiteral("Tuner"),
                       {QStringLiteral("antenne"), QStringLiteral("swr")});
}

} // namespace

class TestWidgetPicker : public QObject
{
    Q_OBJECT

private slots:
    // ── Die Liste pflegt sich selbst ─────────────────────────────────
    void everyRegisteredWidgetIsOffered()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);

        QCOMPARE(p.entries(), vis.registeredIds());
        QCOMPARE(p.entries().size(), 3);
    }

    void aWidgetRegisteredLaterShowsUpAfterRefresh()
    {
        // Der Fall, der eine handgepflegte Liste auffliegen laesst.
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);
        QCOMPARE(p.entries().size(), 3);

        vis.registerApplet(QStringLiteral("Rade"), QStringLiteral("RADE"),
                           false);
        p.refresh();
        // refresh() zieht nur nach; fuer eine neue Zeile braucht es den
        // Neuaufbau, den ein erneutes Oeffnen ausloest.
        WidgetPicker again(&vis);
        QVERIFY2(again.entries().contains(QStringLiteral("Rade")),
                 "ein spaeter angemeldetes Widget fehlt in der Auswahl");
    }

    void theHooksMatchTheState()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);

        QVERIFY(p.isChecked(QStringLiteral("Rx")));
        QVERIFY(p.isChecked(QStringLiteral("Tx")));
        QVERIFY(!p.isChecked(QStringLiteral("Tuner")));
    }

    // ── Ein Haken aendert wirklich etwas ─────────────────────────────
    void tickingReachesTheController()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);

        QVERIFY(p.toggle(QStringLiteral("Tuner")));
        QVERIFY2(vis.isVisible(QStringLiteral("Tuner")),
                 "der Haken wurde gesetzt, das Widget blieb unsichtbar");

        QVERIFY(p.toggle(QStringLiteral("Rx")));
        QVERIFY2(!vis.isVisible(QStringLiteral("Rx")),
                 "das Entfernen kam nicht an");
    }

    void togglingEmits()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);
        QSignalSpy spy(&p, &WidgetPicker::toggled);

        QVERIFY(p.toggle(QStringLiteral("Tuner")));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("Tuner"));
        QCOMPARE(spy.at(0).at(1).toBool(), true);
    }

    // ── Zwei Bedienwege, ein Zustand ─────────────────────────────────
    void aChangeMadeInTheMenuMovesTheHook()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);
        QVERIFY(!p.isChecked(QStringLiteral("Tuner")));

        // So, wie es der Menueeintrag tut.
        vis.setVisible(QStringLiteral("Tuner"), true);
        QVERIFY2(p.isChecked(QStringLiteral("Tuner")),
                 "die Auswahl hat die Aenderung von aussen verschlafen");
    }

    // ── Nicht verfuegbar ist nicht ausgeblendet ──────────────────────
    void anUnavailableWidgetIsShownButNotOperable()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        vis.setAvailable(QStringLiteral("Tuner"), false);

        WidgetPicker p(&vis);
        QVERIFY2(p.entries().contains(QStringLiteral("Tuner")),
                 "ein nicht verfuegbares Widget wurde ganz verschwiegen — "
                 "man soll sehen, dass es es gibt");
        QVERIFY2(!p.isEnabled(QStringLiteral("Tuner")),
                 "ein nicht verfuegbares Widget war bedienbar");
        QVERIFY2(!p.toggle(QStringLiteral("Tuner")),
                 "es liess sich trotzdem umschalten");
    }

    void availabilityComingBackReEnablesTheRow()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        vis.setAvailable(QStringLiteral("Tuner"), false);
        WidgetPicker p(&vis);
        QVERIFY(!p.isEnabled(QStringLiteral("Tuner")));

        vis.setAvailable(QStringLiteral("Tuner"), true);
        QVERIFY2(p.isEnabled(QStringLiteral("Tuner")),
                 "die Zeile blieb grau, obwohl das Geraet wieder da ist");
    }

    void anUnknownIdIsRefusedQuietly()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);
        QVERIFY(!p.toggle(QStringLiteral("GibtsNicht")));
    }

    // ── Das Plus selbst ──────────────────────────────────────────────
    // ── Kategorie und Schlagwoerter ──────────────────────────────────
    void everyWidgetLandsInACategory()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        QCOMPARE(vis.category(QStringLiteral("Rx")),
                 QStringLiteral("Empfang"));
        // Die Spalte links steht in Anmeldereihenfolge, nicht im
        // Alphabet — sonst waere die Gestaltung der Liste hinfaellig.
        QCOMPARE(vis.categories(),
                 QStringList({QStringLiteral("Empfang"),
                              QStringLiteral("Senden"),
                              QStringLiteral("Tuner")}));
    }

    void anUndescribedWidgetIsNotLost()
    {
        AppletVisibilityController vis;
        vis.registerApplet(QStringLiteral("Neu"), QStringLiteral("Neu"), true);
        QCOMPARE(vis.category(QStringLiteral("Neu")),
                 AppletVisibilityController::uncategorised());
    }

    // ── Der Grund, warum es Schlagwoerter gibt ───────────────────────
    void searchFindsWhatTheTitleDoesNotSay()
    {
        // „swr" steht in keinem der drei Titel. Ein Suchfeld, das nur
        // Titel durchsucht, findet genau das, was man ohnehin schon
        // gefunden haette.
        AppletVisibilityController vis;
        registerThree(vis);
        QVERIFY(vis.matches(QStringLiteral("Tx"), QStringLiteral("swr")));
        QVERIFY(vis.matches(QStringLiteral("Tuner"), QStringLiteral("SWR")));
        QVERIFY(!vis.matches(QStringLiteral("Rx"), QStringLiteral("swr")));
    }

    void anEmptySearchMatchesEverything()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        for (const QString& id : vis.registeredIds()) {
            QVERIFY(vis.matches(id, QString{}));
            QVERIFY(vis.matches(id, QStringLiteral("   ")));
        }
    }

    void theTitleIsSearchableToo()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        QVERIFY(vis.matches(QStringLiteral("Tuner"), QStringLiteral("tun")));
    }

    // ── Die Form aus der Vorlage ─────────────────────────────────────
    void theCategoryColumnStartsWithAll()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);
        const QStringList col = p.categoryColumn();
        QCOMPARE(col.first(), WidgetPicker::allCategory());
        QCOMPARE(col.size(), 4);              // Alle + drei Kategorien
        QCOMPARE(p.currentCategory(), WidgetPicker::allCategory());
    }

    void choosingACategoryHidesTheRest()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);
        QCOMPARE(p.entries().size(), 3);

        p.setCategory(QStringLiteral("Senden"));
        QCOMPARE(p.entries(), QStringList({QStringLiteral("Tx")}));

        p.setCategory(WidgetPicker::allCategory());
        QCOMPARE(p.entries().size(), 3);
    }

    void searchNarrowsTheCards()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);

        // „swr" steht in keinem Titel, aber in zwei Schlagwortzeilen.
        p.setSearch(QStringLiteral("swr"));
        QCOMPARE(p.entries(), QStringList({QStringLiteral("Tx"),
                                           QStringLiteral("Tuner")}));

        p.setSearch(QString{});
        QCOMPARE(p.entries().size(), 3);
    }

    void categoryAndSearchNarrowTogether()
    {
        // Zwei Filter, die einander aufheben, sind ein haeufiger Fehler:
        // erst filtert die Kategorie, dann setzt die Suche alles zurueck.
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);

        p.setCategory(QStringLiteral("Tuner"));
        p.setSearch(QStringLiteral("swr"));
        QCOMPARE(p.entries(), QStringList({QStringLiteral("Tuner")}));

        p.setSearch(QStringLiteral("filter"));   // gehoert zu Rx
        QVERIFY2(p.entries().isEmpty(),
                 "die Suche hat die Kategorie ueberstimmt");
    }

    void aHiddenCardCannotBeToggled()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        WidgetPicker p(&vis);
        p.setCategory(QStringLiteral("Senden"));
        QVERIFY2(!p.toggle(QStringLiteral("Rx")),
                 "eine weggefilterte Karte liess sich umschalten");
    }

    void theButtonOwnsAPickerOnDemand()
    {
        AppletVisibilityController vis;
        registerThree(vis);
        AddWidgetButton b(&vis);
        QVERIFY2(b.picker() == nullptr,
                 "die Auswahl wurde gebaut, bevor jemand das Plus "
                 "gedrueckt hat");
        b.openPicker();
        QVERIFY(b.picker() != nullptr);
        QCOMPARE(b.picker()->entries().size(), 3);
    }
};

QTEST_MAIN(TestWidgetPicker)
#include "tst_widget_picker.moc"
