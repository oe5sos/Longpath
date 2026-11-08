// no-port-check: Longpath-original design tool (Entwurfsblaetter), no Thetis logic.
//
// ── Logbuch: Entwurfsblaetter (2026-11-08) ──────────────────────────
//
// Betreiber: "das logbuch sollte auch weiter graphisch verbessert werden
// ... nichts bauen ... bringe grafische eigene ideen" -> drei Ideen, je
// drei Betriebsfaelle, aus dem ECHTEN Logbuch gerechnet:
//
//   A  Bedarfs-Matrix beim Tippen (Band x CW/PHONE/DIGI fuer das Land)
//   B  Band x UTC-Stunde: wann welches Band von hier aus geht
//   C  Richtungsrose der QSOs (Peilung x Entfernung) mit Rotorstrahl
//
// Kein Produktcode. Laeuft nur mit LONGPATH_LOGBOOK_FIXTURE=<Kopie> und
// LONGPATH_GRAB_DIR; sonst uebersprungen. Ein Bild je Fall, in wirklicher
// Groesse (Geraetepixelverhaeltnis 2, wie am MacBook).

#include <QtTest/QtTest>

#include <QDir>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetricsF>

#include <algorithm>
#include <cmath>

#include "core/AdifLog.h"
#include "core/CtyDatParser.h"
#include "core/Maidenhead.h"
#include "core/QsoConfirmation.h"
#include "core/WorkedBefore.h"
#include "gui/StyleConstants.h"
#include "models/LogEntry.h"

using namespace Longpath;

namespace {

const QStringList kBaender = {QStringLiteral("160m"), QStringLiteral("80m"),
    QStringLiteral("60m"), QStringLiteral("40m"), QStringLiteral("30m"),
    QStringLiteral("20m"), QStringLiteral("17m"), QStringLiteral("15m"),
    QStringLiteral("12m"), QStringLiteral("10m"), QStringLiteral("6m"),
    QStringLiteral("2m")};
const QStringList kGruppen = {QStringLiteral("CW"), QStringLiteral("PHONE"),
                              QStringLiteral("DATA")};   // WorkedBefore::modeGroup
const QString kMeinFeld = QStringLiteral("JN67VV");

QColor farbe(const char* hex, int alpha = 255)
{
    QColor c{QString::fromLatin1(hex)};
    c.setAlpha(alpha);
    return c;
}
QFont schrift(int px, QFont::Weight w = QFont::Normal)
{
    QFont f;
    f.setPixelSize(px);
    f.setWeight(w);
    return f;
}
QFont mono(int px, QFont::Weight w = QFont::Normal) { return Style::monoFont(QFont(), px, w); }

QImage leinwand(QSize logisch)
{
    QImage img(logisch * 2, QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio(2.0);
    img.fill(farbe(Style::kPanelBg));
    return img;
}
void panel(QPainter& p, const QRectF& r)
{
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0.0, farbe(Style::kGlassPanelTop));
    g.setColorAt(1.0, farbe(Style::kGlassPanelBot));
    p.setPen(QPen(farbe(Style::kBorder), 1.0));
    p.setBrush(g);
    p.drawRoundedRect(r, Style::kGlassPanelRadius, Style::kGlassPanelRadius);
}
void kopf(QPainter& p, const QRectF& r, const QString& links, const QString& rechts = {})
{
    p.setFont(Style::capsFont(QFont(), Style::kFontCaption));
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, links);
    if (!rechts.isEmpty()) {
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(r, Qt::AlignRight | Qt::AlignVCenter, rechts);
    }
}

double norm360(double a) { double r = std::fmod(a, 360.0); return r < 0 ? r + 360 : r; }

} // namespace

class TstLogbuchBlaetter : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void bedarfsMatrix();
    void bandUndStunde();
    void richtungsrose();

private:
    QString land(const QString& call) const { return m_cty.resolvePrimaryPrefix(call); }
    QString feldVon(const LogEntry& e, bool nurLocator, bool* ausLand = nullptr) const;
    void zeichneBedarf(const QString& datei, const QString& call, const QString& band,
                       const QString& gruppe, const QString& frequenz);
    void zeichneHeatmap(const QString& datei, const QString& titel,
                        const std::function<bool(const LogEntry&)>& nimm);
    void zeichneRose(const QString& datei, const QString& titel,
                     const std::function<bool(const LogEntry&)>& nimm, bool rotor,
                     bool nurLocator = false);

    QVector<LogEntry> m_log;
    CtyDatParser m_cty;
    QString m_aus;
    QDateTime m_jetzt;
};

