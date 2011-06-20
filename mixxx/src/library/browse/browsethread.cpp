/*
 * browsethread.cpp         (C) 2011 Tobias Rafreider
 */

#include <QStringList>
#include <QDirIterator>
#include <QtCore>

#include "library/browse/browsethread.h"
#include "library/browse/browsetablemodel.h"
#include "soundsourceproxy.h"
#include "mixxxutils.cpp"


BrowseThread* BrowseThread::m_instance = 0;
static QMutex s_Mutex;

/*
 * This class is a singleton and represents a thread
 * that is used to read ID3 metadata
 * from a particular folder.
 *
 * The BroseTableModel uses this class.
 * Note: Don't call getInstance() from places
 * other than the GUI thread. BrowseThreads emit
 * signals to BrowseModel objects. It does not
 * make sense to use this class in non-GUI threads
 */
BrowseThread::BrowseThread(QObject *parent): QThread(parent)
{
    m_bStopThread = false;
    //start Thread
    start(QThread::LowestPriority);

}

BrowseThread::~BrowseThread() {
    qDebug() << "Wait to finish browser background thread";
    m_bStopThread = true;
    //wake up thread since it might wait for user input
    m_locationUpdated.wakeAll();
    //Wait until thread terminated
    //terminate();
    wait();
    qDebug() << "Browser background thread terminated!";
}
BrowseThread* BrowseThread::getInstance(){
    if (!m_instance)
    {
        s_Mutex.lock();;

         if (!m_instance)
               m_instance = new BrowseThread();

         s_Mutex.unlock();
    }
    return m_instance;
}
void BrowseThread::destroyInstance()
{
    s_Mutex.lock();
    if(m_instance){
        delete m_instance;
        m_instance = 0;
    }
    s_Mutex.unlock();
}

void BrowseThread::executePopulation(QString& path, BrowseTableModel* client) {
    m_path = path;
    m_model_observer = client;
    m_locationUpdated.wakeAll();
}

void BrowseThread::run() {
    while(!m_bStopThread) {
        m_mutex.lock();
        //Wait until the user has selected a folder
        m_locationUpdated.wait(&m_mutex);

        //Terminate thread if Mixxx closes
        if(m_bStopThread)
            return;
        // Populate the model
        populateModel();
        m_mutex.unlock();
    }
}

void BrowseThread::populateModel() {
    //Refresh the name filters in case we loaded new
    //SoundSource plugins.
    QStringList nameFilters(SoundSourceProxy::supportedFileExtensionsString().split(" "));
    QDirIterator fileIt(m_path, nameFilters, QDir::Files | QDir::NoDotAndDotDot);
    QString thisPath(m_path);

    /*
     * remove all rows
     * This is a blocking operation
     * see signal/slot connection in BrowseTableModel
     */
    emit(clearModel(m_model_observer));

    QList< QList<QStandardItem*> > rows;

    int row = 0;
    //Iterate over the files
    while (fileIt.hasNext())
    {
        /*
         * If a user quickly jumps through the folders
         * the current task becomes "dirty"
         */
        if(thisPath != m_path){
            qDebug() << "Exit populateModel()";
            return populateModel();
        }

        QString filepath = fileIt.next();
        TrackInfoObject tio(filepath);
        QList<QStandardItem*> row_data;

        QStandardItem* item = new QStandardItem(tio.getFilename());
        row_data.insert(COLUMN_FILENAME, item);

        item = new QStandardItem(tio.getArtist());
        row_data.insert(COLUMN_ARTIST, item);

        item = new QStandardItem(tio.getTitle());
        row_data.insert(COLUMN_TITLE, item);

        item = new QStandardItem(tio.getAlbum());
        row_data.insert(COLUMN_ALBUM, item);

        item = new QStandardItem(tio.getTrackNumber());
        row_data.insert(COLUMN_TRACK_NUMBER, item);

        item = new QStandardItem(tio.getYear());
        row_data.insert(COLUMN_YEAR, item);

        item = new QStandardItem(tio.getGenre());
        row_data.insert(COLUMN_GENRE, item);

        item = new QStandardItem(QString::number(tio.getRating()));
        row_data.insert(COLUMN_RATING, item);

        item = new QStandardItem(tio.getComment());
        row_data.insert(COLUMN_COMMENT, item);

        QString duration = MixxxUtils::secondsToMinutes(qVariantValue<int>(tio.getDuration()));
        item = new QStandardItem(duration);
        row_data.insert(COLUMN_DURATION, item);

        item = new QStandardItem(tio.getBpmStr());
        row_data.insert(COLUMN_BPM, item);

        item = new QStandardItem(tio.getKey());
        row_data.insert(COLUMN_KEY, item);

        item = new QStandardItem(tio.getType());
        row_data.insert(COLUMN_TYPE, item);

        item = new QStandardItem(tio.getBitrateStr());
        row_data.insert(COLUMN_BITRATE, item);

        item = new QStandardItem(filepath);
        row_data.insert(COLUMN_LOCATION, item);

        rows.append(row_data);
        ++row;
        //If 10 tracks have been analyzed, send it to GUI
        //Will limit GUI freezing
        if(row % 10 == 0){
            //this is a blocking operation
            emit(rowsAppended(rows, m_model_observer));

            rows.clear();
        }
        //Sleep additionally for 10ms which prevents us from GUI freezes
        msleep(20);
    }
    emit(rowsAppended(rows, m_model_observer));

}
