#include <QtCore>
#include <QtGui>
#include <QtSql>
#include <QDateTime>
#include "library/trackcollection.h"
#include "library/playlisttablemodel.h"

#include "mixxxutils.cpp"

PlaylistTableModel::PlaylistTableModel(QObject* parent,
                                       TrackCollection* pTrackCollection)
        : BaseSqlTableModel(parent, pTrackCollection,
                            pTrackCollection->getDatabase(),
                            "mixxx.db.model.playlist"),
          m_pTrackCollection(pTrackCollection),
          m_playlistDao(m_pTrackCollection->getPlaylistDAO()),
          m_trackDao(m_pTrackCollection->getTrackDAO()),
          m_iPlaylistId(-1) {
    connect(this, SIGNAL(doSearch(const QString&)),
            this, SLOT(slotSearch(const QString&)));
}

PlaylistTableModel::~PlaylistTableModel() {
}

void PlaylistTableModel::setPlaylist(int playlistId) {
    qDebug() << "PlaylistTableModel::setPlaylist" << playlistId;

    if (m_iPlaylistId == playlistId) {
        qDebug() << "Already focused on playlist " << playlistId;
        return;
    }

    m_iPlaylistId = playlistId;

    QString playlistTableName = "playlist_" + QString("%1").arg(m_iPlaylistId);
    QSqlQuery query(m_pTrackCollection->getDatabase());
    //query.prepare("DROP VIEW " + playlistTableName);
    //query.exec();

    // Escape the playlist name
    QSqlDriver* driver = m_pTrackCollection->getDatabase().driver();
    QSqlField playlistNameField("name", QVariant::String);
    playlistNameField.setValue(playlistTableName);

    QStringList columns;
    columns << PLAYLISTTRACKSTABLE_TRACKID
            << PLAYLISTTRACKSTABLE_POSITION;

    QString queryString = QString(
        "CREATE TEMPORARY VIEW IF NOT EXISTS %1 AS "
        "SELECT %2 FROM %3 WHERE playlist_id = %4")
            .arg(driver->formatValue(playlistNameField))
            .arg(columns.join(","))
            .arg("PlaylistTracks")
            .arg(playlistId);
    query.prepare(queryString);

    if (!query.exec()) {
        // It's normal for this to fail.
        qDebug() << query.executedQuery() << query.lastError();
    }

    // Print out any SQL error, if there was one.
    if (query.lastError().isValid()) {
     	qDebug() << __FILE__ << __LINE__ << query.lastError();
    }

    setTable(playlistTableName, columns[0], columns,
             m_pTrackCollection->getTrackSource("default"));
    initHeaderData();
    setSearch("", LibraryTableModel::DEFAULT_LIBRARYFILTER);
    setDefaultSort(fieldIndex("position"), Qt::AscendingOrder);
}

bool PlaylistTableModel::addTrack(const QModelIndex& index, QString location) {
    const int positionColumn = fieldIndex(PLAYLISTTRACKSTABLE_POSITION);
    int position = index.sibling(index.row(), positionColumn).data().toInt();

    // Handle weird cases like a drag-and-drop to an invalid index
    if (position <= 0) {
        position = rowCount() + 1;
    }

    // If a track is dropped but it isn't in the library, then add it because
    // the user probably dropped a file from outside Mixxx into this playlist.
    QFileInfo fileInfo(location);

    // Adds track, does not insert duplicates, handles unremoving logic.
    int trackId = m_trackDao.addTrack(fileInfo, true);

    // Do nothing if the location still isn't in the database.
    if (trackId < 0) {
        return false;
    }

    m_playlistDao.insertTrackIntoPlaylist(trackId, m_iPlaylistId, position);

    // TODO(rryan) signal an add to the base, don't select
    select(); //Repopulate the data model.
    return true;
}

TrackPointer PlaylistTableModel::getTrack(const QModelIndex& index) const {
    //FIXME: use position instead of location for playlist tracks?

    //const int locationColumnIndex = this->fieldIndex(LIBRARYTABLE_LOCATION);
    //QString location = index.sibling(index.row(), locationColumnIndex).data().toString();
    int trackId = getTrackId(index);
    return m_trackDao.getTrack(trackId);
}

void PlaylistTableModel::removeTrack(const QModelIndex& index) {
    if (m_playlistDao.isPlaylistLocked(m_iPlaylistId)) {
        return;
    }

    const int positionColumnIndex = fieldIndex(PLAYLISTTRACKSTABLE_POSITION);
    int position = index.sibling(index.row(), positionColumnIndex).data().toInt();
    m_playlistDao.removeTrackFromPlaylist(m_iPlaylistId, position);
    select(); //Repopulate the data model.
}

