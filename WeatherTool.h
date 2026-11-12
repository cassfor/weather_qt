#ifndef WEATHERTOOL_H
#define WEATHERTOOL_H
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <map>
#include <QFile>
#include <QCoreApplication>

class WeatherTool
{
public:
    QStringList cityNames() const {
        QStringList names;
        for (const auto &entry : m_mapCity2Code)
            names.append(entry.first);
        return names;
    }
    WeatherTool(){
        QJsonParseError err;
        QFile file(":/data/citycode-2019-08-23.json");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        QByteArray json = file.readAll();
        file.close();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(json,&err);
        if (err.error != QJsonParseError::NoError || !jsonDoc.isArray())
            return;
        QJsonArray citys = jsonDoc.array();
        for(int i=0;i<citys.size();i++)
        {
            QString code = citys.at(i).toObject().value("city_code").toString();
            QString city = citys.at(i).toObject().value("city_name").toString();
            if(code.size()>0){
                m_mapCity2Code.insert(std::pair<QString,QString>(city,code));
            }
        }
    }
    QString operator[](const QString& city){
        const QString name = city.trimmed();
        std::map<QString,QString>::iterator it = m_mapCity2Code.find(name);
        if(it==m_mapCity2Code.end()){
            it = m_mapCity2Code.find(name+u8"市");
        }
        if(it!=m_mapCity2Code.end()){
            return it->second;
        }
        return "000000000";
    }
private:
    std::map<QString,QString> m_mapCity2Code;
};

#endif // WEATHERTOOL_H


