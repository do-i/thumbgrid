#include "centralwidget.h"

#include <utility>
#include "components/actionmanager/actionmanager.h"
#include "utils/logging.h"

CentralWidget::CentralWidget(std::shared_ptr<DocumentWidget> _docWidget, std::shared_ptr<FolderViewProxy> _folderView, QWidget *parent)
    : QStackedWidget(parent),
      documentView(std::move(_docWidget)),
      folderView(std::move(_folderView))
{
    setMouseTracking(true);
    if(!documentView || !folderView)
        qCWarning(logGui) << "[CentralWidget] Error: child widget is null. We will crash now.  Bye.";

    // docWidget - 0, folderView - 1
    addWidget(documentView.get());
    if(folderView)
        addWidget(folderView.get());
    showDocumentView();
}

void CentralWidget::showDocumentView() {
    if(mode == MODE_DOCUMENT)
        return;
    mode = MODE_DOCUMENT;
    if(actionManager)
        actionManager->setContext(mode);
    setCurrentIndex(0);
    widget(0)->setFocus();
    documentView->viewWidget()->startPlayback();
}

void CentralWidget::showFolderView() {
    if(mode == MODE_FOLDERVIEW)
        return;

    mode = MODE_FOLDERVIEW;
    if(actionManager)
        actionManager->setContext(mode);
    setCurrentIndex(1);
    widget(1)->show();
    widget(1)->setFocus();
    documentView->viewWidget()->stopPlayback();
}

void CentralWidget::toggleViewMode() {
    (mode == MODE_DOCUMENT) ? showFolderView() : showDocumentView();
}

ViewMode CentralWidget::currentViewMode() {
    return mode;
}
