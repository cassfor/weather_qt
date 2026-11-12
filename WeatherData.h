#pragma once
#include <QString>
#include <QByteArray>
struct Today
{
    QString date;
    QString wendu;
    QString city;
    QString shidu;
    QString pm25;
    QString quality;
    QString ganmao;
    QString fx;
    QString fl;
    QString type;
    QString sunrise;
    QString sunset;
    QString notice;
};

struct Forecast
{
    QString date;
    QString high;
    QString low;
    QString aqi;
    QString type;
};

struct WeatherData { Today today; Forecast forecast[6]; };
bool parseWeatherData(const QByteArray &bytes, const QString &expectedCode, WeatherData &result, QString &error);
