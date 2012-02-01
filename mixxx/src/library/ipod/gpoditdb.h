/*
 * gpoditdb.h
 *
 *  Created on: 01.02.2012
 *      Author: daniel
 */

#include <QObject>

extern "C"
{
#include <gpod/itdb.h>
}

#ifndef GPODITDB_H_
#define GPODITDB_H_

class GPodItdb : public QObject {
    Q_OBJECT
public:
    GPodItdb();
    virtual ~GPodItdb();
    bool isSupported() { return m_libGPodLoaded; };
    int parse(const QString& mount);
    Itdb_Playlist* playlist_mpl() { return fp_itdb_playlist_mpl(m_itdb); };

    typedef int (*itdb_free__)(Itdb_iTunesDB *itdb);
    itdb_free__ fp_itdb_free;

    typedef Itdb_Playlist* (*itdb_playlist_mpl__)(Itdb_iTunesDB *itdb);
    itdb_playlist_mpl__ fp_itdb_playlist_mpl;

    typedef Itdb_iTunesDB* (*itdb_parse__)(const gchar *mp, GError **error);
    itdb_parse__ fp_itdb_parse;

private:
    Itdb_iTunesDB* m_itdb;
    bool m_libGPodLoaded;
};

#endif /* GPODITDB_H_ */
