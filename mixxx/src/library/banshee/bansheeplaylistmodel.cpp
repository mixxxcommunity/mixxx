#include <QtAlgorithms>
#include <QtDebug>
#include <QTime>


#include "library/banshee/bansheeplaylistmodel.h"
#include "library/banshee/bansheedbconnection.h"
#include "mixxxutils.cpp"
#include "library/starrating.h"
#include "track/beatfactory.h"
#include "track/beats.h"


const bool sDebug = false;

BansheePlaylistModel::BansheePlaylistModel(QObject* pParent, TrackCollection* pTrackCollection, BansheeDbConnection* pConnection)
        :  TrackModel(pTrackCollection->getDatabase(),
                "mixxx.db.model.banshee_playlist"),
           QAbstractTableModel(pParent),
           m_iSortColumn(0),
           m_eSortOrder(Qt::AscendingOrder),
           m_pTrackCollection(pTrackCollection),
           m_trackDAO(m_pTrackCollection->getTrackDAO()),
           m_pConnection(pConnection),
           m_playlistId(-1)
{
    initHeaderData();
}

BansheePlaylistModel::~BansheePlaylistModel() {

}

void BansheePlaylistModel::initHeaderData() {
    // Set the column heading labels, rename them for translations and have
    // proper capitalization

    m_headerList.append(qMakePair(QString(tr("#")), VIEW_ORDER));
    m_headerList.append(qMakePair(QString(tr("Artist")), ARTIST));
    m_headerList.append(qMakePair(QString(tr("Title")), TITLE));
    m_headerList.append(qMakePair(QString(tr("Duration")), DURATION));
    m_headerList.append(qMakePair(QString(tr("Uri")), URI));

 /*
    m_headerList.append(qMakePair(QString(tr("Album")),      offsetof(Itdb_Track, album)));
    m_headerList.append(qMakePair(QString(tr("Year")),       offsetof(Itdb_Track, year)));

    m_headerList.append(qMakePair(QString(tr("Rating")),     offsetof(Itdb_Track, rating)));
    m_headerList.append(qMakePair(QString(tr("Genre")),      offsetof(Itdb_Track, genre)));
    m_headerList.append(qMakePair(QString(tr("Type")),       offsetof(Itdb_Track, filetype)));
    m_headerList.append(qMakePair(QString(tr("Track #")),    offsetof(Itdb_Track, track_nr)));
    m_headerList.append(qMakePair(QString(tr("Date Added")), offsetof(Itdb_Track, time_added)));
    m_headerList.append(qMakePair(QString(tr("BPM")),        offsetof(Itdb_Track, BPM)));
    m_headerList.append(qMakePair(QString(tr("Bitrate")),    offsetof(Itdb_Track, bitrate)));
    m_headerList.append(qMakePair(QString(tr("Location")),   offsetof(Itdb_Track, banshee_path)));
    m_headerList.append(qMakePair(QString(tr("Comment")),    offsetof(Itdb_Track, comment)));
*/
}

QVariant BansheePlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    if (   orientation == Qt::Horizontal
        && role == Qt::DisplayRole
        && section < m_headerList.size()
    ) {
        return QVariant(m_headerList.at(section).first);
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}


void BansheePlaylistModel::initDefaultSearchColumns() {
    QStringList searchColumns;
    searchColumns << "artist"
                  << "album"
                  << "location"
                  << "comment"
                  << "title";
    setSearchColumns(searchColumns);
}

void BansheePlaylistModel::setSearchColumns(const QStringList& searchColumns) {
    m_searchColumns = searchColumns;

    // Convert all the search column names to their field indexes because we use
    // them a bunch.
    m_searchColumnIndices.resize(m_searchColumns.size());
    for (int i = 0; i < m_searchColumns.size(); ++i) {
        m_searchColumnIndices[i] = fieldIndex(m_searchColumns[i]);
    }
}

