#include "log_filter_model.h"

LogFilterModel::LogFilterModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortRole(TimestampRole);
}

void LogFilterModel::setFilterText(const QString& text) {
    m_text = text;
    beginFilterChange();
    endFilterChange();
}

void LogFilterModel::setFilterLevel(const QString& level) {
    m_level = level;
    beginFilterChange();
    endFilterChange();
}

void LogFilterModel::setFilterSource(const QString& source) {
    m_source = source;
    beginFilterChange();
    endFilterChange();
}

void LogFilterModel::showErrorsOnly(bool only) {
    m_level = only ? "ERROR" : QString();
    beginFilterChange();
    endFilterChange();
}

void LogFilterModel::setTimeRange(qint64 start_ns, qint64 end_ns) {
    m_time_start = start_ns;
    m_time_end   = end_ns;
    beginFilterChange();
    endFilterChange();
}

void LogFilterModel::clearFilters() {
    m_text.clear();
    m_level.clear();
    m_source.clear();
    m_time_start = 0;
    m_time_end   = 0;
    beginFilterChange();
    endFilterChange();
}

int LogFilterModel::sourceRow(int proxy_row) const {
    return mapToSource(index(proxy_row, 0)).row();
}

LogItem LogFilterModel::sourceItem(int proxy_row) const {
    auto* model = qobject_cast<LogModel*>(sourceModel());
    return model->at(sourceRow(proxy_row));
}

bool LogFilterModel::filterAcceptsRow(int row, const QModelIndex& parent) const {
    auto* src = sourceModel();
    auto idx  = src->index(row, 0, parent);

    auto level   = src->data(idx, LevelRole).toString();
    auto source  = src->data(idx, SourceRole).toString();
    auto message = src->data(idx, MessageRole).toString();
    auto raw     = src->data(idx, RawRole).toString();
    auto ts      = src->data(idx, TimestampRole).toDateTime()
                   .toMSecsSinceEpoch() * 1000000;

    if (!m_level.isEmpty() && level != m_level)
        return false;

    if (!m_source.isEmpty() && source != m_source)
        return false;

    if (m_time_start > 0 && ts < m_time_start)
        return false;
    if (m_time_end   > 0 && ts > m_time_end)
        return false;

    if (!m_text.isEmpty()) {
        const auto query = m_text.toLower();
        bool matched = message.toLower().contains(query)
                    || source.toLower().contains(query)
                    || level.toLower().contains(query)
                    || raw.toLower().contains(query);
        if (!matched) return false;
    }

    return true;
}

bool LogFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    auto lts = left.data(TimestampRole).toDateTime();
    auto rts = right.data(TimestampRole).toDateTime();
    return lts < rts;
}
