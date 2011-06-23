#include <QSqlTableModel>
#include "widget/wwidget.h"
#include "widget/wskincolor.h"
#include "widget/wtracktableview.h"

#include "library/trackcollection.h"
#include "library/playlisttablemodel.h"
#include "dlgautodj.h"


DlgAutoDJ::DlgAutoDJ(QWidget* parent, ConfigObject<ConfigValue>* pConfig,
                     TrackCollection* pTrackCollection, MixxxKeyboard* pKeyboard)
     : QWidget(parent), Ui::DlgAutoDJ(), m_playlistDao(pTrackCollection->getPlaylistDAO()){
    setupUi(this);

    m_pConfig = pConfig;
    m_pTrackCollection = pTrackCollection;
    m_bAutoDJEnabled = false;

    m_pTrackTableView = new WTrackTableView(this, pConfig, m_pTrackCollection);
    m_pTrackTableView->installEventFilter(pKeyboard);

    connect(m_pTrackTableView, SIGNAL(loadTrack(TrackPointer)),
            this, SIGNAL(loadTrack(TrackPointer)));
    connect(m_pTrackTableView, SIGNAL(loadTrackToPlayer(TrackPointer, QString)),
            this, SIGNAL(loadTrackToPlayer(TrackPointer, QString)));


    QBoxLayout* box = dynamic_cast<QBoxLayout*>(layout());
    Q_ASSERT(box); //Assumes the form layout is a QVBox/QHBoxLayout!
    box->removeWidget(m_pTrackTablePlaceholder);
    m_pTrackTablePlaceholder->hide();
    box->insertWidget(1, m_pTrackTableView);

    m_pAutoDJTableModel =  new PlaylistTableModel(this, pTrackCollection);
    int playlistId = m_playlistDao.getPlaylistIdFromName(AUTODJ_TABLE);
    if (playlistId < 0) {
        m_playlistDao.createPlaylist(AUTODJ_TABLE, true);
        playlistId = m_playlistDao.getPlaylistIdFromName(AUTODJ_TABLE);
    }
    m_pAutoDJTableModel->setPlaylist(playlistId);
    m_pTrackTableView->loadTrackModel(m_pAutoDJTableModel);

    //Override some playlist-view properties:
    //Prevent drag and drop to the waveform or elsewhere so you can't preempt the Auto DJ queue...
    m_pTrackTableView->setDragDropMode(QAbstractItemView::InternalMove);
    //Sort by the position column and lock it
    m_pTrackTableView->sortByColumn(0, Qt::AscendingOrder);
    m_pTrackTableView->setSortingEnabled(false);

    connect(pushButtonShuffle, SIGNAL(clicked(bool)),
            this, SLOT(shufflePlaylist(bool)));

    connect(pushButtonAutoDJ, SIGNAL(toggled(bool)),
            this,  SLOT(toggleAutoDJ(bool))); _blah;
}

DlgAutoDJ::~DlgAutoDJ(){
}

void DlgAutoDJ::onShow()
{
    m_pAutoDJTableModel->select();
}

void DlgAutoDJ::setup(QDomNode node)
{

    QPalette pal = palette();

    // Row colors
    if (!WWidget::selectNode(node, "BgColorRowEven").isNull() &&
        !WWidget::selectNode(node, "BgColorRowUneven").isNull()) {
        QColor r1;
        r1.setNamedColor(WWidget::selectNodeQString(node, "BgColorRowEven"));
        r1 = WSkinColor::getCorrectColor(r1);
        QColor r2;
        r2.setNamedColor(WWidget::selectNodeQString(node, "BgColorRowUneven"));
        r2 = WSkinColor::getCorrectColor(r2);

        // For now make text the inverse of the background so it's readable In
        // the future this should be configurable from the skin with this as the
        // fallback option
        QColor text(255 - r1.red(), 255 - r1.green(), 255 - r1.blue());

        //setAlternatingRowColors ( true );

        QColor fgColor;
        fgColor.setNamedColor(WWidget::selectNodeQString(node, "FgColor"));
        fgColor = WSkinColor::getCorrectColor(fgColor);

        pal.setColor(QPalette::Base, r1);
        pal.setColor(QPalette::AlternateBase, r2);
        pal.setColor(QPalette::Text, text);
        pal.setColor(QPalette::WindowText, fgColor);

    }

    setPalette(pal);

    pushButtonAutoDJ->setPalette(pal);
    //m_pTrackTableView->setPalette(pal); //Since we're getting this passed into us already created,
                                          //shouldn't need to set the palette.
}

void DlgAutoDJ::onSearchStarting()
{
}

void DlgAutoDJ::onSearchCleared()
{
}

void DlgAutoDJ::onSearch(const QString& text)
{
    m_pAutoDJTableModel->search(text);
}

void DlgAutoDJ::loadSelectedTrack() {
    m_pTrackTableView->loadSelectedTrack();
}

void DlgAutoDJ::loadSelectedTrackToGroup(QString group) {
    m_pTrackTableView->loadSelectedTrackToGroup(group);
}

void DlgAutoDJ::moveSelection(int delta) {
    m_pTrackTableView->moveSelection(delta);
}

void DlgAutoDJ::shufflePlaylist(bool buttonChecked){
    Q_UNUSED(buttonChecked);
    m_pTrackTableView->sortByColumn(0, Qt::AscendingOrder);
    qDebug() << "Shuffling AutoDJ playlist";
    m_pAutoDJTableModel->shuffleTracks(m_pAutoDJTableModel->index(0, 0));
    qDebug() << "Shuffling done";
}

void DlgAutoDJ::toggleAutoDJ(bool toggle){
    // Emit signal for AutoDJ
    emit setAutoDJEnabled(toggle);

    if (toggle) { // Enable Auto DJ
        pushButtonAutoDJ->setText(tr("Disable Auto DJ"));
        m_bAutoDJEnabled = true;
    }
    else { // Disable Auto DJ
    }
}

bool DlgAutoDJ::loadNextTrackFromQueue(bool removeTopMostBeforeLoading) {
    if (removeTopMostBeforeLoading) {
        //Only remove the top track if this isn't the start of Auto DJ mode.
        m_pAutoDJTableModel->removeTrack(m_pAutoDJTableModel->index(0, 0));
    }
    //Get the track at the top of the playlist...
    TrackPointer nextTrack = m_pAutoDJTableModel->getTrack(m_pAutoDJTableModel->index(0, 0));

    if (!nextTrack) { // We ran out of tracks in the queue...
        //Disable auto DJ and return...
        pushButtonAutoDJ->setChecked(false);
        emit endOfPlaylist(true);
        return false;
    }
    //m_bNextTrackAlreadyLoaded = false;

    emit(loadTrack(nextTrack));

    return true;
}

void DlgAutoDJ::slotNextTrackNeeded() {

    // Get the track at the top of the playlist
    TrackPointer nextTrack = m_pAutoDJTableModel->getTrack(m_pAutoDJTableModel->index(0, 0));

    if (!nextTrack) {
        // Playlist empty, disable AutoDJ and emit that end of playlist has been reached
        pushButtonAutoDJ->setChecked(false);
        emit endOfPlaylist(true);
        return;
    }
    qDebug() << "Sending track";
    emit sendNextTrack(nextTrack);
}

void DlgAutoDJ::slotDisableAutoDJ() {
    m_bAutoDJEnabled = false;
    pushButtonAutoDJ->setChecked(false);
    pushButtonAutoDJ->setText(tr("Enable Auto DJ"));
}
