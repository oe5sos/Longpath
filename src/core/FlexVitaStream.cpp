// =================================================================
// src/core/FlexVitaStream.cpp  (Longpath)
// =================================================================
//
// Herkunft, Lizenz und das Messprotokoll: siehe FlexVitaStream.h und
// docs/protocols/flex-vita49-gemessen.md.
// Ported from AetherSDR src/core/PanadapterStream.{h,cpp} at d58e2b8a
// (GPLv3, primary author Jeremy [KK7GWY]) — project-level citation per
// docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include "FlexVitaStream.h"

#include "AppSettings.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QNetworkDatagram>
#include <QStringList>

#include <algorithm>

namespace Longpath {

Q_LOGGING_CATEGORY(lcFlexStream, "longpath.flex.strom", QtInfoMsg)

FlexVitaStream::FlexVitaStream(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead,
            this, &FlexVitaStream::onReadyRead);
}

quint16 FlexVitaStream::start(const QHostAddress& radioAddress)
{
    if (m_socket.state() == QAbstractSocket::BoundState) {
        return m_localPort;
    }
    if (radioAddress.isNull()) {
        qCWarning(lcFlexStream) << "keine Geraeteadresse — nichts zu melden";
        return 0;
    }

    // Das Betriebssystem waehlt den Port; welcher, erfaehrt die Maschine
    // gleich darauf aus der Absenderadresse.
    if (!m_socket.bind(QHostAddress::AnyIPv4, 0)) {
        qCWarning(lcFlexStream) << "UDP-Bind gescheitert:" << m_socket.errorString();
        return 0;
    }
    m_radioAddress = radioAddress;
    m_localPort = m_socket.localPort();

    // From AetherSDR PanadapterStream.cpp:261-268 [@d58e2b8a] — ein
    // Null-Byte an die Maschine; sie lernt IP:Port aus der
    // Absenderadresse. Das ist der Weg, der sicher trägt; der
    // TCP-Befehl `client udpport` kommt daneben und wird vom Aufrufer
    // geschickt, weil nur der den Befehlskanal hat.
    const QByteArray anmeldung(1, '\x00');
    const qint64 gesendet =
        m_socket.writeDatagram(anmeldung, m_radioAddress, kRadioPort);
    if (gesendet != 1) {
        qCWarning(lcFlexStream) << "UDP-Anmeldung nicht hinausgegangen:"
                                << m_socket.errorString();
    } else {
        qCInfo(lcFlexStream) << "angemeldet bei" << m_radioAddress.toString()
                             << ":" << kRadioPort
                             << "— hoere auf eigenem Port" << m_localPort;
    }

    // Alle fuenf Sekunden sagen, WAS ankommt. Ein Strom, der schweigt,
    // und ein Strom, der nur Ton bringt, sehen von aussen gleich aus —
    // und genau diese zwei Faelle hat am 2026-10-21 niemand
    // auseinanderhalten koennen, weil hier nichts protokolliert wurde.
    m_proKlasse.clear();
    m_straenge.clear();
    m_tonLuecken.clear();
    m_gefuellteRahmen = 0;
    m_lueckenfuellung =
        AppSettings::instance()
            .value(QStringLiteral("FlexAudioLueckenfuellung"),
                   QStringLiteral("True")).toString()
        == QStringLiteral("True");
    m_verlorenGesamt = 0;
    m_verlorenGemeldet = 0;
    m_uhr.start();
    m_bericht.setInterval(5000);
    m_bericht.callOnTimeout(this, [this] { berichteKlassen(); });
    m_bericht.start();
    return m_localPort;
}

