#pragma once
#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>
#include <QMutex>
#include <deque>
#include <QColor>

struct LogItem {
    QString     id;
    QString     source;
    QString     raw;
    qint64      timestamp_ns;
    QString     level;
    QString     message;
    QString     module;
    QVariantMap fields;
    QVariantMap tags;
    QVariantMap meta;
    bool        has_ai_report = false;
    QString     ai_summary;
};

enum LogModelRole {
    IdRole            = Qt::UserRole + 1,
    SourceRole,
    RawRole,
    TimestampRole,
    LevelRole,
    MessageRole,
    ModuleRole,
    FieldsRole,
    TagsRole,
    MetaRole,
    HasAiReportRole,
    AiSummaryRole,
    LevelColorRole,
    IsErrorRole
};

class LogModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum CapacityPreset {
        DefaultCapacity = 10000,
        PerformanceMode = 50000,
        DebugMode       = 100000
    };

    explicit LogModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendLog(const LogItem& item);
    void appendLogs(const QVector<LogItem>& items);
    void clear();

    qint64 earliestTimestamp() const;
    qint64 latestTimestamp()   const;
    int    totalCount()        const;
    int    errorCount()        const;

    LogItem at(int row) const;

    void setCapacity(int max_items);
    int  capacity() const;

signals:
    void logCountChanged(int current, int total);
    void errorCountChanged(int count);
    void bufferTrimmed(int removed);

private:
    void enforce_capacity();
    static QColor level_color(const QString& level);

    std::deque<LogItem>  m_items;
    mutable QMutex       m_mutex;
    int    m_capacity     = DefaultCapacity;
    qint64 m_earliest_ts  = 0;
    qint64 m_latest_ts    = 0;
    int    m_count_total  = 0;
    int    m_error_count  = 0;
};