int BansheePlaylistModel::findSortInsertionPoint(int trackId, TrackPointer pTrack,
                                              const QVector<QPair<int, QHash<int, QVariant> > >& rowInfo) {
    QVariant trackValue = getTrackValueForColumn(trackId, m_iSortColumn, pTrack);

    int min = 0;
    int max = rowInfo.size()-1;

    if (sDebug) {
        qDebug() << this << "Trying to insertion sort:"
                 << trackValue << "min" << min << "max" << max;
    }

    while (min <= max) {
        int mid = min + (max - min) / 2;
        const QPair<int, QHash<int, QVariant> >& otherRowInfo = rowInfo[mid];
        int otherTrackId = otherRowInfo.first;
        // const QHash<int, QVariant>& otherRowCache = otherRowInfo.second; // not used


        // This should not happen, but it's a recoverable error so we should only log it.
        if (!m_recordCache.contains(otherTrackId)) {
            qDebug() << "WARNING: track" << otherTrackId << "was not in index";
//           updateTrackInIndex(otherTrackId);
        }

        QVariant tableValue = getTrackValueForColumn(otherTrackId, m_iSortColumn);
        int compare = compareColumnValues(m_iSortColumn, m_eSortOrder, trackValue, tableValue);

        if (sDebug) {
            qDebug() << this << "Comparing" << trackValue
                     << "to" << tableValue << ":" << compare;
        }

        if (compare == 0) {
            // Alright, if we're here then we can insert it here and be
            // "correct"
            min = mid;
            break;
        } else if (compare > 0) {
            min = mid + 1;
        } else {
            max = mid - 1;
        }
    }
    return min;
}


const QString BansheePlaylistModel::currentSearch() const {
    return m_currentSearch;
}

void BansheePlaylistModel::setSort(int column, Qt::SortOrder order) {
    if (sDebug) {
        qDebug() << this << "setSort()";
    }

    m_iSortColumn = column;
    m_eSortOrder = order;
}

void BansheePlaylistModel::sort(int column, Qt::SortOrder order) {
    if (sDebug) {
        qDebug() << this << "sort()" << column << order;
    }

    m_iSortColumn = column;
    m_eSortOrder = order;

    emit layoutAboutToBeChanged();
//     qSort(m_sortedPlaylist.begin(), m_sortedPlaylist.end(), columnLessThan);
    emit layoutChanged();
}

int BansheePlaylistModel::rowCount(const QModelIndex& parent) const {

    if (!parent.isValid()) {
        return m_sortedPlaylist.size();
    }
    return 0;
}

int BansheePlaylistModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_headerList.size();
}

int BansheePlaylistModel::fieldIndex(const QString& fieldName) const {
    // Usually a small list, so O(n) is small
    //return m_queryRecord.indexOf(fieldName);
    //return m_columnNames.indexOf(fieldName);
    QHash<QString, int>::const_iterator it = m_columnIndex.constFind(fieldName);
    if (it != m_columnIndex.end()) {
        return it.value();
    }
    return -1;
}

