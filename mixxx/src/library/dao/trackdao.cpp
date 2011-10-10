
#include <QtDebug>
#include <QtCore>
#include <QtSql>

#include "library/dao/trackdao.h"

#include "audiotagger.h"
#include "library/queryutil.h"
#include "soundsourceproxy.h"
#include "track/beatfactory.h"
#include "track/beats.h"
#include "trackinfoobject.h"

// The number of tracks to cache in memory at once. Once the n+1'th track is
// created, the TrackDAO's QCache deletes its TrackPointer to the track, which
// allows the track reference count to drop to zero. The track cache basically
// functions to hold a reference to the track so its reference count stays above
// 0.
#define TRACK_CACHE_SIZE 5

TrackDAO::TrackDAO(QSqlDatabase& database, CueDAO& cueDao, ConfigObject<ConfigValue> * pConfig)
        : m_database(database),
          m_cueDao(cueDao),
          m_pConfig(pConfig),
          m_trackCache(TRACK_CACHE_SIZE) {
}

void TrackDAO::finish() {
    //clear out played information on exit
    //crash prevention: if mixxx crashes, played information will be maintained
    qDebug() << "Clearing played information for this session";
    QSqlQuery query(m_database);
    if (!query.exec("UPDATE library SET played=0")) {
        LOG_FAILED_QUERY(query)
                << "Error clearing played value";
    }
}

TrackDAO::~TrackDAO() {
    qDebug() << "~TrackDAO()";
}

void TrackDAO::initialize() {
    qDebug() << "TrackDAO::initialize" << QThread::currentThread() << m_database.connectionName();
}

/** Retrieve the track id for the track that's located at "location" on disk.
    @return the track id for the track located at location, or -1 if the track
            is not in the database.
*/
int TrackDAO::getTrackId(QString absoluteFilePath) {
    //qDebug() << "TrackDAO::getTrackId" << QThread::currentThread() << m_database.connectionName();

    QSqlQuery query(m_database);
    query.prepare("SELECT library.id FROM library INNER JOIN track_locations ON library.location = track_locations.id WHERE track_locations.location=:location");
    query.bindValue(":location", absoluteFilePath);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return -1;
    }

    int libraryTrackId = -1;
    if (query.next()) {
        libraryTrackId = query.value(query.record().indexOf("id")).toInt();
    }
    //query.finish();

    return libraryTrackId;
}

/** Some code (eg. drag and drop) needs to just get a track's location, and it's
    not worth retrieving a whole TrackInfoObject.*/
QString TrackDAO::getTrackLocation(int trackId) {
    qDebug() << "TrackDAO::getTrackLocation"
             << QThread::currentThread() << m_database.connectionName();
    QSqlQuery query(m_database);
    QString trackLocation = "";
    query.prepare("SELECT track_locations.location FROM track_locations INNER JOIN library ON library.location = track_locations.id WHERE library.id=:id");
    query.bindValue(":id", trackId);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        return "";
    }
    while (query.next()) {
        trackLocation = query.value(query.record().indexOf("location")).toString();
    }
    //query.finish();

    return trackLocation;
}

/** Check if a track exists in the library table already.
    @param file_location The full path to the track on disk, including the filename.
    @return true if the track is found in the library table, false otherwise.
*/
bool TrackDAO::trackExistsInDatabase(QString absoluteFilePath) {
    return (getTrackId(absoluteFilePath) != -1);
}

void TrackDAO::saveTrack(TrackPointer track) {
    if (track) {
        saveTrack(track.data());
    }
}

void TrackDAO::saveTrack(TrackInfoObject* pTrack) {
    if (!pTrack) {
        qWarning() << "TrackDAO::saveTrack() was given NULL track.";
    }
    //qDebug() << "TrackDAO::saveTrack" << pTrack->getId() << pTrack->getInfo();
    // If track's id is not -1, then update, otherwise add.
    int trackId = pTrack->getId();
    if (trackId != -1) {
        if (pTrack->isDirty()) {
            if (!m_dirtyTracks.contains(trackId)) {
                qDebug() << "WARNING: Inconsistent state in TrackDAO. Track is dirty while TrackDAO thinks it is clean.";
            }

            //qDebug() << this << "Dirty tracks before claen save:" << m_dirtyTracks.size();
            //qDebug() << "TrackDAO::saveTrack. Dirty. Calling update";
            updateTrack(pTrack);

            // Write audio meta data, if enabled in the preferences
            writeAudioMetaData(pTrack);

            //qDebug() << this << "Dirty tracks remaining after clean save:" << m_dirtyTracks.size();
        } else {
            //qDebug() << "TrackDAO::saveTrack. Not Dirty";
            //qDebug() << this << "Dirty tracks remaining:" << m_dirtyTracks.size();

            // Q_ASSERT(!m_dirtyTracks.contains(trackId));
            if (m_dirtyTracks.contains(trackId)) {
                qDebug() << "WARNING: Inconsistent state in TrackDAO. Track is clean while TrackDAO thinks it is dirty. Correcting.";
                m_dirtyTracks.remove(trackId);
            }

            //qDebug() << "Skipping track update for track" << pTrack->getId();
        }
    } else {
        addTrack(pTrack);
    }
}

