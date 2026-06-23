#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTimer>

class QDateTimeEdit;

class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr);
    void setFilterText(const QString& text) { m_search_input->setText(text); }
    void addSource(const QString& source);

signals:
    void filterChanged(const QString& text);
    void levelFilterChanged(const QString& level);
    void sourceFilterChanged(const QString& source);
    void timeRangeChanged(qint64 start_ns, qint64 end_ns);

private slots:
    void onTextChanged(const QString& text);
    void onLevelSelected(int index);
    void onSourceSelected(int index);
    void onTimeRangeChanged();

private:
    QLineEdit*      m_search_input;
    QComboBox*      m_level_combo;
    QComboBox*      m_source_combo;
    QDateTimeEdit*  m_time_start;
    QDateTimeEdit*  m_time_end;
    QPushButton*    m_clear_btn;
    QTimer*         m_debounce_timer;
};
