// =================================================================
// tests/tst_zeus_profil_import.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigener Pruefstand.
//
// Zeus hat kein Austauschformat. Auf der Platte liegen nur `.zeusdb`
// (rohe LiteDB) und `.zeus`; die Audio-Suite ist eine Datei des
// proprietaeren Clients. Seine TX-Audio-Profile stehen nur bei der
// LAUFENDEN Maschine, hinter
//
//     GET http://127.0.0.1:<port>/api/tx-audio-profiles
//
// Die Antwort ist JSON, und ihre Felder sind in Zeus' offengelegtem
// Maschinenteil (v2.0.61) Zeile fuer Zeile belegt —
// `Zeus.Contracts/Dtos.cs:3838-3886` und die darin benutzten Teile.
//
// Dieser Stand faehrt den ganzen Weg OHNE NETZ: eine Antwort, wie die
// Maschine sie schickt, hinein — Profile und Hinweise heraus. Das ist
// dieselbe Trennung wie bei tst_thetis_profil_import, und sie hat
// denselben Grund: eine Schicht, die ein laufendes Fremdprogramm
// braucht, um geprueft zu werden, wird nicht geprueft.
//
// Die beiden Faelle, an denen am meisten haengt:
//
//   `dasSendefilterKommtAlsBetrag` — Zeus traegt „operator-typed
//   positive magnitudes; server re-signs per mode-family"
//   (Dtos.cs:3857). Wer das Vorzeichen stillschweigend uebernimmt,
//   baut bei unterem Seitenband ein vertauschtes Sendefilter ein, und
//   die Anzeige sieht dabei richtig aus.
//
//   `wasZeusFuehrtUndWirNichtWirdGesagt` — Zeus' halbe Kette (Tor,
//   De-Esser, Roehre, Hall) steckt in `nativePluginStates`, deren
//   Schluessel nicht offengelegt sind. Ein Profilname ohne diesen
//   Hinweis waere eine Behauptung.
// =================================================================

#include <QtTest/QtTest>

#include "core/ZeusProfilImport.h"
#include "core/ProfilHerkunft.h"
#include "core/ZeusAbfrage.h"

using namespace Longpath;

namespace {

// Eine Antwort, wie `/api/tx-audio-profiles` sie schickt. Die
// Feldnamen sind die von TxAudioProfileDto, in der Schreibweise, in
// der .NET sie voreingestellt serialisiert (camelCase).
QByteArray zeusAntwort()
{
    return QByteArrayLiteral(R"JSON({
      "profiles": [
        {
          "id": "essb-wide",
          "name": "ESSB Wide",
          "micGainDb": 18,
          "levelerMaxGainDb": 7.5,
          "txLeveling": {
            "alcMaxGainDb": 3.0,
            "alcDecayMs": 10,
            "levelerEnabled": true,
            "levelerDecayMs": 100,
            "compressorEnabled": true,
            "compressorGainDb": 4.0,
            "cessbEnabled": true,
            "cessbBandwidthHz": 4000
          },
          "cfcConfig": {
            "enabled": true,
            "postEqEnabled": true,
            "preCompDb": 2.0,
            "prePeqDb": -1.0,
            "fftSize": 8192,
            "bands": [
              {"freqHz": 50,   "compLevelDb": 1, "postGainDb": -1},
              {"freqHz": 100,  "compLevelDb": 2, "postGainDb": 0},
              {"freqHz": 200,  "compLevelDb": 3, "postGainDb": 1},
              {"freqHz": 500,  "compLevelDb": 4, "postGainDb": 2},
              {"freqHz": 1000, "compLevelDb": 5, "postGainDb": 3},
              {"freqHz": 1500, "compLevelDb": 6, "postGainDb": 2},
              {"freqHz": 2000, "compLevelDb": 7, "postGainDb": 1},
              {"freqHz": 2500, "compLevelDb": 8, "postGainDb": 0},
              {"freqHz": 3000, "compLevelDb": 9, "postGainDb": -1},
              {"freqHz": 5000, "compLevelDb": 0, "postGainDb": -2}
            ]
          },
          "txPhaseRotator": {
            "enabled": true, "cornerHz": 338, "stages": 8, "reverse": false
          },
          "lowCutHz": 100,
          "highCutHz": 3100,
          "processingMode": "native",
          "masterBypass": false,
          "chainOrder": ["gate", "deess", "tube"],
          "chainParked": [],
          "nativePluginStates": {
            "gate": {"threshold": "-40"},
            "tube": {"drive": "3"}
          },
          "vstPluginStates": {},
          "targetSpectralDensity": 62,
          "schemaVersion": 2,
          "dexp": {
            "enabled": true,
            "thresholdDbv": -40.0,
            "attackMs": 2.0,
            "holdMs": 500.0,
            "releaseMs": 100.0,
            "expansionDb": 15.0,
            "hysteresisDb": 2.5,
            "detectorTauMs": 20.0,
            "sideChannelFilterEnabled": true,
            "sideChannelLowHz": 300.0,
            "sideChannelHighHz": 3000.0,
            "lookAheadEnabled": false,
            "lookAheadMs": 0.0
          },
          "legacyTxEq": {
            "enabled": true,
            "preampDb": -6,
            "frequenciesHz": [32,63,125,250,500,1000,2000,4000,8000,16000],
            "gainsDb": [0,1,2,3,4,5,4,3,2,1]
          },
          "carrierLevelPercent": 100
        }
      ]
    })JSON");
}

