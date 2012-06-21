# -*- coding: utf-8 -*-

import os
import util
from mixxx import Dependence, Feature
import SCons.Script as SCons

class PortAudio(Dependence):

    def configure(self, build, conf):
        if not conf.CheckLib('portaudio'):
            raise Exception('Did not find libportaudio.a, portaudio.lib, or the PortAudio-v19 development header files.')

        #Turn on PortAudio support in Mixxx
        build.env.Append(CPPDEFINES = '__PORTAUDIO__');

    def sources(self, build):
        return ['sounddeviceportaudio.cpp']

class PortMIDI(Dependence):

    def configure(self, build, conf):
        #Check for PortTime
        if not conf.CheckLib(['porttime', 'libporttime']) and \
                not conf.CheckHeader(['porttime.h']):
            raise Exception("Did not find PortTime or its development headers.")
        if not conf.CheckLib(['portmidi', 'libportmidi']) and \
                not conf.CheckHeader(['portmidi.h']):
            raise Exception('Did not find PortMidi or its development headers.')

    def sources(self, build):
        return ['controllers/midi/portmidienumerator.cpp', 'controllers/midi/portmidicontroller.cpp']

class OpenGL(Dependence):

    def configure(self, build, conf):
        # Check for OpenGL (it's messy to do it for all three platforms) XXX
        # this should *NOT* have hardcoded paths like this
        if (not conf.CheckLib('GL') and
            not conf.CheckLib('opengl32') and
            not conf.CheckCHeader('/System/Library/Frameworks/OpenGL.framework/Versions/A/Headers/gl.h') and
            not conf.CheckCHeader('GL/gl.h')):
            raise Exception('Did not find OpenGL development files, exiting!')

        if (not conf.CheckLib('GLU') and
            not conf.CheckLib('glu32') and
            not conf.CheckCHeader('/System/Library/Frameworks/OpenGL.framework/Versions/A/Headers/glu.h')):
            raise Exception('Did not find GLU development files, exiting!')

        if build.platform_is_osx:
            build.env.Append(CPPPATH='/Library/Frameworks/OpenGL.framework/Headers/')
            build.env.Append(LINKFLAGS='-framework OpenGL')

class OggVorbis(Dependence):

    def configure(self, build, conf):
#        if build.platform_is_windows and build.machine_is_64bit:
            # For some reason this has to be checked this way on win64,
            # otherwise it looks for the dll lib which will cause a conflict
            # later
#            if not conf.CheckLib('vorbisfile_static'):
#                raise Exception('Did not find vorbisfile_static.lib or the libvorbisfile development headers.')
#        else:
        if not conf.CheckLib(['libvorbisfile', 'vorbisfile']):
            Exception('Did not find libvorbisfile.a, libvorbisfile.lib, '
                'or the libvorbisfile development headers.')

        if not conf.CheckLib(['libvorbis', 'vorbis']):
            raise Exception('Did not find libvorbis.a, libvorbis.lib, or the libvorbisfile development headers.')

        if not conf.CheckLib(['libogg', 'ogg']):
            raise Exception('Did not find libogg.a, libogg.lib, or the libogg development headers, exiting!')

    def sources(self, build):
        return ['soundsourceoggvorbis.cpp']


class SndFile(Dependence):

    def configure(self, build, conf):
        #if not conf.CheckLibWithHeader(['sndfile', 'libsndfile'], 'sndfile.h', 'C'):
        if not conf.CheckLib(['sndfile', 'libsndfile']):
            raise Exception("Did not find libsndfile or it\'s development headers, exiting!")
        build.env.Append(CPPDEFINES = '__SNDFILE__')

    def sources(self, build):
        return ['soundsourcesndfile.cpp']

class FLAC(Dependence):
    def configure(self, build, conf):
        if not conf.CheckHeader('FLAC/stream_decoder.h'):
            raise Exception('Did not find libFLAC development headers, exiting!')
        elif not conf.CheckLib(['libFLAC', 'FLAC']):
            raise Exception('Did not find libFLAC development libraries, exiting!')
        return

    def sources(self, build):
        return ['soundsourceflac.cpp',]