QVariant BansheePlaylistModel::data(const QModelIndex& index, int role) const {
    // qDebug() << this << "data()";

    QVariant value = QVariant();

    if (    role != Qt::DisplayRole
         && role != Qt::EditRole
         && role != Qt::CheckStateRole
         && role != Qt::ToolTipRole
    ) {
        return value;
    }

    int row = index.row();
    if (row < m_sortedPlaylist.size()) {
        int duration;
        switch (m_headerList.at(index.column()).second) {
        case VIEW_ORDER:
            value = m_sortedPlaylist.at(row).viewOrder;
            break;
        case ARTIST:
            value = m_sortedPlaylist.at(row).pArtist->name;
            break;
        case TITLE:
            value = m_sortedPlaylist.at(row).pTrack->title;
            break;
        case DURATION:
            duration = m_sortedPlaylist.at(row).pTrack->duration;
            if (duration) {
                value = MixxxUtils::millisecondsToMinutes(duration, true);
            }
            break;
        case URI:
            value = m_sortedPlaylist.at(row).pTrack->uri;
            break;
        default:
            break; // Not supported
        }
    }


/*
    Itdb_Track* pTrack = getPTrackFromModelIndex(index);
    if (!pTrack) {
        return QVariant();
    }

    size_t structOffset = m_headerList.at(index.column()).second;


    if (!pTrack) {
        return QVariant();
    }

    if (structOffset == 0xffff) { // "#"
        value = m_sortedPlaylist.at(index.row()).pos;
    } else if (structOffset == offsetof(Itdb_Track, year)) {
        if (pTrack->year) {
            value = QVariant(pTrack->year);
        }
    } else if (structOffset == offsetof(Itdb_Track, tracklen)) {
        if (pTrack->tracklen) {
            value = MixxxUtils::millisecondsToMinutes(pTrack->tracklen, true);
        }
    } else if (structOffset == offsetof(Itdb_Track, rating)) {
        value = qVariantFromValue(StarRating(pTrack->rating));
    } else if (structOffset == offsetof(Itdb_Track, track_nr)) {
        if (pTrack->track_nr) {
            value = QVariant(pTrack->track_nr);
        }
    } else if (structOffset == offsetof(Itdb_Track, BPM)) {
        if (pTrack->BPM) {
            value = QVariant(pTrack->BPM);
        }
    } else if (structOffset == offsetof(Itdb_Track, bitrate)) {
        if (pTrack->bitrate) {
            value = QVariant(pTrack->bitrate);
        }
    } else if (structOffset == offsetof(Itdb_Track, time_added)) {
        if (pTrack->time_added) {
            QDateTime timeAdded;
            timeAdded.setTime_t(pTrack->time_added);
            timeAdded.toString(Qt::ISODate);
            value = QVariant(timeAdded.toString(Qt::ISODate));
        }
    } else {
        // for the gchar* elements
        //qDebug() << *(gchar**)((char*)(pTrack) + structOffset);
        QString ret = QString::fromUtf8(*(gchar**)((char*)(pTrack) + structOffset));

        value = QVariant(ret);
    }
*/

/*
    // This value is the value in its most raw form. It was looked up either
    // from the SQL table or from the cached track layer.
    QVariant value = getBaseValue(index, role);
*/


    // Format the value based on whether we are in a tooltip, display, or edit
    // role
    switch (role) {
        case Qt::ToolTipRole:
        case Qt::DisplayRole:
            /*
            if (column == fieldIndex(LIBRARYTABLE_DURATION)) {
                if (qVariantCanConvert<int>(value))
                    value = MixxxUtils::secondsToMinutes(qVariantValue<int>(value));
            } else if (column == fieldIndex(LIBRARYTABLE_RATING)) {
                if (qVariantCanConvert<int>(value))
                    value = qVariantFromValue(StarRating(value.toInt()));
            } else if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
                if (qVariantCanConvert<int>(value))
                    value =  QString("(%1)").arg(value.toInt());
            } else if (column == fieldIndex(LIBRARYTABLE_PLAYED)) {
                // Convert to a bool. Not really that useful since it gets converted
                // right back to a QVariant
                value = (value == "true") ? true : false;
            }
            */
            break;
        case Qt::EditRole:
            /*
            if (column == fieldIndex(LIBRARYTABLE_BPM)) {
                return value.toDouble();
            } else if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
                return index.sibling(row, fieldIndex(LIBRARYTABLE_PLAYED)).data().toBool();
            } else if (column == fieldIndex(LIBRARYTABLE_RATING)) {
                if (qVariantCanConvert<int>(value))
                    value = qVariantFromValue(StarRating(value.toInt()));
            }
            */
            break;
        case Qt::CheckStateRole:
            /*
            if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
                bool played = index.sibling(row, fieldIndex(LIBRARYTABLE_PLAYED)).data().toBool();
                value = played ? Qt::Checked : Qt::Unchecked;
            } else {
            */
            value = QVariant();
            //}
            break;
        default:
            break;
    }
    return value;
}

