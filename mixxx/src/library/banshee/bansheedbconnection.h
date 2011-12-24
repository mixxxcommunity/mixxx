#ifndef BANSHEEDBCONNECTION_H
#define BANSHEEDBCONNECTION_H

#include <QSqlDatabase>
#include <QUrl>

class BansheeDbConnection
{
public:

    struct Playlist {
        QString playlistId;
        QString name;
    };

    struct Track {
        QString title;
        QUrl uri;
    };

    struct PlaylistEntry {
        int trackId;
        int viewOrder;
        struct Track* pTrack;
    };

    BansheeDbConnection();
    virtual ~BansheeDbConnection();

    static QString getDatabaseFile();

    bool open(const QString& databaseFile);
    int getSchemaVersion();
    QList<struct Playlist> getPlaylists();
    QList<struct PlaylistEntry> getPlaylistEntries(int playlistId);

private:
    QSqlDatabase m_database;
    QMap<int, struct Track> m_trackMap;

};

#endif // BANSHEEDBCONNECTION_H