bool TrackDAO::isDirty(int trackId) {
    return m_dirtyTracks.contains(trackId);
}

void TrackDAO::slotTrackDirty(TrackInfoObject* pTrack) {
    //qDebug() << "TrackDAO::slotTrackDirty" << pTrack->getInfo();
    // This is a private slot that is connected to TIO's created by this
    // TrackDAO. It is a way for the track to ask that it be saved. The only
    // time this could be unsafe is when the TIO's reference count drops to
    // 0. When that happens, the TIO is deleted with QObject:deleteLater, so Qt
    // will wait for this slot to comlete.
    if (pTrack) {
        int id = pTrack->getId();
        if (id != -1) {
            m_dirtyTracks.insert(id);
            emit(trackDirty(id));
        }
    }
}

void TrackDAO::slotTrackClean(TrackInfoObject* pTrack) {
    //qDebug() << "TrackDAO::slotTrackClean" << pTrack->getInfo();
    // This is a private slot that is connected to TIO's created by this
    // TrackDAO. It is a way for the track to ask that it be saved. The only
    // time this could be unsafe is when the TIO's reference count drops to
    // 0. When that happens, the TIO is deleted with QObject:deleteLater, so Qt
    // will wait for this slot to comlete.

    if (pTrack) {
        int id = pTrack->getId();
        if (id != -1) {
            m_dirtyTracks.remove(id);
            emit(trackClean(id));
        }
    }
}

void TrackDAO::slotTrackChanged(TrackInfoObject* pTrack) {
    //qDebug() << "TrackDAO::slotTrackChanged" << pTrack->getInfo();
    // This is a private slot that is connected to TIO's created by this
    // TrackDAO. It is a way for the track to ask that it be saved. The only
    // time this could be unsafe is when the TIO's reference count drops to
    // 0. When that happens, the TIO is deleted with QObject:deleteLater, so Qt
    // will wait for this slot to comlete.
    if (pTrack) {
        int id = pTrack->getId();
        if (id != -1) {
            emit(trackChanged(id));
        }
    }
}

void TrackDAO::slotTrackSave(TrackInfoObject* pTrack) {
    //qDebug() << "TrackDAO::slotTrackSave" << pTrack->getId() << pTrack->getInfo();
    // This is a private slot that is connected to TIO's created by this
    // TrackDAO. It is a way for the track to ask that it be saved. The last
    // time it is used is when the track is being deleted (i.e. its reference
    // count has dropped to 0). The TIO is deleted with QObject:deleteLater, so
    // Qt will wait for this slot to comlete.
    if (pTrack) {
        saveTrack(pTrack);
    }
}

void TrackDAO::saveDirtyTracks() {
    qDebug() << "TrackDAO::saveDirtyTracks()";
    QHashIterator<int, TrackWeakPointer> it(m_tracks);
    while (it.hasNext()) {
        it.next();
        // Auto-cast from TrackWeakPointer to TrackPointer
        TrackPointer pTrack = it.value();
        if (pTrack && pTrack->isDirty()) {
            saveTrack(pTrack);
        }
    }
    clearCache();
}

void TrackDAO::prepareTrackLocationsInsert(QSqlQuery& query) {
    query.prepare("INSERT INTO track_locations (location, directory, filename, filesize, fs_deleted, needs_verification) "
                  "VALUES (:location, :directory, :filename, :filesize, :fs_deleted, :needs_verification)");
}

void TrackDAO::bindTrackToTrackLocationsInsert(QSqlQuery& query, TrackInfoObject* pTrack) {
    query.bindValue(":location", pTrack->getLocation());
    query.bindValue(":directory", pTrack->getDirectory());
    query.bindValue(":filename", pTrack->getFilename());
    query.bindValue(":filesize", pTrack->getLength());
    // Should this check pTrack->exists()?
    query.bindValue(":fs_deleted", 0);
    query.bindValue(":needs_verification", 0);
}

void TrackDAO::prepareLibraryInsert(QSqlQuery& query) {
    query.prepare("INSERT INTO library (artist, title, album, year, genre, tracknumber, "
                  "filetype, location, comment, url, duration, rating, key, "
                  "bitrate, samplerate, cuepoint, bpm, replaygain, wavesummaryhex, "
                  "timesplayed, "
                  "channels, mixxx_deleted, header_parsed, beats_version, beats) "
                  "VALUES (:artist, "
                  ":title, :album, :year, :genre, :tracknumber, "
                  ":filetype, :location, :comment, :url, :duration, :rating, :key, "
                  ":bitrate, :samplerate, :cuepoint, :bpm, :replaygain, :wavesummaryhex, "
                  ":timesplayed, "
                  ":channels, :mixxx_deleted, :header_parsed, :beats_version, :beats)");
}

