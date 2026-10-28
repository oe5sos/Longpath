// =================================================================
// tools/begleitgeraet_probe.cpp  (Longpath)
// =================================================================
//
// WERKZEUG, kein Programmteil.
//
// Die ganze Kette gegen ein ECHTES rigctld, nicht gegen ein
// gefaelschtes: Hamlibs Dummy-Geraet (Modell 1) laeuft als rigctld,
// CompanionRigController verbindet sich darauf, liest die Frequenz,
// und der Abstimmbefehl geht denselben Weg zurueck.
//
// Warum das neben den Pruefstaenden steht: tst_rigctld_client prueft
// gegen einen selbstgebauten Server, der genau das antwortet, was
// Longpath erwartet. Ein Rundlauf gegen das echte rigctld faengt, was
// so ein Server nicht faengt -- Zeilenenden, Reihenfolge, Begruessung,
// die Form der RPRT-Antworten. Dieselbe Lehre wie beim Sender gegen den
// eigenen Empfaenger.
//
// Braucht kein Funkgeraet. Startet rigctld selbst, auf Loopback, mit
// Hamlibs Dummy -- was danach auf dem Anschluss haengt, ist niemandes
// Hardware.
//
// Bauen und laufen:
//   cmake --build build --target begleitgeraet_probe
//   ./build/begleitgeraet_probe
//
// =================================================================
// Modification history (Longpath):
//   2026-10-28 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QElapsedTimer>
#include <QDir>
#include <QEventLoop>
#include <QApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include "core/AppSettings.h"
#include "core/CompanionRigController.h"
#include "core/TransverterSetup.h"
#include "gui/SpectrumWidget.h"
#include "gui/meters/VfoDisplayItem.h"

using namespace Longpath;

namespace {

QTextStream& aus()
{
    static QTextStream s(stdout);
    return s;
}

/// Wartet, bis `pruefe()` wahr ist, hoechstens `msMax` lang. Gibt
/// zurueck, ob es geklappt hat.
template <typename F>
bool warteAuf(F pruefe, int msMax)
{
    QElapsedTimer uhr;
    uhr.start();
    while (!pruefe() && uhr.elapsed() < msMax) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
    return pruefe();
}

} // namespace