// Ein Schritt je Paket: Zaehlersprung und Ankunftsabstand. Beides
// braucht einen Zustand JE STROM — siehe die Begruendung am `Strang`.
void FlexVitaStream::zaehleLuecke(const FlexVita::Header& h,
                                  qint64 jetztNsVorgabe)
{
    const quint64 schluessel =
        (static_cast<quint64>(h.packetClass) << 32) | h.streamId;
    Strang& st = m_straenge[schluessel];
    if (st.pakete == 0) {
        st.klasse = static_cast<quint16>(h.packetClass);
        st.strom  = h.streamId;
    }
    ++st.pakete;

    const qint64 jetztNs = jetztNsVorgabe >= 0
                               ? jetztNsVorgabe
                               : (m_uhr.isValid() ? m_uhr.nsecsElapsed() : -1);
    qint64 abstandNs = 0;
    if (st.letzteAnkunftNs >= 0 && jetztNs > st.letzteAnkunftNs) {
        abstandNs = jetztNs - st.letzteAnkunftNs;
        if (abstandNs > st.groessteLueckeNs) {
            st.groessteLueckeNs = abstandNs;
            st.groessteLueckeUm =
                QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"));
        }
    }
    st.letzteAnkunftNs = jetztNs;

    // Das erste Paket hat keinen Vorgaenger; daraus eine Luecke zu
    // rechnen hiesse, Abwesenheit als Befund zu lesen.
    if (!st.gesehen) {
        st.gesehen = true;
        st.letzterZaehler = h.counter;
        return;
    }
    const int fehlend =
        FlexVita::fehlendeNachZaehler(st.letzterZaehler, h.counter);
    st.letzterZaehler = h.counter;

    // ── Den ueblichen Abstand mitfuehren ──────────────────────────
    //
    // Gleitend, und NUR ueber Abstaende, die selbst keine Luecke
    // sind: sonst zoege die erste Unterbrechung den Mittelwert hoch
    // und die naechste faende keine Schwelle mehr vor.
    if (abstandNs > 0 && st.pakete > 1) {
        const bool plausibel = st.mittelAbstandNs == 0
                               || abstandNs < st.mittelAbstandNs * 4;
        if (plausibel) {
            st.mittelAbstandNs = st.mittelAbstandNs == 0
                                     ? abstandNs
                                     : (st.mittelAbstandNs * 7 + abstandNs) / 8;
        }
    }

    // ── Unterbrechung oder Luecke? ────────────────────────────────
    //
    // Ueber einem Zaehlerumlauf ist der Sprung bedeutungslos (siehe
    // FlexVitaStream.h, kUmlaufFaktor). Solche Pausen zaehlen
    // getrennt -- die eigene Sendung ist die haeufigste davon.
    const qint64 schwelleNs = st.mittelAbstandNs * kUmlaufFaktor;
    const bool unterbrochen = st.pakete >= kMindestPaketeFuerMittel
                              && schwelleNs > 0
                              && abstandNs > schwelleNs;
    if (unterbrochen) {
        ++st.unterbrechungen;
        if (abstandNs > st.laengsteUnterbrechungNs) {
            st.laengsteUnterbrechungNs = abstandNs;
        }
        return;
    }

    if (fehlend > 0) {
        st.verloren += fehlend;
        ++st.spruenge;
        m_verlorenGesamt += fehlend;

        // Fuer den Tonweg: was fehlt, soll als Stille wieder
        // eingeschoben werden, damit die Welle nicht springt. Die
        // Decke steht in FlexLueckenfuellung.h und ist Absicht — eine
        // halbe Sekunde Stille glatt zu buegeln waere Vertuschung.
        //
        // ── Eine UNTERBRECHUNG ist keine Luecke (2026-10-30) ─────
        //
        // Waehrend des Sendens schickt die Maschine keinen
        // Empfangston. Das erste Paket danach kommt dreizehn Sekunden
        // spaeter, und der 4-Bit-Zaehler ist in der Zeit laengst
        // mehrfach umgelaufen — sein Sprung ist dann eine Zufallszahl
        // zwischen 0 und 15, kein Verlust.
        //
        // Am Geraet gemessen: die Fuellung schob nach jeder Sendung
        // bis zu acht Pakete (43 ms) Stille ein, die niemand verloren
        // hatte. Ueber 35 Minuten Rufbetrieb waren das **3034 ms**,
        // und jede davon verschiebt die Zeitachse des Empfangs gegen
        // die Wirklichkeit.
        //
        // Darum: nur fuellen, wenn der Abstand zum letzten Paket noch
        // nach Paketabstand aussieht. Bei 187 Paketen je Sekunde sind
        // das 5,3 ms; die Decke von acht Paketen deckt 43 ms. Wer
        // laenger weg war, war nicht "weg", sondern abgeschaltet.
        if (static_cast<FlexVitaClass>(h.packetClass)
                == FlexVitaClass::NarrowAudio
            && (abstandNs == 0 || abstandNs <= kUnterbrechungAbNs)) {
            // `abstandNs == 0` heisst NICHT „gleichzeitig", sondern
            // „keine Zeitangabe" -- die Uhr laeuft erst ab `start()`,
            // und ein Prueflauf, der Datagramme direkt einspeist, hat
            // keine. Abwesenheit ist keine Aussage: ohne Zeit wird
            // gefuellt wie bisher, die Unterbrechungspruefung greift
            // nur, wenn eine Zeit WIRKLICH dasteht.
            //
            // Beim ersten Anlauf stand hier `abstandNs > 0`, und
            // damit waere die Fuellung ueberall dort ausgefallen, wo
            // keine Uhr laeuft. Die Gegenprobe
            // `ohnePauseWirdDerselbeSprungGefuellt` hat es gefangen.
            FlexVita::Lueckenzustand& lz = m_tonLuecken[h.streamId];
            lz.offeneLuecken = std::min(lz.offeneLuecken + fehlend,
                                        FlexVita::kMaxGefuellteLuecken);
        }
    }
}

