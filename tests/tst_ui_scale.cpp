// Die Darstellungsgroesse.
//
// Der Unterbau stand schon: `main.cpp` liest `UiScalePercent` aus der
// Einstellungsdatei und setzt daraus `QT_SCALE_FACTOR` -- und zwar VOR
// dem QApplication-Konstruktor, weil Qt den Massstab nur dort liest.
// Tot war das Menue: jeder Eintrag `setEnabled(false)` mit dem Tooltip
// "NYI", man konnte die Groesse also ueberhaupt nicht einstellen.
//
// Der wichtigste Fall hier ist `derSchluesselPasstZuDemWasMainCppLiest`.
// Zwischen dieser Einheit und `main.cpp` steht eine Kopplung ueber eine
// blosse Zeichenkette: `main.cpp` sucht von Hand nach dem XML-Element
// `<UiScalePercent>`, weil es zu dem Zeitpunkt noch kein AppSettings
// gibt. Laufen die beiden auseinander, schreibt das Menue brav in die
// Datei und beim Start passiert -- nichts. Kein Absturz, keine Meldung,
// die Einstellung wirkt einfach nie. Der Fall schreibt darum wirklich
// und liest die Datei so, wie main.cpp es tut.

#include <QFile>
#include <QStandardPaths>
#include <QtTest>

#include "core/AppSettings.h"
#include "core/UiScale.h"

using namespace Longpath;

class TstUiScale : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void dieStufenSindBrauchbar();
    void dieGrenzenHaltenUnsinnDraussen();
    void dieVorgabeIstHundert();
    void einUnsinnigerWertInDerDateiGiltNicht();
    void speichernUndLesenPasstZusammen();
    void derSchluesselPasstZuDemWasMainCppLiest();
    void derHinweisSagtWasZuTunIst();
};

void TstUiScale::initTestCase()
{
    // Nie in die echten Einstellungen schreiben.
    QVERIFY2(!qEnvironmentVariable("LONGPATH_CONFIG_DIR").isEmpty()
                 || !QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                         .isEmpty(),
             "kein beschreibbarer Einstellungsort");
}

void TstUiScale::cleanup()
{
    AppSettings::instance().setValue(QString::fromLatin1(UiScale::kSettingsKey), 100);
}

void TstUiScale::dieStufenSindBrauchbar()
{
    const QList<int>& stufen = UiScale::steps();
    QVERIFY2(stufen.contains(100), "100 % fehlt -- die Vorgabe muss waehlbar sein");
    // Zeus' Stufen, die den Anstoss gaben.
    for (int pct : {115, 130, 150}) {
        QVERIFY2(stufen.contains(pct),
                 qPrintable(QStringLiteral("Stufe %1 % fehlt").arg(pct)));
    }
    // Aufsteigend und ohne Dubletten -- sonst steht das Menue durcheinander.
    for (int i = 1; i < stufen.size(); ++i) {
        QVERIFY2(stufen.at(i) > stufen.at(i - 1), "Die Stufen sind nicht aufsteigend");
    }
    // Und jede Stufe muss auch gelten.
    for (int pct : stufen) {
        QVERIFY2(UiScale::isValid(pct),
                 qPrintable(QStringLiteral("Stufe %1 gilt nicht").arg(pct)));
    }
}

void TstUiScale::dieGrenzenHaltenUnsinnDraussen()
{
    QVERIFY(UiScale::isValid(50));
    QVERIFY(UiScale::isValid(300));
    QVERIFY(!UiScale::isValid(49));
    QVERIFY(!UiScale::isValid(301));
    QVERIFY(!UiScale::isValid(0));
    QVERIFY(!UiScale::isValid(-100));
}

void TstUiScale::dieVorgabeIstHundert()
{
    AppSettings::instance().setValue(QString::fromLatin1(UiScale::kSettingsKey), 100);
    QCOMPARE(UiScale::current(), 100);
}

