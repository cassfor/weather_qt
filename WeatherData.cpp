#include "WeatherData.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDate>
#include <QTime>
#include <QRegularExpression>

static QString optionalNumber(const QJsonValue &v) {
    return v.isDouble() && v.toDouble() >= 0 ? QString::number(v.toDouble()) : QString::fromUtf8(u8"暂无数据");
}
bool parseWeatherData(const QByteArray &bytes, const QString &expectedCode, WeatherData &result, QString &error)
{
    QJsonParseError jsonError;
    const auto doc = QJsonDocument::fromJson(bytes, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !doc.isObject()) {
        error = QString::fromUtf8(u8"天气数据格式异常"); return false;
    }
    const auto jsObj = doc.object();
    if (jsObj.value("status").toInt() != 200 || !jsObj.value("message").toString().contains("success")) {
        error = QString::fromUtf8(u8"天气服务未返回有效结果"); return false;
    }
    const auto info = jsObj.value("cityInfo").toObject();
    const auto data = jsObj.value("data").toObject();
    const auto days = data.value("forecast").toArray();
    bool temperatureOk = false;
    data.value("wendu").toString().toDouble(&temperatureOk);
    if (info.value("citykey").toString() != expectedCode || info.value("city").toString().isEmpty()
        || !QDate::fromString(jsObj.value("date").toString(), "yyyyMMdd").isValid()
        || !temperatureOk || days.size() < 5 || !data.value("yesterday").isObject()) {
        error = QString::fromUtf8(u8"天气关键数据缺失或城市不匹配"); return false;
    }
    const QRegularExpression highPattern(QString::fromUtf8(u8"^高温\\s+(-?[0-9]+(?:\\.[0-9]+)?)℃$"));
    const QRegularExpression lowPattern(QString::fromUtf8(u8"^低温\\s+(-?[0-9]+(?:\\.[0-9]+)?)℃$"));
    for (int i = -1; i < 5; ++i) {
        const auto day = i < 0 ? data.value("yesterday").toObject() : days.at(i).toObject();
        if (!QDate::fromString(day.value("ymd").toString(), "yyyy-MM-dd").isValid()
            || !highPattern.match(day.value("high").toString()).hasMatch()
            || !lowPattern.match(day.value("low").toString()).hasMatch()) {
            error = QString::fromUtf8(u8"天气日期或温度数据异常"); return false;
        }
    }
    WeatherData parsed;
    QString dateStr = jsObj.value("date").toString();
    parsed.today.date = QDate::fromString(dateStr, "yyyyMMdd").toString("yyyy-MM-dd");
    parsed.today.city = jsObj.value("cityInfo").toObject().value("city").toString();

    // 解析data
    QJsonObject dataObj = jsObj.value("data").toObject();
    parsed.today.shidu = dataObj.value("shidu").toString();
    parsed.today.pm25 = optionalNumber(dataObj.value("pm25"));
    parsed.today.quality = dataObj.value("quality").toString();
    parsed.today.wendu = dataObj.value("wendu").toString() + u8"°";
    parsed.today.ganmao = dataObj.value("ganmao").toString();

    // 解析data中的yesterday
    QJsonObject yestObj = dataObj.value("yesterday").toObject();
    parsed.forecast[0].date = yestObj.value("ymd").toString();
    parsed.forecast[0].high = yestObj.value("high").toString();
    parsed.forecast[0].low = yestObj.value("low").toString();
    parsed.forecast[0].aqi = optionalNumber(yestObj.value("aqi"));
    parsed.forecast[0].type = yestObj.value("type").toString();

    // 解析data中的forecast
    QJsonArray forecastArr = dataObj.value("forecast").toArray();
    int j = 0;
    for (int i = 1; i < 6; i++)
    {
        QJsonObject dateObj = forecastArr.at(j).toObject();
        parsed.forecast[i].date = dateObj.value("ymd").toString();
        parsed.forecast[i].aqi = optionalNumber(dateObj.value("aqi"));
        parsed.forecast[i].high = dateObj.value("high").toString();
        parsed.forecast[i].low = dateObj.value("low").toString();
        parsed.forecast[i].type = dateObj.value("type").toString();
        j++;
    }

    // 取得今日数据
    QJsonObject todayObj = forecastArr.at(0).toObject();
    parsed.today.fx = todayObj.value("fx").toString();
    parsed.today.fl = todayObj.value("fl").toString();
    parsed.today.type = todayObj.value("type").toString();
    parsed.today.sunrise = todayObj.value("sunrise").toString();
    parsed.today.sunset = todayObj.value("sunset").toString();
    parsed.today.notice = todayObj.value("notice").toString();


    QString *optional[] = {&parsed.today.shidu, &parsed.today.quality, &parsed.today.ganmao,
        &parsed.today.fx, &parsed.today.fl, &parsed.today.type, &parsed.today.notice};
    for (auto value : optional) if (value->isEmpty()) *value = QString::fromUtf8(u8"暂无数据");
    for (auto &day : parsed.forecast) if (day.type.isEmpty()) day.type = QString::fromUtf8(u8"暂无数据");
    if (!QTime::fromString(parsed.today.sunrise, "HH:mm").isValid()
        || !QTime::fromString(parsed.today.sunset, "HH:mm").isValid()) {
        parsed.today.sunrise.clear(); parsed.today.sunset.clear();
    }
    result = parsed;
    error.clear();
    return true;
}
