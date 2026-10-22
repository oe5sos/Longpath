// Verify AppletVisibilityController state, persistence, and signal emission.
// no-port-check: Longpath-original — no Thetis source.

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "gui/applets/AppletVisibilityController.h"
#include "core/AppSettings.h"

using namespace Longpath;

class TstAppletVisibilityController : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()    { AppSettings::instance().clear(); }
    void cleanup()         { AppSettings::instance().clear(); }

    void default_visible_when_no_settings_key();
    void default_hidden_when_no_settings_key();
    void persisted_value_overrides_default();
    void setVisible_persists_and_emits();
    void setVisible_idempotent_no_emit_on_same_value();
    void registeredIds_returns_insertion_order();
    void displayName_round_trip();
    void ensureVisible_meldet_auch_ohne_wechsel();
    void ensureVisible_respektiert_die_verfuegbarkeit();
};

void TstAppletVisibilityController::default_visible_when_no_settings_key()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"), true);
    QVERIFY(c.isVisible(QStringLiteral("Rx")));
}

void TstAppletVisibilityController::default_hidden_when_no_settings_key()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Cwx"), QStringLiteral("CW Keyer"), false);
    QVERIFY(!c.isVisible(QStringLiteral("Cwx")));
}

void TstAppletVisibilityController::persisted_value_overrides_default()
{
    AppSettings::instance().setValue(QStringLiteral("AppletRxVisible"),
                                     QStringLiteral("False"));
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"), QStringLiteral("RX"), true);
    QVERIFY(!c.isVisible(QStringLiteral("Rx")));
}

void TstAppletVisibilityController::setVisible_persists_and_emits()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Tx"), QStringLiteral("TX"), true);
    QSignalSpy spy(&c, &AppletVisibilityController::visibilityChanged);

    c.setVisible(QStringLiteral("Tx"), false);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QStringLiteral("Tx"));
    QCOMPARE(spy.first().at(1).toBool(), false);
    QCOMPARE(AppSettings::instance().value(QStringLiteral("AppletTxVisible"))
             .toString(), QStringLiteral("False"));

    AppletVisibilityController c2;
    c2.registerApplet(QStringLiteral("Tx"), QStringLiteral("TX"), true);
    QVERIFY(!c2.isVisible(QStringLiteral("Tx")));
}

void TstAppletVisibilityController::setVisible_idempotent_no_emit_on_same_value()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Vax"), QStringLiteral("VAX"), true);
    QSignalSpy spy(&c, &AppletVisibilityController::visibilityChanged);

    c.setVisible(QStringLiteral("Vax"), true);
    QCOMPARE(spy.count(), 0);

    c.setVisible(QStringLiteral("Vax"), false);
    QCOMPARE(spy.count(), 1);

    c.setVisible(QStringLiteral("Vax"), false);
    QCOMPARE(spy.count(), 1);
}

void TstAppletVisibilityController::registeredIds_returns_insertion_order()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Rx"),         QStringLiteral("RX"),          true);
    c.registerApplet(QStringLiteral("Tx"),         QStringLiteral("TX"),          true);
    c.registerApplet(QStringLiteral("PhoneCw"),    QStringLiteral("Phone / CW"),  true);
    c.registerApplet(QStringLiteral("Vax"),        QStringLiteral("VAX"),         true);
    c.registerApplet(QStringLiteral("PureSignal"), QStringLiteral("PureSignal"),  true);

    QStringList expected{
        QStringLiteral("Rx"), QStringLiteral("Tx"), QStringLiteral("PhoneCw"),
        QStringLiteral("Vax"), QStringLiteral("PureSignal")
    };
    QCOMPARE(c.registeredIds(), expected);
}

void TstAppletVisibilityController::displayName_round_trip()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("PhoneCw"), QStringLiteral("Phone / CW"), true);
    QCOMPARE(c.displayName(QStringLiteral("PhoneCw")),
             QStringLiteral("Phone / CW"));
    QCOMPARE(c.displayName(QStringLiteral("Unknown")), QString{});
}

// ── ensureVisible: melden, auch wenn sich nichts geaendert hat ───────
//
// Am 2026-10-21 am Geraet aufgefallen: das FT8-Panel liess sich ueber
// das × an seiner Kopfzeile schliessen, ohne dass der gemerkte Zustand
// mitging. In den Einstellungen stand weiter
// AppletFt8DecoderVisible=True, das Panel war weg -- und jeder Druck auf
// FT8 verpuffte, weil setVisible() bei unveraendertem Wert frueh
// aussteigt. Nur MainWindow::applyAppletVisibility blendet das Fenster
// wirklich ein, und das haengt an diesem Signal.
void TstAppletVisibilityController::ensureVisible_meldet_auch_ohne_wechsel()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Ft8Decoder"),
                     QStringLiteral("FT8 Decoder"), true);
    QVERIFY(c.isVisible(QStringLiteral("Ft8Decoder")));

    QSignalSpy sichtbar(&c, &AppletVisibilityController::visibilityChanged);
    QSignalSpy wirksam(&c,
                       &AppletVisibilityController::effectiveVisibilityChanged);

    // Gegenprobe zuerst: der alte Weg schweigt -- das IST der Fehler.
    c.setVisible(QStringLiteral("Ft8Decoder"), true);
    QCOMPARE(sichtbar.count(), 0);
    QCOMPARE(wirksam.count(), 0);

    // Und der neue meldet trotzdem.
    c.ensureVisible(QStringLiteral("Ft8Decoder"));
    QCOMPARE(sichtbar.count(), 1);
    QCOMPARE(wirksam.count(), 1);
    QCOMPARE(wirksam.at(0).at(1).toBool(), true);
}

// Ein Panel, das die Modusachse gerade NICHT freigibt, darf auch
// ensureVisible nicht aufzwingen -- sonst stuende der FT8-Dekoder in CW
// im Bild. Die Nutzerspalte wird gesetzt, die wirksame Sichtbarkeit
// bleibt aus.
void TstAppletVisibilityController::ensureVisible_respektiert_die_verfuegbarkeit()
{
    AppletVisibilityController c;
    c.registerApplet(QStringLiteral("Ft8Decoder"),
                     QStringLiteral("FT8 Decoder"), true);
    c.setAvailable(QStringLiteral("Ft8Decoder"), false);

    QSignalSpy wirksam(&c,
                       &AppletVisibilityController::effectiveVisibilityChanged);
    c.ensureVisible(QStringLiteral("Ft8Decoder"));

    QCOMPARE(wirksam.count(), 1);
    QCOMPARE(wirksam.at(0).at(1).toBool(), false);
}

QTEST_MAIN(TstAppletVisibilityController)
#include "tst_applet_visibility_controller.moc"
