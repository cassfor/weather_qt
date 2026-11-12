#pragma once
#include "WeatherData.h"
#include <QDateTime>

struct CachedWeather {
    WeatherData data;
    QDateTime fetchedAt;
    bool expired = false;
};

class WeatherCache {
public:
    explicit WeatherCache(const QString &directory = QString());
    bool load(const QString &code, CachedWeather &result, QString &error,
              const QDateTime &now = QDateTime::currentDateTimeUtc()) const;
    bool save(const QString &code, const QByteArray &body, const QDateTime &fetchedAt,
              QString &error) const;
    static bool isExpired(const WeatherData &data, const QDateTime &fetchedAt,
                          const QDateTime &now = QDateTime::currentDateTimeUtc());
private:
    QString directory;
};