QVector<FlexVitaStream::Luecken> FlexVitaStream::luecken() const
{
    QVector<Luecken> raus;
    raus.reserve(m_straenge.size());
    for (auto it = m_straenge.cbegin(); it != m_straenge.cend(); ++it) {
        const Strang& st = it.value();
        Luecken l;
        l.klasse           = st.klasse;
        l.strom            = st.strom;
        l.pakete           = st.pakete;
        l.verloren         = st.verloren;
        l.spruenge         = st.spruenge;
        l.groessteLueckeMs = st.groessteLueckeNs / 1000000LL;
        l.groessteLueckeUm = st.groessteLueckeUm;
        l.unterbrechungen  = st.unterbrechungen;
        l.laengsteUnterbrechungMs = st.laengsteUnterbrechungNs / 1000000LL;
        raus.push_back(l);
    }
    std::sort(raus.begin(), raus.end(), [] (const Luecken& a, const Luecken& b) {
        return a.klasse != b.klasse ? a.klasse < b.klasse : a.strom < b.strom;
    });
    return raus;
}

// Eine Zeile je Bericht, und nur wenn sich seit der letzten etwas
// bewegt hat — ein stehender Strom soll das Protokoll nicht fluten.
void FlexVitaStream::berichteKlassen()
{
    if (m_datagrams == m_zuletztGemeldet) {
        return;
    }
    m_zuletztGemeldet = m_datagrams;

    QStringList teile;
    for (auto it = m_proKlasse.cbegin(); it != m_proKlasse.cend(); ++it) {
        const QString name = [k = it.key()] () -> QString {
            switch (static_cast<FlexVitaClass>(k)) {
            case FlexVitaClass::PanadapterFft:  return QStringLiteral("Bild-FFT");
            case FlexVitaClass::WaterfallTile:  return QStringLiteral("Wasserfall");
            case FlexVitaClass::NarrowAudio:    return QStringLiteral("Ton");
            case FlexVitaClass::ReducedBwAudio: return QStringLiteral("Ton schmal");
            case FlexVitaClass::Meter:          return QStringLiteral("Messwerte");
            default: break;
            }
            return k == 0 ? QStringLiteral("unlesbar")
                          : QStringLiteral("0x%1").arg(k, 4, 16, QLatin1Char('0'));
        }();
        teile << QStringLiteral("%1 %2").arg(name).arg(it.value());
    }
    // Die zweite Zeile ist die wichtigere: Pakete kommen an, lange
    // bevor daraus ein Rahmen wird. Steht hier 0, waehrend oben
    // Tausende stehen, dann liegt es am Zusammensetzen, nicht am Netz.
    qCInfo(lcFlexStream) << "Strom:" << m_datagrams << "Pakete —"
                         << qPrintable(teile.join(QStringLiteral(", ")));
    if (m_fftRahmen > 0) {
        qCInfo(lcFlexStream).nospace()
            << "Strom: Bins ueber alle " << m_fftRahmen << " Rahmen — "
            << m_binKleinsterJe << " bis " << m_binGroessterJe
            << " (" << (m_binGroessterJe - m_binKleinsterJe) << " Stufen)";
    }
    qCInfo(lcFlexStream) << "Strom: daraus fertig —"
                         << m_fftRahmen << "Bildrahmen,"
                         << m_wasserfallZeilen << "Wasserfallzeilen;"
                         << "Maszstab gesetzt fuer" << m_scales.size()
                         << "Strom-Kennung(en)";

    // Die Luecken. Immer eine Zeile, auch wenn nichts fehlt: eine
    // saubere Null ist der Befund, der die naechste These toetet.
    QStringList lueckenTeile;
    for (const Luecken& l : luecken()) {
        if (l.pakete < 2) { continue; }
        // Luecken und Unterbrechungen GETRENNT. Bis zum 2026-10-30
        // stand die eigene Sendung als „groeszte Pause 13422 ms" in
        // derselben Zeile wie der Netzverlust -- und 13,2 s sind
        // genau die FT8-Tastung des Betreibers.
        QString teil = QStringLiteral("0x%1/%2: %3 verloren in %4 Spruengen")
                           .arg(l.klasse, 4, 16, QLatin1Char('0'))
                           .arg(l.strom, 0, 16)
                           .arg(l.verloren)
                           .arg(l.spruenge);
        if (l.groessteLueckeMs > 0 && !l.groessteLueckeUm.isEmpty()) {
            teil += QStringLiteral(", groeszte Luecke %1 ms um %2")
                        .arg(l.groessteLueckeMs)
                        .arg(l.groessteLueckeUm);
        }
        if (l.unterbrechungen > 0) {
            teil += QStringLiteral("; %1 Unterbrechung(en), laengste "
                                   "%2 ms (eigene Sendung o.ae., KEIN "
                                   "Netzverlust)")
                        .arg(l.unterbrechungen)
                        .arg(l.laengsteUnterbrechungMs);
        }
        lueckenTeile << teil;
    }
    if (!lueckenTeile.isEmpty()) {
        qCInfo(lcFlexStream).nospace()
            << "Strom: Luecken — " << m_verlorenGesamt
            << " Pakete fehlen insgesamt; "
            << qPrintable(lueckenTeile.join(QStringLiteral(" | ")));
    }
    if (m_gefuellteRahmen > 0) {
        qCInfo(lcFlexStream).nospace()
            << "Strom: Lueckenfuellung hat " << (m_gefuellteRahmen / 24)
            << " ms Stille eingeschoben — so viel Ton fehlte, und so "
            << "viel laenger stimmt die Zeitachse jetzt";
    }
    // Ein Sprung im laufenden Betrieb ist der Augenblick, der erklaert,
    // warum gerade nichts zu hoeren war — der soll nicht erst im
    // Fuenf-Sekunden-Bericht auftauchen, sondern mit eigener Zeile.
    if (m_verlorenGesamt > m_verlorenGemeldet) {
        qCWarning(lcFlexStream).nospace()
            << "Strom: " << (m_verlorenGesamt - m_verlorenGemeldet)
            << " Pakete in den letzten 5 s verloren — das ist der Netzweg, "
            << "nicht die Maschine";
        m_verlorenGemeldet = m_verlorenGesamt;
    }
}