bool BansheePlaylistModel::setData(const QModelIndex& index, const QVariant& value, int role) {

    if (!index.isValid())
        return false;

    int row = index.row();
    int column = index.column();

    if (sDebug) {
        qDebug() << this << "setData() column:" << column << "value:" << value << "role:" << role;
    }

    // Over-ride sets to TIMESPLAYED and re-direct them to PLAYED
    if (role == Qt::CheckStateRole) {
        if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
            QString val = value.toInt() > 0 ? QString("true") : QString("false");
            QModelIndex playedIndex = index.sibling(index.row(), fieldIndex(LIBRARYTABLE_PLAYED));
            return setData(playedIndex, val, Qt::EditRole);
        }
        return false;
    }

    if (row < 0 || row >= m_rowInfo.size()) {
        return false;
    }

    const QPair<int, QHash<int, QVariant> >& rowInfo = m_rowInfo[row];
    int trackId = rowInfo.first;

    // You can't set something in the table columns because we have no way of
    // persisting it.
    const QHash<int, QVariant>& columns = rowInfo.second;
    if (columns.contains(column)) {
        return false;
    }

    TrackPointer pTrack = m_trackDAO.getTrack(trackId);
    setTrackValueForColumn(pTrack, column, value);

    // Do not save the track here. Changing the track dirties it and the caching
    // system will automatically save the track once it is unloaded from
    // memory. rryan 10/2010
    //m_trackDAO.saveTrack(pTrack);

    return true;
}

TrackModel::CapabilitiesFlags BansheePlaylistModel::getCapabilities() const {
    return TRACKMODELCAPS_NONE
            | TRACKMODELCAPS_ADDTOPLAYLIST
            | TRACKMODELCAPS_ADDTOCRATE
            | TRACKMODELCAPS_ADDTOAUTODJ
            | TRACKMODELCAPS_LOADTODECK
            | TRACKMODELCAPS_LOADTOSAMPLER;
}

Qt::ItemFlags BansheePlaylistModel::flags(const QModelIndex &index) const {
    return readWriteFlags(index);
}

Qt::ItemFlags BansheePlaylistModel::readWriteFlags(const QModelIndex &index) const {
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);

    //Enable dragging songs from this data model to elsewhere (like the waveform
    //widget to load a track into a Player).
    defaultFlags |= Qt::ItemIsDragEnabled;

    //int row = index.row(); // not used
    int column = index.column();

    if ( column == fieldIndex(LIBRARYTABLE_FILETYPE)
         || column == fieldIndex(LIBRARYTABLE_LOCATION)
         || column == fieldIndex(LIBRARYTABLE_DURATION)
         || column == fieldIndex(LIBRARYTABLE_BITRATE)
         || column == fieldIndex(LIBRARYTABLE_DATETIMEADDED)) {
        return defaultFlags;
    } else if (column == fieldIndex(LIBRARYTABLE_TIMESPLAYED)) {
        return defaultFlags | Qt::ItemIsUserCheckable;
    } else {
        return defaultFlags | Qt::ItemIsEditable;
    }
}

Qt::ItemFlags BansheePlaylistModel::readOnlyFlags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    //Enable dragging songs from this data model to elsewhere (like the waveform widget to
    //load a track into a Player).
    defaultFlags |= Qt::ItemIsDragEnabled;

    return defaultFlags;
}

void BansheePlaylistModel::trackChanged(int trackId) {
    if (sDebug) {
        qDebug() << this << "trackChanged" << trackId;
    }
    // TODO(daschuer): eg. use that for played column
    //m_trackOverrides.insert(trackId);
    //QLinkedList<int> rows = getTrackRows(trackId);
/*
    foreach (int row, rows) {
        //qDebug() << "Row in this result set was updated. Signalling update. track:" << trackId << "row:" << row;
        QModelIndex left = index(row, 0);
        QModelIndex right = index(row, columnCount());
        emit(dataChanged(left, right));
    }
*/
}

int BansheePlaylistModel::compareColumnValues(int iColumnNumber, Qt::SortOrder eSortOrder,
                                           QVariant val1, QVariant val2) {
    int result = 0;

    if (iColumnNumber == fieldIndex(PLAYLISTTRACKSTABLE_POSITION) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_BITRATE) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_BPM) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_DURATION) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_TIMESPLAYED) ||
        iColumnNumber == fieldIndex(LIBRARYTABLE_RATING)) {
        // Sort as floats.
        double delta = val1.toDouble() - val2.toDouble();

        if (fabs(delta) < .00001)
            result = 0;
        else if (delta > 0.0)
            result = 1;
        else
            result = -1;
    } else {
        // Default to case-insensitive string comparison
        result = val1.toString().compare(val2.toString(), Qt::CaseInsensitive);
    }

    // If we're in descending order, flip the comparison.
    if (eSortOrder == Qt::DescendingOrder) {
        result = -result;
    }

    return result;
}


