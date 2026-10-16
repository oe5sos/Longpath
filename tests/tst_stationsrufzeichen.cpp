// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_stationsrufzeichen.cpp  (Longpath)
// =================================================================
// Wer die Station ist — und warum vier Stellen es nicht wussten.
//
// Das Rufzeichen steht kanonisch unter `User/Callsign`; SpotHubDialog
// schreibt es ausdruecklich so („Writes canonical keys:
// User/Callsign, User/GridSquare"), und RadioModel loest die alten
// Schluessel je Quelle (DxClusterCallsign, RbnCallsign …) seit der
// 3J-2-Nachbesserung mit einer Rueckfallkette auf.
//
// VIER Stellen taten das nicht, sondern lasen nur `StationCallsign` —
// und den schreibt NIEMAND:
//
//   SotaClient            Kennung im User-Agent
//   MainWindow_KiwiSdr    Betreiberkennung an den Empfaenger
//   QsoMapWindow          der eigene Marker auf der Karte
//   RadioModel            das Rufzeichen im FlexRadio-Rundruf
//
// In Martins Einstellungen steht unter fuenf Schluesseln OE5SOS — und
// genau dieser ist leer. Die QSO-Karte zeichnete seine eigene Station
// deshalb gar nicht (applyStationMarker kehrt bei leerem Rufzeichen
// sofort zurueck), und im Netz rief er sich „LONGPATH".
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>
#include <QTemporaryDir>

#include "core/AppSettings.h"

using namespace Longpath;

class TstStationsrufzeichen : public QObject
{
    Q_OBJECT

private:
    void leere()
    {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("StationCallsign"), QString());
        s.setValue(QStringLiteral("User/Callsign"), QString());
        s.setValue(QStringLiteral("StationGridSquare"), QString());
        s.setValue(QStringLiteral("User/GridSquare"), QString());
    }

private slots:
    // Der Fall, der Martin betraf: nur der kanonische Schluessel ist
    // gesetzt. Vorher sahen vier Stellen hier NICHTS.
    void derKanonischeSchluesselGenuegt()
    {
        leere();
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        QCOMPARE(AppSettings::stationCallsign(), QStringLiteral("OE5SOS"));

        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));
        QCOMPARE(AppSettings::stationGridSquare(), QStringLiteral("JN67VV"));
    }

    // Eine von Hand gesetzte Fassung gilt weiter und schlaegt den
    // kanonischen — sonst verloere jemand, der den alten Schluessel
    // gepflegt hat, seine Einstellung.
    void derAlteSchluesselSchlaegtDenKanonischen()
    {
        leere();
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("StationCallsign"), QStringLiteral("OE5XXX"));
        s.setValue(QStringLiteral("User/Callsign"),   QStringLiteral("OE5SOS"));
        QCOMPARE(AppSettings::stationCallsign(), QStringLiteral("OE5XXX"));

        s.setValue(QStringLiteral("StationGridSquare"), QStringLiteral("JN78AA"));
        s.setValue(QStringLiteral("User/GridSquare"),   QStringLiteral("JN67VV"));
        QCOMPARE(AppSettings::stationGridSquare(), QStringLiteral("JN78AA"));
    }

    // Ist gar nichts gesetzt, kommt auch nichts heraus — die Aufrufer
    // haben alle einen Fall dafuer (kein Marker, Kennung weglassen,
    // „LONGPATH" im Rundruf).
    void ohneAllesBleibtEsLeer()
    {
        leere();
        QVERIFY(AppSettings::stationCallsign().isEmpty());
        QVERIFY(AppSettings::stationGridSquare().isEmpty());
    }

    // Leerzeichen zaehlen nicht als Rufzeichen: ein Feld, in dem nur
    // ein Leerschlag steht, darf nicht den kanonischen verdecken.
    void nurLeerzeichenZaehltNicht()
    {
        leere();
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("StationCallsign"), QStringLiteral("   "));
        s.setValue(QStringLiteral("User/Callsign"),   QStringLiteral("OE5SOS"));
        QCOMPARE(AppSettings::stationCallsign(), QStringLiteral("OE5SOS"));
    }
};

QTEST_MAIN(TstStationsrufzeichen)
#include "tst_stationsrufzeichen.moc"
