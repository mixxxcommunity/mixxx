/***************************************************************************
                          mixxx.cpp  -  description
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

#include <QtDebug>
#include <QtCore>
#include <QtGui>
#include <QTranslator>

#include "mixxx.h"

#include "analyserqueue.h"
#include "controlobjectthreadmain.h"
#include "controlpotmeter.h"
#include "defs_urls.h"
#include "defs_version.h"
#include "dlgabout.h"
#include "dlgpreferences.h"
#include "engine/enginemaster.h"
#include "engine/enginemicrophone.h"
#include "library/libraryfeatures.h"
#include "library/libraryscanner.h"
#include "library/librarytablemodel.h"
#include "controllers/controllermanager.h"
#include "mixxxkeyboard.h"
#include "playermanager.h"
#include "recording/defs_recording.h"
#include "recording/recordingmanager.h"
#include "skin/legacyskinparser.h"
#include "skin/skinloader.h"
#include "soundmanager.h"
#include "soundmanagerutil.h"
#include "soundsourceproxy.h"
#include "trackinfoobject.h"
#include "upgrade.h"
#include "waveform/waveformwidgetfactory.h"
#include "buildversion.h"


#ifdef __TAGREADER__
#include "core/tagreaderclient.h"
#endif

#ifdef __VINYLCONTROL__
#include "vinylcontrol/vinylcontrol.h"
#include "vinylcontrol/vinylcontrolmanager.h"
#endif

extern "C" void crashDlg()
{
    QMessageBox::critical(0, "Mixxx",
        "Mixxx has encountered a serious error and needs to close.");
}


bool loadTranslations(const QLocale& systemLocale, QString userLocale,
                      QString translation, QString prefix,
                      QString translationPath, QTranslator* pTranslator) {

    if (userLocale.size() == 0) {
#if QT_VERSION >= 0x040800
        QStringList uiLanguages = systemLocale.uiLanguages();
        if (uiLanguages.size() > 0 && uiLanguages.first() == "en") {
            // Don't bother loading a translation if the first ui-langauge is
            // English because the interface is already in English. This fixes
            // the case where the user's install of Qt doesn't have an explicit
            // English translation file and the fact that we don't ship a
            // mixxx_en.qm.
            return false;
        }
        return pTranslator->load(systemLocale, translation, prefix, translationPath);
#else
        userLocale = systemLocale.name();
#endif  // QT_VERSION
    }
    return pTranslator->load(translation + prefix + userLocale, translationPath);
}

MixxxApp::MixxxApp(QApplication *pApp, const CmdlineArgs& args)
{

    // This is the first line in mixxx.log
    qDebug() << "Mixxx" << BuildVersion::buildInfoFormatted() << "is starting...";
    qDebug() << "Qt version is:" << qVersion();

    QCoreApplication::setApplicationName("Mixxx");
    QCoreApplication::setApplicationVersion(BuildVersion::versionName());
#ifdef __APPLE__
    setWindowTitle(tr("Mixxx")); //App Store
#elif defined(AMD64) || defined(EM64T) || defined(x86_64)
    setWindowTitle(tr("Mixxx " VERSION " x64"));
#elif defined(IA64)
    setWindowTitle(tr("Mixxx " VERSION " Itanium"));
#else
    setWindowTitle(tr("Mixxx " VERSION));
#endif
    setWindowIcon(QIcon(":/images/ic_mixxx_window.png"));

    //Reset pointer to players
    m_pSoundManager = NULL;
    m_pPrefDlg = NULL;
    m_pControllerManager = 0;
    m_pRecordingManager = NULL;

    // Check to see if this is the first time this version of Mixxx is run
    // after an upgrade and make any needed changes.
    Upgrade upgrader;
    m_pConfig = upgrader.versionUpgrade(args.getSettingsPath());
    bool bFirstRun = upgrader.isFirstRun();
    bool bUpgraded = upgrader.isUpgraded();

    QString resourcePath = m_pConfig->getResourcePath();
    QString translationsFolder = resourcePath + "translations/";

    // Load Qt base translations
    QString userLocale = args.getLocale();
    QLocale systemLocale = QLocale::system();

    // Attempt to load user locale from config
    if (userLocale == "") {
        userLocale = m_pConfig->getValueString(ConfigKey("[Config]","Locale"));
    }

    // Load Qt translations for this locale from the system translation
    // path. This is the lowest precedence QTranslator.
    QTranslator* qtTranslator = new QTranslator(pApp);
    if (loadTranslations(systemLocale, userLocale, "qt", "_",
                         QLibraryInfo::location(QLibraryInfo::TranslationsPath),
                         qtTranslator)) {
        pApp->installTranslator(qtTranslator);
    } else {
        delete qtTranslator;
    }

    // Load Qt translations for this locale from the Mixxx translations
    // folder.
    QTranslator* mixxxQtTranslator = new QTranslator(pApp);
    if (loadTranslations(systemLocale, userLocale, "qt", "_",
                         translationsFolder,
                         mixxxQtTranslator)) {
        pApp->installTranslator(mixxxQtTranslator);
    } else {
        delete mixxxQtTranslator;
    }

    // Load Mixxx specific translations for this locale from the Mixxx
    // translations folder.
    QTranslator* mixxxTranslator = new QTranslator(pApp);
    bool mixxxLoaded = loadTranslations(systemLocale, userLocale, "mixxx", "_",
                                        translationsFolder, mixxxTranslator);
    qDebug() << "Loading translations for locale"
             << (userLocale.size() > 0 ? userLocale : systemLocale.name())
             << "from translations folder" << translationsFolder << ":"
             << (mixxxLoaded ? "success" : "fail");
    if (mixxxLoaded) {
        pApp->installTranslator(mixxxTranslator);
    } else {
        delete mixxxTranslator;
    }

    // Store the path in the config database
    m_pConfig->set(ConfigKey("[Config]", "Path"), ConfigValue(resourcePath));

    // Set the default value in settings file
    if (m_pConfig->getValueString(ConfigKey("[Keyboard]","Enabled")).length() == 0)
        m_pConfig->set(ConfigKey("[Keyboard]","Enabled"), ConfigValue(1));

    // Read keyboard configuration and set kdbConfig object in WWidget
    // Check first in user's Mixxx directory
    QString userKeyboard = args.getSettingsPath() + "Custom.kbd.cfg";

    //Empty keyboard configuration
    m_pKbdConfigEmpty = new ConfigObject<ConfigValueKbd>("");

    QString shortcutSource = m_pConfig->getValueString(ConfigKey("[Controls]", "ShortcutsSource"),"1");

    if (shortcutSource == "2" && QFile::exists(userKeyboard)) {
        qDebug() << "Found and will use custom keyboard preset" << userKeyboard;
        m_pKbdConfig = new ConfigObject<ConfigValueKbd>(userKeyboard);
    } else if (shortcutSource != "0") {
        // Use the default config for local keyboard
        QLocale locale = QApplication::keyboardInputLocale();

        // check if a default keyboard exists
        QString defaultKeyboard = QString(resourcePath).append("keyboard/");
        defaultKeyboard += locale.name();
        defaultKeyboard += ".kbd.cfg";

        if (!QFile::exists(defaultKeyboard)) {
            qDebug() << defaultKeyboard << " not found, using en_US.kbd.cfg";
            defaultKeyboard = QString(resourcePath).append("keyboard/").append("en_US.kbd.cfg");
            if (!QFile::exists(defaultKeyboard)) {
                qDebug() << defaultKeyboard << " not found, starting without shortcuts";
                defaultKeyboard = "";
            }
        }
        m_pKbdConfig = new ConfigObject<ConfigValueKbd>(defaultKeyboard);
    } else {
        m_pKbdConfig = new ConfigObject<ConfigValueKbd>("");
    }

    // TODO(XXX) leak pKbdConfig, MixxxKeyboard owns it? Maybe roll all keyboard
    // initialization into MixxxKeyboard
    // Workaround for today: MixxxKeyboard calls delete
    bool keyboardShortcutsEnabled = m_pConfig->getValueString(
        ConfigKey("[Keyboard]", "Enabled")) == "1";
    m_pKeyboard = new MixxxKeyboard(keyboardShortcutsEnabled ? m_pKbdConfig : m_pKbdConfigEmpty);

    //create RecordingManager
    m_pRecordingManager = new RecordingManager(m_pConfig);

    // Starting the master (mixing of the channels and effects):
    m_pEngine = new EngineMaster(m_pConfig, "[Master]", true);

    connect(m_pEngine, SIGNAL(isRecording(bool)),
            m_pRecordingManager,SLOT(slotIsRecording(bool)));
    connect(m_pEngine, SIGNAL(bytesRecorded(int)),
            m_pRecordingManager,SLOT(slotBytesRecorded(int)));

    // Initialize player device
    // while this is created here, setupDevices needs to be called sometime
    // after the players are added to the engine (as is done currently) -- bkgood
    m_pSoundManager = new SoundManager(m_pConfig, m_pEngine);

    EngineMicrophone* pMicrophone = new EngineMicrophone("[Microphone]");
    AudioInput micInput = AudioInput(AudioPath::MICROPHONE, 0, 0); // What should channelbase be?
    m_pEngine->addChannel(pMicrophone);
    m_pSoundManager->registerInput(micInput, pMicrophone);

    // Get Music dir
    bool hasChanged_MusicDir = false;
    QDir dir(m_pConfig->getValueString(ConfigKey("[Playlist]","Directory")));
    if (m_pConfig->getValueString(
        ConfigKey("[Playlist]","Directory")).length() < 1 || !dir.exists())
    {
        // TODO this needs to be smarter, we can't distinguish between an empty
        // path return value (not sure if this is normally possible, but it is
        // possible with the Windows 7 "Music" library, which is what
        // QDesktopServices::storageLocation(QDesktopServices::MusicLocation)
        // resolves to) and a user hitting 'cancel'. If we get a blank return
        // but the user didn't hit cancel, we need to know this and let the
        // user take some course of action -- bkgood
        QString fd = QFileDialog::getExistingDirectory(
            this, tr("Choose music library directory"),
            QDesktopServices::storageLocation(QDesktopServices::MusicLocation));

        if (fd != "")
        {
            m_pConfig->set(ConfigKey("[Playlist]","Directory"), fd);
            m_pConfig->Save();
            hasChanged_MusicDir = true;
        }
    }
#ifdef __TAGREADER__
    m_tag_reader_client = TagReaderClient::Instance();
    if (!m_tag_reader_client) {
        m_tag_reader_client = new TagReaderClient(this);
        MoveToNewThread(m_tag_reader_client, "TagReaderClient");
        m_tag_reader_client->Start();
    }
#else
    // Do not write meta data back to ID3 when meta data has changed
    // Because multiple TrackDao objects can exists for a particular track
    // writing meta data may ruine your MP3 file if done simultaneously.
    // see Bug #728197
    // For safety reasons, we deactivate this feature.
    m_pConfig->set(ConfigKey("[Library]","WriteAudioTags"), ConfigValue(0));
#endif // __TAGREADER__

    // library dies in seemingly unrelated qtsql error about not having a
    // sqlite driver if this path doesn't exist. Normally config->Save()
    // above would make it but if it doesn't get run for whatever reason
    // we get hosed -- bkgood
    if (!QDir(args.getSettingsPath()).exists()) {
        QDir().mkpath(args.getSettingsPath());
    }
    m_pLibrary = new LibraryFeatures(this, m_pConfig,
                             bFirstRun || bUpgraded,
                             m_pRecordingManager);
    qRegisterMetaType<TrackPointer>("TrackPointer");

    // Create the player manager.
    m_pPlayerManager = new PlayerManager(m_pConfig, m_pEngine, m_pLibrary);
    m_pPlayerManager->addDeck();
    m_pPlayerManager->addDeck();
    m_pPlayerManager->addDeck();
    m_pPlayerManager->addDeck();
    m_pPlayerManager->addSampler();
    m_pPlayerManager->addSampler();
    m_pPlayerManager->addSampler();
    m_pPlayerManager->addSampler();

    // register the engine's outputs
    m_pSoundManager->registerOutput(AudioOutput(AudioOutput::MASTER),
        m_pEngine);
    m_pSoundManager->registerOutput(AudioOutput(AudioOutput::HEADPHONES),
        m_pEngine);
    for (unsigned int deck = 0; deck < m_pPlayerManager->numDecks(); ++deck) {
        // TODO(bkgood) make this look less dumb by putting channelBase after
        // index in the AudioOutput() params
        m_pSoundManager->registerOutput(
            AudioOutput(AudioOutput::DECK, 0, deck), m_pEngine);
    }
    for (int o = EngineChannel::LEFT ; o <= EngineChannel::RIGHT ; o++) {
        m_pSoundManager->registerOutput(
            AudioOutput(AudioOutput::XFADERINPUT, 0, o), m_pEngine);
    }

#ifdef __VINYLCONTROL__
    m_pVCManager = new VinylControlManager(this, m_pConfig);
    for (unsigned int deck = 0; deck < m_pPlayerManager->numDecks(); ++deck) {
        m_pSoundManager->registerInput(
            AudioInput(AudioInput::VINYLCONTROL, 0, deck),
            m_pVCManager);
    }
#else
    m_pVCManager = NULL;
#endif

    //Scan the library directory.
    m_pLibraryScanner = new LibraryScanner(m_pLibrary->getTrackCollection());

    //Refresh the library models when the library (re)scan is finished.
    connect(m_pLibraryScanner, SIGNAL(scanFinished()),
            m_pLibrary, SLOT(slotRefreshLibraryModels()));

    //Scan the library for new files and directories
    bool rescan = (bool)m_pConfig->getValueString(ConfigKey("[Library]","RescanOnStartup")).toInt();
    // rescan the library if we get a new plugin
    QSet<QString> prev_plugins = QSet<QString>::fromList(m_pConfig->getValueString(
        ConfigKey("[Library]", "SupportedFileExtensions")).split(",", QString::SkipEmptyParts));
    QSet<QString> curr_plugins = QSet<QString>::fromList(
        SoundSourceProxy::supportedFileExtensions());
    rescan = rescan || (prev_plugins != curr_plugins);

    if(rescan || hasChanged_MusicDir){
        m_pLibraryScanner->scan(
            m_pConfig->getValueString(ConfigKey("[Playlist]", "Directory")));
        qDebug() << "Rescan finished";
    }
    m_pConfig->set(ConfigKey("[Library]", "SupportedFileExtensions"),
        QStringList(SoundSourceProxy::supportedFileExtensions()).join(","));

    // Call inits to invoke all other construction parts

    // Intialize default BPM system values
    if (m_pConfig->getValueString(ConfigKey("[BPM]", "BPMRangeStart"))
            .length() < 1)
    {
        m_pConfig->set(ConfigKey("[BPM]", "BPMRangeStart"),ConfigValue(65));
    }

    if (m_pConfig->getValueString(ConfigKey("[BPM]", "BPMRangeEnd"))
            .length() < 1)
    {
        m_pConfig->set(ConfigKey("[BPM]", "BPMRangeEnd"),ConfigValue(135));
    }

    if (m_pConfig->getValueString(ConfigKey("[BPM]", "AnalyzeEntireSong"))
            .length() < 1)
    {
        m_pConfig->set(ConfigKey("[BPM]", "AnalyzeEntireSong"),ConfigValue(1));
    }

    //ControlObject::getControl(ConfigKey("[Channel1]","TrackEndMode"))->queueFromThread(m_pConfig->getValueString(ConfigKey("[Controls]","TrackEndModeCh1")).toDouble());
    //ControlObject::getControl(ConfigKey("[Channel2]","TrackEndMode"))->queueFromThread(m_pConfig->getValueString(ConfigKey("[Controls]","TrackEndModeCh2")).toDouble());

    // Initialize controller sub-system,
    //  but do not set up controllers until the end of the application startup
    qDebug() << "Creating ControllerManager";
    m_pControllerManager = new ControllerManager(m_pConfig);

    WaveformWidgetFactory::create();
    WaveformWidgetFactory::instance()->setConfig(m_pConfig);

    m_pSkinLoader = new SkinLoader(m_pConfig);

    // Initialize preference dialog
    m_pPrefDlg = new DlgPreferences(this, m_pSkinLoader, m_pSoundManager, m_pPlayerManager,
                                    m_pControllerManager, m_pVCManager, m_pConfig);
    m_pPrefDlg->setWindowIcon(QIcon(":/images/ic_mixxx_window.png"));
    m_pPrefDlg->setHidden(true);

    // Try open player device If that fails, the preference panel is opened.
    int setupDevices = m_pSoundManager->setupDevices();
    unsigned int numDevices = m_pSoundManager->getConfig().getOutputs().count();
    // test for at least one out device, if none, display another dlg that
    // says "mixxx will barely work with no outs"
    while (setupDevices != OK || numDevices == 0)
    {
        // Exit when we press the Exit button in the noSoundDlg dialog
        // only call it if setupDevices != OK
        if (setupDevices != OK) {
            if (noSoundDlg() != 0) {
                exit(0);
            }
        } else if (numDevices == 0) {
            bool continueClicked = false;
            int noOutput = noOutputDlg(&continueClicked);
            if (continueClicked) break;
            if (noOutput != 0) {
                exit(0);
            }
        }
        setupDevices = m_pSoundManager->setupDevices();
        numDevices = m_pSoundManager->getConfig().getOutputs().count();
    }

    //setFocusPolicy(QWidget::StrongFocus);
    //grabKeyboard();

    // Load tracks in args.qlMusicFiles (command line arguments) into player
    // 1 and 2:
    for (int i = 0; i < (int)m_pPlayerManager->numDecks()
            && i < args.getMusicFiles().count(); ++i) {
        if ( SoundSourceProxy::isFilenameSupported(args.getMusicFiles().at(i))) {
            m_pPlayerManager->slotLoadToDeck(args.getMusicFiles().at(i), i+1);
        }
    }

    //Automatically load specially marked promotional tracks on first run
    if (bFirstRun || bUpgraded) {
        QList<TrackPointer> tracksToAutoLoad =
            m_pLibrary->getTracksToAutoLoad();
        for (int i = 0; i < (int)m_pPlayerManager->numDecks()
                && i < tracksToAutoLoad.count(); i++) {
            m_pPlayerManager->slotLoadToDeck(tracksToAutoLoad.at(i)->getLocation(), i+1);
        }
    }

    initActions();
    initMenuBar();

    // Use frame as container for view, needed for fullscreen display
    m_pView = new QFrame();

    m_pWidgetParent = NULL;
    // Loads the skin as a child of m_pView
    // assignment intentional in next line
    if (!(m_pWidgetParent = m_pSkinLoader->loadDefaultSkin(
        m_pView, m_pKeyboard, m_pPlayerManager, m_pControllerManager, m_pLibrary, m_pVCManager))) {
        qCritical("default skin cannot be loaded see <b>mixxx</b> trace for more information.");

        //TODO (XXX) add dialog to warn user and launch skin choice page
        resize(640,480);
    } else {
        // this has to be after the OpenGL widgets are created or depending on a
        // million different variables the first waveform may be horribly
        // corrupted. See bug 521509 -- bkgood ?? -- vrince
        setCentralWidget(m_pView);

        // keep gui centered (esp for fullscreen)
        m_pView->setLayout( new QHBoxLayout(m_pView));
        m_pView->layout()->setContentsMargins(0,0,0,0);
        m_pView->layout()->addWidget(m_pWidgetParent);
        resize(m_pView->size());
    }

    //move the app in the center of the primary screen
    slotToCenterOfPrimaryScreen();

    // Check direct rendering and warn user if they don't have it
    checkDirectRendering();

    //Install an event filter to catch certain QT events, such as tooltips.
    //This allows us to turn off tooltips.
    pApp->installEventFilter(this); // The eventfilter is located in this
                                      // Mixxx class as a callback.

    // If we were told to start in fullscreen mode on the command-line,
    // then turn on fullscreen mode.
    if (args.getStartInFullscreen()) {
        slotOptionsFullScreen(true);
    }

    // Refresh the GUI (workaround for Qt 4.6 display bug)
    /* // TODO(bkgood) delete this block if the moving of setCentralWidget
     * //              totally fixes this first-wavefore-fubar issue for
     * //              everyone
    QString QtVersion = qVersion();
    if (QtVersion>="4.6.0") {
        qDebug() << "Qt v4.6.0 or higher detected. Using rebootMixxxView() "
            "workaround.\n    (See bug https://bugs.launchpad.net/mixxx/"
            "+bug/521509)";
        rebootMixxxView();
    } */

    // Wait until all other ControlObjects are set up
    //  before initializing controllers
    m_pControllerManager->setUpDevices();
}

