// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_c_numeric_locale.cpp  (Longpath)
// =================================================================
// Zahlen aus Text lesen, auch mit deutscher Systemsprache.
//
// Befund 2026-09-27: Longpath aus dem Terminal (LANG=de_AT.UTF-8) meldete
// „TLE-Satz: 0 Satelliten, 95 verworfen" -- SGP4 liest die Bahndaten mit
// sscanf("%lf"), und nach Qts setlocale(LC_ALL, "") war das Dezimalzeichen
// ein Komma. main() setzt jetzt useCNumericLocale() (core/CNumericLocale.h).
//
// Geprueft:
//   - mit deutschem LC_NUMERIC verwirft SGP4 die Bahndaten (der Test sieht
//     den Fehler wirklich, statt zufaellig im „C"-Gebietsschema zu laufen);
//   - nach useCNumericLocale() liest es sie wieder.
// Ohne installiertes deutsches Gebietsschema (manche Linux-Laeufer) wird
// uebersprungen -- dort kann der Fehler gar nicht auftreten.
//
// Modification history (Longpath):
//   2026-09-27 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================
#include <QtTest>

#include <clocale>

#include "core/CNumericLocale.h"
#include "core/sat/SatelliteTracker.h"

using namespace Longpath;

namespace {

// Zwei Saetze aus CelesTrak „amateur" (Stand 2026-09-24), Pruefsummen heil.
const char* kTwoSats =
    "OSCAR 7 (AO-7)\n"
    "1 07530U 74089B   26266.86626550 -.00000026  00000+0  12979-3 0  9992\n"
    "2 07530 101.9918 281.1438 0011971 303.9282 178.5058 12.53699707372880\n"
    "PHASE 3B (AO-10)\n"
    "1 14129U 83058B   26263.04235857 -.00000060  00000+0  00000+0 0  9999\n"
    "2 14129  25.9788 204.0605 5980330 141.6651 281.5604  2.05869937297448\n";

bool useGermanNumbers()
{
    for (const char* name : {"de_AT.UTF-8", "de_DE.UTF-8", "de_AT", "de_DE",
                             "German_Austria.1252", "German_Germany.1252"}) {
        if (std::setlocale(LC_NUMERIC, name)) {
            // Nur wenn das Gebietsschema wirklich ein Komma bringt.
            const lconv* lc = std::localeconv();
            if (lc && lc->decimal_point && lc->decimal_point[0] == ',') { return true; }
        }
    }
    return false;
}

} // namespace

class TstCNumericLocale : public QObject
{
    Q_OBJECT
private slots:
    void cleanup() { std::setlocale(LC_NUMERIC, "C"); }

    void aGermanDecimalCommaRejectsEveryOrbit()
    {
        if (!useGermanNumbers()) {
            QSKIP("kein deutsches Gebietsschema installiert");
        }
        SatelliteTracker t;
        int rejected = -1;
        QCOMPARE(t.loadTle(QString::fromLatin1(kTwoSats), &rejected), 0);
        QCOMPARE(rejected, 2);
    }

    void theCNumericLocaleReadsThemAgain()
    {
        if (!useGermanNumbers()) {
            QSKIP("kein deutsches Gebietsschema installiert");
        }
        useCNumericLocale();
        SatelliteTracker t;
        int rejected = -1;
        QCOMPARE(t.loadTle(QString::fromLatin1(kTwoSats), &rejected), 2);
        QCOMPARE(rejected, 0);
        QCOMPARE(std::localeconv()->decimal_point[0], '.');
    }
};

QTEST_MAIN(TstCNumericLocale)
#include "tst_c_numeric_locale.moc"
