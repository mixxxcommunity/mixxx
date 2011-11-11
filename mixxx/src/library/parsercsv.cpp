//
// C++ Implementation: parsercsv
//
// Description: module to parse Comma-Separated Values (CSV) formated playlists (rfc4180)
//
//
// Author: Ingo Kossyk <kossyki@cs.tu-berlin.de>, (C) 2004
// Author: Tobias Rafreider trafreider@mixxx.org, (C) 2011
// Author: Daniel Schürmann daschuer@gmx.de, (C) 2011
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include "parsercsv.h"
#include <QUrl>

ParserCsv::ParserCsv() : Parser()
{
}

ParserCsv::~ParserCsv()
{

}


QList<QString> ParserCsv::parse(QString sFilename)
{
    QFile file(sFilename);
    QString basepath = sFilename.section('/', 0, -2);

    clearLocations();
    //qDebug() << "ParserCsv: Starting to parse.";
    if (file.open(QIODevice::ReadOnly) && !isBinary(sFilename)) {
        /* Unfortunately, QT 4.7 does not handle <CR> (=\r or asci value 13) line breaks.
         * This is important on OS X where iTunes, e.g., exports CSV playlists using <CR>
         * rather that <LF>
         *
         * Using QFile::readAll() we obtain the complete content of the playlist as a ByteArray.
         * We replace any '\r' with '\n' if applicaple
         * This ensures that playlists from iTunes on OS X can be parsed
         */
        QByteArray ba = file.readAll();
        //detect encoding
        bool isCRLF_encoded = ba.contains("\r\n");
        bool isCR_encoded = ba.contains("\r");
        if(isCR_encoded && !isCRLF_encoded)
            ba.replace('\r','\n');
        QTextStream textstream(ba.data());
        
        while(!textstream.atEnd()) {
            QString sLine = getFilepath(&textstream, basepath);
            if(sLine.isEmpty())
                break;

            //qDebug() << "ParserCsv: parsed: " << (sLine);
            m_sLocations.append(sLine);
        }

        file.close();

        if(m_sLocations.count() != 0)
            return m_sLocations;
        else
            return QList<QString>(); // NULL pointer returned when no locations were found

    }

    file.close();
    return QList<QString>(); //if we get here something went wrong
}


QString ParserCsv::getFilepath(QTextStream *stream, QString basepath)
{
    QString textline,filename = "";

    textline = stream->readLine();

    while(!textline.isEmpty()){
        //qDebug() << "Untransofrmed text: " << textline;
        if(textline.isNull())
            break;

        if(!textline.contains("#")){
            filename = textline;
            filename.remove("file://");
            QByteArray strlocbytes = filename.toUtf8();
            //qDebug() << "QByteArray UTF-8: " << strlocbytes;
            QUrl location = QUrl::fromEncoded(strlocbytes);
            //qDebug() << "QURL UTF-8: " << location;
            QString trackLocation = location.toString();
            //qDebug() << "UTF8 TrackLocation:" << trackLocation;
            if(isFilepath(trackLocation)) {
                return trackLocation;
            } else {
                // Try relative to csv dir
                QString rel = basepath + "/" + trackLocation;
                if (isFilepath(rel)) {
                    return rel;
                }
                // We couldn't match this to a real file so ignore it
            }
        }
        textline = stream->readLine();
    }

    // Signal we reached the end
    return 0;

}
bool ParserCsv::writeCSVFile(const QString &file_str, PlaylistTableModel* pPlaylistTableModel, bool useRelativePath)
{
    /*
     * Important note:
     * On Windows \n will produce a <CR><CL> (=\r\n)
     * On Linux and OS X \n is <CR> (which remains \n)
     */

    QFile file(file_str);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)){
        QMessageBox::warning(NULL,tr("Playlist Export Failed"),
                             tr("Could not create file")+" "+file_str);
        return false;
    }
    //Base folder of file
    QString base = file_str.section('/', 0, -2);
    QDir base_dir(base);

    qDebug() << "Basepath: " << base;
    QTextStream out(&file);
    out.setCodec("UTF-8"); // rfc4180: Common usage of CSV is US-ASCII ...
                           // Using UTF-8 to get around codepage issues
                           // and it's the default encoding in Ooo Calc

    // writing header section
    bool first = true;
    int columns = pPlaylistTableModel->columnCount();
    for (int i = 0; i < columns; ++i) {
        if (pPlaylistTableModel->isColumnInternal(i)) {
            continue;
        }
        if (!first){
            out << ",";
        } else {
            first = false;
        }
        out << "\"";
        QString field = pPlaylistTableModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
        out << field.replace('\"', "\"\"");  // escape "
        out << "\"";
    }
    out << "\r\n"; // CRLF according to rfc4180


    int rows = pPlaylistTableModel->rowCount();
    for (int j = 0; j < rows; j++) {
        // writing fields section
        first = true;
        for(int i = 0; i < columns; ++i){
            if (pPlaylistTableModel->isColumnInternal(i)) {
                continue;
            }
            if (!first){
                out << ",";
            } else {
                first = false;
            }
            out << "\"";
            QString field = pPlaylistTableModel->data(pPlaylistTableModel->index(j,i)).toString();
            out << field.replace('\"', "\"\"");  // escape "
            out << "\"";
        }
        out << "\r\n"; // CRLF according to rfc4180
    }
    return true;
}