void TrackDAO::bindTrackToLibraryInsert(
    QSqlQuery& query, TrackInfoObject* pTrack, int trackLocationId) {
    query.bindValue(":artist", pTrack->getArtist());
    query.bindValue(":title", pTrack->getTitle());
    query.bindValue(":album", pTrack->getAlbum());
    query.bindValue(":year", pTrack->getYear());
    query.bindValue(":genre", pTrack->getGenre());
    query.bindValue(":tracknumber", pTrack->getTrackNumber());
    query.bindValue(":filetype", pTrack->getType());
    query.bindValue(":location", trackLocationId);
    query.bindValue(":comment", pTrack->getComment());
    query.bindValue(":url", pTrack->getURL());
    query.bindValue(":duration", pTrack->getDuration());
    query.bindValue(":rating", pTrack->getRating());
    query.bindValue(":bitrate", pTrack->getBitrate());
    query.bindValue(":samplerate", pTrack->getSampleRate());
    query.bindValue(":cuepoint", pTrack->getCuePoint());

    query.bindValue(":replaygain", pTrack->getReplayGain());
    query.bindValue(":key", pTrack->getKey());
    const QByteArray* pWaveSummary = pTrack->getWaveSummary();
    query.bindValue(":wavesummaryhex", pWaveSummary ? *pWaveSummary : QVariant(QVariant::ByteArray));
    query.bindValue(":timesplayed", pTrack->getTimesPlayed());
    //query.bindValue(":datetime_added", pTrack->getDateAdded());
    query.bindValue(":channels", pTrack->getChannels());
    query.bindValue(":mixxx_deleted", 0);
    query.bindValue(":header_parsed", pTrack->getHeaderParsed() ? 1 : 0);

    const QByteArray* pBeatsBlob = NULL;
    QString blobVersion = "";
    BeatsPointer pBeats = pTrack->getBeats();
    // Fall back on cached BPM
    double dBpm = pTrack->getBpm();

    if (pBeats) {
        pBeatsBlob = pBeats->toByteArray();
        blobVersion = pBeats->getVersion();
        dBpm = pBeats->getBpm();
    }

    query.bindValue(":bpm", dBpm);
    query.bindValue(":beats_version", blobVersion);
    query.bindValue(":beats", pBeatsBlob ? *pBeatsBlob : QVariant(QVariant::ByteArray));
    delete pBeatsBlob;
}

void TrackDAO::addTracks(QList<TrackInfoObject*> tracksToAdd) {
    QTime time;
    time.start();

    //Start the transaction
    m_database.transaction();

    QSqlQuery query(m_database);
    QSqlQuery query_finder(m_database);
    query_finder.prepare("SELECT id FROM track_locations WHERE location=:location");

    // Major time saver for having this outside the loop
    prepareTrackLocationsInsert(query);

    foreach (TrackInfoObject* pTrack, tracksToAdd) {
        if (!isTrackFormatSupported(pTrack)) {
            continue;
        }

        bindTrackToTrackLocationsInsert(query, pTrack);

        int trackLocationId = -1;
        if (!query.exec()) {
            qDebug() << "Location " << pTrack->getLocation() << " is already in the DB";
            query.bindValue(":location", pTrack->getLocation());

            if (!query.exec()) {
                // We can't even select this, something is wrong.
                LOG_FAILED_QUERY(query);
                m_database.rollback();
                return;
            }
            while (query.next()) {
                trackLocationId = query.value(query.record().indexOf("id")).toInt();
            }
        } else {
            // Inserting succeeded, so just get the last rowid.
            QVariant lastInsert = query.lastInsertId();
            trackLocationId = lastInsert.toInt();
        }

        // To save time on future queries, setId the trackLocationId on the
        // track. This takes advantage of the fact that I know the
        // LibraryScanner doesn't use these tracks for anything. rryan 9/2010
        pTrack->setId(trackLocationId);
    }

    // Major time saver for having this outside the loop
    prepareLibraryInsert(query);

    foreach (TrackInfoObject* pTrack, tracksToAdd) {
        bindTrackToLibraryInsert(query, pTrack, pTrack->getId());

        if (!query.exec()) {
            LOG_FAILED_QUERY(query)
                    << "Failed to INSERT new track into library:"
                    << pTrack->getFilename();
            m_database.rollback();
            return;
        }

        int trackId = query.lastInsertId().toInt();

        if (trackId >= 0) {
            m_cueDao.saveTrackCues(trackId, pTrack);
        } else {
            qDebug() << "Could not get track ID to save the track cue points:"
                     << query.lastError();
        }

        // Undo the hack we did above by setting this track to its new id in the
        // database.
        pTrack->setId(trackId);
    }

    m_database.commit();

    qDebug() << "Writing tracks to database by TrackDAO::addTracks() took " << time.elapsed() << " ms";
}

