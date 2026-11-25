// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen. Live-Messung der Mithoerer-Verbindung gegen 1-2 oeffentliche
// Kiwis (2026-11-25), OHNE die Anwendung, ohne Funkgeraet, ohne Scheibe.
//
//   cmake --build build --target kiwi_live_probe
//   LONGPATH_CONFIG_DIR=$(mktemp -d) ./build/kiwi_live_probe JN77 14205000 [Sekunden]
//
// Ein Verzeichnisabruf (wie ein Klick), die zwei naechsten freien Kiwis mit
// erlaubter Schnittstelle, je EIN Kanal als Mithoerer (setProfileListener,
// keine Zuordnung, kein Ton), danach alles trennen.
#include "core/AppSettings.h"
#include "core/KiwiOwnSignal.h"
#include "core/KiwiPublicDirectory.h"
#include "core/KiwiSdrManager.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QUrl>
#include <QHash>
#include <QTextStream>

#include <algorithm>
#include <cstdio>
#include <memory>

using namespace Longpath;

namespace {
class OpenStore final : public IKiwiSdrCredentialStore {
public:
    bool isPersistent() const override { return false; }
    void read(const QString&, QObject* ctx, Callback cb) override
    {
        QTimer::singleShot(0, ctx, [cb]() { KiwiSdrCredentialResult r; cb(r); });
    }
    void write(const QString&, const QString&, QObject*, Callback) override {}
    void remove(const QString&, QObject*, Callback) override {}
};
struct Stat { int rows{0}; double lowMhz{0}, highMhz{0}; QVector<float> last; qint64 firstMs{-1}; };
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    const QString grid = argc > 1 ? QString::fromLatin1(argv[1]) : QStringLiteral("JN77");
    const double freqHz = argc > 2 ? QString::fromLatin1(argv[2]).toDouble() : 14205000.0;
    const int seconds = argc > 3 ? QString::fromLatin1(argv[3]).toInt() : 30;

    KiwiPublicDirectory dir;
    QObject::connect(&dir, &KiwiPublicDirectory::failed, &app, [&](const QString& e) {
        out << "VERZEICHNIS FEHLER: " << e << Qt::endl; app.exit(2);
    });
    QObject::connect(&dir, &KiwiPublicDirectory::ready, &app,
                     [&](const QVector<KiwiPublicReceiver>& all) {
        out << "Verzeichnis: " << all.size() << " Eintraege" << Qt::endl;
        QVector<KiwiCandidate> c = kiwiPreselect(all, grid, freqHz);
        QVector<KiwiCandidate> pick;
        for (const KiwiCandidate& k : c) {
            if (k.km >= 100.0 && k.km < 3000.0 && k.receiver.users == 0 && pick.size() < 2) {
                pick.append(k);
            }
        }
        if (pick.isEmpty()) { out << "kein passender Kiwi" << Qt::endl; app.exit(3); return; }

        auto* mgr = new KiwiSdrManager(&app, std::make_shared<OpenStore>());
        auto stats = std::make_shared<QHash<QString, Stat>>();
        auto timer = std::make_shared<QElapsedTimer>();
        timer->start();
        QStringList ids;
        for (const KiwiCandidate& k : pick) {
            QString ep = QUrl(k.receiver.url).host();
            if (QUrl(k.receiver.url).port() > 0) { ep += ":" + QString::number(QUrl(k.receiver.url).port()); }
            const QString id = mgr->addProfile(k.receiver.name.left(24), ep);
            ids << id;
            out << "gewaehlt: " << k.receiver.name << "  " << ep << "  " << qRound(k.km)
                << " km  users " << k.receiver.users << "/" << k.receiver.usersMax
                << "  extApi " << k.receiver.extApi << "  bands " << k.receiver.bands << Qt::endl;
        }
        QObject::connect(mgr, &KiwiSdrManager::profileStateChanged, &app,
            [&out, timer, mgr](const QString& id, KiwiSdrClient::State s, const QString& d) {
            out << "  [" << timer->elapsed() / 1000.0 << " s] " << mgr->displayName(id)
                << " Zustand " << int(s) << " " << d << Qt::endl;
        });
        QObject::connect(mgr, &KiwiSdrManager::waterfallRowReady, &app,
            [stats, timer](const QString& id, const QString&, const QVector<float>& b,
                           double lo, double hi, quint32) {
            Stat& st = (*stats)[id];
            if (st.firstMs < 0) { st.firstMs = timer->elapsed(); }
            ++st.rows; st.lowMhz = lo; st.highMhz = hi; st.last = b;
        });
        bool anyAudio = false;
        QObject::connect(mgr, &KiwiSdrManager::decodedAudioReady, &app,
                         [&anyAudio](const QString&, const QByteArray&) { anyAudio = true; });

        for (const QString& id : ids) {
            mgr->setProfileListener(id, true);
            mgr->primeProfileTracking(id, 900000 + int(&id - &ids[0]), freqHz / 1e6,
                                      QStringLiteral("USB"), 300, 2700,
                                      QStringLiteral("kiwi-group"), freqHz / 1e6, 0.040,
                                      0, QString(), 0);
        }
        QTimer::singleShot(seconds * 1000, &app, [&, mgr, ids, stats, timer]() {
            out << "\n== Ergebnis nach " << seconds << " s ==" << Qt::endl;
            for (const QString& id : ids) {
                const Stat& st = (*stats)[id];
                out << mgr->displayName(id) << ": Zeilen " << st.rows
                    << ", erste nach " << st.firstMs / 1000.0 << " s, Bereich "
                    << st.lowMhz << ".." << st.highMhz << " MHz";
                if (!st.last.isEmpty()) {
                    QVector<float> s = st.last;
                    std::nth_element(s.begin(), s.begin() + s.size() / 2, s.end());
                    out << ", Bins " << st.last.size() << ", Median " << s[s.size() / 2] << " dBm"
                        << ", Max " << *std::max_element(st.last.begin(), st.last.end()) << " dBm";
                }
                out << ", Zuordnung (Scheibe): " << mgr->assignedSliceForProfile(id)
                    << ", Zustand " << int(mgr->state(id)) << Qt::endl;
            }
            out << "Ton in die Mischung/Dekodierung ohne Mitschnitt: " << (anyAudio ? "JA" : "nein") << Qt::endl;
            for (const QString& id : ids) { mgr->setProfileListener(id, false); }
            mgr->disconnectAll();
            QTimer::singleShot(1500, &app, [&]() {
                for (const QString& id : ids) {
                    out << "getrennt: " << mgr->displayName(id) << " Zustand " << int(mgr->state(id)) << Qt::endl;
                }
                app.quit();
            });
        });
    });
    out << "Verzeichnisabruf (ein Klick) ..." << Qt::endl;
    dir.fetch();
    return app.exec();
}
