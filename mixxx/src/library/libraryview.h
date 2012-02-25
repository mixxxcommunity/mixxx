// libraryview.h
// Created 8/28/2009 by RJ Ryan (rryan@mit.edu)
//
// AbstractLibraryView is an abstract interface that all views to be used with the
// Library widget should support.

#ifndef LIBRARYVIEW_H
#define LIBRARYVIEW_H

#include <QString>
#include <QDomNode>

class AbstractLibraryView {
public:
    virtual void setup(QDomNode node) = 0;
    virtual void onSearchStarting() = 0;
    virtual void onSearchCleared() = 0;
    virtual void onSearch(const QString& text) = 0;
    virtual void onShow() = 0;

    // If applicable, requests that the LibraryView load the selected
    // track. Does nothing otherwise.
    virtual void loadSelectedTrack() = 0;

    // If applicable, requests that the LibraryView load the selected track to
    // the specified group. Does nothing otherwise.
    virtual void loadSelectedTrackToGroup(QString group) = 0;

    // If a selection is applicable for this view, request that the selection be
    // increased or decreased by the provided delta. For example, for a value of
    // 1, the view should move to the next selection in the list.
    virtual void moveSelection(int delta) = 0;
};

#endif /* LIBRARYVIEW_H */