const ThetisProfil& erstes(const ZeusProfilImport::Ergebnis& e)
{
    static ThetisProfil leer;
    return e.profile.isEmpty() ? leer : e.profile.first();
}

bool irgendeinHinweisEnthaelt(const ThetisProfil& p, const QString& teil)
{
    for (const QString& h : p.hinweise) {
        if (h.contains(teil, Qt::CaseInsensitive)) { return true; }
    }
    return false;
}

} // namespace

class TstZeusProfilImport : public QObject
{
    Q_OBJECT

private slots:

    void eineAntwortWirdEinProfil()
    {
        const auto e = ZeusProfilImport::lies(zeusAntwort());
        QVERIFY2(e.fehler.isEmpty(), qPrintable(e.fehler.join(QLatin1Char(' '))));
        QCOMPARE(e.profile.size(), 1);
        QCOMPARE(erstes(e).name, QStringLiteral("ESSB Wide"));
    }

    // ── Die Pegelregelung, Feld fuer Feld ────────────────────────────
    void dieSkalareKommenAn()
    {
        const auto p = erstes(ZeusProfilImport::lies(zeusAntwort()));
        QCOMPARE(p.werte.value(QStringLiteral("MicGain")).toString(),
                 QStringLiteral("18"));
        QCOMPARE(p.werte.value(QStringLiteral("Lev_MaxGain")).toString(),
                 QStringLiteral("7.5"));
        QCOMPARE(p.werte.value(QStringLiteral("Lev_On")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(p.werte.value(QStringLiteral("Lev_Decay")).toString(),
                 QStringLiteral("100"));
        QCOMPARE(p.werte.value(QStringLiteral("ALC_MaximumGain")).toString(),
                 QStringLiteral("3"));
        QCOMPARE(p.werte.value(QStringLiteral("CESSB_On")).toString(),
                 QStringLiteral("True"));
        // Der Pegel des Kompressors kommt mit, sein SCHALTER nicht --
        // der liegt in Longpath ausserhalb des Profils.
        QCOMPARE(p.werte.value(QStringLiteral("CompanderLevel")).toString(),
                 QStringLiteral("4"));
        QVERIFY(!p.werte.contains(QStringLiteral("CompanderOn")));
        QVERIFY(irgendeinHinweisEnthaelt(p, QStringLiteral("Kompressor-Schalter")));
    }

    // ── Zehn Baender, dreissig Felder ────────────────────────────────
    //
    // Zeus traegt eine Liste von Objekten, Longpath dreimal zehn
    // einzelne Felder. Genau hier war beim Thetis-Weg schon einmal
    // eine ganze Bandgruppe stumm liegengeblieben.
    void dieCfcBaenderWerdenAufgefaltet()
    {
        const auto p = erstes(ZeusProfilImport::lies(zeusAntwort()));
        QCOMPARE(p.werte.value(QStringLiteral("CFCEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(p.werte.value(QStringLiteral("CFCEqFreq0")).toString(),
                 QStringLiteral("50"));
        QCOMPARE(p.werte.value(QStringLiteral("CFCEqFreq9")).toString(),
                 QStringLiteral("5000"));
        QCOMPARE(p.werte.value(QStringLiteral("CFCPreComp4")).toString(),
                 QStringLiteral("5"));
        QCOMPARE(p.werte.value(QStringLiteral("CFCPostEqGain4")).toString(),
                 QStringLiteral("3"));
        for (int i = 0; i < 10; ++i) {
            QVERIFY2(p.werte.contains(QStringLiteral("CFCEqFreq%1").arg(i)),
                     qPrintable(QStringLiteral("Band %1 fehlt").arg(i)));
        }
        // Zeus' waehlbare FFT-Groesse gibt es bei uns nicht.
        QVERIFY(irgendeinHinweisEnthaelt(p, QStringLiteral("8192")));
    }

    void derPhasendreherUndDasTor()
    {
        const auto p = erstes(ZeusProfilImport::lies(zeusAntwort()));
        QCOMPARE(p.werte.value(QStringLiteral("CFCPhaseRotatorEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(p.werte.value(QStringLiteral("CFCPhaseRotatorFreq")).toString(),
                 QStringLiteral("338"));
        QCOMPARE(p.werte.value(QStringLiteral("CFCPhaseRotatorStages")).toString(),
                 QStringLiteral("8"));

        QCOMPARE(p.werte.value(QStringLiteral("DEXP_Enabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(p.werte.value(QStringLiteral("DEXP_ExpansionRatioDb")).toString(),
                 QStringLiteral("15"));
        QCOMPARE(p.werte.value(QStringLiteral("DEXP_LowCutHz")).toString(),
                 QStringLiteral("300"));
        // Zeus' Haltezeit gibt es bei uns nicht -- und das wird gesagt.
        QVERIFY(!p.werte.contains(QStringLiteral("DEXP_HoldMs")));
        QVERIFY(irgendeinHinweisEnthaelt(p, QStringLiteral("Haltezeit")));
    }

    void derAlteEntzerrerZaehltAbEins()
    {
        const auto p = erstes(ZeusProfilImport::lies(zeusAntwort()));
        QCOMPARE(p.werte.value(QStringLiteral("TXEQEnabled")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(p.werte.value(QStringLiteral("TXEQPreamp")).toString(),
                 QStringLiteral("-6"));
        // Zeus' erstes Band ist unser TXEQ1 / TxEqFreq1, nicht 0.
        QCOMPARE(p.werte.value(QStringLiteral("TxEqFreq1")).toString(),
                 QStringLiteral("32"));
        QCOMPARE(p.werte.value(QStringLiteral("TXEQ10")).toString(),
                 QStringLiteral("1"));
        QVERIFY(!p.werte.contains(QStringLiteral("TXEQ0")));
    }

    // ── Das Vorzeichen, das Zeus nicht mitschickt ────────────────────
    void dasSendefilterKommtAlsBetrag()
    {
        const auto p = erstes(ZeusProfilImport::lies(zeusAntwort()));
        QCOMPARE(p.werte.value(QStringLiteral("FilterLow")).toString(),
                 QStringLiteral("100"));
        QCOMPARE(p.werte.value(QStringLiteral("FilterHigh")).toString(),
                 QStringLiteral("3100"));
        QVERIFY2(irgendeinHinweisEnthaelt(p, QStringLiteral("Betrag")),
                 "ohne diesen Hinweis baut ein Betreiber bei unterem "
                 "Seitenband ein vertauschtes Sendefilter ein, ohne es "
                 "zu sehen");
    }

    // ── Was NICHT mitkommt, wird gesagt ──────────────────────────────
    void wasZeusFuehrtUndWirNichtWirdGesagt()
    {
        const auto p = erstes(ZeusProfilImport::lies(zeusAntwort()));
        QVERIFY2(irgendeinHinweisEnthaelt(p, QStringLiteral("Audio-Suite")),
                 "Zeus' halbe Kette steckt in nativePluginStates -- ein "
                 "Profilname ohne diesen Satz waere eine Behauptung");
        QVERIFY(irgendeinHinweisEnthaelt(p, QStringLiteral("Dichte")));
        QVERIFY(!ZeusProfilImport::nichtUebernommen().isEmpty());
    }

    // ── Die Formen, in denen die Antwort kommen kann ─────────────────
    void nackteListeUndEinzelnesProfil()
    {
        QByteArray nackt = zeusAntwort();
        // Die Huelle wegschneiden, wie jemand es tut, der die Antwort
        // von Hand in eine Datei legt.
        const int auf = nackt.indexOf('[');
        const int zu  = nackt.lastIndexOf(']');
        QVERIFY(auf > 0 && zu > auf);
        nackt = nackt.mid(auf, zu - auf + 1);

        const auto e = ZeusProfilImport::lies(nackt);
        QVERIFY2(e.fehler.isEmpty(), qPrintable(e.fehler.join(QLatin1Char(' '))));
        QCOMPARE(e.profile.size(), 1);
    }

    void fremdeAntwortWirdAbgelehnt()
    {
        const auto e = ZeusProfilImport::lies(
            QByteArrayLiteral("{\"ok\":true,\"items\":[]}"));
        QVERIFY(e.profile.isEmpty());
        QVERIFY(!e.fehler.isEmpty());

        const auto k = ZeusProfilImport::lies(QByteArrayLiteral("kein json"));
        QVERIFY(!k.fehler.isEmpty());
    }

    // ── Der Erkenner kennt die Antwort wieder ────────────────────────
    void derErkennerBenenntEineAbgelegteAntwort()
    {
        const auto b = ProfilHerkunft::erkenne(zeusAntwort());
        QCOMPARE(b.art, ProfilHerkunft::Art::ZeusTxAudio);
        QVERIFY(b.wohin.contains(QStringLiteral("TX-Profile")));

        // „name" allein ist KEIN Zeus-Merkmal -- sonst erklaert der
        // Erkenner jedes zweite JSON zu einem Zeus-Profil.
        QVERIFY(!ZeusProfilImport::siehtNachZeusAus(
            QByteArrayLiteral("{\"name\":\"x\",\"value\":1}")));
    }

    // ── Gefragt wird nur der eigene Rechner ──────────────────────────
    //
    // Ohne diese Schranke waere der Profilholer ein Werkzeug, mit dem
    // sich beliebige Wirte anfragen lassen. Geprueft wird am NAMEN und
    // nicht ueber eine Namensaufloesung: ein Name, der heute auf
    // 127.0.0.1 zeigt, zeigt morgen woandershin.
    void nurDerEigeneRechner()
    {
        QString grund;
        QVERIFY(ZeusAbfrage::istOertlich(QStringLiteral("http://127.0.0.1:5000")));
        QVERIFY(ZeusAbfrage::istOertlich(QStringLiteral("http://localhost:7331")));

        QVERIFY(!ZeusAbfrage::istOertlich(
            QStringLiteral("http://192.168.1.50:5000"), &grund));
        QVERIFY(!grund.isEmpty());
        QVERIFY(!ZeusAbfrage::istOertlich(QStringLiteral("ftp://127.0.0.1")));
        QVERIFY(!ZeusAbfrage::istOertlich(QStringLiteral("einfach so")));
        QVERIFY(!ZeusAbfrage::istOertlich(QString()));
    }
};

QTEST_MAIN(TstZeusProfilImport)
#include "tst_zeus_profil_import.moc"
