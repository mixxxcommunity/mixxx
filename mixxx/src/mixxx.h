/***************************************************************************
                          mixxx.h  -  description
                             -------------------
    begin                : Mon Feb 18 09:48:17 CET 2002
    copyright            : (C) 2002 by Tue and Ken Haste Andersen
    email                :
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MIXXX_H
#define MIXXX_H

#include <QAction>
#include <QList>
#include <QMainWindow>
#include <QString>

// REMOVE ME
#include <QtDebug>
#include <QResizeEvent>

class EngineMaster;
class Library;
class LibraryScanner;
class ControllerManager;
class MixxxKeyboard;
class PlayerManager;
class RecordingManager;
class SkinLoader;
class VinylControlManager;
class DlgPreferences;
class SoundManager;
class AutoDJ;

#include "configobject.h"

/**
  * This Class is the base class for Mixxx. It sets up the main
  * window and providing a menubar.
  * For the main view, an instance of class MixxxView is
  * created which creates your view.
  */
class MixxxApp : public QMainWindow {
    Q_OBJECT

  public:
    /** Construtor. files is a list of command line arguments */
    MixxxApp(QApplication *app, struct CmdlineArgs args);
    virtual ~MixxxApp();

    /** initializes all QActions of the application */
    void initActions();
    /** initMenuBar creates the menu_bar and inserts the menuitems */
    void initMenuBar();

    void resizeEvent(QResizeEvent *e) { qDebug() << "resize" << e->size();}

    void rebootMixxxView();

  public slots:

    //void slotQuitFullScreen();
    void slotFileLoadSongPlayer(int deck);
    /** Opens a file in player 1 */
    void slotFileLoadSongPlayer1();
    /** Opens a file in player 2 */
    void slotFileLoadSongPlayer2();
    /** exits the application */
    void slotFileQuit();

    /** toggle vinyl control - Don't #ifdef this because MOC is dumb**/
    void slotControlVinylControl(double toggle);
    void slotCheckboxVinylControl(bool toggle);
    void slotControlVinylControl2(double toggle);
    void slotCheckboxVinylControl2(bool toggle);
    /** toggle recording - Don't #ifdef this because MOC is dumb**/
    void slotOptionsRecord(bool toggle);
    /** toogle keyboard on-off */
    void slotOptionsKeyboard(bool toggle);
    /** toogle full screen mode */
    void slotOptionsFullScreen(bool toggle);
    /** Preference dialog */
    void slotOptionsPreferences();
    /** shows an about dlg*/
    void slotHelpAbout();
    /** visits support section of website*/
    void slotHelpSupport();
    // Visits a feedback form
    void slotHelpFeedback();
    // Open the manual.
    void slotHelpManual();
    // Visits translation interface on launchpad.net
    void slotHelpTranslation();
    /** Scan or rescan the music library directory */
    void slotScanLibrary();
    /** Enables the "Rescan Library" menu item. This gets disabled when a scan is running.*/
    void slotEnableRescanLibraryAction();
    /**Updates the checkboxes for Recording and Livebroadcasting when connection drops, or lame is not available **/
    void slotOptionsMenuShow();
    /** toggles Livebroadcasting **/
    void slotOptionsShoutcast(bool value);

    void slotToCenterOfPrimaryScreen();

  protected:
    /** Event filter to block certain events (eg. tooltips if tooltips are disabled) */
    bool eventFilter(QObject *obj, QEvent *event);
    void closeEvent(QCloseEvent *event);

  private:
    void checkDirectRendering();
    bool confirmExit();

    // Pointer to the root GUI widget
    QWidget* m_pView;
    QWidget* m_pWidgetParent;

    QApplication *m_pApp;

    // The mixing engine.
    EngineMaster *m_pEngine;

    // The skin loader
    SkinLoader* m_pSkinLoader;

    // The sound manager
    SoundManager *m_pSoundManager;

    // Keeps track of players
    PlayerManager* m_pPlayerManager;
    // RecordingManager
    RecordingManager* m_pRecordingManager;
    ControllerManager *m_pControllerManager;

    ConfigObject<ConfigValue> *m_pConfig;

    VinylControlManager *m_pVCManager;

    MixxxKeyboard* m_pKeyboard;
    /** Library scanner object */
    LibraryScanner* m_pLibraryScanner;
    // The library management object
    Library* m_pLibrary;
    AutoDJ* m_pAutoDJ;

    /** file_menu contains all items of the menubar entry "File" */
    QMenu *m_pFileMenu;
    /** edit_menu contains all items of the menubar entry "Edit" */
    QMenu *m_pEditMenu;
    /** library menu */
    QMenu *m_pLibraryMenu;
    /** options_menu contains all items of the menubar entry "Options" */
    QMenu *m_pOptionsMenu;
    /** view_menu contains all items of the menubar entry "View" */
    QMenu *m_pViewMenu;
    /** view_menu contains all items of the menubar entry "Help" */
    QMenu *m_pHelpMenu;

    QAction *m_pFileLoadSongPlayer1;
    QAction *m_pFileLoadSongPlayer2;
    QAction *m_pFileQuit;
    QAction *m_pPlaylistsNew;
    QAction *m_pCratesNew;
    QAction *m_pLibraryRescan;
#ifdef __VINYLCONTROL__
    QMenu *m_pVinylControlMenu;
    QAction *m_pOptionsVinylControl;
    QAction *m_pOptionsVinylControl2;
#endif
    QAction *m_pOptionsRecord;
    QAction *m_pOptionsKeyboard;
    QAction *m_pOptionsFullScreen;
    QAction *m_pOptionsPreferences;
#ifdef __SHOUTCAST__
    QAction *m_pOptionsShoutcast;
#endif
    QAction *m_pHelpAboutApp;
    QAction *m_pHelpSupport;
    QAction *m_pHelpFeedback;
    QAction *m_pHelpTranslation;
    QAction *m_pHelpManual;

    int m_iNoPlaylists;

    /** Pointer to preference dialog */
    DlgPreferences *m_pPrefDlg;

    int noSoundDlg(void);
    int noOutputDlg(bool *continueClicked);
    // Fullscreen patch
    QPoint m_winpos;
    bool m_NativeMenuBarSupport;

    ConfigObject<ConfigValueKbd>* m_pKbdConfig;
    ConfigObject<ConfigValueKbd>* m_pKbdConfigEmpty;
};

//A structure to store the parsed command-line arguments
struct CmdlineArgs
{
    QList<QString> qlMusicFiles;    /* List of files to load into players at startup */
    bool bStartInFullscreen;        /* Start in fullscreen mode */
    QString locale;
};


#endif
