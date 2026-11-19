#include "../weather.h"
#include "TestSupport.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>

class Stage4Test {
public:
static int run() {
    WeatherTool tool;
    const QString bj = QString::fromUtf8(u8"北京"), sh = QString::fromUtf8(u8"上海");
    const QString b = tool[bj], s = tool[sh];
    CHECK(tool.cityName(b) == bj && tool.cityName("invalid").isEmpty());
    QTemporaryDir dir;
    CHECK(dir.isValid());
    const QString file = dir.path()+"/favorites.ini";
    QString error;
    FavoriteCities f(tool,file);
    CHECK(f.entries().isEmpty());
    CHECK(!f.add("invalid",error) && !error.isEmpty());
    CHECK(f.add(b,error)); CHECK(f.add(s,error)); CHECK(f.add(b,error));
    CHECK(f.entries() == (QStringList{b,s}));
    CHECK(FavoriteCities(tool,file).entries() == f.entries());
    CHECK(f.remove(b,error)); CHECK(!f.contains(b));
    CHECK(FavoriteCities(tool,file).entries() == (QStringList{s}));
    QStringList codes;
    for (const QString &name : tool.cityNames()) {
        if (!codes.contains(tool[name])) codes.append(tool[name]);
        if (codes.size() == 21) break;
    }
    QSettings corrupt(file,QSettings::IniFormat);
    corrupt.setValue("favorites/cityCodes",QStringList{"invalid",codes[0]}+codes);
    corrupt.sync();
    FavoriteCities filtered(tool,file);
    CHECK(filtered.entries() == codes.mid(0,20));
    CHECK(!filtered.add(codes[20],error) && error.contains("20"));
    CHECK(filtered.remove(codes[0],error)); CHECK(filtered.add(codes[20],error));
    CHECK(filtered.entries().last() == codes[20]);
    // A settings path beneath a regular file deterministically cannot be written.
    FavoriteCities blocked(tool,file+"/blocked.ini");
    CHECK(!blocked.add(b,error) && blocked.contains(b));
    CHECK(error.contains(QString::fromUtf8(u8"收藏保存失败")));
    CHECK(!blocked.remove(b,error) && !blocked.contains(b));

    QSettings settings(QSettings::IniFormat,QSettings::UserScope,"WeatherProject","Weather");
    settings.clear(); settings.sync();
    Manager network;
    network.responses.append({"",0,200,false});
    Weather w(nullptr,dir.path()+"/cache",&network);
    w.stopRequest();
    auto favorite = w.findChild<QPushButton*>("favoriteBt");
    auto view = w.findChild<QPushButton*>("favoriteViewBt");
    auto remove = w.findChild<QPushButton*>("favoriteRemoveBt");
    auto combo = w.findChild<QComboBox*>("favoritesComboBox");
    CHECK(favorite && view && remove && combo);
    CHECK(!favorite->isEnabled() && !view->isEnabled() && !remove->isEnabled());
    CHECK(combo->currentText() == QString::fromUtf8(u8"暂无收藏城市"));
    CHECK(w.city == QString::fromUtf8(u8"长沙"));
    network.responses.append({fixture(b),200,10,false});
    w.requestWeather(bj); QTest::qWait(150);
    CHECK(w.displayedCity == bj && favorite->isEnabled());
    const auto history = w.searchHistory;
    favorite->click(); CHECK(w.favorites.contains(b));
    CHECK(w.searchHistory == history && combo->currentData().toString() == b);
    CHECK(favorite->text() == QString::fromUtf8(u8"取消收藏"));
    CHECK(w.favorites.add(s,error)); w.updateFavoriteControls();
    const int requests = network.requests;
    combo->setCurrentIndex(combo->findData(s));
    QTest::keyClick(combo,Qt::Key_Up); QTest::keyClick(combo,Qt::Key_Down);
    CHECK(network.requests == requests);
    combo->setCurrentIndex(combo->findData(s));
    network.responses.append({"",503,10,false}); view->click(); QTest::qWait(150);
    CHECK(w.displayedCity == bj && w.failedCity == sh);
    favorite->click(); CHECK(!w.favorites.contains(b) && w.favorites.contains(s));
    favorite->click(); CHECK(w.favorites.contains(b));
    CHECK(w.searchHistory == history);
    // Obtain Shanghai once so subsequent favorite queries exercise cache-first.
    network.responses.append({fixture(s),200,10,false});
    w.findChild<QPushButton*>("retryBt")->click(); QTest::qWait(150);
    CHECK(w.displayedCity == sh);
    const auto afterSuccess = w.searchHistory;
    combo->setCurrentIndex(combo->findData(b));
    network.responses.append({fixture(b),200,200,true}); view->click();
    CHECK(w.displayedCity == bj && w.displayedFromCache);
    CHECK(favorite->isEnabled());
    combo->setCurrentIndex(combo->findData(s));
    network.responses.append({fixture(s),200,10,false}); view->click(); QTest::qWait(250);
    CHECK(w.displayedCity == sh && !w.displayedFromCache);
    CHECK(w.searchHistory == afterSuccess);
    // Removal during an active request must not cancel it or clear its weather.
    network.responses.append({fixture(s),200,80,false}); view->click();
    CHECK(w.displayedFromCache);
    remove->click(); CHECK(w.service->isActive() && w.displayedCity == sh);
    CHECK(!w.favorites.contains(s)); QTest::qWait(150);
    CHECK(w.displayedCity == sh && !w.displayedFromCache);
    CHECK(favorite->text() == QString::fromUtf8(u8"收藏"));
    network.responses.append({fixture(s),200,10,false});
    w.on_refreshBt_clicked(); QTest::qWait(150); CHECK(w.failedCity.isEmpty());
    // Timeout and bad data preserve cached Beijing; retry still targets Beijing.
    combo->setCurrentIndex(combo->findData(b));
    w.service->setTimeout(20);
    network.responses.append({fixture(b),200,100,false}); view->click(); QTest::qWait(150);
    CHECK(w.displayedFromCache && w.failedCity == bj);
    w.service->setTimeout(10000);
    network.responses.append({"bad",200,10,false});
    w.findChild<QPushButton*>("retryBt")->click(); QTest::qWait(150);
    CHECK(w.displayedFromCache && w.failedCity == bj);
    network.responses.append({"",0,10,false}); view->click(); QTest::qWait(150);
    CHECK(w.displayedFromCache && w.failedCity == bj);
    network.responses.append({fixture(b),200,10,false});
    w.findChild<QPushButton*>("retryBt")->click(); QTest::qWait(150);
    CHECK(!w.displayedFromCache && w.failedCity.isEmpty());
    w.finishFavoriteChange(false,QString::fromUtf8(u8"收藏保存失败：重启后可能无法保留"));
    CHECK(!w.findChild<QLabel*>("favoriteStatusLb")->isHidden());
    // Layout and keyboard smoke checks, plus images for visual inspection.
    w.show(); w.resize(1000,650); QTest::qWait(100);
    CHECK(view->width() >= view->sizeHint().width());
    favorite->setFocus(); QTest::keyClick(favorite,Qt::Key_Space);
    CHECK(!w.favorites.contains(b));
    CHECK(!view->isEnabled() && !remove->isEnabled());
    w.grab().save("stage4-empty.png");
    favorite->click(); CHECK(w.favorites.contains(b));
    w.finishFavoriteChange(true,QString());
    w.resize(800,450); QTest::qWait(100); w.grab().save("stage4-compact.png");
    w.resize(1200,750); QTest::qWait(100); w.grab().save("stage4-expanded.png");
    w.close();
    Manager restartedNetwork;
    restartedNetwork.responses.append({"",0,200,false});
    Weather restarted(nullptr,dir.path()+"/cache",&restartedNetwork);
    CHECK(restarted.favorites.entries() == (QStringList{b}));
    CHECK(restarted.city == bj && restarted.displayedFromCache);
    CHECK(restarted.findChild<QComboBox*>("cityComboBox")->currentText().isEmpty());
    CHECK(restarted.searchHistory == w.searchHistory);
    restarted.close();
    qInfo() << "Stage 4 checks passed";
    return 0;
}
};
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QTemporaryDir settings;
    if (!settings.isValid()) return 2;
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    return Stage4Test::run();
}
