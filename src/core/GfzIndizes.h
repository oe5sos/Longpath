// no-port-check: Longpath-original, no Thetis logic (Thetis knows no
// space-weather indices).
//
// =================================================================
// src/core/GfzIndizes.h  (Longpath)
// =================================================================
//
// ── Solarflux und Kp des QSO-Tages (2026-11-10) ─────────────────────
//
// Zweite Haelfte von „das Logbuch soll ein QSO erklaeren": wie stand
// die Sonne (F10.7) und wie unruhig war das Erdmagnetfeld (Kp), als
// das QSO lief?
//
// Quelle ist die Tagesdatei des GFZ Potsdam (Geomagnetisches
// Observatorium Niemegk), CC BY 4.0:
//
//   https://kp.gfz.de/app/files/Kp_ap_Ap_SN_F107_since_1932.txt
//
// (kp.gfz-potsdam.de antwortet mit 301 auf diese Adresse; am
// 2026-11-10 nachgesehen.) Eine Zeile je UT-Tag seit 1932: acht Kp-
// Werte (je drei Stunden, Kp1 = 00–03 UT), acht ap, Ap, SN, F10.7obs,
// F10.7adj, D. Fehlende Werte stehen als −1. Die Spalten liest lies()
// aus der Kopfzeile der Datei („#YYY MM DD days days_m Bsr dB Kp1 …"),
// nicht aus festen Positionen — kommt beim GFZ eine Spalte dazu, liest
// es weiter richtig oder gar nicht, aber nie die falsche Zahl.
//
// Gezeigt wird F10.7obs, nicht adj: der Dateikopf selbst empfiehlt den
// beobachteten Wert „for ionospheric and atmospheric studies" — die
// Ionosphaere sieht die Sonne im tatsaechlichen Abstand.
//
// ── Wann geladen wird ───────────────────────────────────────────────
//
// GfzQuelle haelt die Datei im Cache-Ordner des Programms und laedt
// sie hoechstens einmal am Tag neu (≈ 5,5 MB). Ohne Netz gilt der
// Cache; ohne Cache gibt es KEINE Zahlen — lieber nichts zeigen als
// etwas erfinden. In Pruefstaenden (QStandardPaths::isTestModeEnabled)
// geht nie eine Anfrage hinaus.

#pragma once

#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <array>

class QNetworkAccessManager;
class QNetworkReply;

namespace Longpath {

struct GfzTag {
    QDate                 datum;
    std::array<double, 8> kp{{-1, -1, -1, -1, -1, -1, -1, -1}};   // −1 = fehlt
    int                   ap{-1};          // Tages-Ap
    int                   sn{-1};
    double                f107obs{-1.0};
    double                f107adj{-1.0};
    int                   d{-1};           // 0 vorlaeufig, 1/2 endgueltig
};

class GfzIndizes {
public:
    // Liest den Text der GFZ-Datei. Gibt die Zahl der gelesenen Tage
    // zurueck; 0 heisst „nicht diese Datei" (HTML-Fehlerseite, Kopf
    // fehlt, Spalte fehlt) — dann bleibt der alte Inhalt stehen.
    int lies(const QByteArray& text);

    bool  leer() const { return m_tage.isEmpty(); }
    int   anzahl() const { return int(m_tage.size()); }
    QDate ersterTag() const { return m_erster; }
    QDate letzterTag() const { return m_letzter; }

    const GfzTag* tag(const QDate& d) const;

    // F10.7obs des UT-Tages; NaN, wenn er fehlt.
    double sfi(const QDate& d) const;

    // Kp des Dreistundenfensters, in dem `utc` liegt; NaN, wenn er fehlt.
    double kp(const QDateTime& utc) const;

    // Die Schreibweise des GFZ in Dritteln: 2.333 → "2+", 2.667 → "3-",
    // 3.0 → "3".
    static QString kpText(double kp);

    // "SFI 142 · Kp 2+" fuer den Zeitpunkt; leer, wenn nichts da ist,
    // ein Teil allein, wenn nur einer da ist.
    QString kurztext(const QDateTime& utc) const;

    // Der Hinweis dazu: welches Fenster, Ap des Tages, vorlaeufig oder
    // endgueltig, Quelle.
    QString hinweis(const QDateTime& utc) const;

private:
    QHash<qint64, GfzTag> m_tage;          // Schluessel: QDate::toJulianDay()
    QDate                 m_erster;
    QDate                 m_letzter;
};

// Die Datei im Cache, einmal am Tag frisch. Eine je Programm.
class GfzQuelle : public QObject {
    Q_OBJECT
public:
    static GfzQuelle& instanz();

    // Was gerade da ist. Beim ersten Aufruf aus dem Cache gelesen; ist
    // der aelter als einen Tag, wird ein Abruf angestossen (asynchron,
    // danach `aktualisiert`).
    const GfzIndizes& daten();

    QString cachePfad() const { return m_cachePfad; }
    // Fuer Pruefstaende: anderer Cache, Inhalt neu gelesen.
    void setCachePfad(const QString& pfad);

    static QUrl standardQuelle();

    // Laedt neu, wenn der Cache aelter als kFrischStunden ist und der
    // letzte Versuch laenger als kWiederholStunden her. In Pruefstaenden
    // nie.
    void aktualisiereWennAlt();

    static constexpr int kFrischStunden = 24;
    static constexpr int kWiederholStunden = 3;

signals:
    void aktualisiert();

private:
    explicit GfzQuelle(QObject* parent = nullptr);
    void ladeCache();
    void fertig();

    QString                m_cachePfad;
    bool                   m_cacheGelesen{false};
    GfzIndizes             m_daten;
    QNetworkAccessManager* m_nam{nullptr};   // erst beim ersten Abruf
    QPointer<QNetworkReply> m_reply;
};

} // namespace Longpath
