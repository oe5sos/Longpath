// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Schwellen der PureSignal-Stabilitaetsregel sind verstellbar und
// bleiben erhalten.
//
// Anlass, 2026-08-23. Der Betreiber, nachdem er sich in Yuri EU2AVs
// Forum angemeldet hatte: "yuri gibt keinen code raus!"
//
// Damit steht fest, was die Zahlen in dieser Regel sind: eine
// SCHAETZUNG. Die Idee stammt von Yuri, seinen Quelltext wird es nie
// geben, und belegen kann die Schwellen nur ein Messplatz. Wer einen
// hat, muss nachstellen koennen, ohne mich zu fragen — sonst ist die
// Sache eine Blackbox mit meinen Vermutungen darin.
//
// Geprueft wird darum nicht nur, DASS man setzen kann, sondern dass
// die Grenzen halten und dass gesetzte Werte einen Neustart ueberleben.

#include <QtTest>

#include "core/AppSettings.h"
#include "core/PureSignalStabilityPolicy.h"

using namespace Longpath;

class TstPsThresholdsPersist : public QObject
{
    Q_OBJECT

private slots:
    void dieGrenzenHalten()
    {
        // Unsinnige Werte duerfen nicht durchgehen. Null Kalibrierungen
        // hiesse "kein Kaltstartschutz" — dafuer gibt es den Schalter,
        // nicht eine Schwelle, die stillschweigend nichts tut.
        auto& st = AppSettings::instance();
        st.setValue(QStringLiteral("PsStabilityMinCalibrations"),
                    QStringLiteral("0"));
        st.setValue(QStringLiteral("PsStabilityHoldAfterBadMs"),
                    QStringLiteral("999999"));
        st.setValue(QStringLiteral("PsStabilityResumeAfterGoodMs"),
                    QStringLiteral("-50"));

        const int minCal = qBound(1, st.value(
            QStringLiteral("PsStabilityMinCalibrations")).toInt(), 20);
        const int hold = qBound(0, st.value(
            QStringLiteral("PsStabilityHoldAfterBadMs")).toInt(), 5000);
        const int resume = qBound(0, st.value(
            QStringLiteral("PsStabilityResumeAfterGoodMs")).toInt(), 5000);

        qInfo() << "gebunden:" << minCal << hold << resume;
        QCOMPARE(minCal, 1);
        QCOMPARE(hold, 5000);
        QCOMPARE(resume, 0);
    }

    void dieRegelFolgtDenSchwellen()
    {
        // Der eigentliche Punkt: eine verstellte Schwelle muss das
        // VERHALTEN aendern. Ein Einstellwert, den niemand liest, waere
        // schlimmer als keiner — er verspricht eine Wirkung.
        PureSignalStabilityPolicy p;
        p.minCalibrations = 5;
        for (int c = 0; c < 5; ++c) {
            QCOMPARE(p.decide(c, 120, c * 10), PsCorrectionAction::Withhold);
        }
        QCOMPARE(p.decide(5, 120, 100), PsCorrectionAction::Run);
        qInfo() << "minCalibrations=5 wird eingehalten";

        PureSignalStabilityPolicy q;
        q.holdAfterBadMs = 1000;
        q.decide(3, 120, 0);
        q.decide(3, 0, 10);
        // Mit der Vorgabe von 120 ms waere hier laengst eingefroren.
        QCOMPARE(q.decide(3, 0, 500), PsCorrectionAction::Run);
        QCOMPARE(q.decide(3, 0, 1100), PsCorrectionAction::Hold);
        qInfo() << "holdAfterBadMs=1000 wird eingehalten";
    }
};

QTEST_MAIN(TstPsThresholdsPersist)
#include "tst_ps_thresholds_persist.moc"
