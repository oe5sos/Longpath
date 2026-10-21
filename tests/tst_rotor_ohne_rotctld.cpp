// no-port-check: Longpath-eigener Prüfstand; kein Port fremder Logik.
// =================================================================
// tests/tst_rotor_ohne_rotctld.cpp  (Longpath)
// =================================================================
//
// Der eigene GS-232-Treiber braucht kein rotctld — und darf auch
// keines starten.
//
// Gemessen am 2026-10-21 am Gerät: die App meldete
// „Rotator connected — GS-232 über 172.30.30.16:4001" UND legte
// daneben einen rotctld-Prozess auf denselben Netzserver. Zwei Leser
// auf einer Leitung, und weil `RotctldProcess` bei jedem Start einen
// neuen Port vergibt, sammelten sich an diesem Tag drei rotctld-
// Instanzen an, von denen zwei nicht mehr antworteten.
//
// Die Ursache: `ensureRotor()` wertet `Rotor/Backend` aus,
// `connectRotorFromSettings()` tat es nicht und fuhr den rotctld-Zweig
// trotzdem. Dieser Lauf hält beide Richtungen fest.
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Neuer Prüfstand, nachdem der Fehler am Gerät auffiel.
//                Longpath, KI-gestützt via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "core/AppSettings.h"
#include "gui/widgets/RotorLogbookPanel.h"

using namespace Longpath;

class TestRotorOhneRotctld : public QObject {
    Q_OBJECT

private:
    static void setzeBackend(const QString& wert)
    {
        AppSettings::instance().setValue(QStringLiteral("Rotor/Backend"), wert);
    }

private slots:
    void initTestCase()
    {
        // Ein Host, der sicher niemandem gehört: die Verbindung darf
        // scheitern, es geht hier um den WEG, nicht um das Ankommen.
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Rotor/Host"), QStringLiteral("127.0.0.1"));
        s.setValue(QStringLiteral("Rotor/NetPort"), QStringLiteral("4001"));
        // KEIN serielles Geraet eingerichtet. Das ist der ganze Trick
        // dieses Laufs: der rotctld-Weg kann so nicht verbinden und sagt
        // das auch -- wer trotzdem verbindet, ist am eigenen Treiber
        // vorbeigekommen.
        s.setValue(QStringLiteral("RotorUseLocalSerial"), QStringLiteral("True"));
        s.setValue(QStringLiteral("RotorSerialDevice"), QString{});
    }

    void gs232_geht_am_rotctld_vorbei()
    {
        // Der erste Entwurf dieses Laufs prüfte nur, dass kein
        // rotctld-Prozess läuft -- und war mit der ALTEN Fassung
        // ebenso grün, weil der rotctld-Zweig ohne eingerichtetes
        // Gerät schon vor dem Start abbricht. Die Gegenprobe hat ihn
        // entlarvt. Beobachtbar ist nicht, ob ein Prozess fehlt,
        // sondern DASS trotz fehlendem Gerät verbunden wird: nur der
        // eigene Treiber kommt ohne aus.
        setzeBackend(QStringLiteral("gs232"));
        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        QString err;
        const bool ok = panel.connectRotorFromSettings(&err);

        QVERIFY2(ok, qPrintable(QStringLiteral(
                     "gs232 braucht kein serielles Geraet, meldete aber: %1")
                     .arg(err)));
        QVERIFY2(!panel.rotctldRunningForTest(),
                 "mit Rotor/Backend=gs232 darf kein rotctld laufen");
    }

    void rotctld_weg_bleibt_erhalten()
    {
        // Gegenprobe in die andere Richtung: der Umbau darf den alten
        // Weg nicht gekappt haben. Ohne eingerichtetes Gerät meldet er
        // sich mit genau diesem Grund — das belegt, dass der Zweig
        // betreten wurde, ohne dass ein Gerät am Tisch liegen muss.
        setzeBackend(QStringLiteral("rotctld"));
        RotorLogbookPanel panel(nullptr, nullptr, nullptr);
        QString err;
        const bool ok = panel.connectRotorFromSettings(&err);

        QVERIFY2(!ok, "ohne Gerät kann der rotctld-Weg nicht verbinden");
        QVERIFY2(err.contains(QStringLiteral("device"), Qt::CaseInsensitive),
                 qPrintable(QStringLiteral("unerwarteter Grund: %1").arg(err)));
        QVERIFY2(!panel.rotctldRunningForTest(),
                 "ein gescheiterter Start darf nichts laufen lassen");
    }

    void cleanupTestCase()
    {
        setzeBackend(QStringLiteral("rotctld"));
    }
};

QTEST_MAIN(TestRotorOhneRotctld)
#include "tst_rotor_ohne_rotctld.moc"