QVariant BansheePlaylistModel::getTrackValueForColumn(int trackId, int column, TrackPointer pTrack) const {
    QVariant result;

    // The caller can optionally provide a pTrack if they already looked it
    // up. This is just an optimization to help reduce the # of calls to
    // lookupCachedTrack. If they didn't provide it, look it up.
    if (pTrack) {
        result = getTrackValueForColumn(pTrack, column);
    }

    // If the track lookup failed (could happen for track properties we dont
    // keep track of in Track, like playlist position) look up the value in
    // their SQL record.

    // TODO(rryan) this code is flawed for columns that contains row-specific
    // metadata. Currently the upper-levels will not delegate row-specific
    // columns to this method, but there should still be a check here I think.
    if (!result.isValid()) {
        QHash<int, QVector<QVariant> >::const_iterator it =
                m_recordCache.find(trackId);
        if (it != m_recordCache.end()) {
            const QVector<QVariant>& fields = it.value();
            result = fields.value(column, result);
        }
    }
    return result;
}

QVariant BansheePlaylistModel::getTrackValueForColumn(TrackPointer pTrack, int column) const {
    if (!pTrack)
        return QVariant();

    // TODO(XXX) Qt properties could really help here.
    if (fieldIndex(LIBRARYTABLE_ARTIST) == column) {
        return QVariant(pTrack->getArtist());
    } else if (fieldIndex(LIBRARYTABLE_TITLE) == column) {
        return QVariant(pTrack->getTitle());
    } else if (fieldIndex(LIBRARYTABLE_ALBUM) == column) {
        return QVariant(pTrack->getAlbum());
    } else if (fieldIndex(LIBRARYTABLE_YEAR) == column) {
        return QVariant(pTrack->getYear());
    } else if (fieldIndex(LIBRARYTABLE_GENRE) == column) {
        return QVariant(pTrack->getGenre());
    } else if (fieldIndex(LIBRARYTABLE_FILETYPE) == column) {
        return QVariant(pTrack->getType());
    } else if (fieldIndex(LIBRARYTABLE_TRACKNUMBER) == column) {
        return QVariant(pTrack->getTrackNumber());
    } else if (fieldIndex(LIBRARYTABLE_LOCATION) == column) {
        return QVariant(pTrack->getLocation());
    } else if (fieldIndex(LIBRARYTABLE_COMMENT) == column) {
        return QVariant(pTrack->getComment());
    } else if (fieldIndex(LIBRARYTABLE_DURATION) == column) {
        return pTrack->getDuration();
    } else if (fieldIndex(LIBRARYTABLE_BITRATE) == column) {
        return QVariant(pTrack->getBitrate());
    } else if (fieldIndex(LIBRARYTABLE_BPM) == column) {
        return QVariant(pTrack->getBpm());
    } else if (fieldIndex(LIBRARYTABLE_PLAYED) == column) {
        return QVariant(pTrack->getPlayed());
    } else if (fieldIndex(LIBRARYTABLE_TIMESPLAYED) == column) {
        return QVariant(pTrack->getTimesPlayed());
    } else if (fieldIndex(LIBRARYTABLE_RATING) == column) {
        return pTrack->getRating();
    } else if (fieldIndex(LIBRARYTABLE_KEY) == column) {
        return pTrack->getKey();
    }
    return QVariant();
}