void PlaylistTableModel::removeTracks(const QModelIndexList& indices) {
    bool locked = m_playlistDao.isPlaylistLocked(m_iPlaylistId);

    if (locked) {
        return;
    }

    const int positionColumnIndex = fieldIndex(PLAYLISTTRACKSTABLE_POSITION);

    QList<int> trackPositions;
    foreach (QModelIndex index, indices) {
        int trackPosition = index.sibling(index.row(), positionColumnIndex).data().toInt();
        trackPositions.append(trackPosition);
    }

    qSort(trackPositions);
    QListIterator<int> iterator(trackPositions);
    iterator.toBack();

    while (iterator.hasPrevious()) {
        m_playlistDao.removeTrackFromPlaylist(m_iPlaylistId, iterator.previous());
    }

    // Have to re-lookup every track b/c their playlist ranks might have changed
    select();
}

void PlaylistTableModel::moveTrack(const QModelIndex& sourceIndex,
                                   const QModelIndex& destIndex) {
    //QSqlRecord sourceRecord = this->record(sourceIndex.row());
    //sourceRecord.setValue("position", destIndex.row());
    //this->removeRows(sourceIndex.row(), 1);

    //this->insertRecord(destIndex.row(), sourceRecord);

    //TODO: execute a real query to DELETE the sourceIndex.row() row from the PlaylistTracks table.
    //int newPosition = destIndex.row();
    //int oldPosition = sourceIndex.row();
    //const int positionColumnIndex = this->fieldIndex(PLAYLISTTRACKSTABLE_POSITION);
    //int newPosition = index.sibling(destIndex.row(), positionColumnIndex).data().toInt();
    //int oldPosition = index.sibling(sourceIndex.row(), positionColumnIndex).data().toInt();


    int playlistPositionColumn = fieldIndex(PLAYLISTTRACKSTABLE_POSITION);

    // this->record(destIndex.row()).value(PLAYLISTTRACKSTABLE_POSITION).toInt();
    int newPosition = destIndex.sibling(destIndex.row(), playlistPositionColumn).data().toInt();
    // this->record(sourceIndex.row()).value(PLAYLISTTRACKSTABLE_POSITION).toInt();
    int oldPosition = sourceIndex.sibling(sourceIndex.row(), playlistPositionColumn).data().toInt();


    qDebug() << "old pos" << oldPosition << "new pos" << newPosition;

    //Invalid for the position to be 0 or less.
    if (newPosition < 0)
        return;
    else if (newPosition == 0) //Dragged out of bounds, which is past the end of the rows...
        newPosition = rowCount();

    //Start the transaction
    m_pTrackCollection->getDatabase().transaction();

    //Find out the highest position existing in the playlist so we know what
    //position this track should have.
    QSqlQuery query;

    //Insert the song into the PlaylistTracks table

    /** ALGORITHM for code below
      Case 1: destination < source (newPos < oldPos)
        1) Set position = -1 where pos=source -- Gives that track a dummy index to keep stuff simple.
        2) Decrement position where pos > source
        3) increment position where pos > dest
        4) Set position = dest where pos=-1 -- Move track from dummy pos to final destination.

      Case 2: destination > source (newPos > oldPos)
        1) Set position=-1 where pos=source -- Give track a dummy index again.
        2) Decrement position where pos > source AND pos <= dest
        3) Set postion=dest where pos=-1 -- Move that track from dummy pos to final destination
    */

    QString queryString;
    if (newPosition < oldPosition) {
        queryString =
            QString("UPDATE PlaylistTracks SET position=-1 "
                    "WHERE position=%1 AND "
                    "playlist_id=%2").arg(oldPosition).arg(m_iPlaylistId);
        query.exec(queryString);
        //qDebug() << queryString;

        queryString = QString("UPDATE PlaylistTracks SET position=position-1 "
                            "WHERE position > %1 AND "
                            "playlist_id=%2").arg(oldPosition).arg(m_iPlaylistId);
        query.exec(queryString);

        queryString = QString("UPDATE PlaylistTracks SET position=position+1 "
                            "WHERE position >= %1 AND " //position < %2 AND "
                            "playlist_id=%3").arg(newPosition).arg(m_iPlaylistId);
        query.exec(queryString);

        queryString = QString("UPDATE PlaylistTracks SET position=%1 "
                            "WHERE position=-1 AND "
                            "playlist_id=%2").arg(newPosition).arg(m_iPlaylistId);
        query.exec(queryString);
    }
    else if (newPosition > oldPosition)
    {
        queryString = QString("UPDATE PlaylistTracks SET position=-1 "
                              "WHERE position = %1 AND "
                              "playlist_id=%2").arg(oldPosition).arg(m_iPlaylistId);
        //qDebug() << queryString;
        query.exec(queryString);

        queryString = QString("UPDATE PlaylistTracks SET position=position-1 "
                              "WHERE position > %1 AND position <= %2 AND "
                              "playlist_id=%3").arg(oldPosition).arg(newPosition).arg(m_iPlaylistId);
        query.exec(queryString);

        queryString = QString("UPDATE PlaylistTracks SET position=%1 "
                              "WHERE position=-1 AND "
                              "playlist_id=%2").arg(newPosition).arg(m_iPlaylistId);
        query.exec(queryString);
    }

    m_pTrackCollection->getDatabase().commit();

    //Print out any SQL error, if there was one.
    if (query.lastError().isValid()) {
     	qDebug() << query.lastError();
    }

    select();
}