int TrackDAO::addTrack(QFileInfo& fileInfo) {
    int trackId = -1;
    TrackInfoObject * pTrack = new TrackInfoObject(fileInfo);
    if (pTrack) {
        // Add the song to the database.
        addTrack(pTrack);
        trackId = pTrack->getId();
        delete pTrack;
    }
    return trackId;
}

int TrackDAO::addTrack(QString absoluteFilePath) {
    QFileInfo fileInfo(absoluteFilePath);
    return addTrack(fileInfo);
}

void TrackDAO::addTrack(TrackInfoObject* pTrack) {
    QTime time;
    time.start();

    //qDebug() << "TrackDAO::addTrack" << QThread::currentThread() << m_database.connectionName();
    //qDebug() << "TrackCollection::addTrack(), inserting into DB";
    Q_ASSERT(pTrack); //Why you be giving me NULL pTracks

    // Check that track is a supported extension.
    if (!isTrackFormatSupported(pTrack)) {
        return;
    }

    // Start the transaction
    m_database.transaction();

    QSqlQuery query(m_database);
    int trackLocationId = -1;

    // Insert the track location into the corresponding table. This will fail
    // silently if the location is already in the table because it has a UNIQUE
    // constraint.
    prepareTrackLocationsInsert(query);
    bindTrackToTrackLocationsInsert(query, pTrack);

    if (!query.exec()) {
        // Inserting into track_locations failed, so the file already
        // exists. Query for its id.
        query.prepare("SELECT id FROM track_locations WHERE location=:location");
        query.bindValue(":location", pTrack->getLocation());

        if (!query.exec()) {
            // We can't even select this, something is wrong.
            LOG_FAILED_QUERY(query);
            m_database.rollback();
            return;
        }
        while (query.next()) {
            trackLocationId = query.value(query.record().indexOf("id")).toInt();
        }
    } else {
        // Inserting succeeded, so just get the last rowid.
        QVariant lastInsert = query.lastInsertId();
        trackLocationId = lastInsert.toInt();
    }

    //Failure of this assert indicates that we were unable to insert the track
    //location into the table AND we could not retrieve the id of that track
    //location from the same table. "It shouldn't happen"... unless I screwed up
    //- Albert :)
    Q_ASSERT(trackLocationId >= 0);

    prepareLibraryInsert(query);
    bindTrackToLibraryInsert(query, pTrack, trackLocationId);

    int trackId = -1;

    if (!query.exec()) {
        LOG_FAILED_QUERY(query)
                << "Failed to INSERT new track into library";
        m_database.rollback();
        return;
    } else {
        // Inserting succeeded, so just get the last rowid.
        trackId = query.lastInsertId().toInt();
    }
    //query.finish();

    Q_ASSERT(trackId >= 0);

    if (trackId >= 0) {
        m_cueDao.saveTrackCues(trackId, pTrack);
    } else {
        qDebug() << "Could not get track ID to save the track cue points:"
                 << query.lastError();
    }

    pTrack->setId(trackId);
    pTrack->setDirty(false);

    //query.finish();

    //Commit the transaction
    m_database.commit();
    //qDebug() << "addTrack took" << time.elapsed() << "ms";
}

  /** Removes a track from the library track collection. */
void TrackDAO::removeTrack(int id) {
    //qDebug() << "TrackDAO::removeTrack" << QThread::currentThread() << m_database.connectionName();
    Q_ASSERT(id >= 0);
    QSqlQuery query(m_database);

    //Mark the track as deleted!
    query.prepare("UPDATE library "
                  "SET mixxx_deleted=1 "
                  "WHERE id = " + QString("%1").arg(id));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }
}

void TrackDAO::removeTracks(QList<int> ids) {
    QString idList = "";

    foreach (int id, ids) {
        idList.append(QString("%1,").arg(id));
    }

    // Strip the last ,
    if (idList.count() > 0) {
        idList.truncate(idList.count() - 1);
    }

    QSqlQuery query(m_database);
    query.prepare(QString("UPDATE library SET mixxx_deleted=1 WHERE id in (%1)").arg(idList));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }
}

/*** If a track has been manually "removed" from Mixxx's library by the user via
     Mixxx's interface, this lets you add it back. When a track is removed,
     mixxx_deleted in the DB gets set to 1. This clears that, and makes it show
     up in the library views again.
     This function should get called if you drag-and-drop a file that's been
     "removed" from Mixxx back into the library view.
*/
void TrackDAO::unremoveTrack(int trackId) {
    Q_ASSERT(trackId >= 0);
    QSqlQuery query(m_database);
    query.prepare("UPDATE library "
                  "SET mixxx_deleted=0 "
                  "WHERE id = " + QString("%1").arg(trackId));
    if (!query.exec()) {
        LOG_FAILED_QUERY(query)
                << "Failed to set track" << trackId << "as undeleted";
    }
}

