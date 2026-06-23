#pragma once
#include <QSortFilterProxyModel>
#include "log_model.h"

class LogFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText   READ filterText   WRITE setFilterText)
    Q_PROPERTY(QString filterLevel  READ filterLevel  WRITE setFilterLevel)
    Q_PROPERTY(QString filterSource READ filterSource WRITE setFilterSource)
public:
    explicit LogFilterModel(QObject* parent = nullptr);

    QString filterText()   const { return m_text; }
    QString filterLevel()  const { return m_level; }
    QString filterSource() const { return m_source; }

    void setFilterText(const QString& text);
    void setFilterLevel(const QString& level);
    void setFilterSource(const QString& source);
    void showErrorsOnly(bool only);
    void setTimeRange(qint64 start_ns, qint64 end_ns);
    void clearFilters();

    int sourceRow(int proxy_row) const;
    LogItem sourceItem(int proxy_row) const;

signals:
    void filterChanged(int visible_count, int total_count);

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString m_text;
    QString m_level;
    QString m_source;
    qint64  m_time_start = 0;
    qint64  m_time_end   = 0;
};