MixxxApp::~MixxxApp()
{
    QTime qTime;
    qTime.start();

    qDebug() << "Destroying MixxxApp";

    qDebug() << "save config " << qTime.elapsed();
    m_pConfig->Save();

    // Save state of End of track controls in config database
    //m_pConfig->set(ConfigKey("[Controls]","TrackEndModeCh1"), ConfigValue((int)ControlObject::getControl(ConfigKey("[Channel1]","TrackEndMode"))->get()));
    //m_pConfig->set(ConfigKey("[Controls]","TrackEndModeCh2"), ConfigValue((int)ControlObject::getControl(ConfigKey("[Channel2]","TrackEndMode"))->get()));

    // SoundManager depend on Engine and Config
    qDebug() << "delete soundmanager " << qTime.elapsed();
    delete m_pSoundManager;

#ifdef __VINYLCONTROL__
    // VinylControlManager depends on a CO the engine owns
    // (vinylcontrol_enabled in VinylControlControl)
    qDebug() << "delete vinylcontrolmanager " << qTime.elapsed();
    delete m_pVCManager;
#endif

    // View depends on MixxxKeyboard, PlayerManager, Library
    qDebug() << "delete view " << qTime.elapsed();
    delete m_pView;

    // SkinLoader depends on Config
    qDebug() << "delete SkinLoader " << qTime.elapsed();
    delete m_pSkinLoader;

    // ControllerManager depends on Config
    qDebug() << "shutdown & delete ControllerManager " << qTime.elapsed();
    m_pControllerManager->shutdown();
    delete m_pControllerManager;

    // PlayerManager depends on Engine, Library, and Config
    qDebug() << "delete playerManager " << qTime.elapsed();
    delete m_pPlayerManager;

    // EngineMaster depends on Config
    qDebug() << "delete m_pEngine " << qTime.elapsed();
    delete m_pEngine;

    // LibraryScanner depends on Library
    qDebug() << "delete library scanner " <<  qTime.elapsed();
    delete m_pLibraryScanner;

    // Delete the library after the view so there are no dangling pointers to
    // Depends on RecordingManager
    // the data models.
    qDebug() << "delete library " << qTime.elapsed();
    delete m_pLibrary;

    // RecordingManager depends on config
    qDebug() << "delete RecordingManager " << qTime.elapsed();
    delete m_pRecordingManager;

    // HACK: Save config again. We saved it once before doing some dangerous
    // stuff. We only really want to save it here, but the first one was just
    // a precaution. The earlier one can be removed when stuff is more stable
    // at exit.

    //Disable shoutcast so when Mixxx starts again it will not connect
    m_pConfig->set(ConfigKey("[Shoutcast]", "enabled"),0);
    m_pConfig->Save();
    delete m_pPrefDlg;

    qDebug() << "delete config " << qTime.elapsed();
    delete m_pConfig;

    // Check for leaked ControlObjects and give warnings.
    QList<ControlObject*> leakedControls;
    QList<ConfigKey> leakedConfigKeys;

    ControlObject::getControls(&leakedControls);

    if (leakedControls.size() > 0) {
        qDebug() << "WARNING: The following" << leakedControls.size() << "controls were leaked:";
        foreach (ControlObject* pControl, leakedControls) {
            ConfigKey key = pControl->getKey();
            qDebug() << key.group << key.item;
            leakedConfigKeys.append(key);
        }

       foreach (ConfigKey key, leakedConfigKeys) {
           // delete just to satisfy valgrind:
           // check if the pointer is still valid, the control object may have bin already
           // deleted by its parent in this loop
           delete ControlObject::getControl(key);
       }
   }
   qDebug() << "~MixxxApp: All leaking controls deleted.";

   delete m_pKeyboard;
   delete m_pKbdConfigEmpty;
}

