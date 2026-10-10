// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_profil_schluessel.cpp  (Longpath)
// =================================================================
// Ein Profil ist erst dann ein Profil, wenn es den Weg durch die
// Einstellungen überlebt. Hier wird die Empfehlung angewandt,
// gespeichert, in eine frische Kette zurückgelesen und Wert für Wert
// verglichen -- und dabei jeder geschriebene Schlüssel ins Protokoll
// gelegt, damit man sieht, was eine gespeicherte Kette ausmacht.
//
// Modification history (Longpath):
//   2026-10-10 — Original für Longpath von Martin Fischer,
//                 KI-gestützt über Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

using namespace Longpath;

class TstProfilSchluessel : public QObject {
    Q_OBJECT

private slots:
    void theRecommendationSurvivesTheSettings()
    {
        StripChain a;
        a.prepare(48000.0);
        QVERIFY(StripSettings::applyBuiltIn(
            QStringLiteral("Recommended — speech"), a));
        StripSettings::save(a);

        // Alles, was dabei geschrieben wurde -- in der Reihenfolge,
        // in der es in der Datei steht.
        auto& s = AppSettings::instance();
        const QStringList alle = s.allKeys();
        int n = 0;
        for (const QString& k : alle) {
            if (!k.startsWith(QLatin1String(StripSettings::kPrefix))) {
                continue;
            }
            qInfo().noquote() << QStringLiteral("SCHLUESSEL %1 = %2")
                                     .arg(k, s.value(k).toString());
            ++n;
        }
        qInfo().noquote() << QStringLiteral("%1 Schlüssel geschrieben").arg(n);
        QVERIFY2(n > 40, "das Profil schreibt zu wenige Schlüssel");

        StripChain b;
        b.prepare(48000.0);
        StripSettings::restore(b);

        QCOMPARE(b.nr().amount(),            a.nr().amount());
        QCOMPARE(b.gate().thresholdDb(),     a.gate().thresholdDb());
        QCOMPARE(b.comp().thresholdDb(),     a.comp().thresholdDb());
        QCOMPARE(b.comp().ratio(),           a.comp().ratio());
        QCOMPARE(b.comp().autoMakeup(),      a.comp().autoMakeup());
        QCOMPARE(b.deEss().thresholdDb(),    a.deEss().thresholdDb());
        QCOMPARE(b.clipper().driveDb(),      a.clipper().driveDb());
        QCOMPARE(b.limiter().ceilingDb(),    a.limiter().ceilingDb());
        QCOMPARE(b.inputAuto(),              a.inputAuto());
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            const auto st = static_cast<StripChain::Stage>(i);
            QVERIFY2(b.stageEnabled(st) == a.stageEnabled(st),
                     qPrintable(QStringLiteral("Stufe %1 überlebt nicht")
                                    .arg(QString::fromLatin1(
                                        StripChain::stageName(st)))));
        }
    }
};

QTEST_MAIN(TstProfilSchluessel)
#include "tst_profil_schluessel.moc"
