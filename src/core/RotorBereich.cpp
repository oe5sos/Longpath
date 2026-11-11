// no-port-check: Longpath-original, see RotorBereich.h.
//
// src/core/RotorBereich.cpp  (Longpath)

#include "core/RotorBereich.h"

#include "core/AppSettings.h"

#include <cmath>

namespace Longpath {

namespace {
double norm360(double a)
{
    double r = std::fmod(a, 360.0);
    return r < 0.0 ? r + 360.0 : r;
}
QString alsBool(bool b) { return b ? QStringLiteral("True") : QStringLiteral("False"); }

// Die erlaubten Stuecke in Rotorlage (0..360 ab Anschlag). Hoechstens zwei.
struct Stuecke { int n{0}; double lo[2]{}; double hi[2]{}; };
Stuecke stuecke(const RotorBereich& b)
{
    Stuecke s;
    if (!b.an) { s.n = 1; s.lo[0] = 0.0; s.hi[0] = 360.0; return s; }
    // Frei drehend: der Bogen selbst, ab seinem Anfang gerechnet.
    const double anschlag = (b.anschlagGrad < 0.0) ? b.vonGrad : b.anschlagGrad;
    const double laenge = norm360(b.bisGrad - b.vonGrad);
    if (laenge < 1e-9) {                              // von == bis: ganz herum
        s.n = 1; s.lo[0] = 0.0; s.hi[0] = 360.0; return s;
    }
    const double start = norm360(b.vonGrad - anschlag);
    if (start + laenge <= 360.0 + 1e-9) {
        s.n = 1; s.lo[0] = start; s.hi[0] = std::min(360.0, start + laenge);
    } else {
        s.n = 2;
        s.lo[0] = start; s.hi[0] = 360.0;
        s.lo[1] = 0.0;   s.hi[1] = start + laenge - 360.0;
    }
    return s;
}
// Rotorlage einer Richtung; liegt sie auf dem Anschlag, gilt das Ende,
// das zum Stueck passt (0 und 360 sind dieselbe Richtung, aber zwei Lagen).
int stueckFuer(const Stuecke& s, double p, double* lage)
{
    const double r = RotorBereich::kRandGrad;
    for (int i = 0; i < s.n; ++i) {
        for (double q : {p, p + 360.0, p - 360.0}) {
            if (q >= s.lo[i] - r && q <= s.hi[i] + r) { *lage = q; return i; }
        }
    }
    return -1;
}
} // namespace

double RotorBereich::bezug() const
{
    return (anschlagGrad < 0.0) ? vonGrad : anschlagGrad;
}

bool RotorBereich::erlaubt(double az) const
{
    if (!an) { return true; }
    double lage = 0.0;
    return stueckFuer(stuecke(*this), norm360(az - bezug()), &lage) >= 0;
}

bool RotorBereich::wegErlaubt(double von, double nach) const
{
    if (!an) { return true; }
    if (anschlagGrad < 0.0) {
        // Frei drehend: der kurze Weg. Erlaubt, wenn beide Enden und jeder
        // Grad dazwischen im Bogen liegen.
        if (!erlaubt(von) || !erlaubt(nach)) { return false; }
        double d = std::remainder(nach - von, 360.0);
        const int schritte = static_cast<int>(std::ceil(std::abs(d)));
        for (int i = 1; i < schritte; ++i) {
            if (!erlaubt(von + d * i / schritte)) { return false; }
        }
        return true;
    }
    const Stuecke s = stuecke(*this);
    double a = 0.0, b = 0.0;
    const int ia = stueckFuer(s, norm360(von - bezug()), &a);
    const int ib = stueckFuer(s, norm360(nach - bezug()), &b);
    // Beide Enden im selben Stueck: der Rotor bleibt auf dem Weg dazwischen
    // darin, weil er zwischen seinen Anschlaegen linear faehrt.
    return ia >= 0 && ia == ib;
}

bool RotorBereich::reichweite(double az, double* anfangGrad, double* spanneGrad) const
{
    const Stuecke s = stuecke(*this);
    double lage = 0.0;
    const int i = stueckFuer(s, norm360(az - bezug()), &lage);
    if (i < 0) { return false; }
    if (anfangGrad) { *anfangGrad = norm360(bezug() + s.lo[i]); }
    if (spanneGrad) { *spanneGrad = s.hi[i] - s.lo[i]; }
    return true;
}

QString RotorBereich::beschreibung() const
{
    QString anschlag = QStringLiteral("turns freely");
    if (anschlagGrad >= 0.0) {
        anschlag = QStringLiteral("stop at %1°").arg(qRound(norm360(anschlagGrad)));
    }
    if (!an) {
        return QStringLiteral("No limit (%1)").arg(anschlag);
    }
    return QStringLiteral("%1° → %2° clockwise (%3)")
        .arg(qRound(norm360(vonGrad)), 3, 10, QLatin1Char('0'))
        .arg(qRound(norm360(bisGrad)), 3, 10, QLatin1Char('0'))
        .arg(anschlag);
}

RotorBereich RotorBereich::load()
{
    auto& s = AppSettings::instance();
    RotorBereich b;
    b.an = s.value(QStringLiteral("Rotor/LimitOn"), QStringLiteral("False")).toString()
           == QStringLiteral("True");
    b.vonGrad      = s.value(QStringLiteral("Rotor/LimitFromDeg"), 0.0).toDouble();
    b.bisGrad      = s.value(QStringLiteral("Rotor/LimitToDeg"), 360.0).toDouble();
    // Dieselbe Einstellung wie "End stop" im Rotor-Fenster.
    b.anschlagGrad = s.value(QStringLiteral("RotorEndStopDeg"), -1.0).toDouble();
    return b;
}

void RotorBereich::save() const
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("Rotor/LimitOn"), alsBool(an));
    s.setValue(QStringLiteral("Rotor/LimitFromDeg"), vonGrad);
    s.setValue(QStringLiteral("Rotor/LimitToDeg"), bisGrad);
    s.setValue(QStringLiteral("RotorEndStopDeg"), anschlagGrad);
}

} // namespace Longpath
