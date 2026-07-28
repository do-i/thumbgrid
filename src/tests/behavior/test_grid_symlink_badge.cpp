#include "support/thumbgrid_test_support.h"

#include <QDir>
#include <QFile>
#include <QGraphicsItem>
#include <QImage>
#include <QPainter>
#include <QTemporaryDir>

#include "core.h"
#include "gui/customwidgets/thumbnailwidget.h"
#include "gui/folderview/foldergridview.h"
#include "gui/mainwindow.h"

// A symlinked entry looks exactly like the file it points at: same picture, same
// size, same label. The only thing telling the two apart in the grid is the
// badge in the cell's bottom-right corner. Nothing else in the cell changes, so a
// regression here (flag lost between the thumbnailer and the widget, badge not
// enabled on the grid's tiles, badge drawn outside the visible cell) is
// invisible to model-state assertions - it has to be read off the painted
// pixels, which is what this test does.
class GridSymlinkBadgeTest : public QObject {
    Q_OBJECT

private slots:
    void symlinkedEntriesAreBadgedInTheGrid();
};

// Grid tiles, in layout order (flow layout: top to bottom, then left to right).
static QList<ThumbnailWidget *> tilesInOrder(FolderGridView *grid) {
    QList<ThumbnailWidget *> tiles;
    for(QGraphicsItem *item : grid->items()) {
        if(auto tile = qgraphicsitem_cast<ThumbnailWidget *>(item))
            tiles << tile;
    }
    std::sort(tiles.begin(), tiles.end(), [](ThumbnailWidget *a, ThumbnailWidget *b) {
        if(a->scenePos().y() != b->scenePos().y())
            return a->scenePos().y() < b->scenePos().y();
        return a->scenePos().x() < b->scenePos().x();
    });
    return tiles;
}

static int loadedTileCount(FolderGridView *grid) {
    int loaded = 0;
    for(ThumbnailWidget *tile : tilesInOrder(grid))
        if(tile->isLoaded)
            loaded++;
    return loaded;
}

// Paints the grid and returns the bottom-right corner of one cell, which is
// where the badge is drawn. The corner box is kept small enough to stay clear
// of the centered filename label - the baseline assertion below checks that.
static QImage cellCornerOf(FolderGridView *grid, ThumbnailWidget *tile) {
    QImage canvas(grid->viewport()->size(), QImage::Format_ARGB32);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    grid->render(&painter, QRectF(canvas.rect()), grid->viewport()->rect());
    painter.end();

    QRect cell = grid->mapFromScene(tile->sceneBoundingRect()).boundingRect();
    QRect corner(0, 0, qRound(cell.width() * 0.25), qRound(cell.height() * 0.22));
    corner.moveBottomRight(cell.bottomRight());
    return canvas.copy(corner.intersected(canvas.rect()));
}

void GridSymlinkBadgeTest::symlinkedEntriesAreBadgedInTheGrid() {
    QTemporaryDir fixture;
    QVERIFY2(fixture.isValid(), "Test fixture directory should be created.");

    QDir root(fixture.path());
    QVERIFY2(root.mkpath("gallery"), "Gallery folder should be created.");
    const QString galleryPath = root.filePath("gallery");

    // Two identical regular pictures give a baseline: their cells must paint the
    // same, so any difference found on the third cell comes from the badge.
    const QString plainPath = root.filePath("gallery/1-plain.png");
    QVERIFY2(tgtest::writeImage(plainPath, Qt::red), "Plain picture should be written.");
    QVERIFY2(tgtest::writeImage(root.filePath("gallery/2-twin.png"), Qt::red), "Twin picture should be written.");
    QVERIFY2(QFile::link(plainPath, root.filePath("gallery/3-link.png")), "Symlink to the plain picture should be created.");

    Core core;
    QVERIFY2(core.loadPath(galleryPath), "Opening the gallery folder should succeed.");
    core.showGui();

    QTRY_VERIFY2(tgtest::mainWindow() != nullptr, "The thumbgrid window should exist.");
    MW *window = tgtest::mainWindow();
    QTRY_VERIFY2(window->isVisible(), "The thumbgrid window should be visible.");
    QCOMPARE(window->currentViewMode(), MODE_FOLDERVIEW);

    auto grid = window->findChild<FolderGridView *>("thumbnailGrid");
    QVERIFY2(grid != nullptr, "The folder thumbnail grid should exist.");

    QTRY_COMPARE(grid->itemCount(), 4); // ".." + the three pictures
    QTRY_COMPARE(loadedTileCount(grid), 4);
    QTRY_VERIFY2(grid->updatesEnabled(), "The grid must be painting before its pixels are read.");

    QList<ThumbnailWidget *> tiles = tilesInOrder(grid);
    QCOMPARE(tiles.count(), 4);
    // index 0 is the ".." parent tile; the pictures follow in name order
    QImage plain = cellCornerOf(grid, tiles.at(1));
    QImage twin = cellCornerOf(grid, tiles.at(2));
    QImage link = cellCornerOf(grid, tiles.at(3));

    QVERIFY2(!plain.isNull() && !link.isNull(), "Cell corners should have been painted.");
    QCOMPARE(plain.size(), twin.size());
    QCOMPARE(plain.size(), link.size());
    // Two regular files with different names but the same picture: their corner
    // boxes must match, which both gives the baseline and proves the box holds
    // nothing but the badge (no label text bleeding into it).
    QVERIFY2(plain == twin,
             "Two regular pictures must paint the same cell corner - otherwise the badge check below proves nothing.");
    QVERIFY2(plain != link, "A symlinked picture must be badged in its cell corner.");

    int changed = 0;
    for(int y = 0; y < plain.height(); y++)
        for(int x = 0; x < plain.width(); x++)
            if(plain.pixel(x, y) != link.pixel(x, y))
                changed++;
    // A badge, not a repainted corner: it covers a small part of the box.
    QVERIFY2(changed > 8, "The badge should cover more than a few stray pixels.");
    QVERIFY2(changed < plain.width() * plain.height() / 2,
             "The badge should stay a small corner mark, not take over the cell corner.");
}

TG_BEHAVIOR_TEST_MAIN(GridSymlinkBadgeTest)

#include "test_grid_symlink_badge.moc"