void BansheePlaylistModel::setTrackValueForColumn(TrackPointer pTrack, int column, QVariant value) {
    // TODO(XXX) Qt properties could really help here.
    if (fieldIndex(LIBRARYTABLE_ARTIST) == column) {
        pTrack->setArtist(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_TITLE) == column) {
        pTrack->setTitle(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_ALBUM) == column) {
        pTrack->setAlbum(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_YEAR) == column) {
        pTrack->setYear(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_GENRE) == column) {
        pTrack->setGenre(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_FILETYPE) == column) {
        pTrack->setType(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_TRACKNUMBER) == column) {
        pTrack->setTrackNumber(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_LOCATION) == column) {
        pTrack->setLocation(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_COMMENT) == column) {
        pTrack->setComment(value.toString());
    } else if (fieldIndex(LIBRARYTABLE_DURATION) == column) {
        pTrack->setDuration(value.toInt());
    } else if (fieldIndex(LIBRARYTABLE_BITRATE) == column) {
        pTrack->setBitrate(value.toInt());
    } else if (fieldIndex(LIBRARYTABLE_BPM) == column) {
        //QVariant::toFloat needs >= QT 4.6.x
        pTrack->setBpm((float) value.toDouble());
    } else if (fieldIndex(LIBRARYTABLE_PLAYED) == column) {
        pTrack->setPlayed(value.toBool());
    } else if (fieldIndex(LIBRARYTABLE_TIMESPLAYED) == column) {
        pTrack->setTimesPlayed(value.toInt());
    } else if (fieldIndex(LIBRARYTABLE_RATING) == column) {
        StarRating starRating = qVariantValue<StarRating>(value);
        pTrack->setRating(starRating.starCount());
    } else if (fieldIndex(LIBRARYTABLE_KEY) == column) {
        pTrack->setKey(value.toString());
    }
}

QVariant BansheePlaylistModel::getBaseValue(const QModelIndex& index, int role) const {
    if (role != Qt::DisplayRole &&
        role != Qt::ToolTipRole &&
        role != Qt::EditRole) {
        return QVariant();
    }

    int row = index.row();
    int column = index.column();

    if (row < 0 || row >= m_rowInfo.size()) {
        return QVariant();
    }

    const QPair<int, QHash<int, QVariant> >& rowInfo = m_rowInfo[row];
    int trackId = rowInfo.first;

    // If the row info has the row-specific column, return that.
    const QHash<int, QVariant>& columns = rowInfo.second;
    if (columns.contains(column)) {
        if (sDebug) {
            qDebug() << "Returning table-column value" << columns[column] << "for column" << column;
        }
        return columns[column];
    }

    // Otherwise, return the information from the track record cache for the
    // given track ID
    return getTrackValueForColumn(trackId, column);
}



void BansheePlaylistModel::setPlaylist(int playlistId) {

    if (m_playlistId == playlistId) {
        qDebug() << "Already focused on playlist " << playlistId;
        return;
    }

    if (m_playlistId >= 0) {
        beginRemoveRows(QModelIndex(), 0, m_sortedPlaylist.size()-1);
        m_playlistId = -1;
//        d_sortedPlaylist.clear();
        endRemoveRows();
    }

    if (playlistId >= 0) {
//        if (m_pConnection->isOpen()) {

//        }
            //qDebug() << "PlaylistTableModel::setPlaylist" << playlistId;

        QList<struct BansheeDbConnection::PlaylistEntry> list;

        list = m_pConnection->getPlaylistEntries(playlistId);

        beginInsertRows(QModelIndex(), 0, list.size()-1);

        m_sortedPlaylist = list;

 /*

        m_pPlaylist = pPlaylist;
        // walk thought linked list and collect playlist position

        GList* track_node;
        uint pl_position = 0;

        QByteArray search = m_currentSearch.toUtf8();

        for (track_node = g_list_first(pPlaylist->members);
             track_node != NULL;
             track_node = g_list_next(track_node))
        {
            struct playlist_member plMember;
            plMember.pClass = this;
            plMember.pTrack = (Itdb_Track*)track_node->data;
            plMember.pos = ++pl_position;

            // Filter
            bool add = false;
            if( itdb_playlist_is_mpl(pPlaylist) && search.size()){
                // Apply filter only for the master playlist "BANSHEE"
                gchar* haystack = g_strconcat(
                        plMember.pTrack->artist,
                        plMember.pTrack->title,
                        plMember.pTrack->album,
                        plMember.pTrack->comment,
                        plMember.pTrack->genre,
                        NULL);

                if( findInUtf8Case(haystack, search.data())) {
                    add = true;
                }
                g_free(haystack);
            } else {
                add = true;
            }

            if (add) {
                m_sortedPlaylist.append(plMember);
            }

        }
        qSort(m_sortedPlaylist.begin(), m_sortedPlaylist.end(), columnLessThan);
*/
        endInsertRows();
    }

}



