// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_aether_profil.cpp  (Longpath)
// =================================================================
//
// Martins Aether-Profil, Wert fuer Wert, in unserer Kette.
//
// „zur info, mein headset funktioniert mit windows und dem aether
//  channel strip, also die einstellungen bei eather perfekt! wir
//  müssen unsere grafik verwenden, aber alle einstellungen usw. von
//  aether verwenden." (2026-10-13)
//
// Die Datei im Pruefstand ist SEINE: tests/data/aether-ssb-30-07-2026
// .json, eine Kopie von „SSB_30.07.2026 (imported).json", die
// AetherSDR am 2026-10-13 um 15:46 UTC aus dem laufenden Geraet
// geschrieben hat. Sie ist damit kein Beispiel, sondern die
// Grundlage -- und jede Zahl darin ist eine Zusage: wenn der
// Importeur sie verliert oder umrechnet, klingt es hier anders als
// dort, und genau das soll dieser Pruefstand verhindern.
//
// Die Werte unten stehen absichtlich als ZAHLEN im Pruefstand und
// nicht als Verweis auf die Datei. Ein Pruefstand, der seine
// Sollwerte aus derselben Datei liest, die er prueft, prueft nichts.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-13 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "core/AppSettings.h"
#include "core/strip/AetherPreset.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

using namespace Longpath;

namespace {

constexpr double kRate = 48000.0;

QString profilPfad()
{
    return QString::fromLatin1(LONGPATH_TEST_DATA_DIR)
           + QStringLiteral("/aether-ssb-30-07-2026.json");
}

} // namespace

class TstAetherProfil : public QObject
{
    Q_OBJECT
private slots:

    // ── Die Werte ───────────────────────────────────────────────────
    void jede_zahl_aus_seiner_datei_kommt_an()
    {
        StripChain c;
        c.prepare(kRate);
        const auto e = AetherPreset::applyFile(profilPfad(), c);
        QVERIFY2(e.ok, qPrintable(e.fehler));
        QCOMPARE(e.name, QStringLiteral("SSB_30.07.2026"));

        // Tor. Expander, und die Werte DANACH -- setMode schnappt
        // Verhaeltnis und Boden auf ein Paar, und 3,215 ist nicht 2,0.
        QCOMPARE(int(c.gate().mode()), int(ClientGate::Mode::Expander));
        QCOMPARE(c.gate().thresholdDb(), -20.0f);
        QVERIFY(qFuzzyCompare(c.gate().ratio(), 3.215f));
        QVERIFY(qFuzzyCompare(c.gate().returnDb(), 17.1f));
        QCOMPARE(c.gate().floorDb(), -15.0f);
        QCOMPARE(c.gate().holdMs(), 85.0f);
        QCOMPARE(c.gate().lookaheadMs(), 3.0f);

        // Entzerrer. masterGain ist LINEAR: als dB gelesen waere die
        // ganze Kette 2,7 dB zu leise.
        QVERIFY(qFuzzyCompare(c.eq().masterGain(), 1.653f));
        QCOMPARE(int(c.eq().filterFamily()),
                 int(ClientEq::FilterFamily::Chebyshev));
        // Vierzehn, nicht zehn: seine zehn plus unsere vier leeren
        // Griffe. Die Zusage ist nicht die Zahl, sondern dass nichts
        // Hoerbares dazukommt -- jedes Band ab zehn ist abgeschaltet.
        QCOMPARE(c.eq().activeBandCount(), 14);
        for (int i = 10; i < c.eq().activeBandCount(); ++i) {
            QVERIFY2(!c.eq().band(i).enabled,
                     qPrintable(QStringLiteral("Band %1 ist an").arg(i)));
        }

        const ClientEq::BandParams b0 = c.eq().band(0);
        QCOMPARE(int(b0.type), int(ClientEq::FilterType::HighPass));
        QVERIFY(qAbs(b0.freqHz - 62.69f) < 0.01f);
        QVERIFY(qAbs(b0.q - 2.531f) < 0.001f);
        QCOMPARE(b0.slopeDbPerOct, 12);

        const ClientEq::BandParams b1 = c.eq().band(1);   // +5,71 dB bei 68,95 Hz
        QCOMPARE(int(b1.type), int(ClientEq::FilterType::Peak));
        QVERIFY(qAbs(b1.gainDb - 5.71f) < 0.01f);

        const ClientEq::BandParams b7 = c.eq().band(7);   // das eine abgeschaltete
        QCOMPARE(b7.enabled, false);

        const ClientEq::BandParams b8 = c.eq().band(8);
        QCOMPARE(int(b8.type), int(ClientEq::FilterType::HighShelf));
        QVERIFY(qAbs(b8.freqHz - 4981.87f) < 0.01f);

        const ClientEq::BandParams b9 = c.eq().band(9);
        QCOMPARE(int(b9.type), int(ClientEq::FilterType::LowPass));
        QVERIFY(qAbs(b9.freqHz - 8819.87f) < 0.01f);

        // Zischdaempfer.
        QVERIFY(qAbs(c.deEss().frequencyHz() - 3836.18f) < 0.01f);
        QCOMPARE(c.deEss().q(), 5.0f);
        QCOMPARE(c.deEss().thresholdDb(), -34.5f);
        QVERIFY(qAbs(c.deEss().amountDb() - (-15.12f)) < 0.01f);
        QCOMPARE(c.deEss().slopeStages(), 2);

        // Kompressor, samt seinem eingebauten Begrenzer.
        QVERIFY(qAbs(c.comp().thresholdDb() - (-14.0404f)) < 0.001f);
        QVERIFY(qAbs(c.comp().ratio() - 3.09989f) < 0.001f);
        QVERIFY(qAbs(c.comp().kneeDb() - 9.96f) < 0.01f);
        QVERIFY(qAbs(c.comp().makeupDb() - 6.66f) < 0.01f);
        QCOMPARE(c.comp().limiterEnabled(), true);
        QVERIFY(qAbs(c.comp().limiterCeilingDb() - (-3.88f)) < 0.01f);

        // Roehre.
        QCOMPARE(int(c.tube().model()), int(ClientTube::Model::A));
        QVERIFY(qAbs(c.tube().driveDb() - 16.2f) < 0.01f);
        QVERIFY(qAbs(c.tube().tone() - 0.77f) < 0.01f);
        QVERIFY(qAbs(c.tube().dryWet() - 0.99f) < 0.01f);

        // Anreicherer („Enh" bei Aether).
        QCOMPARE(int(c.pudu().mode()), int(ClientPudu::Mode::Aphex));
        QVERIFY(qAbs(c.pudu().pooDriveDb() - 9.96f) < 0.01f);
        QCOMPARE(c.pudu().pooMix(), 1.0f);
        QVERIFY(qAbs(c.pudu().pooTuneHz() - 119.85f) < 0.01f);
        QVERIFY(qAbs(c.pudu().dooTuneHz() - 3971.65f) < 0.01f);
        QVERIFY(qAbs(c.pudu().dooMix() - 0.345f) < 0.001f);

        // Hall.
        QVERIFY(qAbs(c.reverb().mix() - 0.27f) < 0.001f);
        QVERIFY(qAbs(c.reverb().decayS() - 0.906f) < 0.001f);
        QVERIFY(qAbs(c.reverb().size() - 0.165f) < 0.001f);
        QCOMPARE(c.reverb().preDelayMs(), 20.0f);

        // Endbegrenzer.
        QVERIFY(qAbs(c.limiter().ceilingDb() - (-2.91262f)) < 0.001f);
        QVERIFY(qAbs(c.limiter().outputTrimDb() - (-3.96f)) < 0.01f);
        QCOMPARE(c.limiter().dcBlockEnabled(), true);
    }