void TstUiScale::einUnsinnigerWertInDerDateiGiltNicht()
{
    // Steht aus irgendeinem Grund 10 in der Datei, darf die Oberflaeche
    // nicht auf ein Zehntel schrumpfen -- man kaeme an das Menue nicht
    // mehr heran, um es zurueckzustellen.
    AppSettings::instance().setValue(QString::fromLatin1(UiScale::kSettingsKey), 10);
    QCOMPARE(UiScale::current(), 100);
    AppSettings::instance().setValue(QString::fromLatin1(UiScale::kSettingsKey), 5000);
    QCOMPARE(UiScale::current(), 100);
    // Und `store` nimmt so etwas gar nicht erst an.
    QVERIFY2(!UiScale::store(10), "store() nahm 10 % an");
    QVERIFY2(!UiScale::store(5000), "store() nahm 5000 % an");
}

void TstUiScale::speichernUndLesenPasstZusammen()
{
    for (int pct : UiScale::steps()) {
        QVERIFY2(UiScale::store(pct),
                 qPrintable(QStringLiteral("store(%1) schlug fehl").arg(pct)));
        QCOMPARE(UiScale::current(), pct);
    }
}

void TstUiScale::derSchluesselPasstZuDemWasMainCppLiest()
{
    // Die Kopplung, die sonst still auseinanderlaufen koennte.
    //
    // Schritt 1: wirklich speichern und die Datei so lesen, wie main.cpp
    // es tut -- nach dem XML-Element mit genau diesem Namen suchen.
    QVERIFY(UiScale::store(130));

    const QString pfad = AppSettings::resolveSettingsPath(QString());
    QFile f(pfad);
    QVERIFY2(f.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QStringLiteral("Einstellungsdatei nicht lesbar: %1").arg(pfad)));
    const QByteArray daten = f.readAll();
    f.close();

    const QByteArray tag = QByteArrayLiteral("<")
                           + QByteArray(UiScale::kSettingsKey)
                           + QByteArrayLiteral(">");
    const int idx = daten.indexOf(tag);
    QVERIFY2(idx >= 0,
             qPrintable(QStringLiteral("In der Datei steht kein %1 -- das Menue "
                                       "schreibt woandershin, als main.cpp liest")
                            .arg(QString::fromLatin1(tag))));

    // Schritt 2: den Wert so herauslesen, wie main.cpp es tut.
    const int start = idx + tag.size();
    const int ende  = daten.indexOf('<', start);
    QVERIFY(ende > start);
    const int gelesen = daten.mid(start, ende - start).trimmed().toInt();
    QCOMPARE(gelesen, 130);

    // Schritt 3: und genau dieser Name steht auch in main.cpp. Ohne das
    // bliebe die Kopplung ungeprueft -- die Datei koennte stimmen und
    // main.cpp trotzdem nach etwas anderem suchen.
    const QString quellbaum = qEnvironmentVariable("LONGPATH_SOURCE_DIR");
    QFile m(quellbaum + QStringLiteral("/src/main.cpp"));
    if (!quellbaum.isEmpty() && m.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QByteArray quelle = m.readAll();
        m.close();
        QVERIFY2(quelle.contains(tag),
                 qPrintable(QStringLiteral("main.cpp sucht nicht nach %1")
                                .arg(QString::fromLatin1(tag))));
    } else {
        QWARN("LONGPATH_SOURCE_DIR fehlt oder main.cpp nicht lesbar -- Schritt 3 uebersprungen");
    }
}

void TstUiScale::derHinweisSagtWasZuTunIst()
{
    const QString hinweis = UiScale::restartHint(130);
    QVERIFY2(hinweis.contains(QStringLiteral("130")),
             qPrintable(QStringLiteral("Der Hinweis nennt die Groesse nicht: %1")
                            .arg(hinweis)));
    // Dass ein Neustart noetig ist, MUSS dastehen -- sonst sucht der
    // Benutzer den Fehler bei sich, wenn sich nichts aendert.
    QVERIFY2(hinweis.contains(QStringLiteral("Start")),
             qPrintable(QStringLiteral("Der Hinweis sagt nichts vom Neustart: %1")
                            .arg(hinweis)));
}

QTEST_MAIN(TstUiScale)
#include "tst_ui_scale.moc"