int MixxxApp::noSoundDlg(void)
{
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(tr("Sound Device Busy"));
    msgBox.setText(
        "<html>" +
        tr("Mixxx was unable to access all the configured sound devices. "
        "Another application is using a sound device Mixxx is configured to "
        "use or a device is not plugged in.") +
        "<ul>"
            "<li>" +
                tr("<b>Retry</b> after closing the other application "
                "or reconnecting a sound device") +
            "</li>"
            "<li>" +
                tr("<b>Reconfigure</b> Mixxx's sound device settings.") +
            "</li>"
            "<li>" +
                tr("Get <b>Help</b> from the Mixxx Wiki.") +
            "</li>"
            "<li>" +
                tr("<b>Exit</b> Mixxx.") +
            "</li>"
        "</ul></html>"
    );

    QPushButton *retryButton = msgBox.addButton(tr("Retry"),
        QMessageBox::ActionRole);
    QPushButton *reconfigureButton = msgBox.addButton(tr("Reconfigure"),
        QMessageBox::ActionRole);
    QPushButton *wikiButton = msgBox.addButton(tr("Help"),
        QMessageBox::ActionRole);
    QPushButton *exitButton = msgBox.addButton(tr("Exit"),
        QMessageBox::ActionRole);

    while (true)
    {
        msgBox.exec();

        if (msgBox.clickedButton() == retryButton) {
            m_pSoundManager->queryDevices();
            return 0;
        } else if (msgBox.clickedButton() == wikiButton) {
            QDesktopServices::openUrl(QUrl(
                "http://mixxx.org/wiki/doku.php/troubleshooting"
                "#no_or_too_few_sound_cards_appear_in_the_preferences_dialog")
            );
            wikiButton->setEnabled(false);
        } else if (msgBox.clickedButton() == reconfigureButton) {
            msgBox.hide();
            m_pSoundManager->queryDevices();

            // This way of opening the dialog allows us to use it synchronously
            m_pPrefDlg->setWindowModality(Qt::ApplicationModal);
            m_pPrefDlg->exec();
            if (m_pPrefDlg->result() == QDialog::Accepted) {
                m_pSoundManager->queryDevices();
                return 0;
            }

            msgBox.show();

        } else if (msgBox.clickedButton() == exitButton) {
            return 1;
        }
    }
}