    // ── Die Reihenfolge ─────────────────────────────────────────────
    void die_roehre_steht_vorne_wie_bei_ihm()
    {
        StripChain c;
        c.prepare(kRate);
        QVERIFY(AetherPreset::applyFile(profilPfad(), c).ok);

        // Seine Kette: Tube, Gate, Eq, Comp, DeEss, Enh, Reverb.
        // Davor unser Entrauschen (dort steht bei Aether rn2, vor der
        // Kette), dahinter Clipper und Begrenzer.
        const StripChain::Order soll = {
            StripChain::Stage::Denoise, StripChain::Stage::Tube,
            StripChain::Stage::Gate,    StripChain::Stage::Eq,
            StripChain::Stage::Comp,    StripChain::Stage::DeEss,
            StripChain::Stage::Pudu,    StripChain::Stage::Reverb,
            StripChain::Stage::Clip,    StripChain::Stage::Limiter };
        const StripChain::Order ist = c.stageOrder();
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            QCOMPARE(QString::fromLatin1(
                         StripChain::stageToken(ist[size_t(i)])),
                     QString::fromLatin1(
                         StripChain::stageToken(soll[size_t(i)])));
        }
    }

    // ── Was Aether nicht kennt, laeuft nicht mit ─────────────────────
    void nichts_laeuft_mit_das_er_in_aether_nie_hatte()
    {
        StripChain c;
        c.prepare(kRate);

        // Gegenprobe: alles vorher EIN, damit ein Importeur, der
        // einfach nichts tut, auffliegt.
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), true);
        }
        c.setInputAuto(true);
        c.comp().setAutoMakeup(true);
        c.comp().setDriveDb(6.0f);
        c.comp().setPhaseRotatorStages(4);

        QVERIFY(AetherPreset::applyFile(profilPfad(), c).ok);

        // rn2 ist in seiner Datei false.
        QCOMPARE(c.stageEnabled(StripChain::Stage::Denoise), false);
        QCOMPARE(c.stageEnabled(StripChain::Stage::Clip), false);
        QCOMPARE(c.inputAuto(), false);
        QCOMPARE(c.inputGainDb(), 0.0f);
        QCOMPARE(c.comp().autoMakeup(), false);
        QCOMPARE(c.comp().driveDb(), 0.0f);
        QCOMPARE(c.comp().phaseRotatorStages(), 0);

        // Und was in seiner Kette steht, laeuft.
        QCOMPARE(c.stageEnabled(StripChain::Stage::Tube), true);
        QCOMPARE(c.stageEnabled(StripChain::Stage::Gate), true);
        QCOMPARE(c.stageEnabled(StripChain::Stage::Eq), true);
        QCOMPARE(c.stageEnabled(StripChain::Stage::Comp), true);
        QCOMPARE(c.stageEnabled(StripChain::Stage::DeEss), true);
        QCOMPARE(c.stageEnabled(StripChain::Stage::Pudu), true);
        QCOMPARE(c.stageEnabled(StripChain::Stage::Reverb), true);
        QCOMPARE(c.stageEnabled(StripChain::Stage::Limiter), true);

        // Der Hauptschalter bleibt, wie er war -- ein Profil
        // entscheidet, WIE die Kette klingt, nicht OB sie im Sendeweg
        // steht.
        QCOMPARE(c.isEnabled(), false);
    }

    // ── Der Weg zurueck ─────────────────────────────────────────────
    void das_importierte_ueberlebt_speichern_und_laden()
    {
        StripChain c;
        c.prepare(kRate);
        QVERIFY(AetherPreset::applyFile(profilPfad(), c).ok);
        QVERIFY(StripSettings::saveUserPreset(QStringLiteral("AetherPruef"), c));

        StripChain z;
        z.prepare(kRate);
        QVERIFY(StripSettings::applyUserPreset(QStringLiteral("AetherPruef"), z));

        // Die Reihenfolge -- das war bis zum 2026-10-13 gar nicht
        // gespeichert.
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            QCOMPARE(int(z.stageOrder()[size_t(i)]),
                     int(c.stageOrder()[size_t(i)]));
        }
        // Die Betriebsart des Anreicherers -- die fehlte ebenfalls.
        QCOMPARE(int(z.pudu().mode()), int(c.pudu().mode()));

        QVERIFY(qFuzzyCompare(z.eq().masterGain(), c.eq().masterGain()));
        QCOMPARE(int(z.eq().filterFamily()), int(c.eq().filterFamily()));
        QCOMPARE(z.eq().activeBandCount(), c.eq().activeBandCount());
        QVERIFY(qFuzzyCompare(z.tube().driveDb(), c.tube().driveDb()));
        QVERIFY(qFuzzyCompare(z.comp().makeupDb(), c.comp().makeupDb()));
        QCOMPARE(z.comp().autoMakeup(), false);
        QCOMPARE(z.gate().lookaheadMs(), c.gate().lookaheadMs());

        QVERIFY(StripSettings::removeUserPreset(QStringLiteral("AetherPruef")));
    }

    // ── Werkzeug: das Profil in eine echte Einstellungsdatei ────────
    //
    // Kein Pruefstand, sondern der Weg, das Profil EINMAL in die
    // laufende Einstellung zu bringen, ohne im Fenster zu klicken.
    // Laeuft nur mit LONGPATH_AETHER_APPLY=1 und schreibt dorthin, wo
    // LONGPATH_CONFIG_DIR hinzeigt -- im CI also nie.
    void traegt_das_profil_in_die_einstellungen_ein()
    {
        if (qgetenv("LONGPATH_AETHER_APPLY") != QByteArray("1")) {
            QSKIP("LONGPATH_AETHER_APPLY nicht gesetzt");
        }
        const QString pfad = qEnvironmentVariable("LONGPATH_AETHER_FILE",
                                                  profilPfad());
        StripChain c;
        c.prepare(kRate);
        StripSettings::restore(c);          // auf dem, was schon da ist
        const auto e = AetherPreset::applyFile(pfad, c);
        QVERIFY2(e.ok, qPrintable(e.fehler));
        StripSettings::save(c);
        if (!e.name.isEmpty()) {
            StripSettings::saveUserPreset(e.name, c);
        }
        AppSettings::instance().save();
        qInfo("Datei: %s", qPrintable(AppSettings::instance().filePath()));
        for (const QString& h : e.hinweise) { qInfo("Hinweis: %s", qPrintable(h)); }
        qInfo("eingetragen: %s", qPrintable(e.name));
    }

    // ── Werkzeug: dieselbe Kette, der Glanz im Band ─────────────────
    //
    // Martins Aether-Entzerrer hebt mit einem Kuhschwanz bei 4982 Hz
    // um 2,63 dB an -- und der Kanal macht bei 4050 Schluss. Roh hoert
    // er den Glanz, die Gegenseite nie. Diese Fassung schiebt NUR
    // dieses eine Band nach unten, damit die Anhebung innerhalb der
    // Bandbreite liegt; alles andere bleibt, wie es ist. Ein Zug
    // ändert eine Größe, sonst ist der Vergleich nicht lesbar.
    //
    // Wird als EIGENE Kette abgelegt. Sein Grundstock bleibt
    // unangetastet -- „das ist die basis".
    void legt_eine_variante_mit_glanz_im_band_ab()
    {
        if (qgetenv("LONGPATH_AETHER_APPLY") != QByteArray("1")) {
            QSKIP("LONGPATH_AETHER_APPLY nicht gesetzt");
        }
        const QString pfad = qEnvironmentVariable("LONGPATH_AETHER_FILE",
                                                  profilPfad());
        const float neueEcke = qEnvironmentVariable(
            "LONGPATH_AETHER_SHELF", QStringLiteral("3400")).toFloat();

        StripChain c;
        c.prepare(kRate);
        const auto e = AetherPreset::applyFile(pfad, c);
        QVERIFY2(e.ok, qPrintable(e.fehler));

        int verschoben = -1;
        for (int i = 0; i < c.eq().activeBandCount(); ++i) {
            ClientEq::BandParams b = c.eq().band(i);
            if (b.type != ClientEq::FilterType::HighShelf || !b.enabled) {
                continue;
            }
            if (b.freqHz <= 4050.0f) { continue; }   // liegt schon im Band
            const float alt = b.freqHz;
            b.freqHz = neueEcke;
            c.eq().setBand(i, b);
            verschoben = i;
            qInfo("Kuhschwanz Band %d: %.0f Hz -> %.0f Hz (%+.2f dB)",
                  i, double(alt), double(neueEcke), double(b.gainDb));
            break;
        }
        QVERIFY2(verschoben >= 0, "kein Kuhschwanz über der Kanalgrenze");

        const QString name = (e.name.isEmpty() ? QStringLiteral("Aether")
                                               : e.name)
                             + QStringLiteral(" — Glanz im Band");
        QVERIFY(StripSettings::saveUserPreset(name, c));
        AppSettings::instance().save();
        qInfo("abgelegt als: %s", qPrintable(name));
        qInfo("Datei: %s", qPrintable(AppSettings::instance().filePath()));
    }

    // ── Werkzeug: die 4-kHz-Fassung ─────────────────────────────────
    //
    // Zwei Griffe aus seinem Profil arbeiten über der Kanalgrenze und
    // richten dort nichts aus. Gemessen an echter Sprache durch
    // 50–4050 (tst_bandbreiten_genau::whatHelpsInsideTheFourKilohertz):
    //
    //     PUDU „doo" 3972 → 2800 Hz   +1,05 dB Glanz
    //     Kuhschwanz 4982 → 3400 Hz   +0,30 dB
    //
    // Der Anreicherer erzeugt seine Oberwellen OBERHALB der
    // Einstellfrequenz; bei 3972 Hz liegen sie durchweg draußen.
    //
    // Wird als EIGENE Kette abgelegt, sein Grundstock bleibt
    // unangetastet.
    void legt_die_vierkilohertz_fassung_ab()
    {
        if (qgetenv("LONGPATH_AETHER_APPLY") != QByteArray("1")) {
            QSKIP("LONGPATH_AETHER_APPLY nicht gesetzt");
        }
        const QString pfad = qEnvironmentVariable("LONGPATH_AETHER_FILE",
                                                  profilPfad());
        StripChain c;
        c.prepare(kRate);
        const auto e = AetherPreset::applyFile(pfad, c);
        QVERIFY2(e.ok, qPrintable(e.fehler));

        bool shelf = false;
        for (int i = 0; i < c.eq().activeBandCount(); ++i) {
            ClientEq::BandParams b = c.eq().band(i);
            if (b.type != ClientEq::FilterType::HighShelf || !b.enabled) {
                continue;
            }
            if (b.freqHz <= 4050.0f) { continue; }
            qInfo("Kuhschwanz Band %d: %.0f -> 3400 Hz (%+.2f dB)",
                  i, double(b.freqHz), double(b.gainDb));
            b.freqHz = 3400.0f;
            c.eq().setBand(i, b);
            shelf = true;
            break;
        }
        QVERIFY2(shelf, "kein Kuhschwanz über der Kanalgrenze");

        qInfo("PUDU doo: %.0f -> 2800 Hz", double(c.pudu().dooTuneHz()));
        c.pudu().setDooTuneHz(2800.0f);

        const QString name = (e.name.isEmpty() ? QStringLiteral("Aether")
                                               : e.name)
                             + QStringLiteral(" — 4 kHz");
        QVERIFY(StripSettings::saveUserPreset(name, c));
        AppSettings::instance().save();
        qInfo("abgelegt als: %s", qPrintable(name));
    }

    // ── Werkzeug: die ruhige Fassung ────────────────────────────────
    //
    // „alle profile sehr laut mit starken brummen" (2026-10-14).
    //
    // Die Kette kann Brummen nicht erfinden, aber sein Profil hebt es
    // an — zweimal an derselben Stelle: Hochpass 62,7 Hz mit Güte
    // 2,531 (eine Güte über 0,707 ist bei einem Hochpass kein Schnitt,
    // sondern eine Spitze AUF der Ecke) und dazu eine Glocke bei
    // 68,95 Hz mit +5,71 dB. Gemessen
    // (tst_bandbreiten_genau::howMuchHisProfileLiftsMainsHum):
    //
    //     50 Hz   +23,3 dB      1 kHz   +7,4 dB
    //
    // Also sechzehn Dezibel Brumm MEHR als Stimme. Diese Fassung
    // setzt den Hochpass auf 100 Hz mit Güte 0,707 und die Anhebung
    // auf null; alles andere bleibt seins.
    void legt_die_ruhige_fassung_ab()
    {
        if (qgetenv("LONGPATH_AETHER_APPLY") != QByteArray("1")) {
            QSKIP("LONGPATH_AETHER_APPLY nicht gesetzt");
        }
        const QString pfad = qEnvironmentVariable("LONGPATH_AETHER_FILE",
                                                  profilPfad());
        StripChain c;
        c.prepare(kRate);
        const auto e = AetherPreset::applyFile(pfad, c);
        QVERIFY2(e.ok, qPrintable(e.fehler));

        int geaendert = 0;
        for (int i = 0; i < c.eq().activeBandCount(); ++i) {
            ClientEq::BandParams b = c.eq().band(i);
            if (b.type == ClientEq::FilterType::HighPass && b.enabled) {
                qInfo("Hochpass Band %d: %.1f Hz Güte %.3f -> 100 Hz Güte "
                      "0,707", i, double(b.freqHz), double(b.q));
                b.freqHz = 100.0f;
                b.q = 0.707f;
                c.eq().setBand(i, b);
                ++geaendert;
            } else if (b.type == ClientEq::FilterType::Peak && b.enabled
                       && b.freqHz < 150.0f && b.gainDb > 0.5f) {
                qInfo("Glocke Band %d: %.1f Hz %+.2f dB -> 0 dB",
                      i, double(b.freqHz), double(b.gainDb));
                b.gainDb = 0.0f;
                c.eq().setBand(i, b);
                ++geaendert;
            }
        }
        QVERIFY2(geaendert >= 1, "nichts im Brummband gefunden");

        const QString name = (e.name.isEmpty() ? QStringLiteral("Aether")
                                               : e.name)
                             + QStringLiteral(" — ruhig");
        QVERIFY(StripSettings::saveUserPreset(name, c));
        AppSettings::instance().save();
        qInfo("abgelegt als: %s", qPrintable(name));
    }

    // ── Was eine fremde Datei alles anrichten könnte ────────────────
    //
    // Ein Profil kommt aus einem anderen Programm, womöglich aus einer
    // neueren Fassung davon. Die Fälle hier sind nicht erfunden: sie
    // sind die Stellen, an denen ein Importeur still etwas Falsches
    // tut, statt zu sagen, dass er nicht kann.
    void eine_kette_aus_lauter_unbekannten_namen_wird_abgelehnt()
    {
        StripChain c;
        c.prepare(kRate);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), true);
        }

        const QJsonObject fremd{
            { QStringLiteral("schema"), QLatin1String(AetherPreset::kSchema) },
            { QStringLiteral("chain"), QJsonArray{ QStringLiteral("Vocoder"),
                                                   QStringLiteral("Exciter9") } } };
        const auto e = AetherPreset::apply(fremd, c);
        QVERIFY2(!e.ok, "eine Kette aus lauter Unbekanntem wurde angenommen");
        QVERIFY(e.fehler.contains(QStringLiteral("Vocoder")));

        // Und die Kette steht unangetastet da -- nicht lautlos leer.
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            QVERIFY2(c.stageEnabled(static_cast<StripChain::Stage>(i)),
                     qPrintable(QStringLiteral("Stufe %1 wurde abgeschaltet")
                                    .arg(i)));
        }
    }

    void ein_entzerrer_ohne_baender_laesst_die_kurve_stehen()
    {
        StripChain c;
        c.prepare(kRate);
        c.eq().setActiveBandCount(6);
        ClientEq::BandParams merk = c.eq().band(0);
        merk.freqHz = 123.0f;
        merk.gainDb = 4.5f;
        c.eq().setBand(0, merk);

        const QJsonObject profil{
            { QStringLiteral("schema"), QLatin1String(AetherPreset::kSchema) },
            { QStringLiteral("chain"), QJsonArray{ QStringLiteral("Eq") } },
            { QStringLiteral("eq"), QJsonObject{
                  { QStringLiteral("enabled"), true },
                  { QStringLiteral("activeBandCount"), 10 },
                  { QStringLiteral("bands"), QJsonArray{} } } } };
        const auto e = AetherPreset::apply(profil, c);
        QVERIFY2(e.ok, qPrintable(e.fehler));

        // Die Anzahl darf NICHT auf 10 springen: hinter den Plätzen
        // 6..9 stünden Werte aus einer ganz anderen Einstellung.
        QCOMPARE(c.eq().activeBandCount(), 6);
        QVERIFY(qFuzzyCompare(c.eq().band(0).freqHz, 123.0f));
        bool gesagt = false;
        for (const QString& h : e.hinweise) {
            if (h.contains(QStringLiteral("keine Bänder"))) { gesagt = true; }
        }
        QVERIFY2(gesagt, "der Importeur schweigt darüber");
    }

    void ein_name_zweimal_in_der_kette_zaehlt_einmal()
    {
        StripChain c;
        c.prepare(kRate);
        const QJsonObject profil{
            { QStringLiteral("schema"), QLatin1String(AetherPreset::kSchema) },
            { QStringLiteral("chain"), QJsonArray{ QStringLiteral("Tube"),
                                                   QStringLiteral("Gate"),
                                                   QStringLiteral("Tube") } },
            { QStringLiteral("tube"), QJsonObject{
                  { QStringLiteral("enabled"), true },
                  { QStringLiteral("driveDb"), 9.0 } } },
            { QStringLiteral("gate"), QJsonObject{
                  { QStringLiteral("enabled"), true } } } };
        const auto e = AetherPreset::apply(profil, c);
        QVERIFY2(e.ok, qPrintable(e.fehler));

        // Jede Stufe genau einmal, sonst hätte setStageOrder abgelehnt.
        std::array<int, StripChain::kStageCount> zaehler{};
        for (const StripChain::Stage st : c.stageOrder()) {
            ++zaehler[size_t(st)];
        }
        for (int n : zaehler) { QCOMPARE(n, 1); }
        QCOMPARE(static_cast<int>(c.stageOrder()[1]),
                 static_cast<int>(StripChain::Stage::Tube));
        QVERIFY(qFuzzyCompare(c.tube().driveDb(), 9.0f));
    }

    // ── Was keine Kanalzugdatei ist, kommt nicht hinein ──────────────
    void ein_fremdes_json_wird_abgelehnt()
    {
        StripChain c;
        c.prepare(kRate);
        c.eq().setMasterGain(1.0f);

        const QJsonObject fremd{ { QStringLiteral("schema"),
                                   QStringLiteral("irgendwas v9") },
                                 { QStringLiteral("eq"), QJsonObject{
                                       { QStringLiteral("masterGain"), 4.0 } } } };
        const auto e = AetherPreset::apply(fremd, c);
        QVERIFY(!e.ok);
        QVERIFY(!e.fehler.isEmpty());
        // Und nichts ist angefasst worden.
        QCOMPARE(c.eq().masterGain(), 1.0f);
    }
};

QTEST_MAIN(TstAetherProfil)
#include "tst_aether_profil.moc"
