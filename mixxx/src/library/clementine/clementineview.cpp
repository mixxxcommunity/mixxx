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
#include "library/libraryfilterwidget.h"
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

    //m_libraryViewContainer->filter()->SetSettingsGroup(kSettingsGroup);
    m_libraryViewContainer->filter()->SetLibraryModel(library->model());

}

void ClementineView::setupLibraryFilerWidget() {

    // Library filter widget
    //QActionGroup* library_view_group = new QActionGroup(this);

    //library_show_all_ = library_view_group->addAction(tr("Show all songs"));
    //library_show_duplicates_ = library_view_group->addAction(tr("Show only duplicates"));
    //library_show_untagged_ = library_view_group->addAction(tr("Show only untagged"));

    //library_show_all_->setCheckable(true);
    //library_show_duplicates_->setCheckable(true);
    //library_show_untagged_->setCheckable(true);
    //library_show_all_->setChecked(true);

    //connect(library_view_group, SIGNAL(triggered(QAction*)), SLOT(ChangeLibraryQueryMode(QAction*)));

    //QAction* library_config_action = new QAction(
    //    IconLoader::Load("configure"), tr("Configure library..."), this);
    //connect(library_config_action, SIGNAL(triggered()), SLOT(ShowLibraryConfig()));
    //m_libraryViewContainer->filter()->SetSettingsGroup(kSettingsGroup);
    //m_libraryViewContainer->filter()->SetLibraryModel(library_->model());

    //QAction* separator = new QAction(this);
    //separator->setSeparator(true);

    //m_libraryViewContainer->filter()->AddMenuAction(library_show_all_);
    //m_libraryViewContainer->filter()->AddMenuAction(library_show_duplicates_);
    //m_libraryViewContainer->filter()->AddMenuAction(library_show_untagged_);
    //m_libraryViewContainer->filter()->AddMenuAction(separator);
    //m_libraryViewContainer->filter()->AddMenuAction(library_config_action);

}
