#include "textsyntaxhighlighter.h"

#include <QFileInfo>

namespace {

QStringList cFamilyKeywords() {
    return {
        "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue",
        "return", "class", "struct", "enum", "union", "namespace", "template", "typename",
        "public", "private", "protected", "virtual", "override", "final", "static", "const",
        "constexpr", "inline", "new", "delete", "this", "nullptr", "true", "false", "void",
        "int", "long", "short", "char", "bool", "float", "double", "unsigned", "signed",
        "auto", "using", "typedef", "sizeof", "try", "catch", "throw", "goto", "extern",
        // java / js / ts / kotlin / swift / go / rust extras
        "function", "var", "let", "def", "fn", "func", "package", "import", "export",
        "interface", "implements", "extends", "instanceof", "typeof", "null", "undefined",
        "async", "await", "yield", "pub", "mut", "impl", "trait", "match", "loop", "type",
        "final", "abstract", "synchronized", "boolean", "byte", "String", "val"
    };
}

QStringList pythonKeywords() {
    return {
        "False", "None", "True", "and", "as", "assert", "async", "await", "break", "class",
        "continue", "def", "del", "elif", "else", "except", "finally", "for", "from", "global",
        "if", "import", "in", "is", "lambda", "nonlocal", "not", "or", "pass", "raise",
        "return", "try", "while", "with", "yield", "self", "cls"
    };
}

QStringList shellKeywords() {
    return {
        "if", "then", "elif", "else", "fi", "for", "in", "do", "done", "while", "until",
        "case", "esac", "function", "select", "return", "break", "continue", "local",
        "export", "readonly", "shift", "source", "alias", "set", "unset", "echo", "exit",
        "end", "begin", "switch", "and", "or", "not" // fish
    };
}

QStringList sqlKeywords() {
    return {
        "select", "from", "where", "insert", "into", "values", "update", "set", "delete",
        "create", "table", "index", "view", "drop", "alter", "add", "primary", "key",
        "foreign", "references", "join", "inner", "left", "right", "outer", "on", "as",
        "and", "or", "not", "null", "is", "in", "like", "order", "by", "group", "having",
        "limit", "offset", "distinct", "union", "all", "exists", "between", "case", "when",
        "then", "else", "end", "default", "constraint", "unique", "cascade"
    };
}

QStringList cmakeKeywords() {
    return {
        "if", "else", "elseif", "endif", "foreach", "endforeach", "while", "endwhile",
        "function", "endfunction", "macro", "endmacro", "set", "unset", "list", "string",
        "message", "include", "add_executable", "add_library", "add_subdirectory",
        "target_link_libraries", "target_include_directories", "target_sources",
        "target_compile_definitions", "target_compile_features", "find_package",
        "install", "option", "project", "cmake_minimum_required", "add_test", "return"
    };
}

} // namespace

TextSyntaxHighlighter::TextSyntaxHighlighter(QTextDocument *doc, const QString &fileName, bool darkTheme)
    : QSyntaxHighlighter(doc)
{
    // fixed palettes tuned separately for dark & light backgrounds
    if(darkTheme) {
        commentFormat.setForeground(QColor(106, 130, 106));
        keywordFormat.setForeground(QColor(130, 170, 255));
        stringFormat.setForeground(QColor(195, 165, 105));
        numberFormat.setForeground(QColor(210, 140, 210));
        metaFormat.setForeground(QColor(120, 200, 200));
    } else {
        commentFormat.setForeground(QColor(90, 125, 90));
        keywordFormat.setForeground(QColor(30, 80, 180));
        stringFormat.setForeground(QColor(140, 100, 30));
        numberFormat.setForeground(QColor(150, 50, 150));
        metaFormat.setForeground(QColor(20, 130, 130));
    }
    commentFormat.setFontItalic(true);
    keywordFormat.setFontWeight(QFont::Bold);

    buildRules(fileName);
}