// static
void TrackDAO::deleteTrack(TrackInfoObject* pTrack) {
    Q_ASSERT(pTrack);

    qDebug() << "Got deletion call for track" << pTrack << "ID" << pTrack->getId() << pTrack->getInfo();
    // Save the track if it is dirty.
    pTrack->doSave();

    // Now Qt will delete it in the event loop.
    pTrack->deleteLater();
}

TrackPointer TrackDAO::getTrackFromDB(QSqlQuery &query) const {
    //Print out any SQL error, if there was one.
    if (query.lastError().isValid()) {
        LOG_FAILED_QUERY(query);
    }

    int locationId = -1;
    while (query.next()) {
        // Good god! Assign query.record() to a freaking variable!
        int trackId = query.value(query.record().indexOf("id")).toInt();
        QString artist = query.value(query.record().indexOf("artist")).toString();
        QString title = query.value(query.record().indexOf("title")).toString();
        QString album = query.value(query.record().indexOf("album")).toString();
        QString year = query.value(query.record().indexOf("year")).toString();
        QString genre = query.value(query.record().indexOf("genre")).toString();
        QString tracknumber = query.value(query.record().indexOf("tracknumber")).toString();
        QString comment = query.value(query.record().indexOf("comment")).toString();
        QString url = query.value(query.record().indexOf("url")).toString();
        QString key = query.value(query.record().indexOf("key")).toString();
        int duration = query.value(query.record().indexOf("duration")).toInt();
        int bitrate = query.value(query.record().indexOf("bitrate")).toInt();
        int rating = query.value(query.record().indexOf("rating")).toInt();
        int samplerate = query.value(query.record().indexOf("samplerate")).toInt();
        int cuepoint = query.value(query.record().indexOf("cuepoint")).toInt();
        QString bpm = query.value(query.record().indexOf("bpm")).toString();
        QString replaygain = query.value(query.record().indexOf("replaygain")).toString();
        QByteArray* wavesummaryhex = new QByteArray(
            query.value(query.record().indexOf("wavesummaryhex")).toByteArray());
        int timesplayed = query.value(query.record().indexOf("timesplayed")).toInt();
        int played = query.value(query.record().indexOf("played")).toInt();
        int channels = query.value(query.record().indexOf("channels")).toInt();
        int filesize = query.value(query.record().indexOf("filesize")).toInt();
        QString filetype = query.value(query.record().indexOf("filetype")).toString();
        QString location = query.value(query.record().indexOf("location")).toString();
        bool header_parsed = query.value(query.record().indexOf("header_parsed")).toBool();
        QDateTime date_created = query.value(query.record().indexOf("datetime_added")).toDateTime();

        TrackInfoObject* track = new TrackInfoObject(location, false);
        TrackPointer pTrack = TrackPointer(track, &TrackDAO::deleteTrack);

        // TIO already stats the file to see if it exists, what its length is,
        // etc. So don't bother setting it.
        //track->setLength(filesize);

        track->setId(trackId);
        track->setArtist(artist);
        track->setTitle(title);
        track->setAlbum(album);
        track->setYear(year);
        track->setGenre(genre);
        track->setTrackNumber(tracknumber);
        track->setRating(rating);
        track->setKey(key);

        track->setComment(comment);
        track->setURL(url);
        track->setDuration(duration);
        track->setBitrate(bitrate);
        track->setSampleRate(samplerate);
        track->setCuePoint((float)cuepoint);
        track->setBpm(bpm.toFloat());
        track->setReplayGain(replaygain.toFloat());
        track->setWaveSummary(wavesummaryhex, false);
        delete wavesummaryhex;

        QString beatsVersion = query.value(query.record().indexOf("beats_version")).toString();
        QByteArray beatsBlob = query.value(query.record().indexOf("beats")).toByteArray();
        BeatsPointer pBeats = BeatFactory::loadBeatsFromByteArray(pTrack, beatsVersion, &beatsBlob);
        if (pBeats) {
            pTrack->setBeats(pBeats);
        }

        track->setTimesPlayed(timesplayed);
        track->setPlayed(played);
        track->setChannels(channels);
        track->setType(filetype);
        track->setLocation(location);
        track->setHeaderParsed(header_parsed);
        track->setDateAdded(date_created);

        track->setCuePoints(m_cueDao.getCuesForTrack(trackId));
        track->setDirty(false);

        // Listen to dirty and changed signals
        connect(track, SIGNAL(dirty(TrackInfoObject*)),
                this, SLOT(slotTrackDirty(TrackInfoObject*)),
                Qt::DirectConnection);
        connect(track, SIGNAL(clean(TrackInfoObject*)),
                this, SLOT(slotTrackClean(TrackInfoObject*)),
                Qt::DirectConnection);
        connect(track, SIGNAL(changed(TrackInfoObject*)),
                this, SLOT(slotTrackChanged(TrackInfoObject*)),
                Qt::DirectConnection);
        connect(track, SIGNAL(save(TrackInfoObject*)),
                this, SLOT(slotTrackSave(TrackInfoObject*)),
                Qt::DirectConnection);

        // Automatic conversion to a weak pointer
        m_tracks[trackId] = pTrack;
        m_trackCache.insert(trackId, new TrackPointer(pTrack));

        // If the header hasn't been parsed, parse it but only after we set the
        // track clean and hooked it up to the track cache, because this will
        // dirty it.
        if (!header_parsed) {
            pTrack->parse();
        }

        if (!pBeats && pTrack->getBpm() != 0.0f) {
            // The track has no stored beats but has a previously detected BPM
            // or a BPM loaded from metadata. Automatically create a beatgrid
            // for the track. This dirties the track so we have to do it after
            // all the signal connections, etc. are in place.
            BeatsPointer pBeats = BeatFactory::makeBeatGrid(pTrack, pTrack->getBpm(), 0.0f);
            pTrack->setBeats(pBeats);
        }

        return pTrack;
    }
    //query.finish();

    return TrackPointer();
}