int MixxxApp::noOutputDlg(bool *continueClicked)
{
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(tr("No Output Devices"));
    msgBox.setText( "<html>" + tr("Mixxx was configured without any output sound devices. "
                    "Audio processing will be disabled without a configured output device.") +
                    "<ul>"
                        "<li>" +
                            tr("<b>Continue</b> without any outputs.") +
                        "</li>"
                        "<li>" +
                            tr("<b>Reconfigure</b> Mixxx's sound device settings.") +
                        "</li>"
                        "<li>" +
                            tr("<b>Exit</b> Mixxx.") +
                        "</li>"
                    "</ul></html>"
    );

    QPushButton *continueButton = msgBox.addButton(tr("Continue"), QMessageBox::ActionRole);
    QPushButton *reconfigureButton = msgBox.addButton(tr("Reconfigure"), QMessageBox::ActionRole);
    QPushButton *exitButton = msgBox.addButton(tr("Exit"), QMessageBox::ActionRole);

    while (true)
    {
        msgBox.exec();

        if (msgBox.clickedButton() == continueButton) {
            *continueClicked = true;
            return 0;
        } else if (msgBox.clickedButton() == reconfigureButton) {
            msgBox.hide();
            m_pSoundManager->queryDevices();

            // This way of opening the dialog allows us to use it synchronously
            m_pPrefDlg->setWindowModality(Qt::ApplicationModal);
            m_pPrefDlg->exec();
            if ( m_pPrefDlg->result() == QDialog::Accepted) {
                m_pSoundManager->queryDevices();
                return 0;
            }

            msgBox.show();

        } else if (msgBox.clickedButton() == exitButton) {
            return 1;
        }
    }
}

QString buildWhatsThis(QString title, QString text) {
    return QString("%1\n\n%2").arg(title.replace("&", ""), text);
}