/*
//static
bool BansheePlaylistModel::findInUtf8Case(gchar* heystack, gchar* needles) {
    bool ret = true;
    if (heystack == NULL) {
        return false;
    }
    if (needles == NULL) {
        return true;
    }

    gchar* heystack_casefold = g_utf8_casefold(heystack, -1);
    gchar* needles_casefold = g_utf8_casefold(needles, -1);
    gchar** needle = g_strsplit_set(needles_casefold, " ", 0);

    qDebug() << "find" << heystack_casefold;

    // all needles must be found, implicit AND
    for (int i = 0; needle[i]; i++) {
        if (needle[i][0] != 0) {
            if (!g_strrstr(heystack_casefold, needle[i])) {
                ret = false;
                break;
            }
        }
    }
    g_free(heystack_casefold);
    g_free(needles_casefold);
    g_strfreev(needle);
    return ret;
}
*/

TrackPointer BansheePlaylistModel::getTrack(const QModelIndex& index) const {

    int row = index.row();
    if (row >= m_sortedPlaylist.size()) {
        return TrackPointer();
    }

    QUrl url = m_sortedPlaylist.at(row).pTrack->uri;

    QString location;
    location = url.toLocalFile();

    qDebug() << location << " = " << url;

    if (location.isEmpty()) {
        // Try to convert a smb path location = url.toLocalFile();
        QString temp_location = url.toString();

        if (temp_location.startsWith("smb://")) {
            // smb://daniel-desktop/volume/Musik/Lastfm/Limp Bizkit/Chocolate Starfish And The Hot Dog Flavored Water/06 - Rollin' (Air Raid Vehicle).mp3"

            // QDesktopServices::storageLocation(QDesktopServices::HomeLocation);
            // QDir::homePath()

            location = QDir::homePath() + "/.gvfs/";
            location += temp_location.section('/', 3, 3);
            location += " auf ";
            location += temp_location.section('/', 2, 2);
            location += "/";
            location += temp_location.section('/', 4);

            qDebug() << location;
        } else {
            // uri not supported
            qDebug() << "Track url not supported: " << url;
            return TrackPointer();
        }
    }

    TrackDAO& track_dao = m_pTrackCollection->getTrackDAO();
    int track_id = track_dao.getTrackId(location);
    bool track_already_in_library = track_id >= 0;
    if (track_id < 0) {
        // Add Track to library
        track_id = track_dao.addTrack(location, true);
    }

    TrackPointer pTrack;

    if (track_id < 0) {
        // Add Track to library failed, create a transient TrackInfoObject
        pTrack = TrackPointer(new TrackInfoObject(location), &QObject::deleteLater);
    } else {
        pTrack = track_dao.getTrack(track_id);
    }

    // If this track was not in the Mixxx library it is now added and will be
    // saved with the metadata from iTunes. If it was already in the library
    // then we do not touch it so that we do not over-write the user's metadata.
    if (!track_already_in_library) {
        pTrack->setArtist(m_sortedPlaylist.at(row).pArtist->name);
        pTrack->setTitle(m_sortedPlaylist.at(row).pTrack->title);
        //pTrack->setAlbum(album);
        //pTrack->setYear(year);
        //pTrack->setGenre(genre);
        //pTrack->setBpm(bpm);

        // If the track has a BPM, then give it a static beatgrid.
        //if (bpm > 0) {
        //    BeatsPointer pBeats = BeatFactory::makeBeatGrid(pTrack, bpm, 0);
        //    pTrack->setBeats(pBeats);
        //}
    }
    return pTrack;
}

