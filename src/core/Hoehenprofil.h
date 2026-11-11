// no-port-check: Longpath-original, no Thetis logic (Thetis has no
// logbook and no path profile).
//
// =================================================================
// src/core/Hoehenprofil.h  (Longpath)
// =================================================================
//
// ── Warum ein UKW-QSO ging (2026-11-11) ─────────────────────────────
//
// Betreiber waehlte das Hoehenprofil als eines der Logbuch-Bilder,
// die Longpath abheben sollen (Entwurfsblatt werkzeug/logbuch-
// 2026-11-09/blaetter/5-hoehenprofil-2m.png, Rechnung in profil.py
// daneben). Hier dieselbe Geometrie, ohne Oberflaeche:
//
//   * Gelaende entlang des Grosskreises, die Erdkruemmung als
//     Ausbuchtung d1*d2/(2kR) mit k = 4/3 auf das Gelaende gerechnet --
//     dann ist die Sichtlinie eine Gerade zwischen den Antennenspitzen.
//   * Erste Fresnel-Zone r1 = sqrt(lambda*d1*d2/D) bei der Frequenz des
//     QSOs; Gelaende in den inneren 60 % der Zone ist "angeschnitten".
//   * Ohne Sichtweg: die Horizontstrahlen beider Stationen (steilste
//     Tangente an das Gelaende), ihr Schnittpunkt (das gemeinsame
//     Volumen, wo Beugung oder Tropo das Signal hinueberholt), dessen
//     Hoehe ueber Grund und der Streuwinkel zwischen den Strahlen.
//
// Die Ausbuchtungs-Darstellung ist die uebliche der Funkplanung: in ihr
// sind Strahlen Geraden, und der Streuwinkel ist die Summe der beiden
// Steigungswinkel -- er enthaelt die Kruemmung D/(kR) schon (siehe den
// Pruefstand tst_hoehenprofil: ueber flachem Meer ist der Schnittpunkt
// genau dort, wo die Formel fuer zwei Radiohorizonte ihn hinlegt).
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#pragma once

#include <QString>
#include <QVector>

namespace Longpath::Hoehenprofil {

constexpr double kErdradiusKm = 6371.0;
/// Effektiver Erdradius-Faktor fuer die Normalatmosphaere.
constexpr double kK = 4.0 / 3.0;
/// Lichtgeschwindigkeit in m * MHz: lambda[m] = kC / f[MHz].
constexpr double kC = 299.792458;
/// Anteil der ersten Fresnel-Zone, der frei bleiben soll.
constexpr double kZonenAnteil = 0.6;
/// Unter dieser Hoehe ueber der Sichtlinie heisst es "fast Sichtweg":
/// das liegt in der Ungenauigkeit eines Locators und der Hoehendaten.
constexpr double kFastSichtM = 30.0;

struct Punkt {
    double lat{0.0};
    double lon{0.0};
};

/// n Punkte gleichen Abstands auf dem Grosskreis, beide Enden
/// eingeschlossen (n >= 2).
QVector<Punkt> grosskreisPunkte(double lat1, double lon1,
                                double lat2, double lon2, int n);

/// Laenge des Grosskreises in km (Kugel, R = 6371 km).
double grosskreisKm(double lat1, double lon1, double lat2, double lon2);

/// Wie viele Hoehenpunkte ein Weg bekommt: etwa einer je Kilometer,
/// mindestens 100, hoechstens 300 (drei Abrufe bei 100 je Abruf).
int punktzahlFuer(double km);

/// Ausbuchtung der Erde ueber der Sehne, in Metern, am Punkt dKm eines
/// Weges der Laenge gesamtKm.
double ausbuchtungM(double dKm, double gesamtKm, double k = kK);

double wellenlaengeM(double freqMHz);

/// Radius der ersten Fresnel-Zone in Metern; d1/d2 in km.
double fresnelRadiusM(double d1Km, double d2Km, double freqMHz);

/// Frequenz fuer die Fresnel-Zone: die des QSOs, wenn sie im Band
/// liegt, sonst eine typische des Bandes. 0 = kein UKW-Band dieses
/// Bildes (2 m, 70 cm, 23 cm).
double profilFrequenzMHz(const QString& band, double freqMHz);

/// Ist das ein QSO fuer das Profil? 2 m, 70 cm, 23 cm.
bool istUkwBand(const QString& band, double freqMHz);

/// "±3 km" -- wie genau die Mitte eines Locators die Station trifft.
QString locatorGenauigkeit(const QString& locator);
/// Dasselbe als Zahl in km (0,3 / 3 / 60), fuer Eingabe::unsicher*Km.
double locatorUnsicherheitKm(const QString& locator);

enum class Urteil {
    FreieSicht,           ///< nichts in den inneren 60 % der Zone
    FresnelAngeschnitten, ///< Sichtlinie frei, Zone angeschnitten
    FastSicht,            ///< knapp verdeckt (< kFastSichtM)
    KeineSicht,           ///< verdeckt: Beugung oder Tropo
};

struct Eingabe {
    QVector<double> distKm;     ///< aufsteigend, distKm.first() == 0
    QVector<double> gelaendeM;  ///< Hoehe ueber NN je Punkt
    double antenneA{10.0};      ///< ueber Grund am Anfang (eigene)
    double antenneB{10.0};      ///< ueber Grund am Ende (Gegenstation)
    double freqMHz{144.3};
    double k{kK};