class Qt(Dependence):
    DEFAULT_QTDIRS = {'linux': '/usr/share/qt4',
                      'bsd': '/usr/local/lib/qt4',
                      'osx': '/Library/Frameworks',
                      'windows': 'C:\\qt\\4.6.0'}

    def satisfy(self):
        pass

    def configure(self, build, conf):
        # Emit various Qt defines
        build.env.Append(CPPDEFINES = ['QT_SHARED',
                                       'QT_TABLET_SUPPORT'])

        # Enable Qt include paths
        if build.platform_is_linux:
            if not conf.CheckForPKG('QtCore', '4.6'):
                raise Exception('QT >= 4.6 not found')

            #Try using David's qt4.py's Qt4-module finding thingy instead of pkg-config.
            #(This hopefully respects our qtdir=blah flag while linking now.)
            build.env.EnableQt4Modules(['QtCore',
                                        'QtGui',
                                        'QtOpenGL',
                                        'QtXml',
                                        'QtSvg',
                                        'QtSql',
                                        'QtScript',
                                        'QtXmlPatterns',
                                        'QtWebKit'
                                        #'QtUiTools',
                                        #'QtDesigner',
                                        ],
                                       debug=False)
        elif build.platform_is_osx:
            build.env.Append(LINKFLAGS = '-framework QtCore -framework QtOpenGL -framework QtGui -framework QtSql -framework QtXml -framework QtXmlPatterns  -framework QtNetwork -framework QtSql -framework QtScript -framework QtWebKit')
            build.env.Append(CPPPATH = ['/Library/Frameworks/QtCore.framework/Headers/',
                                        '/Library/Frameworks/QtOpenGL.framework/Headers/',
                                        '/Library/Frameworks/QtGui.framework/Headers/',
                                        '/Library/Frameworks/QtXml.framework/Headers/',
                                        '/Library/Frameworks/QtNetwork.framework/Headers/',
                                        '/Library/Frameworks/QtSql.framework/Headers/',
                                        '/Library/Frameworks/QtWebKit.framework/Headers/',
                                        '/Library/Frameworks/QtScript.framework/Headers/'])

        # Setup Qt library includes for non-OSX
        if build.platform_is_linux or build.platform_is_bsd:
            build.env.Append(LIBS = 'QtXml')
            build.env.Append(LIBS = 'QtGui')
            build.env.Append(LIBS = 'QtCore')
            build.env.Append(LIBS = 'QtNetwork')
            build.env.Append(LIBS = 'QtOpenGL')
            build.env.Append(LIBS = 'QtWebKit')
            build.env.Append(LIBS = 'QtScript')
        elif build.platform_is_windows:
            build.env.Append(LIBPATH=['$QTDIR/lib'])
            build.env.Append(LIBS = 'QtXml4')
            build.env.Append(LIBS = 'QtXmlPatterns4')
            build.env.Append(LIBS = 'QtSql4')
            build.env.Append(LIBS = 'QtGui4')
            build.env.Append(LIBS = 'QtCore4')
            build.env.Append(LIBS = 'QtScript4')
            build.env.Append(LIBS = 'QtWebKit4')
            build.env.Append(LIBS = 'QtNetwork4')
            build.env.Append(LIBS = 'QtOpenGL4')

        # Set Qt include paths for non-OSX
        if not build.platform_is_osx:
            build.env.Append(CPPPATH=['$QTDIR/include/QtCore',
                                      '$QTDIR/include/QtGui',
                                      '$QTDIR/include/QtXml',
                                      '$QTDIR/include/QtNetwork',
                                      '$QTDIR/include/QtScript',
                                      '$QTDIR/include/QtSql',
                                      '$QTDIR/include/QtOpenGL',
                                      '$QTDIR/include/QtWebKit',
                                      '$QTDIR/include/Qt'])

        # Set the rpath for linux/bsd/osx.
        # This is not support on OS X before the 10.5 SDK.
        using_104_sdk = (str(build.env["CCFLAGS"]).find("10.4") >= 0)
        compiling_on_104 = False
        if build.platform_is_osx:
            compiling_on_104 = (os.popen('sw_vers').readlines()[1].find('10.4') >= 0)
        if not build.platform_is_windows and not (using_104_sdk or compiling_on_104):
            build.env.Append(LINKFLAGS = "-Wl,-rpath,$QTDIR/lib")

        #QtSQLite DLL
        if build.platform_is_windows:
            build.flags['sqlitedll'] = util.get_flags(build.env, 'sqlitedll', 1)


