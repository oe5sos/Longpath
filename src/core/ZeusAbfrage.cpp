// =================================================================
// src/core/ZeusAbfrage.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigene Datei.
//
// See ZeusAbfrage.h for the full header.
// =================================================================

#include "core/ZeusAbfrage.h"

#include "core/AppSettings.h"

#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

// Ein Weg, der nichts ins Protokoll schreibt, ist nicht
// nachvollziehbar. Am 2026-10-31 stand nach einem Versuch am Geraet
// KEINE einzige Zeile zu Zeus im Protokoll — weder dass gefragt
// wurde, noch mit welchem Ergebnis. Das ist jetzt behoben: jeder
// Versuch und jeder Ausgang steht da.
Q_LOGGING_CATEGORY(lcZeus, "longpath.zeus")

namespace Longpath {

namespace {

constexpr char kSchluessel[] = "Zeus/Basis";
// Je Installation anders; der hier gemessene steht nur da, damit das
// Feld nicht leer ist. Siehe den Kopf — 5000 waere auf macOS der
// AirPlay-Empfaenger.
constexpr char kVorgabe[]    = "http://127.0.0.1:55111";
constexpr char kPfad[]       = "/api/tx-audio-profiles";
constexpr int  kFristMs      = 4000;

} // namespace

ZeusAbfrage::ZeusAbfrage(QObject* parent)
    : QObject(parent)
{
}

ZeusAbfrage::~ZeusAbfrage() = default;

QString ZeusAbfrage::basis()
{
    const QString s = AppSettings::instance()
                          .value(QLatin1String(kSchluessel),
                                 QLatin1String(kVorgabe))
                          .toString();
    return s.isEmpty() ? QString::fromLatin1(kVorgabe) : s;
}

void ZeusAbfrage::setzeBasis(const QString& basis)
{
    AppSettings::instance().setValue(QLatin1String(kSchluessel), basis);
}

bool ZeusAbfrage::istOertlich(const QString& basis, QString* grund)
{
    const QUrl u(basis);
    auto nein = [grund](const QString& g) {
        if (grund != nullptr) { *grund = g; }
        return false;
    };

    if (!u.isValid() || u.host().isEmpty()) {
        return nein(QStringLiteral(
            "„%1\" ist keine Adresse. Erwartet wird etwas wie "
            "http://127.0.0.1:5000 — den Port liest du an deiner "
            "Zeus-Oberfläche ab.").arg(basis));
    }
    const QString schema = u.scheme().toLower();
    if (schema != QLatin1String("http") && schema != QLatin1String("https")) {
        return nein(QStringLiteral(
            "Nur http oder https, nicht „%1\".").arg(u.scheme()));
    }

    // Nur dieser Rechner. Die Pruefung laeuft ueber den Namen und
    // nicht ueber eine Namensaufloesung: ein Name, der heute auf
    // 127.0.0.1 zeigt, zeigt morgen woandershin, und dann waere die
    // Zusage von oben still gebrochen.
    const QString wirt = u.host().toLower();
    const bool oertlich = wirt == QLatin1String("127.0.0.1")
                          || wirt == QLatin1String("::1")
                          || wirt == QLatin1String("[::1]")
                          || wirt == QLatin1String("localhost");
    if (!oertlich) {
        return nein(QStringLiteral(
            "„%1\" liegt nicht auf diesem Rechner. Longpath fragt nur "
            "127.0.0.1 oder localhost — Zeus' eigene Wache lässt "
            "ohnehin nur örtliche Anfragen durch.").arg(u.host()));
    }
    if (grund != nullptr) { grund->clear(); }
    return true;
}

void ZeusAbfrage::hole(int fristMs)
{
    const QString b = basis();
    QString grund;
    if (!istOertlich(b, &grund)) {
        qCWarning(lcZeus).nospace() << "Zeus: " << grund;
        emit gescheitert(grund);
        return;
    }
    qCInfo(lcZeus).nospace()
        << "Zeus: frage " << b << QLatin1String(kPfad) << " ab";

    if (m_netz == nullptr) {
        m_netz = new QNetworkAccessManager(this);
    }

    QUrl url(b);
    url.setPath(QLatin1String(kPfad));

    QNetworkRequest r(url);
    r.setTransferTimeout(fristMs > 0 ? fristMs : kFristMs);
    r.setRawHeader("Accept", "application/json");
    // Kein Umleiten. Eine oertliche Maschine, die umleitet, leitet
    // woandershin — und die Zusage „nur dieser Rechner" gilt fuer die
    // Adresse, die wir geprueft haben, nicht fuer eine, die uns die
    // Antwort nennt.
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::ManualRedirectPolicy);

    QNetworkReply* a = m_netz->get(r);
    connect(a, &QNetworkReply::finished, this, [this, a, b]() {
        a->deleteLater();

        if (a->error() != QNetworkReply::NoError) {
            // Der haeufigste Fall ist „da lauscht nichts", und dann
            // hilft nur eins: sagen, WO nachzusehen ist.
            const bool keinerDa =
                a->error() == QNetworkReply::ConnectionRefusedError
                || a->error() == QNetworkReply::HostNotFoundError
                || a->error() == QNetworkReply::TimeoutError
                || a->error() == QNetworkReply::OperationCanceledError;
            qCWarning(lcZeus).nospace()
                << "Zeus: gescheitert — " << a->errorString()
                << " (Fehler " << int(a->error()) << ")";
            if (keinerDa) {
                emit gescheitert(QStringLiteral(
                    "Unter %1 antwortet keine Zeus-Maschine (%2). Läuft "
                    "Zeus? Und stimmt der Port? Er steht nicht fest — "
                    "lies ihn an der Zeus-Oberfläche ab und trag ihn "
                    "hier ein.").arg(b, a->errorString()));
            } else {
                emit gescheitert(QStringLiteral(
                    "Zeus antwortete nicht wie erwartet: %1")
                    .arg(a->errorString()));
            }
            return;
        }

        const int code =
            a->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (code != 200) {
            // 404 heiszt fast immer: der falsche der beiden
            // Zeus-Prozesse. Die Station Engine lauscht auch, kennt
            // diesen Endpunkt aber nicht — er gehoert dem Produkt.
            // Gemessen am 2026-10-31, siehe den Kopf.
            qCWarning(lcZeus).nospace()
                << "Zeus: antwortete mit " << code;
            if (code == 404) {
                emit gescheitert(QStringLiteral(
                    "Unter %1 lauscht zwar etwas, aber es kennt "
                    "%2 nicht (404). Zeus läuft als ZWEI Prozesse — "
                    "das ist vermutlich die Station Engine. Die "
                    "Profile hat der andere, der Produktprozess. "
                    "Beide Ports stehen in der Prozessliste.")
                    .arg(b, QLatin1String(kPfad)));
                return;
            }
            emit gescheitert(QStringLiteral(
                "Zeus antwortete mit %1 auf %2%3.")
                .arg(code).arg(b, QLatin1String(kPfad)));
            return;
        }

        const QByteArray json = a->readAll();
        if (json.isEmpty()) {
            qCWarning(lcZeus) << "Zeus: Antwort war leer";
            emit gescheitert(QStringLiteral(
                "Zeus antwortete, aber die Antwort war leer."));
            return;
        }
        qCInfo(lcZeus).nospace()
            << "Zeus: " << json.size() << " Bytes gekommen";
        emit gekommen(json);
    });
}

} // namespace Longpath
