// no-port-check: Longpath-eigener Pruefstand.
// =================================================================
// tests/tst_wiedergabe_nimmt_nur_iq.cpp  (Longpath)
// =================================================================
//
// Die Wiedergabe liest I/Q. Der Knopf eine Zeile darueber schreibt Ton.
//
// GEFUNDEN AM 2026-10-22, einen Tag nachdem die Wiedergabe eine Flaeche
// bekam (#309): `PlaybackRadioConnection::openRecording()` verlangt nur
// eine lesbare Stereo-WAV mit brauchbarer Abtastrate. Eine Tonaufnahme
// aus derselben Reihe ist genau das -- Stereo, 48 kHz. Sie wird
// geladen, die Verbindung meldet `Connected`, und 48-kHz-Ton laeuft als
// I/Q durch FFT und WDSP. Ergebnis: Rauschen im Wasserfall, kein Wort
// darueber, warum.
//
// Dazu kommt, dass man die zwei Sorten im Dateiwaehler gar nicht
// unterscheiden konnte: `startAudio()` und `startIq()` riefen BEIDE
// `suggestedPath("wav")`. Gleicher Ordner, gleiches Muster, gleiche
// Sekunde -- dieselbe Datei. Wer beide gleichzeitig startete, verlor
// die erste Aufnahme.
//
// Was hier festgenagelt wird:
//   * eine Tonaufnahme wird ABGELEHNT, mit einem Satz, der I/Q nennt
//   * eine I/Q-Aufnahme laeuft weiter (keine Verschaerfung nach innen)
//   * ein FEHLENDER Beizettel lehnt NICHT ab -- Abwesenheit ist keine
//     Aussage, und alte Aufnahmen haben keinen
//   * Ton und I/Q schlagen verschiedene Dateinamen vor
//
// Modification history (Longpath):
//   2026-10-22 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include <QtTest>
#include <QTemporaryDir>

#include "core/PlaybackRadioConnection.h"
#include "core/audio/IqRecorder.h"
#include "core/audio/WavRecorder.h"
#include "core/ZeitUtc.h"
#include "models/RadioModel.h"
#include "models/RecordingModel.h"

using namespace Longpath;

class TstWiedergabeNimmtNurIq : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    QString pfad(const QString& n) const
    {
        return m_dir.path() + QLatin1Char('/') + n;
    }

    /// Eine echte I/Q-Aufnahme, durch den echten Schreiber -- WAV plus
    /// Beizettel, wie sie auf der Platte liegt.
    QString schreibeIq(const QString& name)
    {
        IqRecordingInfo info;
        info.utcStart   = Longpath::Zeit::utc(QDate(2026, 10, 22), QTime(2, 0));
        info.frequency  = QStringLiteral("14.074.000");
        info.mode       = QStringLiteral("USB");
        info.band       = QStringLiteral("20m");
        info.sampleRate = 192000;

        IqRecorder rec;
        rec.setSampleRate(192000);
        QString fehler;
        const QString ziel = pfad(name);
        if (!rec.start(ziel, info, &fehler)) { return {}; }
        QVector<float> block(512 * 2, 0.25f);
        rec.feed(block.constData(), 512);
        rec.stop();
        return ziel;
    }

    /// Eine echte Tonaufnahme, durch den echten Schreiber. Dieselbe
    /// Gestalt auf der Platte: Stereo-WAV plus Beizettel.
    QString schreibeTon(const QString& name)
    {
        WavRecordingInfo info;
        info.utcStart   = Longpath::Zeit::utc(QDate(2026, 10, 22), QTime(2, 5));
        info.frequency  = QStringLiteral("14.074.000");
        info.mode       = QStringLiteral("USB");
        info.band       = QStringLiteral("20m");
        info.sampleRate = 48000;

        WavRecorder rec;
        rec.setSampleRate(48000);
        QString fehler;
        const QString ziel = pfad(name);
        if (!rec.start(ziel, info, &fehler)) { return {}; }
        QVector<float> block(512, 0.1f);
        rec.feed(block.constData(), 512);
        rec.stop();
        return ziel;
    }

private slots:

    void eineTonaufnahmeWirdAbgelehnt()
    {
        const QString ton = schreibeTon(QStringLiteral("ton.wav"));
        QVERIFY2(!ton.isEmpty(), "die Tonaufnahme liess sich nicht schreiben");
        QVERIFY(QFile::exists(ton));

        PlaybackRadioConnection pb;
        QString fehler;
        const bool angenommen = pb.openRecording(ton, &fehler);

        QVERIFY2(!angenommen,
                 "die Tonaufnahme wurde als I/Q angenommen — sie laeuft "
                 "dann als Rauschen durch FFT und WDSP");
        // Der Satz muss sagen, was stattdessen gebraucht wird. „Fehler"
        // allein schickt den Bediener zurueck zum Suchen.
        QVERIFY2(fehler.contains(QStringLiteral("I/Q")), qPrintable(fehler));
    }

    void eineIqAufnahmeLaeuftWeiter()
    {
        const QString iq = schreibeIq(QStringLiteral("iq.wav"));
        QVERIFY2(!iq.isEmpty(), "die I/Q-Aufnahme liess sich nicht schreiben");

        PlaybackRadioConnection pb;
        QString fehler;
        QVERIFY2(pb.openRecording(iq, &fehler), qPrintable(fehler));
    }

    void ohneBeizettelWirdNichtAbgelehnt()
    {
        // Abwesenheit ist keine Aussage: eine I/Q-Aufnahme, deren
        // Beizettel fehlt (geloescht, von Hand kopiert, aus einer
        // aelteren Fassung), darf nicht als Ton gelesen werden.
        const QString iq = schreibeIq(QStringLiteral("ohnezettel.wav"));
        QVERIFY(!iq.isEmpty());
        QString zettel = iq;
        zettel.chop(4);
        zettel += QStringLiteral(".json");
        QVERIFY(QFile::exists(zettel));
        QVERIFY(QFile::remove(zettel));

        PlaybackRadioConnection pb;
        QString fehler;
        QVERIFY2(pb.openRecording(iq, &fehler), qPrintable(fehler));
    }

    void tonUndIqSchlagenVerschiedeneNamenVor()
    {
        RadioModel radio;
        RecordingModel rec;
        rec.attach(&radio);

        const QString fuerTon = rec.suggestedPath(QStringLiteral("wav"));
        const QString fuerIq  = rec.suggestedPath(QStringLiteral("wav"),
                                                  QStringLiteral("iq"));

        QVERIFY2(fuerTon != fuerIq,
                 "Ton und I/Q schlagen dieselbe Datei vor — wer beide "
                 "in derselben Sekunde startet, verliert die erste");
        QVERIFY2(fuerIq.contains(QStringLiteral("iq")), qPrintable(fuerIq));
        QVERIFY2(fuerIq.endsWith(QStringLiteral(".wav")), qPrintable(fuerIq));
    }
};

QTEST_MAIN(TstWiedergabeNimmtNurIq)
#include "tst_wiedergabe_nimmt_nur_iq.moc"
