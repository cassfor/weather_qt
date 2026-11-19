#include "FavoriteCities.h"
FavoriteCities::FavoriteCities(const WeatherTool &cities, const QString &settingsFile)
    : cities(cities), settings(settingsFile.isEmpty()
        ? QSettings(QSettings::IniFormat, QSettings::UserScope, "WeatherProject", "Weather").fileName()
        : settingsFile, QSettings::IniFormat) {
    for (const QString &code : settings.value("favorites/cityCodes").toStringList())
        if (!cities.cityName(code).isEmpty() && !codes.contains(code) && codes.size() < 20)
            codes.append(code);
}
bool FavoriteCities::save(QString &error) {
    settings.setValue("favorites/cityCodes", codes);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        error = QString::fromUtf8(u8"收藏保存失败：本次操作已在当前窗口生效，重启后可能无法保留");
        return false;
    }
    return true;
}
bool FavoriteCities::add(const QString &code, QString &error) {
    error.clear();
    if (cities.cityName(code).isEmpty()) {
        error = QString::fromUtf8(u8"无法收藏未知城市"); return false;
    }
    if (codes.contains(code)) return save(error);
    if (codes.size() >= 20) {
        error = QString::fromUtf8(u8"最多收藏 20 个城市，请先移除一个城市"); return false;
    }
    codes.append(code);
    return save(error);
}
bool FavoriteCities::remove(const QString &code, QString &error) {
    error.clear();
    codes.removeAll(code);
    return save(error);
}
