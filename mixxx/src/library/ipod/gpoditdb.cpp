/*
 * GPodItdb.cpp
 *
 *  Created on: 01.02.2012
 *      Author: Daniel Schürmann
 */

#include <QLibrary>
#include <QtDebug>
#include <QDir>
#include <QMessageBox>

#include "gpoditdb.h"


GPodItdb::GPodItdb() {

    m_libGPodLoaded = true;
    // Load shared library
    QLibrary libGPod("libgpod");

    fp_itdb_free = (itdb_free__)libGPod.resolve("itdb_free");
    if(!fp_itdb_free) m_libGPodLoaded = false;

    fp_itdb_playlist_mpl = (itdb_playlist_mpl__)libGPod.resolve("itdb_playlist_mpl");
    if(!fp_itdb_playlist_mpl) m_libGPodLoaded = false;

    fp_itdb_parse = (itdb_parse__)libGPod.resolve("itdb_parse");
    if(!fp_itdb_parse) m_libGPodLoaded = false;



    qDebug() << "GPodItdb: try to resolve libgpod functions: " << (m_libGPodLoaded?"success":"failed");
}

GPodItdb::~GPodItdb() {
    if (m_itdb) {
        fp_itdb_free(m_itdb);
    }
}

int GPodItdb::parse(const QString& mount) {
    GError* err = 0;

    qDebug() << "Calling the libgpod db parser for:" << mount;

    if (m_itdb) {
        fp_itdb_free(m_itdb);
        m_itdb = NULL;
    }
    m_itdb = fp_itdb_parse(QDir::toNativeSeparators(mount).toLocal8Bit(), &err);

    if (err) {
        qDebug() << "There was an error, attempting to free db: "
                 << err->message;
        QMessageBox::warning(NULL, tr("Error Loading iPod database"),
                err->message);
        g_error_free(err);
        if (m_itdb) {
            fp_itdb_free(m_itdb);
            m_itdb = NULL;
        }
    }
    return err->code;
}
