#ifndef SEARCH_HISTORY_H
#define SEARCH_HISTORY_H
#include <QStringList>
class SearchHistory {
public:
    explicit SearchHistory(const QStringList &validNames);
    QStringList entries() const { return recent; }
    bool record(const QString &name);
private:
    QStringList allowed;
    QStringList recent;
};
#endif