/** initializes all QActions of the application */
void MixxxApp::initActions()
{
    QString loadTrackText = tr("Load Track to Deck %1");
    QString loadTrackStatusText = tr("Loads a track in deck %1");
    QString openText = tr("Open");

    QString player1LoadStatusText = loadTrackStatusText.arg(QString::number(1));
    m_pFileLoadSongPlayer1 = new QAction(loadTrackText.arg(QString::number(1)), this);
    m_pFileLoadSongPlayer1->setShortcut(QKeySequence(tr("Ctrl+O")));
    m_pFileLoadSongPlayer1->setShortcutContext(Qt::ApplicationShortcut);
    m_pFileLoadSongPlayer1->setStatusTip(player1LoadStatusText);
    m_pFileLoadSongPlayer1->setWhatsThis(
        buildWhatsThis(openText, player1LoadStatusText));
    connect(m_pFileLoadSongPlayer1, SIGNAL(triggered()),
            this, SLOT(slotFileLoadSongPlayer1()));

    QString player2LoadStatusText = loadTrackStatusText.arg(QString::number(2));
    m_pFileLoadSongPlayer2 = new QAction(loadTrackText.arg(QString::number(2)), this);
    m_pFileLoadSongPlayer2->setShortcut(QKeySequence(tr("Ctrl+Shift+O")));
    m_pFileLoadSongPlayer2->setShortcutContext(Qt::ApplicationShortcut);
    m_pFileLoadSongPlayer2->setStatusTip(player2LoadStatusText);
    m_pFileLoadSongPlayer2->setWhatsThis(
        buildWhatsThis(openText, player2LoadStatusText));
    connect(m_pFileLoadSongPlayer2, SIGNAL(triggered()),
            this, SLOT(slotFileLoadSongPlayer2()));

    QString quitTitle = tr("&Exit");
    QString quitText = tr("Quits Mixxx");
    m_pFileQuit = new QAction(quitTitle, this);
    m_pFileQuit->setShortcut(QKeySequence(tr("Ctrl+Q")));
    m_pFileQuit->setShortcutContext(Qt::ApplicationShortcut);
    m_pFileQuit->setStatusTip(quitText);
    m_pFileQuit->setWhatsThis(buildWhatsThis(quitTitle, quitText));
    connect(m_pFileQuit, SIGNAL(triggered()), this, SLOT(slotFileQuit()));

    QString rescanTitle = tr("&Rescan Library");
    QString rescanText = tr("Rescans library folders for changes to tracks.");
    m_pLibraryRescan = new QAction(rescanTitle, this);
    m_pLibraryRescan->setStatusTip(rescanText);
    m_pLibraryRescan->setWhatsThis(buildWhatsThis(rescanTitle, rescanText));
    m_pLibraryRescan->setCheckable(false);
    connect(m_pLibraryRescan, SIGNAL(triggered()),
            this, SLOT(slotScanLibrary()));
    connect(m_pLibraryScanner, SIGNAL(scanFinished()),
            this, SLOT(slotEnableRescanLibraryAction()));

    QString createPlaylistTitle = tr("Add &New Playlist");
    QString createPlaylistText = tr("Create a new playlist");
    m_pPlaylistsNew = new QAction(createPlaylistTitle, this);
    m_pPlaylistsNew->setShortcut(QKeySequence(tr("Ctrl+N")));
    m_pPlaylistsNew->setShortcutContext(Qt::ApplicationShortcut);
    m_pPlaylistsNew->setStatusTip(createPlaylistText);
    m_pPlaylistsNew->setWhatsThis(buildWhatsThis(createPlaylistTitle, createPlaylistText));
    connect(m_pPlaylistsNew, SIGNAL(triggered()),
            m_pLibrary, SLOT(slotCreatePlaylist()));

    QString createCrateTitle = tr("Add New &Crate");
    QString createCrateText = tr("Create a new crate");
    m_pCratesNew = new QAction(createCrateTitle, this);
    m_pCratesNew->setShortcut(QKeySequence(tr("Ctrl+Shift+N")));
    m_pCratesNew->setShortcutContext(Qt::ApplicationShortcut);
    m_pCratesNew->setStatusTip(createCrateText);
    m_pCratesNew->setWhatsThis(buildWhatsThis(createCrateTitle, createCrateText));
    connect(m_pCratesNew, SIGNAL(triggered()),
            m_pLibrary, SLOT(slotCreateCrate()));

    QString fullScreenTitle = tr("&Full Screen");
    QString fullScreenText = tr("Display Mixxx using the full screen");
    m_pOptionsFullScreen = new QAction(fullScreenTitle, this);
#ifdef __APPLE__
    m_pOptionsFullScreen->setShortcut(QKeySequence(tr("Ctrl+Shift+F")));
#else
    m_pOptionsFullScreen->setShortcut(QKeySequence(tr("F11")));
#endif
    m_pOptionsFullScreen->setShortcutContext(Qt::ApplicationShortcut);
    // QShortcut * shortcut = new QShortcut(QKeySequence(tr("Esc")),  this);
    // connect(shortcut, SIGNAL(triggered()), this, SLOT(slotQuitFullScreen()));
    m_pOptionsFullScreen->setCheckable(true);
    m_pOptionsFullScreen->setChecked(false);
    m_pOptionsFullScreen->setStatusTip(fullScreenText);
    m_pOptionsFullScreen->setWhatsThis(buildWhatsThis(fullScreenTitle, fullScreenText));
    connect(m_pOptionsFullScreen, SIGNAL(toggled(bool)),
            this, SLOT(slotOptionsFullScreen(bool)));

    QString keyboardShortcutTitle = tr("Enable &Keyboard Shortcuts");
    QString keyboardShortcutText = tr("Toggles keyboard shortcuts on or off");
    bool keyboardShortcutsEnabled = m_pConfig->getValueString(
        ConfigKey("[Keyboard]", "Enabled")) == "1";
    m_pOptionsKeyboard = new QAction(keyboardShortcutTitle, this);
    m_pOptionsKeyboard->setShortcut(tr("Ctrl+`"));
    m_pOptionsKeyboard->setShortcutContext(Qt::ApplicationShortcut);
    m_pOptionsKeyboard->setCheckable(true);
    m_pOptionsKeyboard->setChecked(keyboardShortcutsEnabled);
    m_pOptionsKeyboard->setStatusTip(keyboardShortcutText);
    m_pOptionsKeyboard->setWhatsThis(buildWhatsThis(keyboardShortcutTitle, keyboardShortcutText));
    connect(m_pOptionsKeyboard, SIGNAL(toggled(bool)),
            this, SLOT(slotOptionsKeyboard(bool)));

    QString preferencesTitle = tr("&Preferences");
    QString preferencesText = tr("Change Mixxx settings (e.g. playback, MIDI, controls)");
    m_pOptionsPreferences = new QAction(preferencesTitle, this);
    m_pOptionsPreferences->setShortcut(QKeySequence(tr("Ctrl+P")));
    m_pOptionsPreferences->setShortcutContext(Qt::ApplicationShortcut);
    m_pOptionsPreferences->setStatusTip(preferencesText);
    m_pOptionsPreferences->setWhatsThis(buildWhatsThis(preferencesTitle, preferencesText));
    connect(m_pOptionsPreferences, SIGNAL(triggered()),
            this, SLOT(slotOptionsPreferences()));

    QString aboutTitle = tr("&About");
    QString aboutText = tr("About the application");
    m_pHelpAboutApp = new QAction(aboutTitle, this);
    m_pHelpAboutApp->setStatusTip(aboutText);
    m_pHelpAboutApp->setWhatsThis(buildWhatsThis(aboutTitle, aboutText));
    connect(m_pHelpAboutApp, SIGNAL(triggered()),
            this, SLOT(slotHelpAbout()));

    QString supportTitle = tr("&Community Support");
    QString supportText = tr("Get help with Mixxx");
    m_pHelpSupport = new QAction(supportTitle, this);
    m_pHelpSupport->setStatusTip(supportText);
    m_pHelpSupport->setWhatsThis(buildWhatsThis(supportTitle, supportText));
    connect(m_pHelpSupport, SIGNAL(triggered()), this, SLOT(slotHelpSupport()));

    QString manualTitle = tr("&User Manual");
    QString manualText = tr("Read the Mixxx user manual.");
    m_pHelpManual = new QAction(manualTitle, this);
    m_pHelpManual->setStatusTip(manualText);
    m_pHelpManual->setWhatsThis(buildWhatsThis(manualTitle, manualText));
    connect(m_pHelpManual, SIGNAL(triggered()), this, SLOT(slotHelpManual()));

    QString feedbackTitle = tr("Send Us &Feedback");
    QString feedbackText = tr("Send feedback to the Mixxx team.");
    m_pHelpFeedback = new QAction(feedbackTitle, this);
    m_pHelpFeedback->setStatusTip(feedbackText);
    m_pHelpFeedback->setWhatsThis(buildWhatsThis(feedbackTitle, feedbackText));
    connect(m_pHelpFeedback, SIGNAL(triggered()), this, SLOT(slotHelpFeedback()));

    QString translateTitle = tr("&Translate This Application");
    QString translateText = tr("Help translate this application into your language.");
    m_pHelpTranslation = new QAction(translateTitle, this);
    m_pHelpTranslation->setStatusTip(translateText);
    m_pHelpTranslation->setWhatsThis(buildWhatsThis(translateTitle, translateText));
    connect(m_pHelpTranslation, SIGNAL(triggered()), this, SLOT(slotHelpTranslation()));

#ifdef __VINYLCONTROL__
    QString vinylControlText = tr("Use timecoded vinyls on external turntables to control Mixxx");
    QString vinylControlTitle1 = tr("Enable Vinyl Control &1");
    QString vinylControlTitle2 = tr("Enable Vinyl Control &2");

    m_pOptionsVinylControl = new QAction(vinylControlTitle1, this);
    m_pOptionsVinylControl->setShortcut(QKeySequence(tr("Ctrl+Y")));
    m_pOptionsVinylControl->setShortcutContext(Qt::ApplicationShortcut);
    // Either check or uncheck the vinyl control menu item depending on what
    // it was saved as.
    m_pOptionsVinylControl->setCheckable(true);
    m_pOptionsVinylControl->setChecked(false);
    m_pOptionsVinylControl->setStatusTip(vinylControlText);
    m_pOptionsVinylControl->setWhatsThis(buildWhatsThis(vinylControlTitle1, vinylControlText));
    connect(m_pOptionsVinylControl, SIGNAL(toggled(bool)), this,
        SLOT(slotCheckboxVinylControl(bool)));
    ControlObjectThreadMain* enabled1 = new ControlObjectThreadMain(
        ControlObject::getControl(ConfigKey("[Channel1]", "vinylcontrol_enabled")),this);
    connect(enabled1, SIGNAL(valueChanged(double)), this,
        SLOT(slotControlVinylControl(double)));

    m_pOptionsVinylControl2 = new QAction(vinylControlTitle2, this);
    m_pOptionsVinylControl2->setShortcut(tr("Ctrl+U"));
    m_pOptionsVinylControl2->setShortcutContext(Qt::ApplicationShortcut);
    m_pOptionsVinylControl2->setCheckable(true);
    m_pOptionsVinylControl2->setChecked(false);
    m_pOptionsVinylControl2->setStatusTip(vinylControlText);
    m_pOptionsVinylControl2->setWhatsThis(buildWhatsThis(vinylControlTitle2, vinylControlText));
    connect(m_pOptionsVinylControl2, SIGNAL(toggled(bool)), this,
        SLOT(slotCheckboxVinylControl2(bool)));

    ControlObjectThreadMain* enabled2 = new ControlObjectThreadMain(
        ControlObject::getControl(ConfigKey("[Channel2]", "vinylcontrol_enabled")),this);
    connect(enabled2, SIGNAL(valueChanged(double)), this,
        SLOT(slotControlVinylControl2(double)));
#endif

#ifdef __SHOUTCAST__
    QString shoutcastTitle = tr("Enable Live &Broadcasting");
    QString shoutcastText = tr("Stream your mixes to a shoutcast or icecast server");
    m_pOptionsShoutcast = new QAction(shoutcastTitle, this);
    m_pOptionsShoutcast->setShortcut(tr("Ctrl+L"));
    m_pOptionsShoutcast->setShortcutContext(Qt::ApplicationShortcut);
    m_pOptionsShoutcast->setCheckable(true);
    bool broadcastEnabled =
        (m_pConfig->getValueString(ConfigKey("[Shoutcast]", "enabled"))
            .toInt() == 1);

    m_pOptionsShoutcast->setChecked(broadcastEnabled);
    m_pOptionsShoutcast->setStatusTip(shoutcastText);
    m_pOptionsShoutcast->setWhatsThis(buildWhatsThis(shoutcastTitle, shoutcastText));
    connect(m_pOptionsShoutcast, SIGNAL(toggled(bool)),
            this, SLOT(slotOptionsShoutcast(bool)));
#endif

    QString recordTitle = tr("&Record Mix");
    QString recordText = tr("Record your mix to a file");
    m_pOptionsRecord = new QAction(recordTitle, this);
    m_pOptionsRecord->setShortcut(tr("Ctrl+R"));
    m_pOptionsRecord->setShortcutContext(Qt::ApplicationShortcut);
    m_pOptionsRecord->setCheckable(true);
    m_pOptionsRecord->setStatusTip(recordText);
    m_pOptionsRecord->setWhatsThis(buildWhatsThis(recordTitle, recordText));
    connect(m_pOptionsRecord, SIGNAL(toggled(bool)),
            this, SLOT(slotOptionsRecord(bool)));

	m_pOptionsKeyboard = new QAction(tr("Enable &keyboard mapping"), this); 
    m_pOptionsKeyboard->setCheckable(true);
    m_pOptionsKeyboard->setChecked(true);
    m_pOptionsKeyboard->setStatusTip(tr("Toggle Keyboard On/Off"));
    m_pOptionsFullScreen->setWhatsThis(
        tr("Enable/Disable keyboard mappings"));
    connect(m_pOptionsKeyboard, SIGNAL(toggled(bool)),
            this, SLOT(slotOptionsKeyboard(bool)));
}