    // ── Wo ein Ende nur ungefaehr bekannt ist (2026-11-11) ───────────
    //
    // Steht eine Station nur als Locator im Log, kann sie irgendwo im
    // Quadrat stehen -- bei sechs Zeichen +-3 km. Das Gelaende so nah an
    // der angenommenen Mitte sagt dann nichts ueber IHREN Horizont: mit
    // einem Punkt je Kilometer lag 2 km neben der Mitte von JN88RS ein
    // Hang, und der Streuwinkel fuer ein QSO, das gelaufen ist, kam auf
    // 10° (mit 100 Punkten im Entwurf 1,8°). Gelaende naeher als diese
    // Strecke an einem Ende wird gezeichnet, zaehlt aber weder fuer das
    // Urteil noch fuer die Horizontstrahlen -- angenommen ist, dass die
    // Station dort frei steht. Ein Gipfel aus der SOTA-Datenbank: 0.
    // Hoechstens ein Viertel des Weges je Ende.
    double unsicherAKm{0.0};
    double unsicherBKm{0.0};
};

struct Ergebnis {
    bool   gueltig{false};
    double gesamtKm{0.0};
    double hoeheA{0.0};          ///< Antennenspitze A ueber NN
    double hoeheB{0.0};

    QVector<double> ausbuchtung; ///< m
    QVector<double> gelaende;    ///< Hoehe + Ausbuchtung, m
    QVector<double> sicht;       ///< Sichtlinie, m
    QVector<double> fresnel;     ///< r1, m
    QVector<bool>   inZone;      ///< Gelaende in den inneren 60 %
    QVector<bool>   verdeckt;    ///< Gelaende ueber der Sichtlinie
    QVector<bool>   unsicher;    ///< zu nah an einem ungefaehren Ende
    double unsicherAKm{0.0};     ///< tatsaechlich verwendet (gekappt)
    double unsicherBKm{0.0};

    /// Groesste Hoehe des Gelaendes ueber der Sichtlinie (negativ =
    /// freier Abstand am knappsten Punkt).
    double maxUeberLinieM{0.0};
    int    maxUeberLinieIndex{-1};
    /// Anteil der Punkte, an denen das Gelaende in der Zone liegt.
    double anteilInZone{0.0};

    /// Horizontstrahlen und gemeinsames Volumen -- nur ohne Sichtweg.
    bool   hatHorizont{false};
    double steigungA{0.0};       ///< m je km, vom Anfang aus
    double steigungB{0.0};       ///< m je km, vom Ende zurueck
    int    tangenteA{-1};        ///< Index des Horizontpunkts von A
    int    tangenteB{-1};
    double schnittKm{0.0};
    double schnittHoeheM{0.0};   ///< ueber NN (im Ausbuchtungsbild)
    double schnittUeberGrundM{0.0};
    double streuwinkelGrad{0.0};

    Urteil urteil{Urteil::FreieSicht};
};

/// Die ganze Rechnung. Fehlende Hoehen (NaN) zaehlen als 0 m (Meer).
Ergebnis rechne(const Eingabe& ein);

/// Ein Satz fuer die Oberflaeche (englisch). `genauigkeit` ist die der
/// Gegenstation ("±3 km"), leer bei einem Gipfel.
QString urteilKurz(Urteil u);
QString urteilText(const Ergebnis& e, const QString& genauigkeit);

} // namespace Longpath::Hoehenprofil
