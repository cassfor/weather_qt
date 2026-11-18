#include "SearchHistory.h"
#include <QSettings>
#include <QDebug>
SearchHistory::SearchHistory(const QStringList &validNames) : allowed(validNames) {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "WeatherProject", "Weather");
    for (const QString &name : settings.value("search/recentCities").toStringList())
        if (allowed.contains(name) && !recent.contains(name) && recent.size() < 20)
            recent.append(name);
}
bool SearchHistory::record(const QString &name) {
    if (!allowed.contains(name)) return false;
    recent.removeAll(name);
    recent.prepend(name);
    while (recent.size() > 20) recent.removeLast();
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "WeatherProject", "Weather");
    settings.setValue("search/recentCities", recent);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning() << "[Weather] Could not save search history:" << settings.fileName();
        return false;
    }
    return true;
}