void MixxxApp::initMenuBar()
{
    // MENUBAR
    m_pFileMenu = new QMenu(tr("&File"), menuBar());
    m_pOptionsMenu = new QMenu(tr("&Options"), menuBar());
    m_pLibraryMenu = new QMenu(tr("&Library"),menuBar());
    m_pViewMenu = new QMenu(tr("&View"), menuBar());
    m_pHelpMenu = new QMenu(tr("&Help"), menuBar());
    connect(m_pOptionsMenu, SIGNAL(aboutToShow()),
            this, SLOT(slotOptionsMenuShow()));
    // menuBar entry fileMenu
    m_pFileMenu->addAction(m_pFileLoadSongPlayer1);
    m_pFileMenu->addAction(m_pFileLoadSongPlayer2);
    m_pFileMenu->addSeparator();
    m_pFileMenu->addAction(m_pFileQuit);

    // menuBar entry optionsMenu
    //optionsMenu->setCheckable(true);
#ifdef __VINYLCONTROL__
    m_pVinylControlMenu = new QMenu(tr("&Vinyl Control"), menuBar());
    m_pVinylControlMenu->addAction(m_pOptionsVinylControl);
    m_pVinylControlMenu->addAction(m_pOptionsVinylControl2);
    m_pOptionsMenu->addMenu(m_pVinylControlMenu);
#endif
    m_pOptionsMenu->addAction(m_pOptionsRecord);
#ifdef __SHOUTCAST__
    m_pOptionsMenu->addAction(m_pOptionsShoutcast);
#endif
    m_pOptionsMenu->addAction(m_pOptionsKeyboard);
    m_pOptionsMenu->addAction(m_pOptionsFullScreen);
    m_pOptionsMenu->addSeparator();
    m_pOptionsMenu->addAction(m_pOptionsPreferences);

    m_pLibraryMenu->addAction(m_pLibraryRescan);
    m_pLibraryMenu->addSeparator();
    m_pLibraryMenu->addAction(m_pPlaylistsNew);
    m_pLibraryMenu->addAction(m_pCratesNew);

    // menuBar entry viewMenu
    //viewMenu->setCheckable(true);

    // menuBar entry helpMenu
    m_pHelpMenu->addAction(m_pHelpSupport);
    m_pHelpMenu->addAction(m_pHelpManual);
    m_pHelpMenu->addAction(m_pHelpFeedback);
    m_pHelpMenu->addAction(m_pHelpTranslation);
    m_pHelpMenu->addSeparator();
    m_pHelpMenu->addAction(m_pHelpAboutApp);

    menuBar()->addMenu(m_pFileMenu);
    menuBar()->addMenu(m_pLibraryMenu);
    menuBar()->addMenu(m_pOptionsMenu);

    //    menuBar()->addMenu(viewMenu);
    menuBar()->addSeparator();
    menuBar()->addMenu(m_pHelpMenu);

    m_NativeMenuBarSupport = menuBar()->isNativeMenuBar();
}