void TstLogbuchBlaetter::initTestCase()
{
    const QString fix = qEnvironmentVariable("LONGPATH_LOGBOOK_FIXTURE");
    m_aus = qEnvironmentVariable("LONGPATH_GRAB_DIR");
    if (fix.isEmpty() || m_aus.isEmpty()) {
        QSKIP("Entwurfswerkzeug: LONGPATH_LOGBOOK_FIXTURE und LONGPATH_GRAB_DIR setzen");
    }
    QString fehler;
    m_log = AdifLog::read(fix, &fehler);
    QVERIFY2(!m_log.isEmpty(), qPrintable(fehler));
    const QString cty = qEnvironmentVariable("LONGPATH_SOURCE_DIR") + QStringLiteral("/cty.dat");
    QVERIFY2(m_cty.loadFromFile(cty), qPrintable(cty));
    QDir().mkpath(m_aus);
    m_jetzt = QDateTime::currentDateTimeUtc();
}

QString TstLogbuchBlaetter::feldVon(const LogEntry& e, bool nurLocator, bool* ausLand) const
{
    if (ausLand) { *ausLand = false; }
    if (isValidGridSquare(e.gridSquare)) { return e.gridSquare; }
    // Auf UKW ist die Landesmitte sinnlos (2 m FM ueber Relais: alles
    // "Austria"); dort nur echte Locatoren.
    if (nurLocator) { return {}; }
    if (ausLand) { *ausLand = true; }
    const DxccEntity* d = m_cty.entityByPrefix(land(e.call));
    if (!d) { return {}; }
    // cty.dat schreibt Laengen positiv nach WESTEN.
    return gridSquareFromLatLon(d->latitude, -d->longitude);
}

// ── A  Bedarfs-Matrix ────────────────────────────────────────────────

