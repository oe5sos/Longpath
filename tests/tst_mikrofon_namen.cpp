// =================================================================
// tests/tst_mikrofon_namen.cpp  (Longpath)
// =================================================================
//
// Erkennt Longpath ein echtes Mikrofon an seinem Namen -- auch dann,
// wenn das System nicht auf Englisch steht?
//
// Der Befund, aus dem dieser Prueflauf entstand (2026-10-29, am Geraet
// des Betreibers, FLEX-8400 am 40-m-Band):
//
//   Flex: waehrend des Sendens kamen 664 Tonpakete
//   Flex-Senden: 170160 Proben hinaus, Spitze -150 dBFS, 164 Zuege am
//   Mikrofon statt an FT8
//
// Es ging also etwas hinaus, und es war digitale Stille. Der Grund
// stand 80 Zeilen weiter oben im selben Protokoll:
//
//   Audiogeraet "Teufel Gaming Headset" (input) nicht gefunden
//   PortAudioBus: input via [Core Audio] on "BoomAudio"
//
// Das eingetragene Headset war nicht angesteckt, und die Ersatzsuche
// nahm „BoomAudio" -- ein virtuelles Geraet, das nichts hoert. Die
// Hardware-Erkennung haette das verhindern muessen; sie suchte aber
// nur nach „Microphone", „Built-in", „Internal", „Mic Input". Auf
// einem deutschen macOS heiszen die echten Eingaenge „Externes
// Mikrofon" und „MacBook Air-Mikrofon". Kein Marker traf, also fiel
// die Kette bis auf „erstes Geraet der Liste" durch.
//
// Longpath-original, kein Port.
// =================================================================

#include "core/audio/PortAudioBus.h"

#include <QString>
#include <QTest>

using Longpath::istHardwareMikrofonName;

class TestMikrofonNamen : public QObject
{
    Q_OBJECT

private slots:
    /// Die Namen vom Geraet des Betreibers, wortwoertlich aus dem
    /// Protokoll vom 2026-10-29 und aus `system_profiler`.
    void deutscheNamenSindMikrofone()
    {
        QVERIFY(istHardwareMikrofonName(QStringLiteral("Externes Mikrofon")));
        QVERIFY(istHardwareMikrofonName(QStringLiteral("MacBook Air-Mikrofon")));
    }

    /// Englisch muss weiter gelten -- der Prueflauf darf die alte
    /// Erkennung nicht ersetzen, nur erweitern.
    void englischeNamenGeltenWeiter()
    {
        QVERIFY(istHardwareMikrofonName(QStringLiteral("MacBook Pro Microphone")));
        QVERIFY(istHardwareMikrofonName(QStringLiteral("Built-in Microphone")));
        QVERIFY(istHardwareMikrofonName(QStringLiteral("Internal Mic")));
        QVERIFY(istHardwareMikrofonName(QStringLiteral("Line / Mic Input")));
    }

    /// Weitere Systemsprachen, nach derselben Konvention.
    void weitereSprachen()
    {
        QVERIFY(istHardwareMikrofonName(QStringLiteral("Micrófono interno")));
        QVERIFY(istHardwareMikrofonName(QStringLiteral("Microfono interno")));
        QVERIFY(istHardwareMikrofonName(QStringLiteral("Ingebouwde microfoon")));
        QVERIFY(istHardwareMikrofonName(QStringLiteral("Microfone interno")));
    }

    /// Das eigentliche Tor: die virtuellen Eingaenge. Jeder einzelne
    /// davon wuerde eine Sendung aus Stille erzeugen, und keiner faellt
    /// am Funkgeraet auf -- die Leistung steht, nur es hoert niemand
    /// etwas.
    ///
    /// „BoomAudio" ist der Fall vom 2026-10-29; „Microsoft Teams
    /// Audio" steht hier, weil es der Grund ist, warum der Marker
    /// „Micro" NICHT in der Liste stehen darf.
    void virtuelleGeraeteSindKeineMikrofone()
    {
        QVERIFY(!istHardwareMikrofonName(QStringLiteral("BoomAudio")));
        QVERIFY(!istHardwareMikrofonName(QStringLiteral("Microsoft Teams Audio")));
        QVERIFY(!istHardwareMikrofonName(QStringLiteral("ZoomAudioDevice")));
        QVERIFY(!istHardwareMikrofonName(QStringLiteral("BlackHole 2ch")));
        QVERIFY(!istHardwareMikrofonName(QStringLiteral("AetherSDR DAX 1")));
        QVERIFY(!istHardwareMikrofonName(QStringLiteral("Longpath VAX 1")));
        QVERIFY(!istHardwareMikrofonName(QStringLiteral("Splashtop Remote Sound")));
        QVERIFY(!istHardwareMikrofonName(QStringLiteral("Externe Kopfhörer")));
    }

    /// Grosz- und Kleinschreibung darf nicht entscheiden: PortAudio
    /// gibt denselben Eingang je nach Host-API verschieden zurueck.
    void schreibweiseEntscheidetNicht()
    {
        QVERIFY(istHardwareMikrofonName(QStringLiteral("externes mikrofon")));
        QVERIFY(istHardwareMikrofonName(QStringLiteral("EXTERNES MIKROFON")));
    }

    /// Ein leerer Name ist kein Mikrofon -- und darf nicht abstuerzen.
    void leererNameIstKeinMikrofon()
    {
        QVERIFY(!istHardwareMikrofonName(QString()));
        QVERIFY(!istHardwareMikrofonName(QStringLiteral("")));
    }
};

QTEST_APPLESS_MAIN(TestMikrofonNamen)
#include "tst_mikrofon_namen.moc"
