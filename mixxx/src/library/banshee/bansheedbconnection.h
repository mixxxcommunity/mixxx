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
        int duration;
        int artistId;
    };

    struct Artist {
        QString name;
    };

    struct PlaylistEntry {
        int trackId;
        int viewOrder;
        struct Track* pTrack;
        struct Artist* pArtist;
    };

    BansheeDbConnection();
    virtual ~BansheeDbConnection();

    static QString getDatabaseFile();

    bool open(const QString& databaseFile);
    int getSchemaVersion();
    QList<struct Playlist> getPlaylists();
    QList<struct PlaylistEntry> getPlaylistEntries(int playlistId);

    static bool viewOrderLessThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.viewOrder < s2.viewOrder; };
    static bool viewOrderGreaterThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.viewOrder > s2.viewOrder; };

    static bool artistLessThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.pArtist->name < s2.pArtist->name; };
    static bool artistGreaterThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.pArtist->name > s2.pArtist->name; };

    static bool titleLessThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.pTrack->title < s2.pTrack->title; };
    static bool titleGreaterThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.pTrack->title > s2.pTrack->title; };

    static bool durationLessThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.pTrack->duration < s2.pTrack->duration; };
    static bool durationGreaterThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.pTrack->duration > s2.pTrack->duration; };

    static bool uriLessThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.pTrack->uri.toString() < s2.pTrack->uri.toString(); };
    static bool uriGreaterThen(struct PlaylistEntry &s1, PlaylistEntry &s2) { return s1.pTrack->uri.toString() > s2.pTrack->uri.toString(); }

private:
    QSqlDatabase m_database;
    QMap<int, struct Track> m_trackMap;
    QMap<int, struct Artist> m_artistMap;

};

#endif // BANSHEEDBCONNECTION_H
