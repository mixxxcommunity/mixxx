#include <QtAlgorithms>
#include <QtDebug>
#include <QTime>

#include "library/ipod/ipodplaylistmodel.h"
#include "mixxxutils.cpp"
#include "library/starrating.h"

extern "C" {
#include <glib-object.h> // g_type_init
}

const bool sDebug = false;

IPodPlaylistModel::IPodPlaylistModel(QObject* pParent, TrackCollection* pTrackCollection)
        :  TrackModel(pTrackCollection->getDatabase(),
                "mixxx.db.model.ipod_playlist"),
           QAbstractTableModel(pParent),
           m_pTrackCollection(pTrackCollection),
           m_trackDAO(m_pTrackCollection->getTrackDAO()),
           m_pPlaylist(NULL)
{
	initHeaderData();
    m_iSortColumn = 0;
    m_eSortOrder = Qt::AscendingOrder;
}

IPodPlaylistModel::~IPodPlaylistModel() {

}

void IPodPlaylistModel::initHeaderData() {
    // Set the column heading labels, rename them for translations and have
    // proper capitalization

	//m_headerList.append(qMakePair(QString(tr("Played")),    offsetof(Itdb_Track, artist)));
	m_headerList.append(qMakePair(QString(tr("Artist")),     offsetof(Itdb_Track, artist)));
	m_headerList.append(qMakePair(QString(tr("Title")),      offsetof(Itdb_Track, title)));
	m_headerList.append(qMakePair(QString(tr("Album")),      offsetof(Itdb_Track, album)));
	m_headerList.append(qMakePair(QString(tr("Year")),       offsetof(Itdb_Track, year)));
	m_headerList.append(qMakePair(QString(tr("Duration")),   offsetof(Itdb_Track, tracklen)));
	m_headerList.append(qMakePair(QString(tr("Rating")),     offsetof(Itdb_Track, rating)));

	m_headerList.append(qMakePair(QString(tr("Genre")),      offsetof(Itdb_Track, genre)));

//	m_headerList.append(qMakePair(QString(tr("Type")),       offsetof(Itdb_Track, type1)));
	m_headerList.append(qMakePair(QString(tr("Location")),   offsetof(Itdb_Track, ipod_path)));
	m_headerList.append(qMakePair(QString(tr("Comment")),    offsetof(Itdb_Track, comment)));


//	m_headerList.append(qMakePair(QString(tr("Bitrate")),    offsetof(Itdb_Track, artist)));
//	m_headerList.append(qMakePair(QString(tr("BPM")),        offsetof(Itdb_Track, BPM)));
//	m_headerList.append(qMakePair(QString(tr("Track #")),    offsetof(Itdb_Track, track_nr)));
//	m_headerList.append(qMakePair(QString(tr("Date Added")), offsetof(Itdb_Track, time_added)));
//	m_headerList.append(qMakePair(QString(tr("#")),          offsetof(Itdb_Track, track_nr)));
//	m_headerList.append(qMakePair(QString(tr("Key")),        offsetof(Itdb_Track, artist)));

	// gchar i;

/*
	  gchar   *filetype;
	  gchar   *category;
	  gchar   *composer;
	  gchar   *grouping;
	  gchar   *description;
	  gchar   *podcasturl;
	  gchar   *podcastrss;
	  Itdb_Chapterdata *chapterdata;
	  gchar   *subtitle;
	  gchar   *tvshow;
	  gchar   *tvepisode;
	  gchar   *tvnetwork;
	  gchar   *albumartist;
	  gchar   *keywords;
	  gchar   *sort_artist;
	  gchar   *sort_title;
	  gchar   *sort_album;
	  gchar   *sort_albumartist;
	  gchar   *sort_composer;
	  gchar   *sort_tvshow;
	  guint32 id;
	  gint32  size;
	  gint32  tracklen;
	  gint32  cd_nr;
	  gint32  cds;
	  gint32  tracks;
	  gint32  bitrate;
	  guint16 samplerate;
	  guint16 samplerate_low;
	  gint32  volume;
	  guint32 soundcheck;
	  time_t  time_added;
	  time_t  time_modified;
	  time_t  time_played;
	  guint32 bookmark_time;
	  guint32 rating;
	  guint32 playcount;
	  guint32 playcount2;
	  guint32 recent_playcount;
	  gboolean transferred;
	  guint8  app_rating;
	  guint8  type1;
	  guint8  type2;
	  guint8  compilation;
	  guint32 starttime;
	  guint32 stoptime;
	  guint8  checked;
	  guint64 dbid;
	  guint32 drm_userid;
	  guint32 visible;
	  guint32 filetype_marker;
	  guint16 artwork_count;
	  guint32 artwork_size;
	  float samplerate2;
	  guint16 unk126;
	  guint32 unk132;
	  time_t  time_released;
	  guint16 unk144;
	  guint16 explicit_flag;
	  guint32 unk148;
	  guint32 unk152;
	  guint32 skipcount;
	  guint32 recent_skipcount;
	  guint32 last_skipped;
	  guint8 has_artwork;
	  guint8 skip_when_shuffling;
	  guint8 remember_playback_position;
	  guint8 flag4;
	  guint64 dbid2;
	  guint8 lyrics_flag;
	  guint8 movie_flag;
	  guint8 mark_unplayed;
	  guint8 unk179;
	  guint32 unk180;
	  guint32 pregap;
	  guint64 samplecount;
	  guint32 unk196;
	  guint32 postgap;
	  guint32 unk204;
	  guint32 mediatype;
	  guint32 season_nr;
	  guint32 episode_nr;
	  guint32 unk220;
	  guint32 unk224;
	  guint32 unk228, unk232, unk236, unk240, unk244;
	  guint32 gapless_data;
	  guint32 unk252;
	  guint16 gapless_track_flag;
	  guint16 gapless_album_flag;
	  guint16 album_id;
	  struct _Itdb_Artwork *artwork;
	  guint32 mhii_link;
	  gint32 reserved_int1;
	  gint32 reserved_int2;
	  gint32 reserved_int3;
	  gint32 reserved_int4;
	  gint32 reserved_int5;
	  gint32 reserved_int6;
	  gpointer reserved1;
	  gpointer reserved2;
	  gpointer reserved3;
	  gpointer reserved4;
	  gpointer reserved5;
	  gpointer reserved6;
	  guint64 usertype;
	  gpointer userdata;
	  ItdbUserDataDuplicateFunc userdata_duplicate;
	  ItdbUserDataDestroyFunc userdata_destroy;

    */
}