qint64 FlexVitaStream::sendeTxPaket(const QByteArray& paket)
{
    if (paket.isEmpty() || m_radioAddress.isNull()) { return 0; }
    if (m_socket.state() != QAbstractSocket::BoundState) { return 0; }
    const qint64 gesendet =
        m_socket.writeDatagram(paket, m_radioAddress, kTxPort);
    if (gesendet > 0) {
        ++m_txPaketeGesendet;
    } else {
        // Nur die ersten paar melden: bei 187 Paketen je Sekunde
        // ertraenkt eine Warnung je Paket das Protokoll.
        static int gemeldet = 0;
        if (++gemeldet <= 5) {
            qCWarning(lcFlexStream) << "Sendepaket nicht hinausgegangen:"
                                    << m_socket.errorString();
        }
    }
    return gesendet;
}

void FlexVitaStream::stop()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
    }
    m_localPort = 0;
}

bool FlexVitaStream::isRunning() const
{
    return m_socket.state() == QAbstractSocket::BoundState;
}

void FlexVitaStream::setYPixels(quint32 streamId, int yPixels)
{
    if (yPixels <= 1) {
        return;
    }
    m_scales[streamId].yPixels = yPixels;
}

void FlexVitaStream::setDbmRange(quint32 streamId, double minDbm, double maxDbm)
{
    if (maxDbm <= minDbm) {
        return;
    }
    Scale& s = m_scales[streamId];
    s.minDbm = minDbm;
    s.maxDbm = maxDbm;
}