class FidLib(Dependence):

    def sources(self, build):
        symbol = None
        if build.platform_is_windows:
            if build.toolchain_is_msvs:
                symbol = 'T_MSVC'
            elif build.crosscompile:
                # Not sure why, but fidlib won't build with mingw32msvc and
                # T_MINGW
                symbol = 'T_LINUX'
            elif build.toolchain_is_gnu:
                symbol = 'T_MINGW'
        else:
            symbol = 'T_LINUX'

        return [build.env.StaticObject('#lib/fidlib-0.9.10/fidlib.c',
                                       CPPDEFINES=symbol)]

    def configure(self, build, conf):
        build.env.Append(CPPPATH='#lib/fidlib-0.9.10/')

class ReplayGain(Dependence):

    def sources(self, build):
        return ["#lib/replaygain/replaygain.cpp"]

    def configure(self, build, conf):
        build.env.Append(CPPPATH="#lib/replaygain")

class SoundTouch(Dependence):
    SOUNDTOUCH_PATH = 'soundtouch-1.6.0'

    def sse_enabled(self, build):
        optimize = int(util.get_flags(build.env, 'optimize', 1))
        return (build.machine_is_64bit or
                (build.toolchain_is_msvs and optimize > 2) or
                (build.toolchain_is_gnu and optimize > 1))

    def sources(self, build):
        sources = ['engine/enginebufferscalest.cpp',
                   '#lib/%s/SoundTouch.cpp' % self.SOUNDTOUCH_PATH,
                   '#lib/%s/TDStretch.cpp' % self.SOUNDTOUCH_PATH,
                   '#lib/%s/RateTransposer.cpp' % self.SOUNDTOUCH_PATH,
                   '#lib/%s/AAFilter.cpp' % self.SOUNDTOUCH_PATH,
                   '#lib/%s/FIFOSampleBuffer.cpp' % self.SOUNDTOUCH_PATH,
                   '#lib/%s/FIRFilter.cpp' % self.SOUNDTOUCH_PATH,
                   '#lib/%s/PeakFinder.cpp' % self.SOUNDTOUCH_PATH,
                   '#lib/%s/BPMDetect.cpp' % self.SOUNDTOUCH_PATH]

        # SoundTouch CPU optimizations are only for x86
        # architectures. SoundTouch automatically ignores these files when it is
        # not being built for an architecture that supports them.
        cpu_detection = '#lib/%s/cpu_detect_x86_win.cpp' if build.toolchain_is_msvs else \
            '#lib/%s/cpu_detect_x86_gcc.cpp'
        sources.append(cpu_detection % self.SOUNDTOUCH_PATH)

        # Check if the compiler has SSE extention enabled
        # Allways the case on x64 (core instructions)
        if self.sse_enabled(build):
            sources.extend(
                ['#lib/%s/mmx_optimized.cpp' % self.SOUNDTOUCH_PATH,
                 '#lib/%s/sse_optimized.cpp' % self.SOUNDTOUCH_PATH,])
        return sources

    def configure(self, build, conf):
        if build.platform_is_windows:
            # Regardless of the bitwidth, ST checks for WIN32
            build.env.Append(CPPDEFINES = 'WIN32')
        build.env.Append(CPPPATH=['#lib/%s' % self.SOUNDTOUCH_PATH])

        # Check if the compiler has SSE extention enabled
        # Allways the case on x64 (core instructions)
        optimize = int(util.get_flags(build.env, 'optimize', 1))
        if self.sse_enabled(build):
            build.env.Append(CPPDEFINES='SOUNDTOUCH_ALLOW_X86_OPTIMIZATIONS')

class TagLib(Dependence):
    def configure(self, build, conf):
        if not conf.CheckLib('tag'):
            raise Exception("Could not find libtag or its development headers.")

        # Karmic seems to have an issue with mp4tag.h where they don't include
        # the files correctly. Adding this folder ot the include path should fix
        # it, though might cause issues. This is safe to remove once we
        # deprecate Karmic support. rryan 2/2011
        build.env.Append(CPPPATH='/usr/include/taglib/')

class ProtoBuf(Dependence):
    def configure(self, build, conf):
        if not conf.CheckLib(['libprotobuf-lite', 'protobuf-lite', 'libprotobuf', 'protobuf']):
            raise Exception("Could not find libprotobuf or its development headers.")