void PlaylistTableModel::shuffleTracks(const QModelIndex& currentIndex) {
    int numOfTracks = rowCount();
    int seed = QDateTime::currentDateTime().toTime_t();
    qsrand(seed);
    QSqlQuery query(m_pTrackCollection->getDatabase());
    const int positionColumnIndex = fieldIndex(PLAYLISTTRACKSTABLE_POSITION);
    int currentPosition = currentIndex.sibling(currentIndex.row(), positionColumnIndex).data().toInt();
    int shuffleStartIndex = currentPosition + 1;

    m_pTrackCollection->getDatabase().transaction();

    // This is a simple Fisher-Yates shuffling algorithm
    for (int i=numOfTracks-1; i >= shuffleStartIndex; i--)
    {
        int random = int(qrand() / (RAND_MAX + 1.0) * (numOfTracks + 1 - shuffleStartIndex) + shuffleStartIndex);
        qDebug() << "Swapping tracks " << i << " and " << random;
        QString swapQuery = "UPDATE PlaylistTracks SET position=%1 WHERE position=%2 AND playlist_id=%3";
        query.exec(swapQuery.arg(-1).arg(i).arg(m_iPlaylistId));
        query.exec(swapQuery.arg(i).arg(random).arg(m_iPlaylistId));
        query.exec(swapQuery.arg(random).arg(-1).arg(m_iPlaylistId));

        if (query.lastError().isValid())
            qDebug() << query.lastError();
    }

    m_pTrackCollection->getDatabase().commit();

    select();
}

void PlaylistTableModel::search(const QString& searchText) {
    // qDebug() << "PlaylistTableModel::search()" << searchText
    //          << QThread::currentThread();
    emit(doSearch(searchText));
}

void PlaylistTableModel::slotSearch(const QString& searchText) {
    BaseSqlTableModel::search(
        searchText, LibraryTableModel::DEFAULT_LIBRARYFILTER);
}

bool PlaylistTableModel::isColumnInternal(int column) {
    if (column == fieldIndex(PLAYLISTTRACKSTABLE_TRACKID) ||
        column == fieldIndex(LIBRARYTABLE_PLAYED) ||
        column == fieldIndex(LIBRARYTABLE_MIXXXDELETED) ||
        column == fieldIndex(TRACKLOCATIONSTABLE_FSDELETED)) {
        return true;
    }
    return false;
}
bool PlaylistTableModel::isColumnHiddenByDefault(int column) {
    if (column == fieldIndex(LIBRARYTABLE_KEY)) {
        return true;
    }
    return false;
}

QItemDelegate* PlaylistTableModel::delegateForColumn(const int i) {
    return NULL;
}

TrackModel::CapabilitiesFlags PlaylistTableModel::getCapabilities() const {
    TrackModel::CapabilitiesFlags caps = TRACKMODELCAPS_RECEIVEDROPS |
            TRACKMODELCAPS_REORDER | TRACKMODELCAPS_ADDTOCRATE |
            TRACKMODELCAPS_ADDTOPLAYLIST | TRACKMODELCAPS_RELOADMETADATA;

    // Only allow Add to AutoDJ if we aren't currently showing the AutoDJ queue.
    if (m_iPlaylistId != m_playlistDao.getPlaylistIdFromName(AUTODJ_TABLE)) {
        caps |= TRACKMODELCAPS_ADDTOAUTODJ;
    }

    bool locked = m_playlistDao.isPlaylistLocked(m_iPlaylistId);

    if (locked) {
        caps |= TRACKMODELCAPS_LOCKED;
    }

    return caps;
}