void TstLogbuchBlaetter::zeichneBedarf(const QString& datei, const QString& call,
                                       const QString& band, const QString& gruppe,
                                       const QString& frequenz)
{
    const QString ent = land(call);
    const DxccEntity* d = m_cty.entityByPrefix(ent);
    // Das Land: je Band x Gruppe gearbeitet / bestaetigt; dieses Rufzeichen.
    QHash<QString, int> gearbeitet, bestaetigt, diesesCall;
    int qsosLand = 0, qsosCall = 0;
    QDateTime zuletzt; QString zuletztText;
    for (const LogEntry& e : m_log) {
        if (land(e.call) != ent) { continue; }
        ++qsosLand;
        const QString k = e.band.toLower() + QLatin1Char('|') + WorkedBefore::modeGroup(e.mode, e.submode);
        ++gearbeitet[k];
        if (QsoConfirmation::isConfirmed(e)) { ++bestaetigt[k]; }
        if (e.call.compare(call, Qt::CaseInsensitive) == 0) {
            ++diesesCall[k];
            ++qsosCall;
            if (!zuletzt.isValid() || e.timeOn > zuletzt) {
                zuletzt = e.timeOn;
                zuletztText = QStringLiteral("%1 · %2 %3").arg(e.timeOn.toString(QStringLiteral("yyyy-MM-dd")),
                                                              e.band, e.mode);
            }
        }
    }
    const QString jetztK = band + QLatin1Char('|') + gruppe;
    bool bandNeu = true, gruppeNeu = true;
    for (auto it = gearbeitet.cbegin(); it != gearbeitet.cend(); ++it) {
        if (it.key().startsWith(band + QLatin1Char('|'))) { bandNeu = false; }
        if (it.key().endsWith(QLatin1Char('|') + gruppe)) { gruppeNeu = false; }
    }
    QString urteil;
    QColor urteilFarbe = farbe(Style::kAmberText);
    if (qsosLand == 0)              { urteil = QStringLiteral("NEW ONE"); }
    else if (!gearbeitet.contains(jetztK) && bandNeu)   { urteil = QStringLiteral("NEW BAND"); }
    else if (!gearbeitet.contains(jetztK) && gruppeNeu) { urteil = QStringLiteral("NEW MODE"); }
    else if (!gearbeitet.contains(jetztK)) { urteil = QStringLiteral("NEW SLOT"); }
    else if (!bestaetigt.contains(jetztK)) { urteil = QStringLiteral("NOT YET CONFIRMED"); urteilFarbe = farbe(Style::kTextPrimary); }
    else { urteil = QStringLiteral("CONFIRMED HERE"); urteilFarbe = farbe(Style::kGreenText); }

    QImage img = leinwand(QSize(800, 262));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Eingabezeile, wie im Logbuch -- nur zur Lage.
    const QRectF zeile(12, 12, 776, 44);
    panel(p, zeile);
    p.setFont(mono(Style::kFontSub, QFont::DemiBold));
    p.setPen(farbe(Style::kTextPrimary));
    p.drawText(zeile.adjusted(14, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, call);
    p.setFont(mono(Style::kFontSmall));
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(zeile.adjusted(150, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("%1   %2   59   59").arg(frequenz, gruppe == QLatin1String("PHONE") ? QStringLiteral("USB") : QStringLiteral("FT8")));
    const QRectF knopf(zeile.right() - 96, zeile.top() + 9, 84, 26);
    p.setPen(QPen(farbe(Style::kAmberBorder), 1.0));
    p.setBrush(farbe(Style::kAmberBg));
    p.drawRoundedRect(knopf, 4, 4);
    p.setPen(farbe(Style::kAmberText));
    p.setFont(schrift(Style::kFontSmall, QFont::DemiBold));
    p.drawText(knopf, Qt::AlignCenter, QStringLiteral("Log QSO"));

    // Die Karte darunter.
    const QRectF karte(12, 66, 776, 184);
    panel(p, karte);
    // Links: Land, Entfernung, dieses Rufzeichen.
    double km = 0.0, grad = 0.0;
    if (d) {
        const QString feld = gridSquareFromLatLon(d->latitude, -d->longitude);
        km = calculateDistanceKm(kMeinFeld, feld);
        grad = calculateBearingInDegrees(kMeinFeld, feld);
    }
    p.setFont(schrift(Style::kFontBody, QFont::DemiBold));
    p.setPen(farbe(Style::kTextPrimary));
    p.drawText(QRectF(karte.left() + 14, karte.top() + 12, 230, 20), Qt::AlignLeft | Qt::AlignVCenter,
               d ? d->name : QStringLiteral("unknown entity"));
    p.setFont(mono(Style::kFontSmall));
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(QRectF(karte.left() + 14, karte.top() + 34, 230, 18), Qt::AlignLeft | Qt::AlignVCenter,
               d ? QStringLiteral("%1 · %2 km · %3°").arg(d->continent).arg(qRound(km)).arg(qRound(grad), 3, 10, QLatin1Char('0'))
                 : QString());
    p.setFont(schrift(Style::kFontSmall));
    const QString zeile1 = qsosCall > 0 ? QStringLiteral("This call: worked %1×").arg(qsosCall)
                                        : QStringLiteral("This call: never worked");
    p.drawText(QRectF(karte.left() + 14, karte.top() + 64, 230, 18), Qt::AlignLeft | Qt::AlignVCenter, zeile1);
    if (qsosCall > 0) {
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(QRectF(karte.left() + 14, karte.top() + 82, 230, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("last %1").arg(zuletztText));
    }
    p.setPen(farbe(Style::kTextSecondary));
    p.drawText(QRectF(karte.left() + 14, karte.top() + 106, 230, 18), Qt::AlignLeft | Qt::AlignVCenter,
               qsosLand > 0 ? QStringLiteral("Entity: %1 QSOs, %2 slots confirmed")
                                  .arg(qsosLand).arg(bestaetigt.size())
                            : QStringLiteral("Entity: not in the log"));
    // Urteil -- so breit wie sein Text.
    p.setFont(Style::capsFont(QFont(), Style::kFontSmall));
    const double chipW = QFontMetricsF(p.font()).horizontalAdvance(urteil) + 28.0;
    const QRectF chip(karte.left() + 14, karte.top() + 136, chipW, 28);
    p.setPen(QPen(urteilFarbe, 1.2));
    QColor chipFill = urteilFarbe; chipFill.setAlpha(36);
    p.setBrush(chipFill);
    p.drawRoundedRect(chip, 5, 5);
    p.setPen(urteilFarbe);
    p.drawText(chip, Qt::AlignCenter, urteil);

    // Rechts: die Matrix.
    const double x0 = karte.left() + 300, y0 = karte.top() + 30;
    const double w = 36, h = 30;
    p.setFont(mono(Style::kFontCaption));
    p.setPen(farbe(Style::kTextScale));
    for (int i = 0; i < kBaender.size(); ++i) {
        const QString b = kBaender[i];
        p.setPen(b == band ? farbe(Style::kTextPrimary) : farbe(Style::kTextScale));
        p.drawText(QRectF(x0 + i * w, y0 - 20, w, 16), Qt::AlignCenter, QString(b).remove(QLatin1Char('m')));
    }
    for (int j = 0; j < kGruppen.size(); ++j) {
        p.setPen(kGruppen[j] == gruppe ? farbe(Style::kTextPrimary) : farbe(Style::kTextScale));
        p.drawText(QRectF(x0 - 56, y0 + j * h, 50, h), Qt::AlignRight | Qt::AlignVCenter,
                   kGruppen[j] == QLatin1String("PHONE") ? QStringLiteral("SSB")
                   : kGruppen[j] == QLatin1String("DATA") ? QStringLiteral("DIGI") : kGruppen[j]);
        for (int i = 0; i < kBaender.size(); ++i) {
            const QString k = kBaender[i] + QLatin1Char('|') + kGruppen[j];
            const QRectF z(x0 + i * w + 4, y0 + j * h + 4, w - 8, h - 8);
            const bool jetzt = (k == jetztK);
            if (bestaetigt.contains(k)) {
                p.setPen(Qt::NoPen);
                p.setBrush(farbe(Style::kGreenText, 200));
                p.drawRoundedRect(z, 4, 4);
            } else if (gearbeitet.contains(k)) {
                p.setPen(QPen(farbe(Style::kAmberText), 1.6));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(z.adjusted(1, 1, -1, -1), 4, 4);
            } else {
                p.setPen(QPen(farbe(Style::kBorder), 1.0));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(z, 4, 4);
            }
            if (diesesCall.contains(k)) {
                p.setPen(Qt::NoPen);
                p.setBrush(farbe(Style::kTextPrimary));
                p.drawEllipse(z.center(), 3.0, 3.0);
            }
            if (jetzt) {
                const bool neu = !gearbeitet.contains(k);
                QColor rand = neu ? farbe(Style::kAmberText) : farbe(Style::kTextPrimary);
                p.setBrush(Qt::NoBrush);
                if (neu) {
                    QColor hof = farbe(Style::kAmberText, 60);
                    p.setPen(QPen(hof, 6.0));
                    p.drawRoundedRect(z.adjusted(-2, -2, 2, 2), 6, 6);
                }
                p.setPen(QPen(rand, 2.0));
                p.drawRoundedRect(z.adjusted(-2, -2, 2, 2), 6, 6);
            }
        }
    }
    // Legende.
    const double ly = y0 + 3 * h + 18;
    p.setFont(schrift(Style::kFontCaption));
    auto legende = [&](double x, int art, const QString& text) {
        const QRectF z(x, ly, 14, 12);
        if (art == 0) { p.setPen(Qt::NoPen); p.setBrush(farbe(Style::kGreenText, 200)); p.drawRoundedRect(z, 3, 3); }
        if (art == 1) { p.setPen(QPen(farbe(Style::kAmberText), 1.4)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(z, 3, 3); }
        if (art == 2) { p.setPen(QPen(farbe(Style::kBorder), 1.0)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(z, 3, 3); }
        if (art == 3) { p.setPen(Qt::NoPen); p.setBrush(farbe(Style::kTextPrimary)); p.drawEllipse(z.center(), 3.0, 3.0); }
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(QRectF(x + 20, ly - 2, 140, 16), Qt::AlignLeft | Qt::AlignVCenter, text);
    };
    legende(x0, 0, QStringLiteral("confirmed"));
    legende(x0 + 100, 1, QStringLiteral("worked"));
    legende(x0 + 190, 2, QStringLiteral("never"));
    legende(x0 + 270, 3, QStringLiteral("this call"));
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(x0 + 360, ly - 2, 120, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("frame = now"));
    p.end();
    QVERIFY(img.save(m_aus + QLatin1Char('/') + datei));
}

void TstLogbuchBlaetter::bedarfsMatrix()
{
    // a) die meistgearbeitete Station
    QHash<QString, int> jeCall;
    for (const LogEntry& e : m_log) { ++jeCall[e.call.toUpper()]; }
    QString oft; int n = 0;
    for (auto it = jeCall.cbegin(); it != jeCall.cend(); ++it) {
        if (it.value() > n) { n = it.value(); oft = it.key(); }
    }
    zeichneBedarf(QStringLiteral("A1_oft_gearbeitet.png"), oft, QStringLiteral("15m"),
                  QStringLiteral("PHONE"), QStringLiteral("21.285.000"));

    // b) ein seltenes Land, nur auf einem Band gearbeitet -> jetzt auf 15 m
    QHash<QString, QSet<QString>> baenderJeLand;
    QHash<QString, QString> callJeLand;
    QHash<QString, int> qsosJeLand;
    for (const LogEntry& e : m_log) {
        const QString l = land(e.call);
        if (l.isEmpty()) { continue; }
        baenderJeLand[l].insert(e.band.toLower());
        callJeLand[l] = e.call.toUpper();
        ++qsosJeLand[l];
    }
    QString selten;
    for (auto it = qsosJeLand.cbegin(); it != qsosJeLand.cend(); ++it) {
        const QSet<QString>& b = baenderJeLand[it.key()];
        if (it.value() >= 2 && it.value() <= 4 && b.size() == 1 && b.contains(QStringLiteral("20m"))) {
            const DxccEntity* d = m_cty.entityByPrefix(it.key());
            if (d && d->continent != QLatin1String("EU")) { selten = it.key(); break; }
        }
    }
    QVERIFY2(!selten.isEmpty(), "kein seltenes Land gefunden");
    zeichneBedarf(QStringLiteral("A2_seltenes_land_neues_band.png"), callJeLand[selten],
                  QStringLiteral("15m"), QStringLiteral("PHONE"), QStringLiteral("21.310.000"));

    // c) ein Land, das noch nie im Log steht (Beispielrufzeichen)
    QString neu;
    for (const QString& kand : {QStringLiteral("VP8ZZ"), QStringLiteral("3Y0Z"),
                                QStringLiteral("FT5WQ"), QStringLiteral("KH1Z"),
                                QStringLiteral("VK0ZZ"), QStringLiteral("ZL9ZZ")}) {
        if (!qsosJeLand.contains(land(kand)) && !land(kand).isEmpty()) { neu = kand; break; }
    }
    QVERIFY2(!neu.isEmpty(), "jedes Beispielland ist schon gearbeitet");
    zeichneBedarf(QStringLiteral("A3_neues_land.png"), neu, QStringLiteral("20m"),
                  QStringLiteral("PHONE"), QStringLiteral("14.195.000"));
}

// ── B  Band x UTC-Stunde ─────────────────────────────────────────────

void TstLogbuchBlaetter::zeichneHeatmap(const QString& datei, const QString& titel,
                                        const std::function<bool(const LogEntry&)>& nimm)
{
    QHash<QString, QVector<int>> z;
    int maxWert = 0, gesamt = 0;
    for (const LogEntry& e : m_log) {
        if (!nimm(e)) { continue; }
        const QString b = e.band.toLower();
        if (!kBaender.contains(b)) { continue; }
        QVector<int>& v = z[b];
        if (v.isEmpty()) { v.fill(0, 24); }
        maxWert = std::max(maxWert, ++v[e.timeOn.toUTC().time().hour()]);
        ++gesamt;
    }
    QStringList zeilen;
    for (const QString& b : kBaender) { if (z.contains(b)) { zeilen << b; } }

    const double zellW = 26, zellH = 22, x0 = 64, y0 = 64;
    const QSize groesse(int(x0 + 24 * zellW + 150), int(y0 + zeilen.size() * zellH + 62));
    QImage img = leinwand(groesse);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    panel(p, QRectF(8, 8, groesse.width() - 16, groesse.height() - 16));
    kopf(p, QRectF(22, 16, groesse.width() - 44, 18),
         QStringLiteral("WHEN DOES WHICH BAND WORK FROM %1").arg(kMeinFeld),
         QStringLiteral("%1 · %2").arg(titel).arg(gesamt));

    // Stunden-Achse
    p.setFont(mono(Style::kFontCaption));
    p.setPen(farbe(Style::kTextScale));
    for (int h = 0; h < 24; h += 3) {
        p.drawText(QRectF(x0 + h * zellW - 6, y0 - 22, zellW + 12, 16), Qt::AlignCenter,
                   QStringLiteral("%1").arg(h, 2, 10, QLatin1Char('0')));
    }
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(x0 + 24 * zellW + 6, y0 - 22, 60, 16), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("UTC"));

    const int stufen = 5;
    for (int r = 0; r < zeilen.size(); ++r) {
        const QVector<int>& v = z[zeilen[r]];
        const double y = y0 + r * zellH;
        p.setFont(mono(Style::kFontSmall));
        p.setPen(farbe(Style::kTextSecondary));
        p.drawText(QRectF(10, y, x0 - 18, zellH), Qt::AlignRight | Qt::AlignVCenter, zeilen[r]);
        int beste = 0;
        for (int h = 0; h < 24; ++h) {
            if (v[h] > v[beste]) { beste = h; }
            const QRectF c(x0 + h * zellW + 1.5, y + 1.5, zellW - 3, zellH - 3);
            if (v[h] == 0) {
                p.setPen(QPen(farbe(Style::kBorder, 120), 1.0));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(c, 3, 3);
                continue;
            }
            // Stufen nach log: 1..5, damit wenige QSOs sichtbar bleiben.
            const double t = std::log(1.0 + v[h]) / std::log(1.0 + maxWert);
            const int stufe = std::clamp(int(std::ceil(t * stufen)), 1, stufen);
            QColor f = farbe(Style::kAmberText);
            f.setAlpha(40 + stufe * 43);
            p.setPen(Qt::NoPen);
            p.setBrush(f);
            p.drawRoundedRect(c, 3, 3);
        }
        // Rechts: Summe und beste Zeit.
        int summe = 0; for (int x : v) { summe += x; }
        p.setFont(mono(Style::kFontCaption));
        p.setPen(farbe(Style::kTextTertiary));
        p.drawText(QRectF(x0 + 24 * zellW + 8, y, 140, zellH), Qt::AlignLeft | Qt::AlignVCenter,
                   QStringLiteral("%1  best %2–%3").arg(summe, 4)
                       .arg(beste, 2, 10, QLatin1Char('0')).arg((beste + 1) % 24, 2, 10, QLatin1Char('0')));
    }
    // Jetzt
    const int h = m_jetzt.time().hour();
    const double xj = x0 + h * zellW + m_jetzt.time().minute() / 60.0 * zellW;
    p.setPen(QPen(farbe(Style::kTextPrimary, 200), 1.2));
    p.drawLine(QPointF(xj, y0 - 4), QPointF(xj, y0 + zeilen.size() * zellH + 4));
    p.setFont(Style::capsFont(QFont(), Style::kFontMicro));
    p.drawText(QRectF(xj - 30, y0 + zeilen.size() * zellH + 6, 60, 12), Qt::AlignCenter,
               QStringLiteral("NOW %1Z").arg(m_jetzt.toString(QStringLiteral("HHmm"))));
    // Legende
    const double ly = groesse.height() - 30;
    p.setFont(schrift(Style::kFontCaption));
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(x0, ly, 60, 14), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("fewer"));
    for (int s = 1; s <= stufen; ++s) {
        QColor f = farbe(Style::kAmberText); f.setAlpha(40 + s * 43);
        p.setPen(Qt::NoPen); p.setBrush(f);
        p.drawRoundedRect(QRectF(x0 + 40 + s * 18, ly + 1, 14, 12), 3, 3);
    }
    p.setPen(farbe(Style::kTextTertiary));
    p.drawText(QRectF(x0 + 40 + 6 * 18 + 4, ly, 60, 14), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("more"));
    p.end();
    QVERIFY(img.save(m_aus + QLatin1Char('/') + datei));
}

void TstLogbuchBlaetter::bandUndStunde()
{
    zeichneHeatmap(QStringLiteral("B1_alle.png"), QStringLiteral("all QSOs"),
                   [](const LogEntry&) { return true; });
    zeichneHeatmap(QStringLiteral("B2_dx_ausserhalb_europa.png"), QStringLiteral("outside Europe"),
                   [this](const LogEntry& e) {
                       const DxccEntity* d = m_cty.entityByPrefix(land(e.call));
                       return d && d->continent != QLatin1String("EU");
                   });
    zeichneHeatmap(QStringLiteral("B3_nordamerika.png"), QStringLiteral("North America"),
                   [this](const LogEntry& e) {
                       const DxccEntity* d = m_cty.entityByPrefix(land(e.call));
                       return d && d->continent == QLatin1String("NA");
                   });
}

// ── C  Richtungsrose ─────────────────────────────────────────────────

void TstLogbuchBlaetter::zeichneRose(const QString& datei, const QString& titel,
                                     const std::function<bool(const LogEntry&)>& nimm, bool rotor,
                                     bool nurLocator)
{
    int ohneOrt = 0, ausLandZahl = 0;
    struct Punkt { double grad; double km; };
    QVector<Punkt> punkte;
    QVector<int> sektor(36, 0);
    for (const LogEntry& e : m_log) {
        if (!nimm(e)) { continue; }
        bool ausLand = false;
        const QString feld = feldVon(e, nurLocator, &ausLand);
        if (feld.isEmpty()) { ++ohneOrt; continue; }
        if (ausLand) { ++ausLandZahl; }
        const double km = calculateDistanceKm(kMeinFeld, feld);
        if (km < 1.0) { continue; }
        const double g = calculateBearingInDegrees(kMeinFeld, feld);
        punkte.push_back({g, km});
        ++sektor[int(norm360(g) / 10.0) % 36];
    }
    const int maxS = *std::max_element(sektor.cbegin(), sektor.cend());

    const QSize groesse(600, 668);
    QImage img = leinwand(groesse);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    panel(p, QRectF(8, 8, 584, 652));
    kopf(p, QRectF(22, 16, 556, 18), QStringLiteral("WHERE YOUR QSOS COME FROM"),
         QStringLiteral("%1 · %2").arg(titel).arg(punkte.size()));

    const QPointF m(300, 322);
    const double R = 214.0;            // Punkte innen
    const double K0 = R + 10.0, K1 = R + 44.0;   // der Kranz: Anteil je 10 Grad
    // Entfernung: ueber 3000 km (KW) logarithmisch 100..20 000 km, sonst
    // linear bis knapp ueber die weiteste Station (2 m: einige 100 km).
    QVector<double> kms; for (const Punkt& q : punkte) { kms << q.km; }
    std::sort(kms.begin(), kms.end());
    // 90. Perzentil: zwei Ausreisser (Es, Tropo) duerfen die Skala nicht
    // auf 3000 km ziehen; sie stehen dann am Rand.
    const double p98 = kms.isEmpty() ? 1000.0 : kms[std::min<int>(kms.size() - 1, int(kms.size() * 0.90))];
    const bool logSkala = p98 > 3000.0;
    double maxKm = 1000.0;
    for (double n : {200.0, 300.0, 500.0, 800.0, 1000.0, 1500.0, 2000.0, 3000.0}) {
        if (n >= p98 * 1.1) { maxKm = n; break; }
    }
    auto radius = [&](double km) {
        if (logSkala) {
            const double t = (std::log10(std::clamp(km, 100.0, 20000.0)) - 2.0) / (std::log10(20000.0) - 2.0);
            return 10.0 + t * (R - 10.0);
        }
        return std::min(km, maxKm) / maxKm * R;
    };
    auto punkt = [&](double grad, double r) {
        const double w = (grad - 90.0) * M_PI / 180.0;
        return QPointF(m.x() + r * std::cos(w), m.y() + r * std::sin(w));
    };

    // Rotor: erlaubter Bogen und Strahl (Fall c).
    if (rotor) {
        // gesperrt 90 -> 270 (Beispiel "zu Hause": 270 -> 90 ueber Nord)
        QPainterPath sperre(m);
        sperre.arcTo(QRectF(m.x() - K1, m.y() - K1, 2 * K1, 2 * K1), 90.0 - 90.0, -180.0);
        sperre.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(farbe(Style::kPanelBg, 200));
        p.drawPath(sperre);
    }
    // Ringe
    p.setFont(mono(Style::kFontMicro));
    QVector<double> ringe = logSkala ? QVector<double>{300.0, 1000.0, 3000.0, 10000.0}
                                     : QVector<double>{maxKm / 4, maxKm / 2, maxKm * 3 / 4};
    for (double km : ringe) {
        const double r = radius(km);
        p.setPen(QPen(farbe(Style::kBorder), 1.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(m, r, r);
        p.setPen(farbe(Style::kTextScale));
        p.drawText(QRectF(m.x() + 4, m.y() - r - 12, 60, 11), Qt::AlignLeft | Qt::AlignVCenter,
                   km >= 1000 ? QStringLiteral("%1k km").arg(km / 1000) : QStringLiteral("%1 km").arg(km));
    }
    p.setPen(QPen(farbe(Style::kBorder), 1.0));
    p.drawEllipse(m, R, R);
    p.setPen(farbe(Style::kTextScale));
    p.drawText(QRectF(m.x() + 4, m.y() - R - 12, 80, 11), Qt::AlignLeft | Qt::AlignVCenter,
               logSkala ? QStringLiteral("20k km") : QStringLiteral("%1 km").arg(maxKm));
    // Der Kranz: Anteil je 10 Grad, aussen -- eine eigene Achse, nicht
    // die Entfernung.
    p.setPen(QPen(farbe(Style::kBorder, 160), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(m, K0, K0);
    int bester = 0;
    for (int s = 1; s < 36; ++s) { if (sektor[s] > sektor[bester]) { bester = s; } }
    for (int s = 0; s < 36; ++s) {
        if (sektor[s] == 0) { continue; }
        const double rr = K0 + (K1 - K0) * std::sqrt(double(sektor[s]) / maxS);
        QPainterPath keil;
        keil.moveTo(punkt(s * 10.0 + 0.8, K0));
        keil.lineTo(punkt(s * 10.0 + 0.8, rr));
        keil.lineTo(punkt(s * 10.0 + 9.2, rr));
        keil.lineTo(punkt(s * 10.0 + 9.2, K0));
        keil.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(farbe(Style::kAmberText, s == bester ? 240 : 150));
        p.drawPath(keil);
    }
    // Punkte
    p.setPen(Qt::NoPen);
    p.setBrush(farbe(Style::kAmberText, 120));
    for (const Punkt& q : punkte) { p.drawEllipse(punkt(q.grad, radius(q.km)), 1.6, 1.6); }
    // Himmelsrichtungen
    p.setFont(Style::capsFont(QFont(), Style::kFontCaption));
    p.setPen(farbe(Style::kTextSecondary));
    const char* namen[4] = {"N", "E", "S", "W"};
    for (int i = 0; i < 4; ++i) {
        const QPointF q = punkt(i * 90.0, K1 + 14.0);
        p.drawText(QRectF(q.x() - 10, q.y() - 8, 20, 16), Qt::AlignCenter, QString::fromLatin1(namen[i]));
    }
    if (rotor) {
        // Strahl: Parkstellung 74 Grad, 60 Grad Keule
        const double az = 74.3;
        QPainterPath kegel(m);
        kegel.arcTo(QRectF(m.x() - R, m.y() - R, 2 * R, 2 * R), 90.0 - az + 30.0, -60.0);
        kegel.closeSubpath();
        p.setPen(QPen(farbe(Style::kTextPrimary, 90), 1.0));
        p.setBrush(farbe(Style::kTextPrimary, 14));
        p.drawPath(kegel);
        p.setPen(QPen(farbe(Style::kTextPrimary), 1.6));
        p.drawLine(m, punkt(az, R));
        // Bogen des erlaubten Bereichs aussen
        p.setPen(QPen(farbe(Style::kGreenText), 3.0, Qt::SolidLine, Qt::FlatCap));
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(m.x() - K1 - 5, m.y() - K1 - 5, 2 * K1 + 10, 2 * K1 + 10),
                  int((90.0 - 270.0) * 16), int(-180.0 * 16));
    }
    // Zusammenfassung: staerkster Sektor, groesste Luecke
    int best = 0; for (int s = 1; s < 36; ++s) { if (sektor[s] > sektor[best]) { best = s; } }
    int lueckeStart = -1, lueckeLen = 0;
    for (int s = 0; s < 36; ++s) {
        int len = 0; while (len < 36 && sektor[(s + len) % 36] == 0) { ++len; }
        if (len > lueckeLen) { lueckeLen = len; lueckeStart = s; }
    }
    p.setFont(schrift(Style::kFontSmall));
    p.setPen(farbe(Style::kTextSecondary));
    QString zus = QStringLiteral("Most QSOs: %1–%2°").arg(best * 10, 3, 10, QLatin1Char('0'))
                      .arg(best * 10 + 10, 3, 10, QLatin1Char('0'));
    if (lueckeLen > 0) {
        zus += QStringLiteral("  ·  never worked: %1–%2°").arg(lueckeStart * 10, 3, 10, QLatin1Char('0'))
                   .arg(((lueckeStart + lueckeLen) % 36) * 10, 3, 10, QLatin1Char('0'));
    }
    if (rotor) { zus += QStringLiteral("  ·  beam 074°, allowed 270°→090°"); }
    p.drawText(QRectF(22, 616, 556, 18), Qt::AlignLeft | Qt::AlignVCenter, zus);
    p.setFont(schrift(Style::kFontCaption));
    p.setPen(farbe(Style::kTextTertiary));
    QStringList fuss;
    if (ausLandZahl > 0) { fuss << QStringLiteral("%1 without a locator placed at their country's centre").arg(ausLandZahl); }
    if (ohneOrt > 0) { fuss << QStringLiteral("%1 without a locator not shown").arg(ohneOrt); }
    p.drawText(QRectF(22, 636, 556, 14), Qt::AlignLeft | Qt::AlignVCenter, fuss.join(QStringLiteral(" · ")));
    p.end();
    QVERIFY(img.save(m_aus + QLatin1Char('/') + datei));
}

void TstLogbuchBlaetter::richtungsrose()
{
    zeichneRose(QStringLiteral("C1_alle_baender.png"), QStringLiteral("all bands"),
                [](const LogEntry&) { return true; }, false);
    zeichneRose(QStringLiteral("C2_20m.png"), QStringLiteral("20 m"),
                [](const LogEntry& e) { return e.band.compare(QStringLiteral("20m"), Qt::CaseInsensitive) == 0; },
                false);
    zeichneRose(QStringLiteral("C3_2m_mit_rotor.png"), QStringLiteral("2 m"),
                [](const LogEntry& e) { return e.band.compare(QStringLiteral("2m"), Qt::CaseInsensitive) == 0; },
                true, true);
}

QTEST_MAIN(TstLogbuchBlaetter)
#include "tst_logbuch_blaetter.moc"
