#include "core/LogbookDatei.h"

#include "models/LogEntry.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

#include "core/AppSettings.h"

namespace Longpath {
namespace LogbookDatei {

QString pfad()
{
    const QString ordner = AppSettings::dataDir();
    QDir().mkpath(ordner);
    return ordner + QStringLiteral("/logbook.adi");
}

bool anhaengen(const LogEntry& eintrag, QString* fehler)
{
    const QString p = pfad();
    const bool neu = !QFile::exists(p);

    QFile f(p);
    if (!f.open(QIODevice::Append | QIODevice::Text)) {
        if (fehler) { *fehler = f.errorString(); }
        return false;
    }
    QTextStream aus(&f);
    if (neu) {
        // Strenge Importeure lehnen eine Datei ab, deren erstes Zeichen ein
        // Datensatz ist statt eines mit <EOH> beendeten Kopfes.
        aus << "Longpath logbook\n"
            << "<ADIF_VER:5>3.1.4 <PROGRAMID:8>Longpath <EOH>\n";
    }
    aus << eintrag.toAdifRecord() << "\n";
    aus.flush();
    return true;
}

}  // namespace LogbookDatei
}  // namespace Longpath