int main(int argc, char** argv)
{
    // Offscreen, immer. Eine Sonde mit echtem Fenster lief am
    // 2026-09-30 sichtbar ueber Martins Schirm.
    qputenv("QT_QPA_PLATFORM", "offscreen");

    // Eigener Einstellungsordner. HOME= isoliert auf macOS NICHT (die
    // Lehre vom 2026-10-15: die echte Datei wurde ueberschrieben);
    // LONGPATH_CONFIG_DIR tut es.
    QTemporaryDir konfig;
    if (!konfig.isValid()) {
        aus() << "kein Ordner fuer die Einstellungen\n";
        return 1;
    }
    qputenv("LONGPATH_CONFIG_DIR", konfig.path().toUtf8());

    QApplication app(argc, argv);

    // ── rigctld mit Hamlibs Dummy-Geraet ────────────────────────────
    //
    // Modell 1 ist "Hamlib Dummy": kein Anschluss, keine Hardware,
    // antwortet aber wie ein Funkgeraet. Port 4599, nicht 4532 --
    // dort koennte Longpaths eigener rigctld-Server stehen, und dann
    // befragte die Sonde das Programm statt das Geraet.
    constexpr quint16 kPort = 4599;

    QProcess rig;
    rig.start(QStringLiteral("rigctld"),
              {QStringLiteral("-m"), QStringLiteral("1"),
               QStringLiteral("-T"), QStringLiteral("127.0.0.1"),
               QStringLiteral("-t"), QString::number(kPort)});
    if (!rig.waitForStarted(3000)) {
        aus() << "rigctld startet nicht: " << rig.errorString() << "\n";
        return 1;
    }
    QThread::msleep(400);   // lauschen lassen

    // ── Einstellungen, als haette der Bediener sie gesetzt ──────────
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("CompanionRigEnabled"), QStringLiteral("True"));
    s.setValue(QStringLiteral("CompanionRigHost"), QStringLiteral("127.0.0.1"));
    s.setValue(QStringLiteral("CompanionRigPort"), kPort);
    s.setValue(QStringLiteral("CompanionRigStartDaemon"), QStringLiteral("False"));
    s.setValue(QStringLiteral("CompanionRigName"), QStringLiteral("Hamlib Dummy"));

    TransverterSetup tv;
    tv.enabled  = true;
    tv.ifLowHz  = 28000000;
    tv.ifHighHz = 30000000;
    tv.offsetHz = 116000000;
    tv.save();

    // ── Die Kette ───────────────────────────────────────────────────
    CompanionRigController regler;

    QObject::connect(&regler, &CompanionRigController::problem,
                     [](const QString& t) { aus() << "  Klage: " << t << "\n"; });

    aus() << "Sandkasten?                " << (AppSettings::isSandbox() ? "ja" : "NEIN")
          << "\n";
    aus() << "darf von selbst verbinden? "
          << (AppSettings::darfVonSelbstVerbinden() ? "ja" : "nein") << "\n";

    // starteWennGewuenscht() verbindet in einem isolierten Lauf NICHT
    // -- das ist die Zusage, und hier wird sie gleich mitgeprueft.
    regler.starteWennGewuenscht();
    const bool vonSelbst = regler.client() != nullptr;
    aus() << "von selbst verbunden?      " << (vonSelbst ? "JA (falsch!)" : "nein")
          << "\n";

    // Von Hand -- so wie der Bediener auf "Test" drueckt.
    regler.verbinde();
    if (!warteAuf([&] { return regler.verbunden(); }, 6000)) {
        aus() << "keine Verbindung zum rigctld\n";
        rig.kill(); rig.waitForFinished(2000);
        return 1;
    }
    aus() << "verbunden.\n";

    if (!warteAuf([&] { return regler.zfFrequenzHz() > 0; }, 6000)) {
        aus() << "keine Frequenz gelesen\n";
        rig.kill(); rig.waitForFinished(2000);
        return 1;
    }

    aus() << "\nVom echten rigctld gelesen:\n";
    aus() << "  ZF        " << regler.zfFrequenzHz() << " Hz\n";
    aus() << "  Antenne   " << regler.rfFrequenzHz() << " Hz\n";
    aus() << "  Modus     " << regler.modus() << "\n";
    aus() << "  Zeile     " << regler.beschreibung() << "\n";

    // ── Abstimmen, und nachsehen ob es ankam ────────────────────────
    const qint64 ziel_zf = 28300000;
    aus() << "\nAbstimmen auf " << ziel_zf << " Hz ZF (= "
          << TransverterSetup::displayHz(ziel_zf) << " Hz Antenne)\n";
    regler.stimmeAbZf(ziel_zf);

    const bool angekommen =
        warteAuf([&] { return regler.zfFrequenzHz() == ziel_zf; }, 6000);
    aus() << "  angekommen? " << (angekommen ? "ja" : "NEIN") << "\n";
    aus() << "  jetzt       " << regler.zfFrequenzHz() << " Hz ZF, "
          << regler.rfFrequenzHz() << " Hz Antenne\n";

    rig.kill();
    rig.waitForFinished(2000);

    if (!angekommen) {
        aus() << "\nDer Abstimmbefehl kam nicht an.\n";
        return 1;
    }

    // ── Und wie es aussieht ─────────────────────────────────────────
    //
    // Zwei Bilder aus dem ECHTEN Malcode, offscreen: die grosse
    // VFO-Zahl mit dem TRV-Schild, und der Panadapter mit Marke,
    // Frequenzachse und Bandplan. Ein Lauf der sichtbaren App waere
    // hier die zweite Instanz auf dem Schirm des Betreibers -- und
    // zwei Instanzen sehen aus wie ein Haenger.
    {
        VfoDisplayItem vfo;
        vfo.setRect(0.0f, 0.0f, 1.0f, 1.0f);
        vfo.setFrequency(ziel_zf);
        vfo.setModeLabel(QStringLiteral("USB"));
        vfo.setFilterLabel(QStringLiteral("2.7k"));
        vfo.setBandLabel(QStringLiteral("2m"));

        QImage bild(420, 90, QImage::Format_ARGB32_Premultiplied);
        bild.fill(QColor(0x0a, 0x0a, 0x12));
        QPainter p(&bild);
        vfo.paint(p, bild.width(), bild.height());
        p.end();
        const QString weg = QStringLiteral("%1/vfo-mit-transverter.png")
                                .arg(QDir::tempPath());
        bild.save(weg);
        aus() << "\nBild 1 (VFO-Zahl + TRV): " << weg << "\n";
    }
    {
        SpectrumWidget sw;
        sw.setFrequencyRange(28300000.0, 200000.0);
        sw.setCompanionMarker(true, ziel_zf);

        QImage bild(900, 260, QImage::Format_ARGB32_Premultiplied);
        bild.fill(QColor(0x0a, 0x0a, 0x12));
        QPainter p(&bild);
        sw.drawBandPlanForTest(p, bild.rect());
        sw.drawCompanionMarkerForTest(p, bild.rect());
        p.end();
        const QString weg = QStringLiteral("%1/panadapter-mit-marke.png")
                                .arg(QDir::tempPath());
        bild.save(weg);
        aus() << "Bild 2 (Marke im Panadapter): " << weg << "\n";
    }

    aus() << "\nAlles durch -- die Kette haelt gegen ein echtes rigctld.\n";
    return 0;
}