class MixxxCore(Feature):

    def description(self):
        return "Mixxx Core Features"

    def enabled(self, build):
        return True

    def sources(self, build):
        sources = ["mixxxkeyboard.cpp",

                   "configobject.cpp",
                   "controlobjectthread.cpp",
                   "controlobjectthreadwidget.cpp",
                   "controlobjectthreadmain.cpp",
                   "controlevent.cpp",
                   "controllogpotmeter.cpp",
                   "controlobject.cpp",
                   "controlnull.cpp",
                   "controlpotmeter.cpp",
                   "controlpushbutton.cpp",
                   "controlttrotary.cpp",
                   "controlbeat.cpp",

                   "dlgpreferences.cpp",
                   "dlgprefsound.cpp",
                   "dlgprefsounditem.cpp",
                   "controllers/dlgprefcontroller.cpp",
                   "controllers/dlgprefmappablecontroller.cpp",
                   "controllers/dlgcontrollerlearning.cpp",
                   "controllers/dlgprefnocontrollers.cpp",
                   "dlgprefplaylist.cpp",
                   "dlgprefcontrols.cpp",
                   "dlgprefbpm.cpp",
                   "dlgprefreplaygain.cpp",
                   "dlgprefnovinyl.cpp",
                   "dlgbpmscheme.cpp",
                   "dlgabout.cpp",
                   "dlgprefeq.cpp",
                   "dlgprefcrossfader.cpp",
                   "dlgtrackinfo.cpp",
                   "dlgprepare.cpp",
                   "dlgautodj.cpp",

                   "engine/engineworker.cpp",
                   "engine/engineworkerscheduler.cpp",
                   "engine/syncworker.cpp",
                   "engine/enginebuffer.cpp",
                   "engine/enginebufferscale.cpp",
                   "engine/enginebufferscaledummy.cpp",
                   "engine/enginebufferscalelinear.cpp",
                   "engine/engineclipping.cpp",
                   "engine/enginefilterblock.cpp",
                   "engine/enginefilteriir.cpp",
                   "engine/enginefilter.cpp",
                   "engine/engineobject.cpp",
                   "engine/enginepregain.cpp",
                   "engine/enginechannel.cpp",
                   "engine/enginemaster.cpp",
                   "engine/enginedelay.cpp",
                   "engine/engineflanger.cpp",
                   "engine/enginevumeter.cpp",
                   "engine/enginevinylsoundemu.cpp",
                   "engine/enginesidechain.cpp",
                   "engine/enginefilterbutterworth8.cpp",
                   "engine/enginexfader.cpp",
                   "engine/enginemicrophone.cpp",
                   "engine/enginedeck.cpp",
                   "engine/enginepassthrough.cpp",

                   "engine/enginecontrol.cpp",
                   "engine/ratecontrol.cpp",
                   "engine/positionscratchcontroller.cpp",
                   "engine/loopingcontrol.cpp",
                   "engine/bpmcontrol.cpp",
                   "engine/cuecontrol.cpp",
                   "engine/quantizecontrol.cpp",
                   "engine/clockcontrol.cpp",
                   "engine/readaheadmanager.cpp",
                   "cachingreader.cpp",

                   "analyserrg.cpp",
                   "analyserqueue.cpp",
                   "analyserbpm.cpp",
                   "analyserwaveform.cpp",

                   "controllers/controller.cpp",
                   "controllers/controllerengine.cpp",
                   "controllers/controllerenumerator.cpp",
                   "controllers/controllerlearningeventfilter.cpp",
                   "controllers/controllermanager.cpp",
                   "controllers/controllerpresetfilehandler.cpp",
                   "controllers/controllerpresetinfo.cpp",
                   "controllers/midi/midicontroller.cpp",
                   "controllers/midi/midicontrollerpresetfilehandler.cpp",
                   "controllers/midi/midienumerator.cpp",
                   "controllers/midi/midioutputhandler.cpp",
                   "controllers/mixxxcontrol.cpp",
                   "controllers/qtscript-bytearray/bytearrayclass.cpp",
                   "controllers/qtscript-bytearray/bytearrayprototype.cpp",
                   "controllers/softtakeover.cpp",

                   "main.cpp",
                   "mixxx.cpp",
                   "errordialoghandler.cpp",
                   "upgrade.cpp",

                   "soundsource.cpp",

                   "sharedglcontext.cpp",
                   "widget/wwidget.cpp",
                   "widget/wlabel.cpp",
                   "widget/wtracktext.cpp",
                   "widget/wnumber.cpp",
                   "widget/wnumberpos.cpp",
                   "widget/wnumberrate.cpp",
                   "widget/wknob.cpp",
                   "widget/wdisplay.cpp",
                   "widget/wvumeter.cpp",
                   "widget/wpushbutton.cpp",
                   "widget/wslidercomposed.cpp",
                   "widget/wslider.cpp",
                   "widget/wstatuslight.cpp",
                   "widget/woverview.cpp",
                   "widget/wspinny.cpp",
                   "widget/wskincolor.cpp",
                   "widget/wabstractcontrol.cpp",
                   "widget/wsearchlineedit.cpp",
                   "widget/wcoverart.cpp",
                   "widget/wpixmapstore.cpp",
                   "widget/hexspinbox.cpp",
                   "widget/wtrackproperty.cpp",
                   "widget/wtime.cpp",

                   "mathstuff.cpp",

                   "rotary.cpp",
                   "widget/wtracktableview.cpp",
                   "widget/wtracktableviewheader.cpp",
                   "widget/wlibrarysidebar.cpp",
                   "widget/wlibrary.cpp",
                   "widget/wlibrarytableview.cpp",
                   "widget/wpreparelibrarytableview.cpp",
                   "widget/wpreparecratestableview.cpp",
                   "widget/wlibrarytextbrowser.cpp",
                   "library/preparecratedelegate.cpp",
                   "library/trackcollection.cpp",
                   "library/basesqltablemodel.cpp",
                   "library/basetrackcache.cpp",
                   "library/librarytablemodel.cpp",
                   "library/searchqueryparser.cpp",
                   "library/preparelibrarytablemodel.cpp",
                   "library/missingtablemodel.cpp",
                   "library/hiddentablemodel.cpp",
                   "library/proxytrackmodel.cpp",

                   "library/playlisttablemodel.cpp",
                   "library/libraryfeature.cpp",
                   "library/preparefeature.cpp",
                   "library/autodjfeature.cpp",
                   "library/mixxxlibraryfeature.cpp",
                   "library/baseplaylistfeature.cpp",
                   "library/playlistfeature.cpp",
                   "library/setlogfeature.cpp",

                   "library/browse/browsetablemodel.cpp",
                   "library/browse/browsethread.cpp",
                   "library/browse/browsefeature.cpp",
                   "library/browse/foldertreemodel.cpp",

                   "library/recording/recordingfeature.cpp",
                   "dlgrecording.cpp",
                   "recording/recordingmanager.cpp",

                   # External Library Features
                   "library/baseexternallibraryfeature.cpp",
                   "library/rhythmbox/rhythmboxfeature.cpp",
                   "library/rhythmbox/rhythmboxtrackmodel.cpp",
                   "library/rhythmbox/rhythmboxplaylistmodel.cpp",
                   "library/itunes/itunesfeature.cpp",
                   "library/itunes/itunestrackmodel.cpp",
                   "library/itunes/itunesplaylistmodel.cpp",
                   "library/traktor/traktorfeature.cpp",
                   "library/traktor/traktortablemodel.cpp",
                   "library/traktor/traktorplaylistmodel.cpp",


                   "library/cratefeature.cpp",
                   "library/sidebarmodel.cpp",
                   "library/libraryscanner.cpp",
                   "library/libraryscannerdlg.cpp",
                   "library/legacylibraryimporter.cpp",
                   "library/library.cpp",
                   "library/searchthread.cpp",

                   "library/dao/cratedao.cpp",
                   "library/cratetablemodel.cpp",
                   "library/dao/cuedao.cpp",
                   "library/dao/cue.cpp",
                   "library/dao/trackdao.cpp",
                   "library/dao/playlistdao.cpp",
                   "library/dao/libraryhashdao.cpp",
                   "library/dao/settingsdao.cpp",
                   "library/dao/analysisdao.cpp",

                   "library/librarycontrol.cpp",
                   "library/schemamanager.cpp",
                   "library/promotracksfeature.cpp",
                   "library/featuredartistswebview.cpp",
                   "library/bundledsongswebview.cpp",
                   "library/songdownloader.cpp",
                   "library/starrating.cpp",
                   "library/stardelegate.cpp",
                   "library/stareditor.cpp",
                   "audiotagger.cpp",

                   "library/treeitemmodel.cpp",
                   "library/treeitem.cpp",

                   "xmlparse.cpp",
                   "library/parser.cpp",
                   "library/parserpls.cpp",
                   "library/parserm3u.cpp",
                   "library/parsercsv.cpp",

                   "bpm/bpmscheme.cpp",

                   "soundsourceproxy.cpp",

                   "widget/wwaveformviewer.cpp",

                   "waveform/waveform.cpp",
                   "waveform/waveformfactory.cpp",
                   "waveform/waveformwidgetfactory.cpp",
                   "waveform/renderers/waveformwidgetrenderer.cpp",
                   "waveform/renderers/waveformrendererabstract.cpp",
                   "waveform/renderers/waveformrenderbackground.cpp",
                   "waveform/renderers/waveformrendermark.cpp",
                   "waveform/renderers/waveformrendermarkrange.cpp",
                   "waveform/renderers/waveformrenderbeat.cpp",
                   "waveform/renderers/waveformrendererendoftrack.cpp",
                   "waveform/renderers/waveformrendererpreroll.cpp",

                   "waveform/renderers/waveformrendererfilteredsignal.cpp",
                   "waveform/renderers/qtwaveformrendererfilteredsignal.cpp",
                   "waveform/renderers/qtwaveformrenderersimplesignal.cpp",
                   "waveform/renderers/glwaveformrendererfilteredsignal.cpp",
                   "waveform/renderers/glwaveformrenderersimplesignal.cpp",
                   "waveform/renderers/glslwaveformrenderersignal.cpp",

                   "waveform/renderers/waveformsignalcolors.cpp",

                   "waveform/renderers/waveformrenderersignalbase.cpp",
                   "waveform/renderers/waveformmark.cpp",
                   "waveform/renderers/waveformmarkset.cpp",
                   "waveform/renderers/waveformmarkrange.cpp",

                   "waveform/widgets/waveformwidgetabstract.cpp",
                   "waveform/widgets/emptywaveformwidget.cpp",
                   "waveform/widgets/softwarewaveformwidget.cpp",
                   "waveform/widgets/qtwaveformwidget.cpp",
                   "waveform/widgets/qtsimplewaveformwidget.cpp",
                   "waveform/widgets/glwaveformwidget.cpp",
                   "waveform/widgets/glsimplewaveformwidget.cpp",

                   "waveform/widgets/glslwaveformwidget.cpp",

                   "skin/imginvert.cpp",
                   "skin/imgloader.cpp",
                   "skin/imgcolor.cpp",
                   "skin/skinloader.cpp",
                   "skin/legacyskinparser.cpp",
                   "skin/colorschemeparser.cpp",
                   "skin/propertybinder.cpp",
                   "skin/tooltips.cpp",

                   "sampleutil.cpp",
                   "trackinfoobject.cpp",
                   "track/beatgrid.cpp",
                   "track/beatmap.cpp",
                   "track/beatfactory.cpp",
                   "track/beatutils.cpp",

                   "baseplayer.cpp",
                   "basetrackplayer.cpp",
                   "deck.cpp",
                   "sampler.cpp",
                   "playermanager.cpp",
                   "samplerbank.cpp",
                   "sounddevice.cpp",
                   "soundmanager.cpp",
                   "soundmanagerconfig.cpp",
                   "soundmanagerutil.cpp",
                   "dlgprefrecord.cpp",
                   "playerinfo.cpp",

                   "recording/enginerecord.cpp",
                   "recording/encoder.cpp",

                   "segmentation.cpp",
                   "tapfilter.cpp",

                   "util/pa_ringbuffer.c",
                   "util/sleepableqthread.cpp",

                   # Add the QRC file which compiles in some extra resources
                   # (prefs icons, etc.)
                   build.env.Qrc('#res/mixxx.qrc')
                   ]

        proto_args = {
            'PROTOCPROTOPATH': ['src'],
            'PROTOCPYTHONOUTDIR': '', # set to None to not generate python
            'PROTOCOUTDIR': build.build_dir,
            'PROTOCCPPOUTFLAGS': '',
            #'PROTOCCPPOUTFLAGS': "dllexport_decl=PROTOCONFIG_EXPORT:"
        }
        proto_sources = SCons.Glob('proto/*.proto')
        proto_objects = [build.env.Protoc([], proto_source, **proto_args)[0]
                        for proto_source in proto_sources]
        sources.extend(proto_objects)


        # Uic these guys (they're moc'd automatically after this) - Generates
        # the code for the QT UI forms
        build.env.Uic4('dlgpreferencesdlg.ui')
        build.env.Uic4('dlgprefsounddlg.ui')

        build.env.Uic4('controllers/dlgprefcontrollerdlg.ui')
        build.env.Uic4('controllers/dlgprefmappablecontrollerdlg.ui')
        build.env.Uic4('controllers/dlgcontrollerlearning.ui')
        build.env.Uic4('controllers/dlgprefnocontrollersdlg.ui')

        build.env.Uic4('dlgprefplaylistdlg.ui')
        build.env.Uic4('dlgprefcontrolsdlg.ui')
        build.env.Uic4('dlgprefeqdlg.ui')
        build.env.Uic4('dlgprefcrossfaderdlg.ui')
        build.env.Uic4('dlgprefbpmdlg.ui')
        build.env.Uic4('dlgprefreplaygaindlg.ui')
        build.env.Uic4('dlgprefbeatsdlg.ui')
        build.env.Uic4('dlgbpmschemedlg.ui')
        # build.env.Uic4('dlgbpmtapdlg.ui')
        build.env.Uic4('dlgprefvinyldlg.ui')
        build.env.Uic4('dlgprefnovinyldlg.ui')
        build.env.Uic4('dlgprefrecorddlg.ui')
        build.env.Uic4('dlgaboutdlg.ui')
        build.env.Uic4('dlgtrackinfo.ui')
        build.env.Uic4('dlgprepare.ui')
        build.env.Uic4('dlgautodj.ui')
        build.env.Uic4('dlgprefsounditem.ui')
        build.env.Uic4('dlgrecording.ui')

        if build.platform_is_windows:
            # Add Windows resource file with icons and such
            # force manifest file creation, apparently not necessary for all
            # people but necessary for this committers handicapped windows
            # installation -- bkgood
            if build.toolchain_is_msvs:
                build.env.Append(LINKFLAGS="/MANIFEST")
        elif build.platform_is_osx:
            build.env.Append(LINKFLAGS="-headerpad=ffff"); #Need extra room for code signing (App Store)
            build.env.Append(LINKFLAGS="-headerpad_max_install_names"); #Need extra room for code signing (App Store)

        return sources

    def configure(self, build, conf):
        # Evaluate this define. There are a lot of different things around the
        # codebase that use different defines. (AMD64, x86_64, x86, i386, i686,
        # EM64T). We need to unify them together.
        if not build.machine=='alpha':
            build.env.Append(CPPDEFINES=build.machine)

        if build.toolchain_is_gnu:
            # Default GNU Options
            # TODO(XXX) always generate debugging info?
            build.env.Append(CCFLAGS = '-pipe')
            build.env.Append(CCFLAGS = '-Wall')
            build.env.Append(CCFLAGS = '-Wextra')
            build.env.Append(CCFLAGS = '-g')

            # Check that g++ is present (yeah, SCONS is a bit dumb here)
            if os.system("which g++ > /dev/null"): #Checks for non-zero return code
                raise Exception("Did not find g++.")
        elif build.toolchain_is_msvs:
            # Validate the specified winlib directory exists
            mixxx_lib_path = SCons.ARGUMENTS.get('winlib', '..\\..\\..\\mixxx-win32lib-msvc100-release')
            if not os.path.exists(mixxx_lib_path):
                print mixxx_lib_path
                raise Exception("Winlib path does not exist! Please specify your winlib directory"
                                "path by running 'scons winlib=[path]'")
                Script.Exit(1)
            #mixxx_lib_path = '#/../../mixxx-win%slib-msvc100-release' % build.bitwidth

            # Set include and library paths to work with this
            build.env.Append(CPPPATH=mixxx_lib_path)
            build.env.Append(LIBPATH=mixxx_lib_path)

            #Ugh, MSVC-only hack :( see
            #http://www.qtforum.org/article/17883/problem-using-qstring-fromstdwstring.html
            build.env.Append(CXXFLAGS = '/Zc:wchar_t-')

            # Still needed?
            build.env.Append(CPPPATH=[
                    "$VCINSTALLDIR/include/atl",
                    "C:/Program Files/Microsoft Platform SDK/Include/atl"])

        if build.platform_is_windows:
            build.env.Append(CPPDEFINES='__WINDOWS__')
            build.env.Append(CPPDEFINES='_ATL_MIN_CRT') #Helps prevent duplicate symbols
            # Need this on Windows until we have UTF16 support in Mixxx
            build.env.Append(CPPDEFINES='UNICODE')
            build.env.Append(CPPDEFINES='WIN%s' % build.bitwidth) # WIN32 or WIN64
            # Tobias: Don't remove this line
            # I used the Windows API in foldertreemodel.cpp
            # to quickly test if a folder has subfolders
            build.env.Append(LIBS = 'shell32');


        elif build.platform_is_linux:
            build.env.Append(CPPDEFINES='__LINUX__')

            #Check for pkg-config >= 0.15.0
            if not conf.CheckForPKGConfig('0.15.0'):
                raise Exception('pkg-config >= 0.15.0 not found.')


        elif build.platform_is_osx:
            #Stuff you may have compiled by hand
            build.env.Append(LIBPATH = ['/usr/local/lib'])
            build.env.Append(CPPPATH = ['/usr/local/include'])

            #Non-standard libpaths for fink and certain (most?) darwin ports
            build.env.Append(LIBPATH = ['/sw/lib'])
            build.env.Append(CPPPATH = ['/sw/include'])

            #Non-standard libpaths for darwin ports
            build.env.Append(LIBPATH = ['/opt/local/lib'])
            build.env.Append(CPPPATH = ['/opt/local/include'])

        elif build.platform_is_bsd:
            build.env.Append(CPPDEFINES='__BSD__')
            build.env.Append(CPPPATH=['/usr/include',
                                      '/usr/local/include',
                                      '/usr/X11R6/include/'])
            build.env.Append(LIBPATH=['/usr/lib/',
                                      '/usr/local/lib',
                                      '/usr/X11R6/lib'])
            build.env.Append(LIBS='pthread')
            # why do we need to do this on OpenBSD and not on Linux?  if we
            # don't then CheckLib("vorbisfile") fails
            build.env.Append(LIBS=['ogg', 'vorbis'])

        # Define for things that would like to special case UNIX (Linux or BSD)
        if build.platform_is_bsd or build.platform_is_linux:
            build.env.Append(CPPDEFINES='__UNIX__')

        # Add the src/ directory to the include path
        build.env.Append(CPPPATH = ['.'])

        # Set up flags for config/track listing files
        if build.platform_is_linux or \
                build.platform_is_bsd:
            mixxx_files = [
                ('SETTINGS_PATH','.mixxx/'),
                ('BPMSCHEME_FILE','mixxxbpmscheme.xml'),
                ('SETTINGS_FILE', 'mixxx.cfg'),
                ('TRACK_FILE', 'mixxxtrack.xml')]
        elif build.platform_is_osx:
            mixxx_files = [
                ('SETTINGS_PATH','Library/Application Support/Mixxx/'),
                ('BPMSCHEME_FILE','mixxxbpmscheme.xml'),
                ('SETTINGS_FILE', 'mixxx.cfg'),
                ('TRACK_FILE', 'mixxxtrack.xml')]
        elif build.platform_is_windows:
            mixxx_files = [
                ('SETTINGS_PATH','Local Settings/Application Data/Mixxx/'),
                ('BPMSCHEME_FILE', 'mixxxbpmscheme.xml'),
                ('SETTINGS_FILE', 'mixxx.cfg'),
                ('TRACK_FILE', 'mixxxtrack.xml')]
        # Escape the filenames so they don't end up getting screwed up in the
        # shell.
        mixxx_files = [(k, r'\"%s\"' % v) for k,v in mixxx_files]
        build.env.Append(CPPDEFINES=mixxx_files)

        # Say where to find resources on Unix. TODO(XXX) replace this with a
        # RESOURCE_PATH that covers Win and OSX too:
        if build.platform_is_linux or build.platform_is_bsd:
            prefix = SCons.ARGUMENTS.get('prefix', '/usr/local')
            share_path = os.path.join(prefix, 'share/mixxx')
            build.env.Append(CPPDEFINES=('UNIX_SHARE_PATH', r'\"%s\"' % share_path))
            lib_path = os.path.join(prefix, 'lib/mixxx')
            build.env.Append(CPPDEFINES=('UNIX_LIB_PATH', r'\"%s\"' % lib_path))

    def depends(self, build):
        return [SoundTouch, ReplayGain, PortAudio, PortMIDI, Qt,
                FidLib, SndFile, FLAC, OggVorbis, OpenGL, TagLib, ProtoBuf]

    def post_dependency_check_configure(self, build, conf):
        """Sets up additional things in the Environment that must happen
        after the Configure checks run."""
        if build.platform_is_windows:
            if build.toolchain_is_msvs:
                build.env.Append(LINKFLAGS = ['/nodefaultlib:LIBCMT.lib',
                                              '/nodefaultlib:LIBCMTd.lib',
                                              '/entry:mainCRTStartup'])
                # Makes the program not launch a shell first
                build.env.Append(LINKFLAGS = '/subsystem:windows')
                build.env.Append(LINKFLAGS = '/manifest') #Force MSVS to generate a manifest (MSVC2010)
            elif build.toolchain_is_gnu:
                # Makes the program not launch a shell first
                build.env.Append(LINKFLAGS = '--subsystem,windows')
                build.env.Append(LINKFLAGS = '-mwindows')
