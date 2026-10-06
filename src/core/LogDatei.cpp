#include "core/LogDatei.h"

#include <QFile>
#include <QMutex>
#include <QMutexLocker>

#include <cstdio>

namespace Longpath::Log {
namespace {

QMutex g_schloss;
QFile* g_datei = nullptr;   // gehoert main.cpp

}  // namespace

void setzeDatei(QFile* datei)
{
    const QMutexLocker sperre(&g_schloss);
    g_datei = datei;
}

void schreibe(const QString& zeile)
{
    // Umwandeln VOR dem Schloss: das ist der teuerste Teil und braucht es
    // nicht. Zwei Fassungen, weil die Datei seit jeher UTF-8 traegt und der
    // Bildschirm die Umgebungskodierung erwartet.
    const QByteArray fuerDatei  = zeile.toUtf8();
    const QByteArray fuerSchirm = zeile.toLocal8Bit();

    const QMutexLocker sperre(&g_schloss);
    if (g_datei && g_datei->isOpen()) {
        // EIN Schreibaufruf, nicht QTextStream: der puffert und gibt in
        // Stuecken ab, und genau an dieser Naht sind die Zeilen zerfallen.
        g_datei->write(fuerDatei);
        g_datei->flush();
    }
    std::fwrite(fuerSchirm.constData(), 1, std::size_t(fuerSchirm.size()), stderr);
}

}  // namespace Longpath::Log
