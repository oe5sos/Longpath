// =================================================================
// tests/tst_ft8_rufzeichen.cpp  (Longpath)
// =================================================================
//
// Rufzeichen, die nur als Hash über den Draht kommen.
//
// Bis zum 2026-10-28 übergab Longpath `nullptr` als Hashtabelle
// (Ft8Decoder.cpp:129). `ft8_lib` schreibt dann wortwörtlich `<...>`
// statt des Rufzeichens (message.c:734) — und genau das stand in den
// Funden.
//
// Geprüft wird durch die Bibliothek selbst, nicht gegen eine eigene
// Nachbildung: `ftx_message_encode` packt ein Sonderrufzeichen, und
// beim Packen ruft ft8_lib `save_hash` auf. Wird dieselbe Nachricht
// danach ausgepackt, muss `lookup_hash` das Rufzeichen zurückgeben.
// Die Gegenprobe fährt denselben Weg mit geleerter Tabelle und muss
// `<...>` liefern — ohne sie belegt der Fall darüber nichts.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-28 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

#include "core/Ft8Rufzeichen.h"

extern "C" {
#include "ft8/message.h"
}

using namespace Longpath;

namespace {

/// Packt `text` und packt ihn sofort wieder aus — durch dieselbe
/// Tabelle, die auch der Dekoder benutzt.
QString durchDieBibliothek(const QString& text)
{
    auto* schnitt = static_cast<ftx_callsign_hash_interface_t*>(
        ft8RufzeichenSchnittstelle());

    ftx_message_t m{};
    ftx_message_init(&m);
    const QByteArray roh = text.toLatin1();
    if (ftx_message_encode(&m, schnitt, roh.constData())
            != FTX_MESSAGE_RC_OK) {
        return QStringLiteral("PACKEN MISSLUNGEN");
    }

    char aus[FTX_MAX_MESSAGE_LENGTH] = {0};
    ftx_message_offsets_t versatz{};
    if (ftx_message_decode(&m, schnitt, aus, &versatz)
            != FTX_MESSAGE_RC_OK) {
        return QStringLiteral("AUSPACKEN MISSLUNGEN");
    }
    return QString::fromLatin1(aus).trimmed();
}

}  // namespace

class TstFt8Rufzeichen : public QObject {
    Q_OBJECT

private slots:
    void init() { Ft8Rufzeichenspeicher::instanz().leeren(); }

    void dieDreiHashlaengenKommenAusEinerRechnung();
    void einPlatzhalterWirdNichtGemerkt();
    void sonderrufzeichenUeberlebtDenRundlauf();
    void ohneTabelleBleibtEsDreiPunkte();   // Gegenprobe
    void dieTabelleLaeuftNichtUeber();
};

void TstFt8Rufzeichen::dieDreiHashlaengenKommenAusEinerRechnung()
{
    auto& t = Ft8Rufzeichenspeicher::instanz();
    const quint32 n22 = 0x2AF3C1u;
    t.merke(QStringLiteral("OE5SOS/P"), n22);

    QCOMPARE(t.suche(22, n22),        QStringLiteral("OE5SOS/P"));
    QCOMPARE(t.suche(12, n22 >> 10),  QStringLiteral("OE5SOS/P"));
    QCOMPARE(t.suche(10, n22 >> 12),  QStringLiteral("OE5SOS/P"));

    // Ein Hash, den niemand gemeldet hat, bleibt leer — und nicht
    // etwa „irgendein Rufzeichen".
    QVERIFY(t.suche(22, n22 + 1).isEmpty());
}

void TstFt8Rufzeichen::einPlatzhalterWirdNichtGemerkt()
{
    auto& t = Ft8Rufzeichenspeicher::instanz();
    t.merke(QStringLiteral("<...>"), 1234u);
    t.merke(QStringLiteral("..."),   1235u);
    QCOMPARE(t.anzahl(), 0);
    QVERIFY(t.suche(22, 1234u).isEmpty());
}

void TstFt8Rufzeichen::sonderrufzeichenUeberlebtDenRundlauf()
{
    // Nicht „CQ PA3XYZ/MM": dort trägt die Nachricht das
    // Sonderrufzeichen VOLLSTÄNDIG in 58 Bit, es wird gar nichts
    // gehasht — der erste Anlauf dieses Prüfstands lief deshalb auch
    // mit geleerter Tabelle durch und belegte nichts.
    //
    // Gehasht wird der PARTNER eines Sonderrufzeichens: Satzart 4 ist
    // „one nonstandard call and one hashed call" (ft8_lib message.h),
    // das Normalrufzeichen steckt in 12 Bit.
    const QString hin = QStringLiteral("K1ABC PA3XYZ/MM RR73");
    const QString zurueck = durchDieBibliothek(hin);

    QVERIFY2(!zurueck.contains(QStringLiteral("...")),
             qPrintable(QStringLiteral("Hash nicht aufgeloest: %1")
                            .arg(zurueck)));
    QVERIFY2(zurueck.contains(QStringLiteral("PA3XYZ/MM")),
             qPrintable(zurueck));
    QVERIFY2(zurueck.contains(QStringLiteral("K1ABC")),
             qPrintable(QStringLiteral("das gehashte Rufzeichen fehlt: %1")
                            .arg(zurueck)));
    QVERIFY(Ft8Rufzeichenspeicher::instanz().anzahl() > 0);
}

void TstFt8Rufzeichen::ohneTabelleBleibtEsDreiPunkte()
{
    // Dieselbe Nachricht, aber die Tabelle weiß beim AUSPACKEN nichts
    // mehr — der Zustand vor dem 2026-10-28.
    auto* schnitt = static_cast<ftx_callsign_hash_interface_t*>(
        ft8RufzeichenSchnittstelle());

    ftx_message_t m{};
    ftx_message_init(&m);
    QVERIFY(ftx_message_encode(&m, schnitt, "K1ABC PA3XYZ/MM RR73")
            == FTX_MESSAGE_RC_OK);

    Ft8Rufzeichenspeicher::instanz().leeren();

    char aus[FTX_MAX_MESSAGE_LENGTH] = {0};
    ftx_message_offsets_t versatz{};
    QVERIFY(ftx_message_decode(&m, schnitt, aus, &versatz)
            == FTX_MESSAGE_RC_OK);

    const QString zurueck = QString::fromLatin1(aus);
    QVERIFY2(zurueck.contains(QStringLiteral("...")),
             qPrintable(QStringLiteral("erwartet: Platzhalter; bekam %1")
                            .arg(zurueck)));
}

void TstFt8Rufzeichen::dieTabelleLaeuftNichtUeber()
{
    auto& t = Ft8Rufzeichenspeicher::instanz();
    const int zuViel = Ft8Rufzeichenspeicher::kHoechstzahl + 500;
    for (int i = 0; i < zuViel; ++i) {
        t.merke(QStringLiteral("OE%1AAA").arg(i), quint32(i) << 2);
    }
    QCOMPARE(t.anzahl(), Ft8Rufzeichenspeicher::kHoechstzahl);

    // Das ZULETZT gehörte muss noch da sein — ein Speicher, der die
    // frischen vergisst, wäre schlimmer als keiner.
    QCOMPARE(t.suche(22, quint32(zuViel - 1) << 2),
             QStringLiteral("OE%1AAA").arg(zuViel - 1));
}

QTEST_MAIN(TstFt8Rufzeichen)
#include "tst_ft8_rufzeichen.moc"