void TextSyntaxHighlighter::buildRules(const QString &filePath) {
    const QFileInfo fi(filePath);
    const QString suffix = fi.suffix().toLower();
    const QString name = fi.fileName().toLower();

    auto addKeywords = [this](const QStringList &words, Qt::CaseSensitivity cs = Qt::CaseSensitive) {
        QStringList escaped;
        for(const QString &w : words)
            escaped << QRegularExpression::escape(w);
        Rule r;
        r.pattern = QRegularExpression("\\b(" + escaped.join('|') + ")\\b",
                                       cs == Qt::CaseInsensitive ? QRegularExpression::CaseInsensitiveOption
                                                                 : QRegularExpression::NoPatternOption);
        r.format = keywordFormat;
        rules << r;
    };
    auto addRule = [this](const QString &pattern, const QTextCharFormat &fmt,
                          QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption) {
        Rule r;
        r.pattern = QRegularExpression(pattern, opts);
        r.format = fmt;
        rules << r;
    };
    auto addNumbers = [&]() {
        addRule("\\b(0[xXbB][0-9a-fA-F]+|\\d+(\\.\\d+)?([eE][+-]?\\d+)?)\\b", numberFormat);
    };
    auto addDoubleQuoteStrings = [&]() {
        addRule("\"[^\"\\\\]*(\\\\.[^\"\\\\]*)*\"", stringFormat);
    };
    auto addSingleQuoteStrings = [&]() {
        addRule("'[^'\\\\]*(\\\\.[^'\\\\]*)*'", stringFormat);
    };
    auto enableCBlockComments = [&]() {
        hasBlockComments = true;
        blockCommentStart = QRegularExpression("/\\*");
        blockCommentEnd = QRegularExpression("\\*/");
    };

    static const QSet<QString> cFamily = {
        "c", "h", "cpp", "hpp", "cc", "hh", "cxx", "hxx", "java", "js", "mjs", "ts",
        "jsx", "tsx", "kt", "kts", "swift", "go", "rs", "php", "scss", "css"
    };
    static const QSet<QString> shellFamily = {"sh", "bash", "zsh", "fish"};
    static const QSet<QString> iniFamily = {"ini", "conf", "cfg", "toml", "properties", "desktop", "service"};
    static const QSet<QString> markupFamily = {"xml", "html", "htm", "xhtml", "svg"};

    if(cFamily.contains(suffix)) {
        addKeywords(cFamilyKeywords());
        addNumbers();
        addDoubleQuoteStrings();
        addSingleQuoteStrings();
        addRule("^\\s*#\\s*\\w+", metaFormat);   // preprocessor
        addRule("//[^\n]*", commentFormat);
        enableCBlockComments();
    } else if(suffix == "py" || name == "sconstruct") {
        addKeywords(pythonKeywords());
        addNumbers();
        addDoubleQuoteStrings();
        addSingleQuoteStrings();
        addRule("@\\w+", metaFormat);            // decorators
        addRule("#[^\n]*", commentFormat);
    } else if(shellFamily.contains(suffix) || name == "pkgbuild") {
        addKeywords(shellKeywords());
        addRule("\\$\\{?\\w+\\}?", metaFormat);  // variables
        addDoubleQuoteStrings();
        addSingleQuoteStrings();
        addRule("#[^\n]*", commentFormat);
    } else if(suffix == "json") {
        addRule("\"[^\"\\\\]*(\\\\.[^\"\\\\]*)*\"\\s*:", keywordFormat); // keys
        addDoubleQuoteStrings();
        addNumbers();
        addKeywords({"true", "false", "null"});
    } else if(suffix == "yaml" || suffix == "yml") {
        addRule("^\\s*-?\\s*[\\w.-]+\\s*:", keywordFormat);              // keys
        addDoubleQuoteStrings();
        addSingleQuoteStrings();
        addNumbers();
        addKeywords({"true", "false", "null", "yes", "no", "on", "off"}, Qt::CaseInsensitive);
        addRule("#[^\n]*", commentFormat);
    } else if(iniFamily.contains(suffix)) {
        addRule("^\\s*\\[[^\\]\n]+\\]", metaFormat);                     // [sections]
        addRule("^\\s*[\\w.-]+\\s*(?==)", keywordFormat);                // keys
        addDoubleQuoteStrings();
        addNumbers();
        addRule("^\\s*[;#][^\n]*", commentFormat);
    } else if(markupFamily.contains(suffix)) {
        addRule("</?[\\w:-]+", keywordFormat);                           // tags
        addRule("[\\w:-]+(?==)", metaFormat);                            // attributes
        addDoubleQuoteStrings();
        addSingleQuoteStrings();
        hasBlockComments = true;
        blockCommentStart = QRegularExpression("<!--");
        blockCommentEnd = QRegularExpression("-->");
    } else if(suffix == "md" || suffix == "markdown" || suffix == "rst") {
        addRule("^#{1,6}[^\n]*", keywordFormat);                         // headings
        addRule("\\*\\*[^*\n]+\\*\\*", metaFormat);                      // bold
        addRule("`[^`\n]+`", stringFormat);                              // code spans
        addRule("\\[[^\\]\n]*\\]\\([^)\n]*\\)", numberFormat);           // links
    } else if(suffix == "sql") {
        addKeywords(sqlKeywords(), Qt::CaseInsensitive);
        addNumbers();
        addSingleQuoteStrings();
        addRule("--[^\n]*", commentFormat);
        enableCBlockComments();
    } else if(suffix == "cmake" || name == "cmakelists.txt") {
        addKeywords(cmakeKeywords(), Qt::CaseInsensitive);
        addRule("\\$\\{[^}\n]*\\}", metaFormat);                         // variables
        addDoubleQuoteStrings();
        addRule("#[^\n]*", commentFormat);
    } else if(suffix == "patch" || suffix == "diff") {
        addRule("^\\+[^\n]*", stringFormat);
        addRule("^-[^\n]*", numberFormat);
        addRule("^@@[^\n]*", metaFormat);
        addRule("^(diff|index|---|\\+\\+\\+)[^\n]*", keywordFormat);
    } else if(name == "makefile" || name == "gnumakefile" || suffix == "mk") {
        addRule("^[\\w.-]+\\s*:", keywordFormat);                        // targets
        addRule("\\$\\(?[\\w@<^]+\\)?", metaFormat);                     // variables
        addRule("#[^\n]*", commentFormat);
    } else if(name == "dockerfile") {
        addKeywords({"FROM", "RUN", "CMD", "COPY", "ADD", "ENV", "ARG", "WORKDIR",
                     "EXPOSE", "ENTRYPOINT", "USER", "VOLUME", "LABEL", "SHELL"}, Qt::CaseInsensitive);
        addDoubleQuoteStrings();
        addRule("#[^\n]*", commentFormat);
    } else if(suffix == "log") {
        addRule("\\b(ERROR|FATAL|CRITICAL)\\b[^\n]*", numberFormat);
        addRule("\\b(WARN|WARNING)\\b[^\n]*", stringFormat);
        addRule("\\b(DEBUG|TRACE)\\b", commentFormat);
        addRule("\\d{4}-\\d{2}-\\d{2}[T ]?\\d{0,2}:?\\d{0,2}:?\\d{0,2}", metaFormat);
    }
    // anything else (plain txt, csv, ...): no rules; shown as plain text
}

void TextSyntaxHighlighter::highlightBlock(const QString &text) {
    for(const Rule &rule : rules) {
        auto it = rule.pattern.globalMatch(text);
        while(it.hasNext()) {
            auto match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    if(!hasBlockComments)
        return;

    // multi-line comment handling (state 1 = inside a block comment)
    setCurrentBlockState(0);
    int startIndex = 0;
    if(previousBlockState() != 1) {
        auto m = blockCommentStart.match(text);
        startIndex = m.hasMatch() ? m.capturedStart() : -1;
    }
    while(startIndex >= 0) {
        auto endMatch = blockCommentEnd.match(text, startIndex);
        int commentLength;
        if(!endMatch.hasMatch()) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endMatch.capturedEnd() - startIndex;
        }
        setFormat(startIndex, commentLength, commentFormat);
        auto m = blockCommentStart.match(text, startIndex + commentLength);
        startIndex = m.hasMatch() ? m.capturedStart() : -1;
    }
}