void FlexVitaStream::ingestDatagramForTesting(const QByteArray& datagram)
{
    handleDatagram(datagram);
}

void FlexVitaStream::onReadyRead()
{
    while (m_socket.hasPendingDatagrams()) {
        const QNetworkDatagram d = m_socket.receiveDatagram();
        if (d.isNull()) {
            continue;
        }
        handleDatagram(d.data());
    }
}

// From AetherSDR PanadapterStream::processDatagram [@d58e2b8a] — an der
// Paketklasse verteilen, alles Unbekannte still fallen lassen.
void FlexVitaStream::handleDatagram(const QByteArray& datagram)
{
    ++m_datagrams;

    const FlexVita::Header h = FlexVita::parseHeader(datagram);
    if (!h.valid) {
        ++m_ignored;
        ++m_proKlasse[0];   // 0 = unlesbarer Kopf
        return;
    }
    ++m_proKlasse[static_cast<quint16>(h.packetClass)];
    zaehleLuecke(h);

    switch (static_cast<FlexVitaClass>(h.packetClass)) {
    case FlexVitaClass::PanadapterFft: {
        const auto rahmen = m_fft.feed(datagram);
        if (!rahmen) {
            return;   // noch nicht vollstaendig, oder nur Wiederholung
        }
        // Der Maszstab kommt von der Stelle, die ihn gesetzt hat — die
        // Maschine meldet y_pixels nicht zurueck (siehe Kopf).
        ++m_fftRahmen;
        // Einmal die ROHWERTE melden. Bis hierher war alles Modell:
        // „ein Bin ist eine Bildzeile 0…y_pixels-1". Stimmt das nicht,
        // nuetzt der richtigste Teiler nichts (2026-10-22: mit der von
        // der Maschine gemeldeten 20 blieb das Bild voll ausgeschlagen).
        // Rahmen 1 ist NICHT aussagekraeftig: nach einer
        // x_pixels-Vergroesserung (50 -> 1024) schickt die Maschine
        // einen Uebergangsrahmen, in dem jeder neue Bin null ist
        // (dokumentiert in FlexVitaReader.h). Am 2026-10-22 gemessen:
        // Rahmen 1 war durchgehend null. Also spaeter nachsehen.
        // Ueber ALLE Rahmen mitfuehren, nicht nur den einen.
        //
        // 2026-10-22: ein einzelner Rahmen zeigte 17,6 dB Hub,
        // SmartSDR am selben Band 50. Spitzen sind kurz — wer einen
        // Augenblick misst, misst die Pause zwischen ihnen mit.
        if (!rahmen->bins.isEmpty()) {
            for (const quint16 b : rahmen->bins) {
                m_binKleinsterJe = std::min(m_binKleinsterJe, b);
                m_binGroessterJe = std::max(m_binGroessterJe, b);
            }
        }

        if ((m_fftRahmen == 50 || m_fftRahmen == 500)
            && !rahmen->bins.isEmpty()) {
            quint16 kleinster = rahmen->bins.first();
            quint16 groesster = rahmen->bins.first();
            double summe = 0.0;
            for (const quint16 b : rahmen->bins) {
                kleinster = std::min(kleinster, b);
                groesster = std::max(groesster, b);
                summe += b;
            }
            qCInfo(lcFlexStream).nospace()
                << "Strom: Rohwerte von Rahmen " << m_fftRahmen << " — "
                << rahmen->bins.size() << " Bins, kleinster " << kleinster
                << ", groesster " << groesster << ", Mittel "
                << (summe / rahmen->bins.size());
        }
        const Scale s = m_scales.value(rahmen->streamId);
        emit spectrumReady(rahmen->streamId,
                           FlexVita::binsToDbm(rahmen->bins,
                                               s.minDbm, s.maxDbm, s.yPixels));
        break;
    }

    case FlexVitaClass::WaterfallTile: {
        const auto zeile = m_tiles.feed(datagram);
        if (!zeile) {
            return;
        }
        ++m_wasserfallZeilen;
        emit waterfallRowReady(zeile->streamId, zeile->values,
                               zeile->lowFreqMhz, zeile->highFreqMhz,
                               zeile->autoBlackLevel);
        break;
    }

    case FlexVitaClass::NarrowAudio: {
        const QVector<float> werte = FlexVita::narrowAudioSamples(datagram);
        if (werte.isEmpty()) {
            return;
        }
        // Fehlende Pakete als ausgeblendete Stille wieder einschieben.
        // Ohne das springt die Welle (Knacken) UND der Ton laeuft um
        // die fehlende Zeit zu kurz — bei den am 2026-10-29 gemessenen
        // 1,2 % sind das 0,7 s auf die Minute.
        FlexVita::Lueckenzustand& lz = m_tonLuecken[h.streamId];
        const int vorher = werte.size();
        const QVector<float> gefuellt =
            FlexVita::fuelleLuecke(werte, lz, m_lueckenfuellung);
        if (gefuellt.size() > vorher) {
            m_gefuellteRahmen += (gefuellt.size() - vorher) / 2;
        }
        emit audioSamplesReady(h.streamId, gefuellt);
        break;
    }

    case FlexVitaClass::Meter: {
        // Seit 2026-10-21 gemessen und damit baubar (§5b des
        // Messblatts). Der Strom liefert nur Index und Rohwert; was
        // ein Index heisst und wie er zu teilen ist, weiss erst der
        // Aufrufer aus den `meter`-Statuszeilen.
        const auto werte = FlexVita::meterSamples(datagram);
        if (!werte.isEmpty()) {
            emit meterSamplesReady(h.streamId, werte);
        }
        break;
    }

    case FlexVitaClass::ReducedBwAudio:
        // Nicht gemessen, also nicht gebaut. Die schmale Tonklasse
        // kommt erst mit `client set send_reduced_bw_dax=1`, und das
        // schickt Longpath nicht; Messwerte hat der Messlauf nie
        // ausgeloest. Abwesenheit ist keine Aussage — hier ist nur
        // nichts darueber belegt, und raten waere schlimmer als
        // fallen lassen.
        ++m_ignored;
        break;

    default:
        ++m_ignored;
        break;
    }
}

}  // namespace Longpath
