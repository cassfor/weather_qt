#ifndef FAVORITE_CITIES_H
#define FAVORITE_CITIES_H
#include "WeatherTool.h"
#include <QSettings>
class FavoriteCities {
public:
    // Optional settings file allows deterministic persistence-failure tests.
    explicit FavoriteCities(const WeatherTool &cities, const QString &settingsFile = QString());
    QStringList entries() const { return codes; }
    bool contains(const QString &code) const { return codes.contains(code); }
    bool add(const QString &code, QString &error);
    bool remove(const QString &code, QString &error);
private:
    bool save(QString &error);
    const WeatherTool &cities;
    QSettings settings;
    QStringList codes;
};
#endif
