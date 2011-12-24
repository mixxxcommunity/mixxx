// This file is based on BansheeDbConnection.cs from the Banshee source code

#include <QtDebug>
#include <QDesktopServices>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include "library/queryutil.h"

/*
using System;
using System.Linq;
using System.Collections.Generic;
using System.IO;
using System.Threading;

using Hyena;
using Hyena.Data;
using Hyena.Jobs;
using Hyena.Data.Sqlite;

using Banshee.Base;
using Banshee.ServiceStack;
using Banshee.Configuration;
*/

#include "bansheedbconnection.h"


BansheeDbConnection::BansheeDbConnection() {


}

BansheeDbConnection::~BansheeDbConnection() {
    qDebug() << "Close Banshee database";
    m_database.close();
}

bool BansheeDbConnection::open(const QString& databaseFile) {
    m_database = QSqlDatabase::addDatabase("QSQLITE", "BANSHE_DB_CONNECTION");
    m_database.setHostName("localhost");
    m_database.setDatabaseName(databaseFile);
    m_database.setConnectOptions("SQLITE_OPEN_READONLY");

    //Open the database connection in this thread.
    if (!m_database.open()) {
        m_database.setConnectOptions(); // clear options
        qDebug() << "Failed to open Banshee database." << m_database.lastError();
        return false;
    } else {
        // TODO(DSC): Verify schema
        qDebug() << "Successful opened Banshee database";
        return true;
    }
}

int BansheeDbConnection::getSchemaVersion() {
    QSqlQuery query(m_database);
    query.prepare("SELECT Value FROM CoreConfiguration WHERE Key = \"DatabaseVersion\"");

    if (query.exec()) {
        if (query.next()) {
            return query.value(0).toInt();
        }
    } else {
        LOG_FAILED_QUERY(query);
    }
    return -1;
}

QList<struct BansheeDbConnection::Playlist> BansheeDbConnection::getPlaylists() {

    QList<struct BansheeDbConnection::Playlist> list;
    struct BansheeDbConnection::Playlist playlist;

    QSqlQuery query(m_database);
    query.prepare("SELECT PlaylistID, Name FROM CorePlaylists");

    if (query.exec()) {
        while (query.next()) {
            playlist.playlistId = query.value(0).toString();
            playlist.name = query.value(1).toString();
            list.append(playlist);
        }
    } else {
        LOG_FAILED_QUERY(query);
    }
    return list;
}

QList<struct BansheeDbConnection::PlaylistEntry> BansheeDbConnection::getPlaylistEntries(int playlistId) {

    QList<struct BansheeDbConnection::PlaylistEntry> list;
    struct BansheeDbConnection::PlaylistEntry entry;

    QSqlQuery query(m_database);
/*
    QString queryString = QString(
        "CREATE TEMPORARY VIEW IF NOT EXISTS %1 AS "
        "SELECT %2 FROM PlaylistTracks "
        "INNER JOIN library ON library.id = PlaylistTracks.track_id "
        "WHERE PlaylistTracks.playlist_id = %3 AND library.mixxx_deleted = 0")
            .arg(escaper.escapeString(playlistTableName))
            .arg(columns.join(","))
            .arg(playlistId);
    query.prepare(queryString);
    if (!query.exec()) {
        LOG_FAILED_QUERY(query);
    }
*/

    QString queryString = QString(
        "SELECT "
        "CorePlaylistEntries.TrackID, "
        "CorePlaylistEntries.ViewOrder, "
        "CoreTracks.Title, "
        "CoreTracks.Uri "
        "FROM CorePlaylistEntries "
        "INNER JOIN CoreTracks ON CoreTracks.TrackID = CorePlaylistEntries.TrackID "
        "WHERE CorePlaylistEntries.PlaylistID = %1")
            .arg(playlistId);
    query.prepare(queryString);

    if (query.exec()) {
        while (query.next()) {
            entry.trackId = query.value(0).toInt();
            entry.viewOrder = query.value(1).toInt();
            m_trackMap[entry.trackId].title = query.value(2).toString();
            m_trackMap[entry.trackId].uri = query.value(3).toString();
            entry.pTrack = &m_trackMap[entry.trackId];
            qDebug() << entry.pTrack->title << entry.pTrack->uri;
            list.append(entry);
        }
    } else {
        LOG_FAILED_QUERY(query);
    }
    return list;
}



