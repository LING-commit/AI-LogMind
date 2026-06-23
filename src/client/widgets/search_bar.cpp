#include "search_bar.h"
#include <QHBoxLayout>
#include <QDateTimeEdit>
#include <QLabel>
#include <QTimer>

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(6);

    m_search_input = new QLineEdit(this);
    m_search_input->setPlaceholderText(QString::fromUtf8(
        "\u641c\u7d22\u65e5\u5fd7... (\u652f\u6301\u5173\u952e\u5b57/\u6765\u6e90/\u7ea7\u522b)"));
    m_search_input->setClearButtonEnabled(true);
    m_debounce_timer = new QTimer(this);
    m_debounce_timer->setSingleShot(true);
    m_debounce_timer->setInterval(300);

    connect(m_search_input, &QLineEdit::textChanged, this, [this]() {
        m_debounce_timer->start();
    });
    connect(m_debounce_timer, &QTimer::timeout, this, [this]() {
        onTextChanged(m_search_input->text());
    });

    m_level_combo = new QComboBox(this);
    m_level_combo->addItems({"\u5168\u90e8", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"});
    connect(m_level_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SearchBar::onLevelSelected);

    m_source_combo = new QComboBox(this);
    m_source_combo->addItem("\u5168\u90e8\u6765\u6e90");
    m_source_combo->setEditable(true);
    connect(m_source_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SearchBar::onSourceSelected);

    auto initialNow = QDateTime::currentDateTime();
    m_time_start = new QDateTimeEdit(initialNow.addSecs(-3600), this);
    m_time_start->setDisplayFormat("MM-dd HH:mm");
    m_time_start->setCalendarPopup(true);
    m_time_end = new QDateTimeEdit(initialNow, this);
    m_time_end->setDisplayFormat("MM-dd HH:mm");
    m_time_end->setCalendarPopup(true);

    connect(m_time_start, &QDateTimeEdit::dateTimeChanged, this, &SearchBar::onTimeRangeChanged);
    connect(m_time_end,   &QDateTimeEdit::dateTimeChanged, this, &SearchBar::onTimeRangeChanged);

    auto* quick1h = new QPushButton("\u8fd11\u5c0f\u65f6", this);
    quick1h->setFixedHeight(24);
    connect(quick1h, &QPushButton::clicked, this, [this]() {
        auto now = QDateTime::currentDateTime();
        m_time_start->setDateTime(now.addSecs(-3600));
        m_time_end->setDateTime(now);
    });

    auto* quick6h = new QPushButton("\u8fd16\u5c0f\u65f6", this);
    quick6h->setFixedHeight(24);
    connect(quick6h, &QPushButton::clicked, this, [this]() {
        auto now = QDateTime::currentDateTime();
        m_time_start->setDateTime(now.addSecs(-21600));
        m_time_end->setDateTime(now);
    });

    m_clear_btn = new QPushButton("\u6e05\u9664", this);
    m_clear_btn->setFixedHeight(24);
    connect(m_clear_btn, &QPushButton::clicked, this, [this]() {
        m_search_input->clear();
        m_level_combo->setCurrentIndex(0);
        m_time_start->setDateTime(QDateTime::currentDateTime().addSecs(-3600));
        m_time_end->setDateTime(QDateTime::currentDateTime());
        m_source_combo->setCurrentIndex(0);
        emit filterChanged("");
        emit levelFilterChanged("");
        emit sourceFilterChanged("");
    });

    layout->addWidget(new QLabel("\U0001F50D", this));
    layout->addWidget(m_search_input, 1);
    layout->addWidget(m_level_combo);
    layout->addWidget(m_time_start);
    layout->addWidget(new QLabel("\u2192", this));
    layout->addWidget(m_time_end);
    layout->addWidget(quick1h);
    layout->addWidget(quick6h);
    layout->addWidget(m_clear_btn);
}

void SearchBar::addSource(const QString& source) {
    if (source.isEmpty()) return;
    for (int i = 1; i < m_source_combo->count(); ++i)
        if (m_source_combo->itemText(i) == source) return;
    m_source_combo->addItem(source);
}

void SearchBar::onTextChanged(const QString& text) {
    emit filterChanged(text);
}

void SearchBar::onLevelSelected(int index) {
    static const QStringList levels = {"", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    emit levelFilterChanged(levels[index]);
}

void SearchBar::onSourceSelected(int index) {
    if (index <= 0) {
        emit sourceFilterChanged("");
    } else {
        emit sourceFilterChanged(m_source_combo->itemText(index));
    }
}

void SearchBar::onTimeRangeChanged() {
    qint64 start_ns = m_time_start->dateTime().toMSecsSinceEpoch() * 1000000;
    qint64 end_ns   = m_time_end->dateTime().toMSecsSinceEpoch() * 1000000;
    emit timeRangeChanged(start_ns, end_ns);
}
