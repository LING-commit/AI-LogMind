#include "log_model.h"
#include <QHash>

LogModel::LogModel(QObject* parent)
    : QAbstractListModel(parent) {}

int LogModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    QMutexLocker l(&m_mutex);
    return static_cast<int>(m_items.size());
}

QHash<int, QByteArray> LogModel::roleNames() const {
    return {
        {IdRole,            "logId"},
        {SourceRole,        "source"},
        {RawRole,           "raw"},
        {TimestampRole,     "timestamp"},
        {LevelRole,         "level"},
        {MessageRole,       "message"},
        {ModuleRole,        "module"},
        {FieldsRole,        "fields"},
        {TagsRole,          "tags"},
        {MetaRole,          "meta"},
        {HasAiReportRole,   "hasAiReport"},
        {AiSummaryRole,     "aiSummary"},
        {LevelColorRole,    "levelColor"},
        {IsErrorRole,       "isError"}
    };
}

QVariant LogModel::data(const QModelIndex& index, int role) const {
    QMutexLocker l(&m_mutex);
    if (!index.isValid() || index.row() >= static_cast<int>(m_items.size()))
        return {};

    const auto& item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case IdRole:            return item.id;
    case SourceRole:        return item.source;
    case RawRole:           return item.raw;
    case TimestampRole:     return QDateTime::fromMSecsSinceEpoch(item.timestamp_ns / 1000000);
    case LevelRole:         return item.level;
    case MessageRole:       return item.message;
    case ModuleRole:        return item.module;
    case FieldsRole:        return item.fields;
    case TagsRole:          return item.tags;
    case MetaRole:          return item.meta;
    case HasAiReportRole:   return item.has_ai_report;
    case AiSummaryRole:     return item.ai_summary;
    case LevelColorRole:    return level_color(item.level);
    case IsErrorRole:       return item.level == "ERROR" || item.level == "FATAL";
    default:                return {};
    }
}

void LogModel::appendLog(const LogItem& item) {
    int row;
    {
        QMutexLocker l(&m_mutex);
        row = static_cast<int>(m_items.size());
    }
    beginInsertRows({}, row, row);
    {
        QMutexLocker l(&m_mutex);
        m_items.push_back(item);
        if (m_items.size() == 1)
            m_earliest_ts = item.timestamp_ns;
        m_latest_ts = item.timestamp_ns;
        m_count_total++;
        if (item.level == "ERROR" || item.level == "FATAL")
            m_error_count++;
    }
    endInsertRows();

    if (item.level == "ERROR" || item.level == "FATAL")
        emit errorCountChanged(m_error_count);

    enforce_capacity();
    emit logCountChanged(static_cast<int>(m_items.size()), m_count_total);
}

void LogModel::appendLogs(const QVector<LogItem>& items) {
    if (items.isEmpty()) return;

    int first, last;
    {
        QMutexLocker l(&m_mutex);
        first = static_cast<int>(m_items.size());
        last = first + static_cast<int>(items.size()) - 1;
    }
    beginInsertRows({}, first, last);
    {
        QMutexLocker l(&m_mutex);
        for (const auto& item : items) {
            m_items.push_back(item);
            if (m_items.size() == 1) m_earliest_ts = item.timestamp_ns;
            if (item.level == "ERROR" || item.level == "FATAL") m_error_count++;
        }
        m_latest_ts = items.last().timestamp_ns;
        m_count_total += static_cast<int>(items.size());
    }
    endInsertRows();

    enforce_capacity();
    emit logCountChanged(static_cast<int>(m_items.size()), m_count_total);
    emit errorCountChanged(m_error_count);
}

void LogModel::clear() {
    beginResetModel();
    {
        QMutexLocker l(&m_mutex);
        m_items.clear();
        m_earliest_ts = 0;
        m_latest_ts = 0;
        m_count_total = 0;
        m_error_count = 0;
    }
    endResetModel();
}

void LogModel::enforce_capacity() {
    int over;
    {
        QMutexLocker l(&m_mutex);
        if (m_items.size() <= static_cast<size_t>(m_capacity)) return;
        over = static_cast<int>(m_items.size()) - m_capacity;
    }
    beginRemoveRows({}, 0, over - 1);
    {
        QMutexLocker l(&m_mutex);
        for (int i = 0; i < over; ++i)
            m_items.pop_front();
        if (!m_items.empty())
            m_earliest_ts = m_items.front().timestamp_ns;
    }
    endRemoveRows();
    emit bufferTrimmed(over);
}

void LogModel::setCapacity(int max_items) {
    QMutexLocker l(&m_mutex);
    m_capacity = qMax(1000, max_items);
}

LogItem LogModel::at(int row) const {
    QMutexLocker l(&m_mutex);
    return m_items[static_cast<size_t>(row)];
}

qint64 LogModel::earliestTimestamp() const {
    QMutexLocker l(&m_mutex);
    return m_earliest_ts;
}

qint64 LogModel::latestTimestamp() const {
    QMutexLocker l(&m_mutex);
    return m_latest_ts;
}

int LogModel::totalCount() const {
    QMutexLocker l(&m_mutex);
    return m_count_total;
}

int LogModel::errorCount() const {
    QMutexLocker l(&m_mutex);
    return m_error_count;
}

int LogModel::capacity() const {
    QMutexLocker l(&m_mutex);
    return m_capacity;
}

QColor LogModel::level_color(const QString& level) {
    if (level == "FATAL")  return QColor("#FF1744");
    if (level == "ERROR")  return QColor("#FF6D00");
    if (level == "WARN")   return QColor("#FFD600");
    if (level == "INFO")   return QColor("#00E676");
    if (level == "DEBUG")  return QColor("#90A4AE");
    return QColor("#FFFFFF");
}