/*
BansheeDbConnection::BansheeDbConnection (string db_path) : base (db_path)
{
    // Each cache page is about 1.5K, so 32768 pages = 49152K = 48M
    int cache_size = (TableExists ("CoreTracks") && Query<long> ("SELECT COUNT(*) FROM CoreTracks") > 10000) ? 32768 : 16384;
    Execute ("PRAGMA cache_size = ?", cache_size);
    Execute ("PRAGMA synchronous = OFF");
    Execute ("PRAGMA temp_store = MEMORY");

    // TODO didn't want this on b/c smart playlists used to rely on it, but
    // now they shouldn't b/c we have smart custom functions we use for sorting/searching.
    // See BGO #603665 for discussion about turning this back on.
    //Execute ("PRAGMA case_sensitive_like=ON");

    Log.DebugFormat ("Opened SQLite (version {1}) connection to {0}", db_path, ServerVersion);

    migrator = new BansheeDbFormatMigrator (this);
    configuration = new DatabaseConfigurationClient (this);

    if (ApplicationContext.CommandLine.Contains ("debug-sql")) {
        Hyena.Data.Sqlite.HyenaSqliteCommand.LogAll = true;
        WarnIfCalledFromThread = ThreadAssist.MainThread;
    }
}



BansheeDbConnection::BansheeDbConnection () : this (DatabaseFile) {
    validate_schema = ApplicationContext.CommandLine.Contains ("validate-db-schema");
}

IEnumerable<string> BansheeDbConnection::SortedTableColumns (string table) {
    return GetSchema (table).Keys.OrderBy (n => n);
}

void BansheeDbConnection::IInitializeService.Initialize () {
    lock (this) {
        migrator.Migrate ();
        migrator = null;

        try {
            OptimizeDatabase ();
        } catch (Exception e) {
            Log.Exception ("Error determining if ANALYZE is necessary", e);
        }

        // Update cached sorting keys
        BeginTransaction ();
        try {
            SortKeyUpdater.Update ();
            CommitTransaction ();
        } catch {
            RollbackTransaction ();
        }
    }

    if (Banshee.Metrics.BansheeMetrics.EnableCollection.Get ()) {
        Banshee.Metrics.BansheeMetrics.Start ();
    }

    if (validate_schema) {
        ValidateSchema ();
    }
}

void BansheeDbConnection::OptimizeDatabase ()
{
    bool needs_analyze = false;
    long analyze_threshold = configuration.Get<long> ("Database", "AnalyzeThreshold", 100);
    string [] tables_with_indexes = {"CoreTracks", "CoreArtists", "CoreAlbums",
            "CorePlaylistEntries", "PodcastItems", "PodcastEnclosures",
             "PodcastSyndications", "CoverArtDownloads"};

    if (TableExists ("sqlite_stat1")) {
        foreach (string table_name in tables_with_indexes) {
            if (TableExists (table_name)) {
                long count = Query<long> (String.Format ("SELECT COUNT(*) FROM {0}", table_name));
                string stat = Query<string> ("SELECT stat FROM sqlite_stat1 WHERE tbl = ? LIMIT 1", table_name);
                // stat contains space-separated integers,
                // the first is the number of records in the table
                long items_indexed = stat != null ? long.Parse (stat.Split (' ')[0]) : 0;

                if (Math.Abs (count - items_indexed) > analyze_threshold) {
                    needs_analyze = true;
                    break;
                }
            }
        }
    } else {
        needs_analyze = true;
    }

    if (needs_analyze) {
        Log.DebugFormat ("Running ANALYZE against database to improve performance");
        Execute ("ANALYZE");
    }
}

BansheeDbConnection::BansheeDbFormatMigrator Migrator {
    get { lock (this) { return migrator; } }
}

bool BansheeDbConnection::ValidateSchema ()
{
    bool is_valid = true;
    var new_db_path = Paths.GetTempFileName (Paths.TempDir);
    var new_db = new BansheeDbConnection (new_db_path);
    ((IInitializeService)new_db).Initialize ();

    Hyena.Log.DebugFormat ("Validating db schema for {0}", DbPath);

    var tables = new_db.QueryEnumerable<string> (
            "select name from sqlite_master where type='table' order by name"
    );

    foreach (var table in tables) {
        if (!TableExists (table)) {
            Log.ErrorFormat ("Table {0} does not exist!", table);
            is_valid = false;
        } else {
            var a = new_db.SortedTableColumns (table);
            var b = SortedTableColumns (table);

            a.Except (b).ForEach (c => { is_valid = false; Hyena.Log.ErrorFormat ("Table {0} should contain column {1}", table, c); });
            b.Except (a).ForEach (c => Hyena.Log.DebugFormat ("Table {0} has extra (probably obsolete) column {1}", table, c));
        }
    }

    using (var reader = new_db.Query (
            "select name,sql from sqlite_master where type='index' AND name NOT LIKE 'sqlite_autoindex%' order by name")) {
        while (reader.Read ()) {
            string name = (string)reader[0];
            string sql = (string)reader[1];
            if (!IndexExists (name)) {
                Log.ErrorFormat ("Index {0} does not exist!", name);
                is_valid = false;
            } else {
                string our_sql = Query<string> ("select sql from sqlite_master where type='index' and name=?", name);
                if (our_sql != sql) {
                    Log.ErrorFormat ("Index definition of {0} differs, should be `{1}` but is `{2}`", name, sql, our_sql);
                    is_valid = false;
                }
            }
        }
    }

    Hyena.Log.DebugFormat ("Done validating db schema for {0}", DbPath);
    System.IO.File.Delete (new_db_path);
    return is_valid;
}

*/


// static
QString BansheeDbConnection::getDatabaseFile() {

    QString dbfile;

    // Banshee Application Data Path
    // on Windows - "%APPDATA%\banshee-1" ("<Drive>:\Documents and Settings\<login>\<Application Data>\banshee-1")
    // on Unix and Mac OS X - "$HOME/.config/banshee-1"

    QSettings ini(QSettings::IniFormat, QSettings::UserScope,
            "banshee-1","banshee");
    dbfile = QFileInfo(ini.fileName()).absolutePath();
    dbfile += "/banshee.db";
    if (QFile::exists(dbfile)) {
        return dbfile;
    }

    // Legacy Banshee Application Data Path
    QSettings ini2(QSettings::IniFormat, QSettings::UserScope,
            "banshee","banshee");
    dbfile = QFileInfo(ini2.fileName()).absolutePath();
    dbfile += "/banshee.db";
    if (QFile::exists(dbfile)) {
        return dbfile;
    }

    // Legacy Banshee Application Data Path
    dbfile = QDesktopServices::storageLocation(QDesktopServices::HomeLocation);
    dbfile += "/.gnome2/banshee/banshee.db";
    if (QFile::exists(dbfile)) {
        return dbfile;
    }

    return QString();
}

