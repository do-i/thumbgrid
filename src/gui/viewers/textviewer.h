#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QLabel>

// Read-only text viewer with syntax highlighting.
// Uses KSyntaxHighlighting when built with it, otherwise falls back to a
// lightweight built-in regex highlighter (textsyntaxhighlighter.h).
class TextViewer : public QWidget {
    Q_OBJECT
public:
    explicit TextViewer(QWidget *parent = nullptr);

    bool showFile(const QString &filePath);
    void clear();

private slots:
    void readSettings();

private:
    void applyHighlighter(const QString &filePath);
    QVBoxLayout layout;
    QPlainTextEdit *textEdit;
    QLabel *noticeLabel; // "truncated" banner for very large files
    QObject *highlighter = nullptr;

    static const qint64 maxDisplayBytes = 1024 * 1024; // 1 MiB is plenty for a preview
};
