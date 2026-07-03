#pragma once

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QTextCharFormat>

// Built-in fallback highlighter used when KSyntaxHighlighting is not available.
// Generic rule-based highlighting: comments, strings, numbers and a per-language
// keyword set chosen from the file suffix. Intentionally approximate - it aims
// for "readable code preview", not editor-grade accuracy.
class TextSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    TextSyntaxHighlighter(QTextDocument *doc, const QString &fileName, bool darkTheme);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    void buildRules(const QString &fileName);
    QList<Rule> rules;
    // multi-line comment support
    QRegularExpression blockCommentStart, blockCommentEnd;
    QTextCharFormat commentFormat, keywordFormat, stringFormat, numberFormat, metaFormat;
    bool hasBlockComments = false;
};