TrackPointer TrackDAO::getTrack(int id, bool cacheOnly) const {
    //qDebug() << "TrackDAO::getTrack" << QThread::currentThread() << m_database.connectionName();

    // If the track cache contains the track, use it to get a strong reference
    // to the track. We do this first so that the QCache keeps track of the
    // least-recently-used track so that it expires them intelligently.
    if (m_trackCache.contains(id)) {
        TrackPointer pTrack = *m_trackCache[id];

        // If the strong reference is still valid (it should be), then return
        // it. Otherwise query the DB for the track.
        if (pTrack)
            return pTrack;
    }

    // Next, check the weak-reference cache to see if the track was ever loaded
    // into memory. It's possible that something is currently using this track,
    // so its reference count is non-zero despite it not being present in the
    // track cache. m_tracks is a map of weak pointers to the tracks.
    if (m_tracks.contains(id)) {
        //qDebug() << "Returning cached TIO for track" << id;
        TrackPointer pTrack = m_tracks[id];

        // If the pointer to the cached copy is still valid, return
        // it. Otherwise, re-query the DB for the track.
        if (pTrack)
            return pTrack;
    }

    // The person only wanted the track if it was cached.
    if (cacheOnly) {
        //qDebug() << "TrackDAO::getTrack()" << id << "Caller wanted track but only if it was cached. Returning null.";
        return TrackPointer();
    }

    QTime time;
    time.start();
    QSqlQuery query(m_database);

    query.prepare(
        "SELECT library.id, artist, title, album, year, genre, tracknumber, "
        "filetype, rating, key, track_locations.location as location, "
        "track_locations.filesize as filesize, comment, url, duration, bitrate, "
        "samplerate, cuepoint, bpm, replaygain, wavesummaryhex, channels, "
        "header_parsed, timesplayed, played, beats_version, beats, datetime_added "
        "FROM Library "
        "INNER JOIN track_locations "
            "ON library.location = track_locations.id "
        "WHERE library.id=" + QString("%1").arg(id)
    );

    TrackPointer pTrack;

    if (query.exec()) {
         pTrack = getTrackFromDB(query);
    } else {
        LOG_FAILED_QUERY(query)
                << QString("getTrack(%1)").arg(id);
    }
    //qDebug() << "getTrack hit the database, took " << time.elapsed() << "ms";

    return pTrack;
}

