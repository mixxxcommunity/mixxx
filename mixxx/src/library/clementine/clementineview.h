/*
 * ClementineView.h
 *
 *  Created on: 25.02.2012
 *      Author: daniel
 */

#ifndef CLEMENTINEVIEW_H_
#define CLEMENTINEVIEW_H_

#include <QtCore/qobject.h>

#include <trackinfoobject.h>

#include "library/abstractlibraryview.h"

// From clementine-player
#include "library/libraryviewcontainer.h"

class QSortFilterProxyModel;
class LibraryViewContainer;
class Library;
class TaskManager;
class QModelIndex;
class QMimeData;
class ClementineFeature;
class TrackCollection;
class Song;
class QUrl;

class ClementineView: public QWidget, public virtual AbstractLibraryView {
    Q_OBJECT
public:
    ClementineView(QWidget* parent, TrackCollection* pTrackCollection);
    virtual ~ClementineView();

    void setup(QDomNode node) {};
    void onSearchStarting() {};
    void onSearchCleared() {};
    void onSearch(const QString& text) {};
    void onShow() {};

    // If applicable, requests that the LibraryView load the selected
    // track. Does nothing otherwise.
    void loadSelectedTrack() {};

    // If applicable, requests that the LibraryView load the selected track to
    // the specified group. Does nothing otherwise.
    void loadSelectedTrackToGroup(QString group) {};

    // If a selection is applicable for this view, request that the selection be
    // increased or decreased by the provided delta. For example, for a value of
    // 1, the view should move to the next selection in the list.
    void moveSelection(int delta) {};

    void connectLibrary(Library* library, TaskManager* task_manager);

//signals:
//    void loadTrack(TrackPointer tio);

  public slots:
    void slotLibraryViewRightClicked(QMimeData* data);
    void slotSendToAutoDJ();
    void slotSendToAutoDJTop();
    void loadSelectionToGroup(QString group);
//    void loadTrackToPlayer(TrackPointer, QString);

private:
    void setupLibraryFilerWidget();
    void addToAutoDJ(bool bTop);
    TrackPointer getTrack(const Song& song);
    TrackPointer getTrack(const QUrl& url);

    LibraryViewContainer* m_libraryViewContainer;
    QSortFilterProxyModel* m_librarySortModel;
    TrackCollection* m_pTrackCollection;

    QMimeData* m_pData;
};

#endif /* CLEMENTINEVIEW_H_ */