void IPodPlaylistModel::initDefaultSearchColumns() {
    QStringList searchColumns;
    searchColumns << "artist"
                  << "album"
                  << "location"
                  << "comment"
                  << "title";
    setSearchColumns(searchColumns);
}

void IPodPlaylistModel::setSearchColumns(const QStringList& searchColumns) {
    m_searchColumns = searchColumns;

    // Convert all the search column names to their field indexes because we use
    // them a bunch.
    m_searchColumnIndices.resize(m_searchColumns.size());
    for (int i = 0; i < m_searchColumns.size(); ++i) {
        m_searchColumnIndices[i] = fieldIndex(m_searchColumns[i]);
    }
}

QVariant IPodPlaylistModel::headerData(int section, Qt::Orientation orientation, int role) const {
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


int IPodPlaylistModel::findSortInsertionPoint(int trackId, TrackPointer pTrack,
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


QString IPodPlaylistModel::currentSearch() const {
    return m_currentSearch;
}

void IPodPlaylistModel::search(const QString& searchText, const QString extraFilter) {
    if (sDebug)
        qDebug() << this << "search" << searchText;

    bool searchIsDifferent = m_currentSearch.isNull() || m_currentSearch != searchText;
    bool filterDisabled = (m_currentSearchFilter.isNull() && extraFilter.isNull());
    bool searchFilterIsDifferent = m_currentSearchFilter != extraFilter;

    if (!searchIsDifferent && (filterDisabled || !searchFilterIsDifferent)) {
        // Do nothing if the filters are no different.
        return;
    }

    m_currentSearch = searchText;
    m_currentSearchFilter = extraFilter;

    if (m_bIndexBuilt) {
//        select();
    }
}

void IPodPlaylistModel::setSort(int column, Qt::SortOrder order) {
    if (sDebug) {
        qDebug() << this << "setSort()";
    }

    m_iSortColumn = column;
    m_eSortOrder = order;

    if (m_bIndexBuilt) {
//        select();
    }
}

void IPodPlaylistModel::sort(int column, Qt::SortOrder order) {
    if (sDebug) {
        qDebug() << this << "sort()" << column << order;
    }

    m_iSortColumn = column;
    m_eSortOrder = order;

//    select();
}

int IPodPlaylistModel::rowCount(const QModelIndex& parent) const {
	if (m_pPlaylist && !parent.isValid()) {
		return m_pPlaylist->num;
	}
	return 0;
}

int IPodPlaylistModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_headerList.size();
}

int IPodPlaylistModel::fieldIndex(const QString& fieldName) const {
    // Usually a small list, so O(n) is small
    //return m_queryRecord.indexOf(fieldName);
    //return m_columnNames.indexOf(fieldName);
    QHash<QString, int>::const_iterator it = m_columnIndex.constFind(fieldName);
    if (it != m_columnIndex.end()) {
        return it.value();
    }
    return -1;
}

QVariant IPodPlaylistModel::data(const QModelIndex& index, int role) const {
    //qDebug() << this << "data()";
    if (    role != Qt::DisplayRole
         && role != Qt::EditRole
         && role != Qt::CheckStateRole
         && role != Qt::ToolTipRole
    ) {
        return QVariant();
    }


    Itdb_Track* pTrack = getPTrackFromModelIndex(index);
    if (!pTrack) {
    	return QVariant();
    }

    size_t structOffset = m_headerList.at(index.column()).second;
    QVariant value = QVariant();

    if (!pTrack) {
    	return QVariant();
    }

    if (structOffset == offsetof(Itdb_Track, year)) {
    	if (pTrack->year) {
    		value = QVariant(pTrack->year);
    	}
    } else if (structOffset == offsetof(Itdb_Track, tracklen)) {
    	if (pTrack->tracklen) {
    		value = MixxxUtils::millisecondsToMinutes(pTrack->tracklen, true);
    	}
    } else if (structOffset == offsetof(Itdb_Track, rating)) {
    	value = qVariantFromValue(StarRating(pTrack->rating));
    } else {
        // for the gchar* elements
        qDebug() << *(gchar**)((char*)(pTrack) + structOffset);
        QString ret = QString::fromUtf8(*(gchar**)((char*)(pTrack) + structOffset));

        value = QVariant(ret);
    }

    /*
    switch (structOffset) {
    case: offsetof(Itdb_Track, artist)
    case: offsetof(Itdb_Track, title)));
    case: offsetof(Itdb_Track, album)));
    case: offsetof(Itdb_Track, genre)));


    case: offsetof(Itdb_Track, year)));
    case: nd(qMakePair(QString(tr("Type")),       offsetof(Itdb_Track, artist)));
    		m_headerList.append(qMakePair(QString(tr("Location")),   offsetof(Itdb_Track, ipod_path)));
    		m_headerList.append(qMakePair(QString(tr("Comment")),    offsetof(Itdb_Track, comment)));
    		m_headerList.append(qMakePair(QString(tr("Duration")),   offsetof(Itdb_Track, tracklen)));
    		m_headerList.append(qMakePair(QString(tr("Rating")),     offsetof(Itdb_Track, rating)));
    		m_headerList.append(qMakePair(QString(tr("Bitrate")),    offsetof(Itdb_Track, artist)));
    		m_headerList.append(qMakePair(QString(tr("BPM")),        offsetof(Itdb_Track, BPM)));
    		m_headerList.append(qMakePair(QString(tr("Track #")),    offsetof(Itdb_Track, track_nr)));
    		m_headerList.append(qMakePair(QString(tr("Date Added")), offsetof(Itdb_Track, time_added)));
    		m_headerList.append(qMakePair(QString(tr("#")),          offsetof(Itdb_Track, track_nr)));
    		m_headerList.append(qMakePair(QString(tr("Key")),        offsetof(Itdb_Track, artist)));
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

bool IPodPlaylistModel::setData(const QModelIndex& index, const QVariant& value, int role) {

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

TrackModel::CapabilitiesFlags IPodPlaylistModel::getCapabilities() const {
    return   TRACKMODELCAPS_ADDTOAUTODJ
    	   | TRACKMODELCAPS_ADDTOCRATE
    	   | TRACKMODELCAPS_ADDTOPLAYLIST;
}

Qt::ItemFlags IPodPlaylistModel::flags(const QModelIndex &index) const {
    return readWriteFlags(index);
}

Qt::ItemFlags IPodPlaylistModel::readWriteFlags(const QModelIndex &index) const {
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

Qt::ItemFlags IPodPlaylistModel::readOnlyFlags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QAbstractItemModel::flags(index);
    if (!index.isValid())
        return Qt::ItemIsEnabled;

    //Enable dragging songs from this data model to elsewhere (like the waveform widget to
    //load a track into a Player).
    defaultFlags |= Qt::ItemIsDragEnabled;

    return defaultFlags;
}

const QLinkedList<int> IPodPlaylistModel::getTrackRows(int trackId) const {
    // Todo: haben wir eine TrackID?
	if (m_trackIdToRows.contains(trackId)) {
        return m_trackIdToRows[trackId];
    }
    return QLinkedList<int>();
}

void IPodPlaylistModel::trackChanged(int trackId) {
    if (sDebug) {
        qDebug() << this << "trackChanged" << trackId;
    }
    m_trackOverrides.insert(trackId);
    QLinkedList<int> rows = getTrackRows(trackId);
    foreach (int row, rows) {
        //qDebug() << "Row in this result set was updated. Signalling update. track:" << trackId << "row:" << row;
        QModelIndex left = index(row, 0);
        QModelIndex right = index(row, columnCount());
        emit(dataChanged(left, right));
    }
}


int IPodPlaylistModel::compareColumnValues(int iColumnNumber, Qt::SortOrder eSortOrder,
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


QVariant IPodPlaylistModel::getTrackValueForColumn(int trackId, int column, TrackPointer pTrack) const {
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

QVariant IPodPlaylistModel::getTrackValueForColumn(TrackPointer pTrack, int column) const {
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

void IPodPlaylistModel::setTrackValueForColumn(TrackPointer pTrack, int column, QVariant value) {
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

QVariant IPodPlaylistModel::getBaseValue(const QModelIndex& index, int role) const {
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

void IPodPlaylistModel::setPlaylist(Itdb_Playlist* pPlaylist) {
	if (m_pPlaylist == pPlaylist) {
		return; // Nothing to do;
	}

	if (m_pPlaylist) {
		beginRemoveRows(QModelIndex(), 0, m_pPlaylist->num-1);
		m_pPlaylist = NULL;
		endRemoveRows();
	}

	if (pPlaylist) {
		beginInsertRows(QModelIndex(), 0, pPlaylist->num-1);
		m_pPlaylist = pPlaylist;
		endInsertRows();
		qDebug() << "IPodPlaylistModel::setPlaylist" << pPlaylist->name;
	}
}


TrackPointer IPodPlaylistModel::getTrack(const QModelIndex& index) const {

	Itdb_Track* pTrack = getPTrackFromModelIndex(index);
	if (!pTrack) {
		return TrackPointer();
	}

	//QString artist = index.sibling(index.row(), fieldIndex("artist")).data().toString();
	//QString title = index.sibling(index.row(), fieldIndex("title")).data().toString();
	//QString album = index.sibling(index.row(), fieldIndex("album")).data().toString();
	//QString year = index.sibling(index.row(), fieldIndex("year")).data().toString();
	//QString genre = index.sibling(index.row(), fieldIndex("genre")).data().toString();
	//float bpm = index.sibling(index.row(), fieldIndex("bpm")).data().toString().toFloat();

	QString location = itdb_get_mountpoint(m_pPlaylist->itdb);
	QString ipod_path = pTrack->ipod_path;
	ipod_path.replace(QString(":"), QString("/"));
	location += ipod_path;

	qDebug() << location;

	TrackDAO& track_dao = m_pTrackCollection->getTrackDAO();
	int track_id = track_dao.getTrackId(location);
	if (track_id < 0) {
		// Add Track to library
		track_id = track_dao.addTrack(location);
	}

	TrackPointer pTrackP;

	if (track_id < 0) {
		// Add Track to library failed
		// Create own TrackInfoObject
		pTrackP = TrackPointer(new TrackInfoObject(location), &QObject::deleteLater);
	}
	else {
		pTrackP = track_dao.getTrack(track_id);
	}

	// Overwrite metadata from Ipod
	// Note: This will be written to the mixxx library as well
	pTrackP->setArtist(QString::fromUtf8(pTrack->artist));
	pTrackP->setTitle(QString::fromUtf8(pTrack->title));
	pTrackP->setAlbum(QString::fromUtf8(pTrack->album));
	pTrackP->setYear(QString::number(pTrack->year));
	pTrackP->setGenre(QString::fromUtf8(pTrack->genre));
	pTrackP->setBpm((float)pTrack->BPM);
	pTrackP->setComment(QString::fromUtf8(pTrack->comment));

	return pTrackP;
}
// Gets the on-disk location of the track at the given location.
QString IPodPlaylistModel::getTrackLocation(const QModelIndex& index) const {

	Itdb_Track* pTrack = getPTrackFromModelIndex(index);
	if (!pTrack) {
		return QString();
	}

	QString location = itdb_get_mountpoint(m_pPlaylist->itdb);
	QString ipod_path = pTrack->ipod_path;
	ipod_path.replace(QString(":"), QString("/"));
	location += ipod_path;

	return location;
}

// Gets the track ID of the track at the given QModelIndex
int IPodPlaylistModel::getTrackId(const QModelIndex& index) const {
	return 0;
}

void IPodPlaylistModel::search(const QString& searchText) {

}

const QString IPodPlaylistModel::currentSearch() {
	return QString();
}

bool IPodPlaylistModel::isColumnInternal(int column) {
	return false;
}
    /** if no header state exists, we may hide some columns so that the user can reactivate them **/
bool IPodPlaylistModel::isColumnHiddenByDefault(int column) {
	return false;
}

void IPodPlaylistModel::removeTrack(const QModelIndex& index) {

}
void IPodPlaylistModel::removeTracks(const QModelIndexList& indices) {

}

bool IPodPlaylistModel::addTrack(const QModelIndex& index, QString location) {
	return false;
}
void IPodPlaylistModel::moveTrack(const QModelIndex& sourceIndex, const QModelIndex& destIndex) {

}

QItemDelegate* IPodPlaylistModel::delegateForColumn(const int i) {
	return NULL;
}

Itdb_Track* IPodPlaylistModel::getPTrackFromModelIndex(const QModelIndex& index) const {
    if (   !index.isValid()
    	|| m_pPlaylist == NULL
    ) {
        return NULL;
    }

    int row = index.row();
    int column = index.column();

    if (row >= m_pPlaylist->num || column >= m_headerList.size()) {
    	// index is outside the valid range
    	return NULL;
    }

    return (Itdb_Track*)(g_list_nth(m_pPlaylist->members, row)->data);
}
