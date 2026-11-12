#include "WeatherCache.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDebug>

static bool validCode(const QString &code) {
    return QRegularExpression("^[0-9]{9}$").match(code).hasMatch();
}
WeatherCache::WeatherCache(const QString &path)
    : directory(path.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                              + "/weather-cache" : path) {}

bool WeatherCache::isExpired(const WeatherData &data, const QDateTime &at, const QDateTime &now) {
    return !at.isValid() || at > now || at.msecsTo(now) > 30 * 60 * 1000
        || QDate::fromString(data.today.date, "yyyy-MM-dd") != now.toOffsetFromUtc(8 * 3600).date();
}
bool WeatherCache::load(const QString &code, CachedWeather &result, QString &error, const QDateTime &now) const {
    error.clear();
    auto fail = [&](const QString &reason) { error = reason; qInfo() << "[WeatherCache]" << code << reason; return false; };
    if (!validCode(code)) return fail("invalid city code");
    QFile file(QDir(directory).filePath(code + ".json"));
    if (!file.open(QIODevice::ReadOnly)) return fail(file.errorString());
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return fail("invalid cache JSON");
    const auto obj = doc.object();
    if (obj.value("version").toDouble() != 1 || obj.value("cityCode").toString() != code)
        return fail("unsupported version or city mismatch");
    CachedWeather candidate;
    const QString timestamp = obj.value("fetchedAt").toString();
    candidate.fetchedAt = QDateTime::fromString(timestamp, Qt::ISODateWithMs);
    if (!timestamp.endsWith('Z') || !candidate.fetchedAt.isValid() || candidate.fetchedAt > now)
        return fail("invalid or future timestamp");
    if (!obj.value("response").isString()
        || !parseWeatherData(obj.value("response").toString().toUtf8(), code, candidate.data, error))
        return fail("invalid weather payload: " + error);
    candidate.expired = isExpired(candidate.data, candidate.fetchedAt, now);
    result = candidate;
    error.clear();
    return true;
}
bool WeatherCache::save(const QString &code, const QByteArray &body, const QDateTime &at, QString &error) const {
    WeatherData checked;
    if (!validCode(code) || !at.isValid()) { error = "invalid cache metadata"; return false; }
    if (!parseWeatherData(body, code, checked, error)) return false;
    if (!QDir().mkpath(directory)) { error = "cannot create cache directory"; return false; }
    QSaveFile file(QDir(directory).filePath(code + ".json"));
    if (!file.open(QIODevice::WriteOnly)) { error = file.errorString(); return false; }
    const QByteArray bytes = QJsonDocument(QJsonObject{
        {"version", 1}, {"cityCode", code}, {"fetchedAt", at.toUTC().toString(Qt::ISODateWithMs)},
        {"response", QString::fromUtf8(body)}}).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit()) { error = file.errorString(); return false; }
    error.clear();
    return true;
}