void MixxxApp::slotFileLoadSongPlayer(int deck) {
    QString group = m_pPlayerManager->groupForDeck(deck-1);
    ControlObject* play =
        ControlObject::getControl(ConfigKey(group, "play"));

    QString loadTrackText = tr("Load track to Deck %1").arg(QString::number(deck));
    QString deckWarningMessage = tr("Deck %1 is currently playing a track.")
            .arg(QString::number(deck));
    QString areYouSure = tr("Are you sure you want to load a new track?");

    if (play->get() == 1.) {
        int ret = QMessageBox::warning(this, tr("Mixxx"),
            deckWarningMessage + "\n" + areYouSure,
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (ret != QMessageBox::Yes)
            return;
    }

    QString s =
        QFileDialog::getOpenFileName(
            this,
            loadTrackText,
            m_pConfig->getValueString(ConfigKey("[Playlist]", "Directory")),
            QString("Audio (%1)")
                .arg(SoundSourceProxy::supportedFileExtensionsString()));

    if (!s.isNull()) {
        m_pPlayerManager->slotLoadToDeck(s, deck);
    }
}

void MixxxApp::slotFileLoadSongPlayer1() {
    slotFileLoadSongPlayer(1);
}

void MixxxApp::slotFileLoadSongPlayer2() {
    slotFileLoadSongPlayer(2);
}

void MixxxApp::slotFileQuit()
{
    if (!confirmExit()) {
        return;
    }
    hide();
    qApp->quit();
}

void MixxxApp::slotOptionsKeyboard(bool toggle) {
    if (toggle) {
        //qDebug() << "Enable keyboard shortcuts/mappings";
        m_pKeyboard->setKeyboardConfig(m_pKbdConfig);
        m_pConfig->set(ConfigKey("[Keyboard]","Enabled"), ConfigValue(1));
    } else {
        //qDebug() << "Disable keyboard shortcuts/mappings";
        m_pKeyboard->setKeyboardConfig(m_pKbdConfigEmpty);
        m_pConfig->set(ConfigKey("[Keyboard]","Enabled"), ConfigValue(0));
    }
}

void MixxxApp::slotOptionsFullScreen(bool toggle)
{
    if (m_pOptionsFullScreen)
        m_pOptionsFullScreen->setChecked(toggle);

    if (isFullScreen() == toggle) {
        return;
    }

    if (toggle) {
#if defined(__LINUX__) || defined(__APPLE__)
         // this and the later move(m_winpos) doesn't seem necessary
         // here on kwin, if it's necessary with some other x11 wm, re-enable
         // it, I guess -bkgood
         //m_winpos = pos();
         // fix some x11 silliness -- for some reason the move(m_winpos)
         // is moving the currentWindow to (0, 0), not the frame (as it's
         // supposed to, I might add)
         // if this messes stuff up on your distro yell at me -bkgood
         //m_winpos.setX(m_winpos.x() + (geometry().x() - x()));
         //m_winpos.setY(m_winpos.y() + (geometry().y() - y()));
#endif
#ifdef __LINUX__
        // Fix for "No menu bar with ubuntu unity in full screen mode" Bug #885890
        // Not for Mac OS because the native menu bar will unhide when moving
        // the mouse to the top of screen
        menuBar()->setNativeMenuBar(false);
#endif
        showFullScreen();
    } else {
        showNormal();
#ifdef __LINUX__
        menuBar()->setNativeMenuBar(m_NativeMenuBarSupport);
        //move(m_winpos);
#endif
    }
}

void MixxxApp::slotOptionsPreferences()
{
    m_pPrefDlg->setHidden(false);
    m_pPrefDlg->activateWindow();
}

void MixxxApp::slotControlVinylControl(double toggle)
{
#ifdef __VINYLCONTROL__
    if (m_pVCManager->vinylInputEnabled(1)) {
        m_pOptionsVinylControl->setChecked((bool)toggle);
    } else {
        m_pOptionsVinylControl->setChecked(false);
        if (toggle) {
            QMessageBox::warning(this, tr("Mixxx"),
                tr("No input device(s) select.\nPlease select your soundcard(s) "
                    "in the sound hardware preferences."),
                QMessageBox::Ok,
                QMessageBox::Ok);
            m_pPrefDlg->show();
            m_pPrefDlg->showSoundHardwarePage();
            ControlObject::getControl(ConfigKey("[Channel1]", "vinylcontrol_status"))->set(VINYL_STATUS_DISABLED);
            ControlObject::getControl(ConfigKey("[Channel1]", "vinylcontrol_enabled"))->set(0);
        }
    }
#endif
}

void MixxxApp::slotCheckboxVinylControl(bool toggle)
{
#ifdef __VINYLCONTROL__
    ControlObject::getControl(ConfigKey("[Channel1]", "vinylcontrol_enabled"))->set((double)toggle);
#endif
}

void MixxxApp::slotControlVinylControl2(double toggle)
{
#ifdef __VINYLCONTROL__
    if (m_pVCManager->vinylInputEnabled(2)) {
        m_pOptionsVinylControl2->setChecked((bool)toggle);
    } else {
        m_pOptionsVinylControl2->setChecked(false);
        if (toggle) {
            QMessageBox::warning(this, tr("Mixxx"),
                tr("No input device(s) select.\nPlease select your soundcard(s) "
                    "in the sound hardware preferences."),
                QMessageBox::Ok,
                QMessageBox::Ok);
            m_pPrefDlg->show();
            m_pPrefDlg->showSoundHardwarePage();
            ControlObject::getControl(ConfigKey("[Channel2]", "vinylcontrol_status"))->set(VINYL_STATUS_DISABLED);
            ControlObject::getControl(ConfigKey("[Channel2]", "vinylcontrol_enabled"))->set(0);
        }
    }
#endif
}

void MixxxApp::slotCheckboxVinylControl2(bool toggle)
{
#ifdef __VINYLCONTROL__
    ControlObject::getControl(ConfigKey("[Channel2]", "vinylcontrol_enabled"))->set((double)toggle);
#endif
}

//Also can't ifdef this (MOC again)
void MixxxApp::slotOptionsRecord(bool toggle)
{
    //Only start recording if checkbox was set to true and recording is inactive
    if(toggle && !m_pRecordingManager->isRecordingActive()) //start recording
        m_pRecordingManager->startRecording();
    //Only stop recording if checkbox was set to false and recording is active
    else if(!toggle && m_pRecordingManager->isRecordingActive())
        m_pRecordingManager->stopRecording();
}

void MixxxApp::slotHelpAbout() {
    DlgAbout *about = new DlgAbout(this);
    about->version_label->setText(BuildVersion::versionLable());

    QString s_devTeam = QString(tr("Mixxx %1 Development Team")).arg(BuildVersion::versionName());
    QString s_contributions = tr("With contributions from:");
    QString s_specialThanks = tr("And special thanks to:");
    QString s_pastDevs = tr("Past Developers");
    QString s_pastContribs = tr("Past Contributors");

    QString credits = QString("<p align=\"center\"><b>%1</b></p>"
"<p align=\"center\">"
"Adam Davison<br>"
"Albert Santoni<br>"
"RJ Ryan<br>"
"Garth Dahlstrom<br>"
"Sean Pappalardo<br>"
"Phillip Whelan<br>"
"Tobias Rafreider<br>"
"S. Brandt<br>"
"Bill Good<br>"
"Owen Williams<br>"
"Vittorio Colao<br>"
"Daniel Sch&uuml;rmann<br>"
"Thomas Vincent<br>"

"</p>"
"<p align=\"center\"><b>%2</b></p>"
"<p align=\"center\">"
"Mark Hills<br>"
"Andre Roth<br>"
"Robin Sheat<br>"
"Mark Glines<br>"
"Mathieu Rene<br>"
"Miko Kiiski<br>"
"Brian Jackson<br>"
"Andreas Pflug<br>"
"Bas van Schaik<br>"
"J&aacute;n Jockusch<br>"
"Oliver St&ouml;neberg<br>"
"Jan Jockusch<br>"
"C. Stewart<br>"
"Bill Egert<br>"
"Zach Shutters<br>"
"Owen Bullock<br>"
"Graeme Mathieson<br>"
"Sebastian Actist<br>"
"Jussi Sainio<br>"
"David Gnedt<br>"
"Antonio Passamani<br>"
"Guy Martin<br>"
"Anders Gunnarsson<br>"
"Alex Barker<br>"
"Mikko Jania<br>"
"Juan Pedro Bol&iacute;var Puente<br>"
"Linus Amvall<br>"
"Irwin C&eacute;spedes B<br>"
"Micz Flor<br>"
"Daniel James<br>"
"Mika Haulo<br>"
"Matthew Mikolay<br>"
"Tom Mast<br>"
"Miko Kiiski<br>"
"Vin&iacute;cius Dias dos Santos<br>"
"Joe Colosimo<br>"
"Shashank Kumar<br>"
"Till Hofmann<br>"
"Peter V&aacute;gner<br>"
"Thanasis Liappis<br>"
"Jens Nachtigall<br>"
"Scott Ullrich<br>"
"Jonas &Aring;dahl<br>"
"Jonathan Costers<br>"
"Daniel Lindenfelser<br>"
"Maxime Bochon<br>"
"Akash Shetye<br>"
"Pascal Bleser<br>"
"Florian Mahlknecht<br>"
"Ben Clark<br>"
"Ilkka Tuohela<br>"
"Tom Gascoigne<br>"
"Max Linke<br>"
"Neale Pickett<br>"
"Aaron Mavrinac<br>"

"</p>"
"<p align=\"center\"><b>%3</b></p>"
"<p align=\"center\">"
"Vestax<br>"
"Stanton<br>"
"Hercules<br>"
"EKS<br>"
"Echo Digital Audio<br>"
"JP Disco<br>"
"Adam Bellinson<br>"
"Alexandre Bancel<br>"
"Melanie Thielker<br>"
"Julien Rosener<br>"
"Pau Arum&iacute;<br>"
"David Garcia<br>"
"Seb Ruiz<br>"
"Joseph Mattiello<br>"
"</p>"

"<p align=\"center\"><b>%4</b></p>"
"<p align=\"center\">"
"Tue Haste Andersen<br>"
"Ken Haste Andersen<br>"
"Cedric Gestes<br>"
"John Sully<br>"
"Torben Hohn<br>"
"Peter Chang<br>"
"Micah Lee<br>"
"Ben Wheeler<br>"
"Wesley Stessens<br>"
"Nathan Prado<br>"
"Zach Elko<br>"
"Tom Care<br>"
"Pawel Bartkiewicz<br>"
"Nick Guenther<br>"
"Bruno Buccolo<br>"
"Ryan Baker<br>"
"</p>"

"<p align=\"center\"><b>%5</b></p>"
"<p align=\"center\">"
"Ludek Hor&#225;cek<br>"
"Svein Magne Bang<br>"
"Kristoffer Jensen<br>"
"Ingo Kossyk<br>"
"Mads Holm<br>"
"Lukas Zapletal<br>"
"Jeremie Zimmermann<br>"
"Gianluca Romanin<br>"
"Tim Jackson<br>"
"Stefan Langhammer<br>"
"Frank Willascheck<br>"
"Jeff Nelson<br>"
"Kevin Schaper<br>"
"Alex Markley<br>"
"Oriol Puigb&oacute;<br>"
"Ulrich Heske<br>"
"James Hagerman<br>"
"quil0m80<br>"
"Martin Sakm&#225;r<br>"
"Ilian Persson<br>"
"Dave Jarvis<br>"
"Thomas Baag<br>"
"Karlis Kalnins<br>"
"Amias Channer<br>"
"Sacha Berger<br>"
"James Evans<br>"
"Martin Sakmar<br>"
"Navaho Gunleg<br>"
"Gavin Pryke<br>"
"Michael Pujos<br>"
"Claudio Bantaloukas<br>"
"Pavol Rusnak<br>"
    "</p>").arg(s_devTeam,s_contributions,s_specialThanks,s_pastDevs,s_pastContribs);

    about->textBrowser->setHtml(credits);
    about->show();

}

void MixxxApp::slotHelpSupport() {
    QUrl qSupportURL;
    qSupportURL.setUrl(MIXXX_SUPPORT_URL);
    QDesktopServices::openUrl(qSupportURL);
}

void MixxxApp::slotHelpFeedback() {
    QUrl qFeedbackUrl;
    qFeedbackUrl.setUrl(MIXXX_FEEDBACK_URL);
    QDesktopServices::openUrl(qFeedbackUrl);
}

void MixxxApp::slotHelpTranslation() {
    QUrl qTranslationUrl;
    qTranslationUrl.setUrl(MIXXX_TRANSLATION_URL);
    QDesktopServices::openUrl(qTranslationUrl);
}

void MixxxApp::slotHelpManual() {
    QDir resourceDir(m_pConfig->getResourcePath());
    // Default to the mixxx.org hosted version of the manual.
    QUrl qManualUrl(MIXXX_MANUAL_URL);
#if defined(__APPLE__)
    // We don't include the PDF manual in the bundle on OSX. Default to the
    // web-hosted version.
#elif defined(__WINDOWS__)
    // On Windows, the manual PDF sits in the same folder as the 'skins' folder.
    if (resourceDir.exists(MIXXX_MANUAL_FILENAME)) {
        qManualUrl = QUrl::fromLocalFile(
                resourceDir.absoluteFilePath(MIXXX_MANUAL_FILENAME));
    }
#elif defined(__LINUX__)
    // On GNU/Linux, the manual is installed to e.g. /usr/share/mixxx/doc/
    resourceDir.cd("doc");
    if (resourceDir.exists(MIXXX_MANUAL_FILENAME)) {
        qManualUrl = QUrl::fromLocalFile(
                resourceDir.absoluteFilePath(MIXXX_MANUAL_FILENAME));
    }
#else
    // No idea, default to the mixxx.org hosted version.
#endif
    QDesktopServices::openUrl(qManualUrl);
}

void MixxxApp::rebootMixxxView() {

    if (!m_pWidgetParent || !m_pView)
        return;

    qDebug() << "Now in rebootMixxxView...";

    QPoint initPosition = pos();
    QSize initSize = size();

    m_pView->hide();

    WaveformWidgetFactory::instance()->stop();
    WaveformWidgetFactory::instance()->destroyWidgets();

    // Workaround for changing skins while fullscreen, just go out of fullscreen
    // mode. If you change skins while in fullscreen (on Linux, at least) the
    // window returns to 0,0 but and the backdrop disappears so it looks as if
    // it is not fullscreen, but acts as if it is.
    bool wasFullScreen = m_pOptionsFullScreen->isChecked();
    slotOptionsFullScreen(false);

    //delete the view cause swaping central widget do not remove the old one !
    if (m_pView) {
        delete m_pView;
    }
    m_pView = new QFrame();

    // assignment in next line intentional
    if (!(m_pWidgetParent = m_pSkinLoader->loadDefaultSkin(m_pView,
                                                           m_pKeyboard,
                                                           m_pPlayerManager,
                                                           m_pControllerManager,
                                                           m_pLibrary,
                                                           m_pVCManager))) {

        QMessageBox::critical(this,
                              tr("Error in skin file"),
                              tr("The selected skin cannot be loaded."));
    }
    else {
        // don't move this before loadDefaultSkin above. bug 521509 --bkgood
        // NOTE: (vrince) I don't know this comment is relevant now ...
        setCentralWidget(m_pView);

        // keep gui centered (esp for fullscreen)
        m_pView->setLayout( new QHBoxLayout(m_pView));
        m_pView->layout()->setContentsMargins(0,0,0,0);
        m_pView->layout()->addWidget(m_pWidgetParent);

         //qDebug() << "view size" << m_pView->size();

        //TODO: (vrince) size is good but resize do not append !!
        resize(m_pView->size());
    }

    if( wasFullScreen) {
        slotOptionsFullScreen(true);
    } else {
        move(initPosition.x() + (initSize.width() - width()) / 2,
             initPosition.y() + (initSize.height() - height()) / 2);
    }

    WaveformWidgetFactory::instance()->start();

#ifdef __APPLE__
    // Original the following line fixes issue on OSX where menu bar went away
    // after a skin change. It was original surrounded by #if __OSX__
    // Now it seems it causes the opposite see Bug #1000187
    //menuBar()->setNativeMenuBar(m_NativeMenuBarSupport);
#endif

    qDebug() << "rebootMixxxView DONE";
}

/** Event filter to block certain events. For example, this function is used
  * to disable tooltips if the user specifies in the preferences that they
  * want them off. This is a callback function.
  */
bool MixxxApp::eventFilter(QObject *obj, QEvent *event)
{
    static int tooltips =
        m_pConfig->getValueString(ConfigKey("[Controls]", "Tooltips")).toInt();

    if (event->type() == QEvent::ToolTip) {
        // QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        // unused, remove? TODO(bkgood)
        if (tooltips == 1)
            return false;
        else
            return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
}

void MixxxApp::closeEvent(QCloseEvent *event) {
    if (!confirmExit()) {
        event->ignore();
    }
}

void MixxxApp::slotScanLibrary()
{
    m_pLibraryRescan->setEnabled(false);
    m_pLibraryScanner->scan(
        m_pConfig->getValueString(ConfigKey("[Playlist]", "Directory")));
}

void MixxxApp::slotEnableRescanLibraryAction()
{
    m_pLibraryRescan->setEnabled(true);
}

void MixxxApp::slotOptionsMenuShow(){
    // Check recording if it is active.
    m_pOptionsRecord->setChecked(m_pRecordingManager->isRecordingActive());

#ifdef __SHOUTCAST__
    bool broadcastEnabled =
        (m_pConfig->getValueString(ConfigKey("[Shoutcast]", "enabled")).toInt()
            == 1);
    if (broadcastEnabled)
      m_pOptionsShoutcast->setChecked(true);
    else
      m_pOptionsShoutcast->setChecked(false);
#endif
}

void MixxxApp::slotOptionsShoutcast(bool value){
#ifdef __SHOUTCAST__
    m_pOptionsShoutcast->setChecked(value);
    m_pConfig->set(ConfigKey("[Shoutcast]", "enabled"),ConfigValue(value));
#else
    Q_UNUSED(value);
#endif
}

void MixxxApp::slotToCenterOfPrimaryScreen() {
    if (!m_pView)
        return;

    QDesktopWidget* desktop = QApplication::desktop();
    int primaryScreen = desktop->primaryScreen();
    QRect primaryScreenRect = desktop->availableGeometry(primaryScreen);

    move(primaryScreenRect.left() + (primaryScreenRect.width() - m_pView->width()) / 2,
         primaryScreenRect.top() + (primaryScreenRect.height() - m_pView->height()) / 2);
}

void MixxxApp::checkDirectRendering() {
    // IF
    //  * A waveform viewer exists
    // AND
    //  * The waveform viewer is an OpenGL waveform viewer
    // AND
    //  * The waveform viewer does not have direct rendering enabled.
    // THEN
    //  * Warn user

    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();
    if (!factory)
        return;

    if (!factory->isOpenGLAvailable() &&
        m_pConfig->getValueString(ConfigKey("[Direct Rendering]", "Warned")) != QString("yes")) {
        QMessageBox::warning(
            0, tr("OpenGL Direct Rendering"),
            tr("Direct rendering is not enabled on your machine.<br><br>"
               "This means that the waveform displays will be very<br>"
               "<b>slow and may tax your CPU heavily</b>. Either update your<br>"
               "configuration to enable direct rendering, or disable<br>"
               "the waveform displays in the Mixxx preferences by selecting<br>"
               "\"Empty\" as the waveform display in the 'Interface' section.<br><br>"
               "NOTE: If you use NVIDIA hardware,<br>"
               "direct rendering may not be present, but you should<br>"
               "not experience degraded performance."));
        m_pConfig->set(ConfigKey("[Direct Rendering]", "Warned"), QString("yes"));
    }
}

bool MixxxApp::confirmExit() {
    bool playing(false);
    bool playingSampler(false);
    unsigned int deckCount = m_pPlayerManager->numDecks();
    unsigned int samplerCount = m_pPlayerManager->numSamplers();
    for (unsigned int i = 0; i < deckCount; ++i) {
        ControlObject *pPlayCO(
            ControlObject::getControl(
                ConfigKey(QString("[Channel%1]").arg(i + 1), "play")
            )
        );
        if (pPlayCO && pPlayCO->get()) {
            playing = true;
            break;
        }
    }
    for (unsigned int i = 0; i < samplerCount; ++i) {
        ControlObject *pPlayCO(
            ControlObject::getControl(
                ConfigKey(QString("[Sampler%1]").arg(i + 1), "play")
            )
        );
        if (pPlayCO && pPlayCO->get()) {
            playingSampler = true;
            break;
        }
    }
    if (playing) {
        QMessageBox::StandardButton btn = QMessageBox::question(this,
            tr("Confirm Exit"),
            tr("A deck is currently playing. Exit Mixxx?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn == QMessageBox::No) {
            return false;
        }
    } else if (playingSampler) {
        QMessageBox::StandardButton btn = QMessageBox::question(this,
            tr("Confirm Exit"),
            tr("A sampler is currently playing. Exit Mixxx?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn == QMessageBox::No) {
            return false;
        }
    }
    if (m_pPrefDlg->isVisible()) {
        QMessageBox::StandardButton btn = QMessageBox::question(
            this, tr("Confirm Exit"),
            tr("The preferences window is still open.") + "<br>" +
            tr("Discard any changes and exit Mixxx?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn == QMessageBox::No) {
            return false;
        }
        else {
            m_pPrefDlg->close();
        }
    }
    return true;
}

void MixxxApp::MoveToNewThread(QObject* object, const QString& name) {
  QThread* thread = new QThread(this);
  thread->setObjectName(name);

  MoveToThread(object, thread);

  thread->start();
  //threads_ << thread;
}

void MixxxApp::MoveToThread(QObject* object, QThread* thread) {
  object->setParent(NULL);
  object->moveToThread(thread);
  // objects_in_threads_ << object;
}