/** Saves a track's info back to the database */
void TrackDAO::updateTrack(TrackInfoObject* pTrack) {
    m_database.transaction();
    QTime time;
    time.start();
    Q_ASSERT(pTrack);
    //qDebug() << "TrackDAO::updateTrackInDatabase" << QThread::currentThread() << m_database.connectionName();

    //qDebug() << "Updating track" << pTrack->getInfo() << "in database...";

    int trackId = pTrack->getId();
    Q_ASSERT(trackId >= 0);

    QSqlQuery query(m_database);

    //Update everything but "location", since that's what we identify the track by.
    query.prepare("UPDATE library "
                  "SET artist=:artist, "
                  "title=:title, album=:album, year=:year, genre=:genre, "
                  "filetype=:filetype, tracknumber=:tracknumber, "
                  "comment=:comment, url=:url, duration=:duration, rating=:rating, key=:key, "
                  "bitrate=:bitrate, samplerate=:samplerate, cuepoint=:cuepoint, "
                  "bpm=:bpm, replaygain=:replaygain, wavesummaryhex=:wavesummaryhex, "
                  "timesplayed=:timesplayed, played=:played, "
                  "channels=:channels, header_parsed=:header_parsed, "
                  "beats_version=:beats_version, beats=:beats "
                  "WHERE id="+QString("%1").arg(trackId));
    query.bindValue(":artist", pTrack->getArtist());
    query.bindValue(":title", pTrack->getTitle());
    query.bindValue(":album", pTrack->getAlbum());
    query.bindValue(":year", pTrack->getYear());
    query.bindValue(":genre", pTrack->getGenre());
    query.bindValue(":filetype", pTrack->getType());
    query.bindValue(":tracknumber", pTrack->getTrackNumber());
    query.bindValue(":comment", pTrack->getComment());
    query.bindValue(":url", pTrack->getURL());
    query.bindValue(":duration", pTrack->getDuration());
    query.bindValue(":bitrate", pTrack->getBitrate());
    query.bindValue(":samplerate", pTrack->getSampleRate());
    query.bindValue(":cuepoint", pTrack->getCuePoint());

    query.bindValue(":replaygain", pTrack->getReplayGain());
    query.bindValue(":key", pTrack->getKey());
    query.bindValue(":rating", pTrack->getRating());
    const QByteArray* pWaveSummary = pTrack->getWaveSummary();
    query.bindValue(":wavesummaryhex", pWaveSummary ? *pWaveSummary : QVariant(QVariant::ByteArray));
    query.bindValue(":timesplayed", pTrack->getTimesPlayed());
    query.bindValue(":played", pTrack->getPlayed());
    query.bindValue(":channels", pTrack->getChannels());
    query.bindValue(":header_parsed", pTrack->getHeaderParsed() ? 1 : 0);
    //query.bindValue(":location", pTrack->getLocation());

    BeatsPointer pBeats = pTrack->getBeats();
    QByteArray* pBeatsBlob = NULL;
    QString beatsVersion = "";
    double dBpm = pTrack->getBpm();

    if (pBeats) {
        pBeatsBlob = pBeats->toByteArray();
        beatsVersion = pBeats->getVersion();
        dBpm = pBeats->getBpm();
    }

    query.bindValue(":beats", pBeatsBlob ? *pBeatsBlob : QVariant(QVariant::ByteArray));
    query.bindValue(":beats_version", beatsVersion);
    query.bindValue(":bpm", dBpm);

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
        m_database.rollback();
        return;
    }

    if (query.numRowsAffected() == 0) {
        qWarning() << "updateTrack had no effect: trackId" << trackId << "invalid";
        m_database.rollback();
        return;
    }

    //query.finish();

    //qDebug() << "Update track took : " << time.elapsed() << "ms. Now updating cues";
    time.start();
    m_cueDao.saveTrackCues(trackId, pTrack);
    m_database.commit();

    //qDebug() << "Update track in database took: " << time.elapsed() << "ms";
    time.start();
    pTrack->setDirty(false);
    //qDebug() << "Dirtying track took: " << time.elapsed() << "ms";
}

/** Mark all the tracks whose paths begin with libraryPath as invalid.
    That means we'll need to later check that those tracks actually
    (still) exist as part of the library scanning procedure. */
void TrackDAO::invalidateTrackLocationsInLibrary(QString libraryPath) {
    //qDebug() << "TrackDAO::invalidateTrackLocations" << QThread::currentThread() << m_database.connectionName();
    //qDebug() << "invalidateTrackLocations(" << libraryPath << ")";
    libraryPath += "%"; //Add wildcard to SQL query to match subdirectories!

    QSqlQuery query(m_database);
    query.prepare("UPDATE track_locations "
                  "SET needs_verification=1 "
                  "WHERE directory LIKE :directory");
    query.bindValue(":directory", libraryPath);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query)
                << "Couldn't mark tracks in directory" << libraryPath
                <<  "as needing verification.";
    }
}

void TrackDAO::markTrackLocationAsVerified(QString location)
{
    //qDebug() << "TrackDAO::markTrackLocationAsVerified" << QThread::currentThread() << m_database.connectionName();
    //qDebug() << "markTrackLocationAsVerified()" << location;

    QSqlQuery query(m_database);
    query.prepare("UPDATE track_locations "
                  "SET needs_verification=0, fs_deleted=0 "
                  "WHERE location=:location");
    query.bindValue(":location", location);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query)
                << "Couldn't mark track" << location << " as verified.";
    }
}

void TrackDAO::markTracksInDirectoryAsVerified(QString directory) {
    //qDebug() << "TrackDAO::markTracksInDirectoryAsVerified" << QThread::currentThread() << m_database.connectionName();
    //qDebug() << "markTracksInDirectoryAsVerified()" << directory;

    QSqlQuery query(m_database);
    query.prepare("UPDATE track_locations "
                  "SET needs_verification=0 "
                  "WHERE directory=:directory");
    query.bindValue(":directory", directory);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query)
                << "Couldn't mark tracks in" << directory << " as verified.";
    }
}

