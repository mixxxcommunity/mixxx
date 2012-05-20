#include <gtest/gtest.h>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QThread>
#include <QAbstractSocket>
#include <qmetatype.h>

#include "trackinfoobject.h"
#include "soundsourceproxy.h"
#include "core/tagreaderclient.h"

#define BIGBUF_SIZE (1024 * 1024)  //Megabyte
#define CANARY_SIZE (1024*4)
#define MAGIC_FLOAT 1234.567890f
#define CANARY_FLOAT 0.0f

namespace {

class TagreaderTest: public testing::Test {
  protected:
    virtual void SetUp() {
        qRegisterMetaType<QAbstractSocket::SocketState>("QAbstractSocket::SocketState");
        //QAbstractSocket::SocketState' is registered using qRegisterMetaType()

        QFile file("src/test/tagreadertest.mp3");
        file.copy("src/test/temptagreadertest.mp3");
        m_file = new QFile("src/test/temptagreadertest.mp3");

        m_tag_reader_client = TagReaderClient::Instance();
        if (!m_tag_reader_client) {
            m_tag_reader_client = new TagReaderClient();
            m_thread = new QThread();
            m_tag_reader_client->setParent(NULL);
            m_tag_reader_client->moveToThread(m_thread);
            m_thread->start();
            m_tag_reader_client->Start();
        }
    }

    virtual void TearDown() {
        m_file->remove();
        delete m_file;
        delete m_tag_reader_client;
        delete m_thread;
       // delete m_app;
    }

    QFile* m_file;
    TagReaderClient* m_tag_reader_client;
    QThread* m_thread;
};

TEST_F(TagreaderTest, readWriteRead) {
    EXPECT_TRUE(m_file->exists());

    TrackPointer pTrack(new TrackInfoObject(m_file->fileName()), &QObject::deleteLater);
    qDebug() << pTrack->getArtist();
    EXPECT_TRUE(pTrack->getArtist().isEmpty());

    SoundSourceProxy* soundSource = new SoundSourceProxy(pTrack);
    EXPECT_TRUE(soundSource->open() == OK);

    pTrack->setArtist(QString("tagreader"));


    EXPECT_TRUE(m_tag_reader_client->SaveFile(pTrack->getLocation(),*pTrack));

    TrackPointer pTrack2(new TrackInfoObject(m_file->fileName()), &QObject::deleteLater);
    qDebug() << pTrack2->getArtist();

    delete soundSource;

    TrackPointer pTrack3(new TrackInfoObject(m_file->fileName()), &QObject::deleteLater);
    qDebug() << pTrack3->getArtist();
}
}