// Gets the on-disk location of the track at the given location.
QString BansheePlaylistModel::getTrackLocation(const QModelIndex& index) const {

    //Itdb_Track* pTrack = getPTrackFromModelIndex(index);
    //if (!pTrack) {
        return QString();
 /*   }

    QString location; // = itdb_get_mountpoint(m_pPlaylist->itdb);
    QString banshee_path; // = pTrack->banshee_path;
    banshee_path.replace(QString(":"), QString("/"));
    location += banshee_path;

    return location;
    */
}

// Gets a significant hint of the track at the given QModelIndex
// This is used to restore the selection after WTrackTableView::doSortByColumn
int BansheePlaylistModel::getTrackId(const QModelIndex& index) const {
    // in our case the position in the playlist is a significant hint
    int row = index.row();
    if (row < m_sortedPlaylist.size()) {
        return m_sortedPlaylist.at(index.row()).viewOrder;
    }
    else {
        return 0;
    }
}

const QLinkedList<int> BansheePlaylistModel::getTrackRows(int trackId) const {
    // In this case we get the position as trackId, returned from getTrackId above.
    QLinkedList<int> ret;
    for (int i = 0; i < m_sortedPlaylist.size(); ++i) {
        if (m_sortedPlaylist.at(i).viewOrder == trackId ){
            ret.push_back(i);
            break;
        }
    }
    return ret;
}

void BansheePlaylistModel::search(const QString& searchText) {
    if (sDebug)
        qDebug() << this << "search" << searchText;

    if (m_currentSearch != searchText) {
        m_currentSearch = searchText;
 //       if (itdb_playlist_is_mpl(m_pPlaylist)) {
//           setPlaylist(m_pPlaylist);
 //       }
    }
}

const QString BansheePlaylistModel::currentSearch() {
    return m_currentSearch;
}

bool BansheePlaylistModel::isColumnInternal(int column) {
    Q_UNUSED(column);
    return false;
}

QMimeData* BansheePlaylistModel::mimeData(const QModelIndexList &indexes) const {
    QMimeData *mimeData = new QMimeData();
    QList<QUrl> urls;

    //Ok, so the list of indexes we're given contains separates indexes for
    //each column, so even if only one row is selected, we'll have like 7 indexes.
    //We need to only count each row once:
    QList<int> rows;

    foreach (QModelIndex index, indexes) {
        if (index.isValid()) {
            if (!rows.contains(index.row())) {
                rows.push_back(index.row());
                QUrl url = QUrl::fromLocalFile(getTrackLocation(index));
                if (!url.isValid())
                    qDebug() << "ERROR invalid url\n";
                else
                    urls.append(url);
            }
        }
    }
    mimeData->setUrls(urls);
    return mimeData;
}
    /** if no header state exists, we may hide some columns so that the user can reactivate them **/
bool BansheePlaylistModel::isColumnHiddenByDefault(int column) {
    Q_UNUSED(column);
    return false;
}

void BansheePlaylistModel::removeTrack(const QModelIndex& index) {
    Q_UNUSED(index);
}

void BansheePlaylistModel::removeTracks(const QModelIndexList& indices) {
    Q_UNUSED(indices);
}

bool BansheePlaylistModel::addTrack(const QModelIndex& index, QString location) {
    Q_UNUSED(index);
    Q_UNUSED(location);
    return false;
}

void BansheePlaylistModel::moveTrack(const QModelIndex& sourceIndex, const QModelIndex& destIndex) {
    Q_UNUSED(destIndex);
    Q_UNUSED(sourceIndex);
}

QItemDelegate* BansheePlaylistModel::delegateForColumn(const int i) {
    Q_UNUSED(i);
    return NULL;
}

/*
Itdb_Track* BansheePlaylistModel::getPTrackFromModelIndex(const QModelIndex& index) const {
    if (   !index.isValid()
        || m_pPlaylist == NULL
    ) {
        return NULL;
    }

    int row = index.row();
    int column = index.column();

    if (row >= m_sortedPlaylist.size() || column >= m_headerList.size()) {
        // index is outside the valid range
        return NULL;
    }
    return m_sortedPlaylist.at(row).pTrack;
}
*/