void TrackDAO::markUnverifiedTracksAsDeleted() {
    //qDebug() << "TrackDAO::markUnverifiedTracksAsDeleted" << QThread::currentThread() << m_database.connectionName();
    //qDebug() << "markUnverifiedTracksAsDeleted()";

    QSqlQuery query(m_database);
    query.prepare("UPDATE track_locations "
                  "SET fs_deleted=1, needs_verification=0 "
                  "WHERE needs_verification=1");
    if (!query.exec()) {
        LOG_FAILED_QUERY(query)
                << "Couldn't mark unverified tracks as deleted.";
    }

}

void TrackDAO::markTrackLocationsAsDeleted(QString directory) {
    //qDebug() << "TrackDAO::markTrackLocationsAsDeleted" << QThread::currentThread() << m_database.connectionName();
    QSqlQuery query(m_database);
    query.prepare("UPDATE track_locations "
                  "SET fs_deleted=1 "
                  "WHERE directory=:directory");
    query.bindValue(":directory", directory);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query)
                << "Couldn't mark tracks in" << directory << "as deleted.";
    }
}

/** Look for moved files. Look for files that have been marked as "deleted on disk"
    and see if another "file" with the same name and filesize exists in the track_locations
    table. That means the file has moved instead of being deleted outright, and so
    we can salvage your existing metadata that you have in your DB (like cue points, etc.). */
void TrackDAO::detectMovedFiles() {
    //This function should not start a transaction on it's own!
    //When it's called from libraryscanner.cpp, there already is a transaction
    //started!

    QSqlQuery query(m_database);
    QSqlQuery query2(m_database);
    int oldTrackLocationId = -1;
    int newTrackLocationId = -1;
    QString filename;
    int fileSize;

    query.prepare("SELECT id, filename, filesize FROM track_locations WHERE fs_deleted=1");

    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }

    //For each track that's been "deleted" on disk...
    while (query.next()) {
        newTrackLocationId = -1; //Reset this var
        oldTrackLocationId = query.value(query.record().indexOf("id")).toInt();
        filename = query.value(query.record().indexOf("filename")).toString();
        fileSize = query.value(query.record().indexOf("filesize")).toInt();

        query2.prepare("SELECT id FROM track_locations WHERE "
                       "fs_deleted=0 AND "
                       "filename=:filename AND "
                       "filesize=:filesize");
        query2.bindValue(":filename", filename);
        query2.bindValue(":filesize", fileSize);
        Q_ASSERT(query2.exec());

        Q_ASSERT(query2.size() <= 1); //WTF duplicate tracks?
        while (query2.next())
        {
            newTrackLocationId = query2.value(query2.record().indexOf("id")).toInt();
        }

        //If we found a moved track...
        if (newTrackLocationId >= 0)
        {
            qDebug() << "Found moved track!" << filename;

            //Remove old row from track_locations table
            query2.prepare("DELETE FROM track_locations WHERE "
                           "id=:id");
            query2.bindValue(":id", oldTrackLocationId);
            Q_ASSERT(query2.exec());

            //The library scanner will have added a new row to the Library
            //table which corresponds to the track in the new location. We need
            //to remove that so we don't end up with two rows in the library table
            //for the same track.
            query2.prepare("DELETE FROM library WHERE "
                           "location=:location");
            query2.bindValue(":location", newTrackLocationId);
            Q_ASSERT(query2.exec());

            //Update the location foreign key for the existing row in the library table
            //to point to the correct row in the track_locations table.
            query2.prepare("UPDATE library "
                           "SET location=:newloc WHERE location=:oldloc");
            query2.bindValue(":newloc", newTrackLocationId);
            query2.bindValue(":oldloc", oldTrackLocationId);
            Q_ASSERT(query2.exec());
        }
    }
}

void TrackDAO::clearCache() {
    m_trackCache.clear();
    m_dirtyTracks.clear();
}

void TrackDAO::writeAudioMetaData(TrackInfoObject* pTrack){
    if (m_pConfig && m_pConfig->getValueString(ConfigKey("[Library]","WriteAudioTags")).toInt() == 1) {
        AudioTagger tagger(pTrack->getLocation());

        tagger.setArtist(pTrack->getArtist());
        tagger.setTitle(pTrack->getTitle());
        tagger.setGenre(pTrack->getGenre());
        tagger.setAlbum(pTrack->getAlbum());
        tagger.setComment(pTrack->getComment());
        tagger.setTracknumber(pTrack->getTrackNumber());
        tagger.setBpm(pTrack->getBpmStr());

        tagger.save();
    }
}

bool TrackDAO::isTrackFormatSupported(TrackInfoObject* pTrack) const {
    return SoundSourceProxy::isFilenameSupported(pTrack->getFilename());
}
