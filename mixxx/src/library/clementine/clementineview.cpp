// clementineview.cpp
// Created 2/25/2011 by Daniel Schürmann (daschuer@gmx.de)
//
// This shows the Clementine-Player library in Mixxx


#include "clementineview.h"

// from clementine
#include "library/libraryviewcontainer.h"
#include "library/libraryview.h"
#include "library/library.h"
#include "library/librarymodel.h"
#include "core/taskmanager.h"

#include <QSortFilterProxyModel>


ClementineView::ClementineView(QWidget* parent)
    : QWidget(parent),
      m_libraryViewContainer(new LibraryViewContainer(this)),
      m_librarySortModel(new QSortFilterProxyModel(this)) {



}

ClementineView::~ClementineView() {
}

void ClementineView::connectLibrary(Library* library, TaskManager* task_manager) {

    qDebug() << "Creating models";
    m_librarySortModel->setSourceModel((QAbstractItemModel*)library->model());
    m_librarySortModel->setSortRole(LibraryModel::Role_SortText);
    m_librarySortModel->setDynamicSortFilter(true);
    m_librarySortModel->sort(0);


    m_libraryViewContainer->view()->setModel((QAbstractItemModel *)m_librarySortModel);
    m_libraryViewContainer->view()->SetLibrary(library->model());
    m_libraryViewContainer->view()->SetTaskManager(task_manager);
    // m_libraryViewContainer->view()->SetDeviceManager(devices_);
    // m_libraryViewContainer->view()->SetCoverProviders(cover_providers_);
}

