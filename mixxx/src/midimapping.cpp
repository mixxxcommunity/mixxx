/***************************************************************************
                             midimapping.cpp - "Wow, I wrote more new code!"
                           MIDI Mapping Class
                           -------------------
    begin                : Sat Jan 17 2009
    copyright            : (C) 2009 Sean M. Pappalardo
    email                : pegasus@c64.org

***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include <qapplication.h>
#include "widget/wwidget.h"    // FIXME: This should be xmlparse.h
#include "mixxxcontrol.h"
#include "midimessage.h"
#include "midiinputmappingtablemodel.h"
#include "midimapping.h"
#include "midiledhandler.h"
#include "configobject.h"

#define REQUIRED_MAPPING_FILE "midi-mappings-scripts.js"
#define XML_SCHEMA_VERSION "1"

static QString toHex(QString numberStr) {
    return "0x" + QString("0" + QString::number(numberStr.toUShort(), 16).toUpper()).right(2);
}

MidiMapping::MidiMapping(MidiObject& midi_object) : QObject(), m_rMidiObject(midi_object) {
#ifdef __MIDISCRIPT__
    m_pScriptEngine = midi_object.getMidiScriptEngine();
#endif
    m_pMidiInputMappingTableModel = new MidiInputMappingTableModel(this);
}

MidiMapping::~MidiMapping() {
}

/* ============================== MIDI Input Mapping Modifiers */

/*
 * Return the total number of current input mappings.
 */ 
int MidiMapping::numInputMidiMessages() {
    return m_inputMapping.size();
}

/*
 * Return true if the index corresponds to an input mapping key.
 */ 
bool MidiMapping::isInputIndexValid(int index) {
    if(index < 0 || index >= numInputMidiMessages()) {
        return false;
    }
    return true;
}

bool MidiMapping::isMidiMessageMapped(MidiMessage command) {
    return m_inputMapping.contains(command);
}

/*
 * Lookup the MidiMessage corresponding to a given index.
 */
MidiMessage MidiMapping::getInputMidiMessage(int index) {
    if(!isInputIndexValid(index)) {
        return MidiMessage(); // do something bad.
    }
    return m_inputMapping.keys().at(index);
}

/*
 * Lookup the MixxxControl mapped to a given MidiMessage (by index).
 */
MixxxControl MidiMapping::getInputMixxxControl(int index) {
    if(!isInputIndexValid(index)) {
        return MixxxControl(); // do something bad.
    }
    MidiMessage key = m_inputMapping.keys().at(index);
    return m_inputMapping.value(key);
}

/*
 * Lookup the MixxxControl mapped to a given MidiMessage.
 */
MixxxControl MidiMapping::getInputMixxxControl(MidiMessage command) {
    if(!m_inputMapping.contains(command)) {
        return MixxxControl(); // do something bad.
    }
    return m_inputMapping.value(command);
}

/*
 * Set a MidiMessage -> MixxxControl mapping, replacing an existing one
 * if necessary.
 */
void MidiMapping::setInputMidiMapping(MidiMessage command, MixxxControl control) {
    // If the command is already in the mapping, it will be replaced
    m_inputMapping.insert(command,control);
    emit(inputMappingChanged());
}

/*
 * Clear a specific mapping for a MidiMessage by index.
 */
void MidiMapping::clearInputMidiMapping(int index) {
    if(!isInputIndexValid(index)) {
        return;
    }
    MidiMessage key = m_inputMapping.keys().at(index);
    m_inputMapping.remove(key);
    emit(inputMappingChanged());
    //emit(inputMappingChanged(index, numInputMidiMessages()-1));
}

/*
 * Clear a specific mapping for a MidiMessage.
 */
void MidiMapping::clearInputMidiMapping(MidiMessage command) {
    int changed = m_inputMapping.remove(command);
    if(changed > 0)
        emit(inputMappingChanged());
}

/*
 * Clears a range of input mappings. (This really only exists so that
 * a caller can atomically remove a range of rows)
 *
 */
void MidiMapping::clearInputMidiMapping(int index, int count) {
    QList<MidiMessage> keys = m_inputMapping.keys();
    int changed = 0;
    for(int i=index; i < index+count; i++) {
        MidiMessage command = keys.at(i);
        changed += m_inputMapping.remove(command);
    }
    if(changed > 0)
        emit(inputMappingChanged());
}

#ifdef __MIDISCRIPT__
/* -------- ------------------------------------------------------
   Purpose: Adds an entry to the list of script file names
            & associated list of function prefixes
   Input:   QString file name, QString function prefix
   Output:  -
   -------- ------------------------------------------------------ */
void MidiMapping::addScriptFile(QString filename, QString functionprefix) {
   m_pScriptFileNames.append(filename);
   m_pScriptFunctionPrefixes.append(functionprefix);
}
#endif

/* loadInitialPreset()
 * Loads a set of MIDI bindings from either the default file or one specified on the command line.
 */
void MidiMapping::loadInitialPreset() {
    // Try to read in the current XML bindings file, one from the command line, or create one if nothing is available
    QStringList commandLineArgs = QApplication::arguments();
    int loadXML = commandLineArgs.indexOf("--loadXMLfile");

    if (loadXML!=-1) {
        qDebug() << "MidiMapping: Loading custom MIDI mapping file:" << commandLineArgs.at(loadXML+1);
        loadPreset(commandLineArgs.at(loadXML+1));
    }
    else loadPreset(BINDINGS_PATH);
    applyPreset();
}

/* loadPreset(QString)
 * Overloaded function for convenience
 */
void MidiMapping::loadPreset(QString path) {
    qDebug() << "MidiMapping: Loading MIDI XML from" << path;
    loadPreset(WWidget::openXMLFile(path, "controller"));
}

/* loadPreset(QDomElement)
 * Loads a set of MIDI bindings from a QDomElement structure.
 */
void MidiMapping::loadPreset(QDomElement root) {
    //qDebug() << QString("MidiMapping: loadPreset() called in thread ID=%1").arg(this->thread()->currentThreadId(),0,16);

    if (root.isNull()) return;

    // For each controller in the DOM
    m_Bindings = root;
    QDomElement controller = m_Bindings.firstChildElement("controller");
    while (!controller.isNull()) {
        // For each controller
        // Get deviceid
        QString device = controller.attribute("id","");
        qDebug() << device << " settings found" << endl;

#ifdef __MIDISCRIPT__

        // Get a list of MIDI script files to load
        QDomElement scriptFile = controller.firstChildElement("scriptfiles").firstChildElement("file");

        // Default currently required file
        addScriptFile(REQUIRED_MAPPING_FILE,"");

        // Look for additional ones
        while (!scriptFile.isNull()) {

            QString functionPrefix = scriptFile.attribute("functionprefix","");
            QString filename = scriptFile.attribute("filename","");
            addScriptFile(filename, functionPrefix);

            scriptFile = scriptFile.nextSiblingElement("file");
        }

        // Load Script files
        ConfigObject<ConfigValue> *m_pConfig = new ConfigObject<ConfigValue>(QDir::homePath().append("/").append(SETTINGS_FILE));

        qDebug() << "MidiMapping: Loading & evaluating all MIDI script code";

        QListIterator<QString> it(m_pScriptFileNames);
        while (it.hasNext()) {
            QString curScriptFileName = it.next();
            m_pScriptEngine->evaluate(m_pConfig->getConfigPath().append("midi/").append(curScriptFileName));

            if(m_pScriptEngine->hasErrors(curScriptFileName)) {
                qDebug() << "Errors occured while loading " << curScriptFileName;
            }
            
        }

        // Call each script's init function if it exists
        QListIterator<QString> prefixIt(m_pScriptFunctionPrefixes);
        while (prefixIt.hasNext()) {
            QString initName = prefixIt.next();
            if (initName!="") {
                initName.append(".init");
                qDebug() << "MidiMapping: Executing" << initName;
                if (!m_pScriptEngine->execute(initName))
                    qWarning() << "MidiMapping: No" << initName << "function in script";
            }
        }

        QStringList scriptFunctions = m_pScriptEngine->getScriptFunctions();

#endif

        QDomElement control = controller.firstChildElement("controls").firstChildElement("control");

        //Iterate through each <control> block in the XML
        while (!control.isNull()) {

            //Unserialize these objects from the XML
            MidiMessage midiMessage(control);
            MixxxControl mixxxControl(control);
#ifdef __MIDISCRIPT__
            // Verify script functions are loaded
            if (mixxxControl.getMidiOption()==MIDI_OPT_SCRIPT && 
                    scriptFunctions.indexOf(mixxxControl.getControlObjectValue())==-1) {
                // Need some way to signal to the dialog that this control will not be bound instead of just dying
                qCritical() << "Error: Function" << mixxxControl.getControlObjectValue() << "was not found in loaded scripts.";
            } else {
#endif
            //Add to the input mapping.
            m_inputMapping.insert(midiMessage, mixxxControl);
#ifdef __MIDISCRIPT__
            }
#endif
            control = control.nextSiblingElement("control");
        }

        qDebug() << "MidiMapping: Input parsed!";

        QDomElement output = controller.firstChildElement("outputs").firstChildElement("output");

        //Iterate through each <control> block in the XML
        while (!output.isNull()) {
            //Unserialize these objects from the XML
            MidiMessage midiMessage(output);
            MixxxControl mixxxControl(output);

            //Add to the output mapping.
            m_outputMapping.insert(mixxxControl, midiMessage);
            
            output = output.nextSiblingElement("output");
        }

        qDebug() << "MidiMapping: Output parsed!";        
        controller = controller.nextSiblingElement("controller");
    }
    
}   // END loadPreset(QDomElement)

/* -------- ------------------------------------------------------
   Purpose: Returns a reference to the QList of parameters for
            dlgprefmidibinding's addRow function.
   Input:   -
   Output:  Reference to QList of QHashes, each hash containing
            a parameter by name
   -------- ------------------------------------------------------ */
MidiInputMapping* MidiMapping::getInputMapping() {

//     qDebug() << QString("MidiMapping: getRowParams() called in thread ID=%1").arg(this->thread()->currentThreadId(),0,16);
    qDebug() << "MidiMapping: Getting rowParams";

    return &m_inputMapping;
}


/* savePreset(QString)
 * Given a path, saves the current table of bindings to an XML file.
 */
void MidiMapping::savePreset(QString path) {

    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    QTextStream outputstream(&output);
    // Construct the DOM from the table
   buildDomElement();
    // Save the DOM to the XML file
    m_Bindings.save(outputstream, 4);
    output.close();
}

/* applyPreset()
 * Load the current bindings set into the MIDI handler, and the outputs info into
 * the LED handler.
 */
void MidiMapping::applyPreset() {

    MidiLedHandler::destroyHandlers();

    QDomElement controller = m_Bindings.firstChildElement("controller");
    // For each device
    ConfigObject<ConfigValueMidi> * MidiConfig;
    while (!controller.isNull()) {
        // Device Outputs - LEDs
        QString deviceId = controller.attribute("id","");

        qDebug() << "MidiMapping: Processing MIDI Control Bindings for" << deviceId;
        //MidiConfig = new ConfigObject<ConfigValueMidi>(controller.namedItem("controls"));

        qDebug() << "MidiMapping: Processing MIDI Output Bindings for" << deviceId;
        MidiLedHandler::createHandlers(controller.namedItem("outputs").firstChild(), m_rMidiObject, deviceId);

        // Next device
        controller = controller.nextSiblingElement("controller");
    }
}

/* clearPreset()
 * Creates a blank bindings preset.
 */
void MidiMapping::clearPreset() {
    // Create a new blank DomNode
    QString blank = "<MixxxMIDIPreset schemaVersion=\"" + QString(XML_SCHEMA_VERSION) + "\" mixxxVersion=\"" + QString(VERSION) + "+\">\n"
    "</MixxxMIDIPreset>\n";
    QDomDocument doc("Bindings");
    doc.setContent(blank);
    m_Bindings = doc.documentElement();
}

/* buildDomElement()
 * Updates the DOM with what is currently in the table
 */
 void MidiMapping::buildDomElement() {
     clearPreset(); // Create blank document

     const QString wtfbbqdevicename = "foobar";
#ifdef __MIDISCRIPT__
      //This sucks, put this code inside MidiScriptEngine instead of here,
      // and just ask MidiScriptEngine to spit it out for us.
     qDebug() << "MidiMapping: Writing script block!";
     for (int i = 0; i < m_pScriptFileNames.count(); i++) {
         qDebug() << "MidiMapping: writing script block for" << m_pScriptFileNames[i];
          QString filename = m_pScriptFileNames[i];
          if (filename != REQUIRED_MAPPING_FILE) { //Don't need to write anything for the required mapping file.
              QString functionPrefix = m_pScriptFunctionPrefixes[i];
              //and now for the worst XML code since... WWidget...
              QDomDocument sucksBalls;
              QDomElement scriptFile = sucksBalls.createElement("file");
              scriptFile.setAttribute("filename", filename);
              scriptFile.setAttribute("functionprefix", functionPrefix);
              
              //Add the XML dom element to the right spot in the XML document.
              addMidiScriptInfo(scriptFile, wtfbbqdevicename);
          }
     }
#endif

    //Iterate over all of the command/control pairs in the input mapping
     QMapIterator<MidiMessage, MixxxControl> it(m_inputMapping);
     while (it.hasNext()) {
         it.next();
         QDomElement controlNode;
         QDomDocument nodeMaker;

         //Create <control> block
         controlNode = nodeMaker.createElement("control");

         //Save the MidiMessage and MixxxControl objects as XML
         it.key().serializeToXML(controlNode);
         it.value().serializeToXML(controlNode);

          //Add the control node we just created to the XML document in the proper spot
         addControl(controlNode, wtfbbqdevicename); //FIXME: Remove this device shit until we have multiple device support.
     }
     
     //Iterate over all of the control/command pairs in the OUTPUT mapping
     QMapIterator<MixxxControl, MidiMessage> outIt(m_outputMapping);
     while (outIt.hasNext()) {
         outIt.next();
         QDomElement outputNode;
         QDomDocument nodeMaker;

         //Create <output> block
         outputNode = nodeMaker.createElement("output");

         //Save the MidiMessage and MixxxControl objects as XML
         outIt.key().serializeToXML(outputNode, true);
         outIt.value().serializeToXML(outputNode, true);

          //Add the control node we just created to the XML document in the proper spot
         addOutput(outputNode, wtfbbqdevicename); //FIXME: Remove this device shit until we have multiple device support.
     }
 }

/* -------- ------------------------------------------------------
   Purpose: Adds an input MIDI mapping block to the XML.
   Input:   QDomElement control, QString device
   Output:  -
   -------- ------------------------------------------------------ */
void MidiMapping::addControl(QDomElement &control, QString device) {
    QDomDocument nodeMaker;
    //Add control to correct device tag - find the correct tag
    QDomElement controller = m_Bindings.firstChildElement("controller");
    while (controller.attribute("id","") != device && !controller.isNull()) {
        controller = controller.nextSiblingElement("controller");
    }
    if (controller.isNull()) {
        // No tag was found - create it
        controller = nodeMaker.createElement("controller");
        controller.setAttribute("id", device);
        m_Bindings.appendChild(controller);
    }
    // Check for controls tag
    QDomElement controls = controller.firstChildElement("controls");
    if (controls.isNull()) {
        controls = nodeMaker.createElement("controls");
        controller.appendChild(controls);
    }
    controls.appendChild(control);
}

/* -------- ------------------------------------------------------
   Purpose: This code sucks, temporary hack
   Input:   QDomElement control, QString device
   Output:  -
   -------- ------------------------------------------------------ */
void MidiMapping::addMidiScriptInfo(QDomElement &scriptFile, QString device) {
    QDomDocument nodeMaker;
    //Add control to correct device tag - find the correct tag
    QDomElement controller = m_Bindings.firstChildElement("controller");
    while (controller.attribute("id","") != device && !controller.isNull()) {
        controller = controller.nextSiblingElement("controller");
    }
    if (controller.isNull()) {
        // No tag was found - create it
        controller = nodeMaker.createElement("controller");
        controller.setAttribute("id", device);
        m_Bindings.appendChild(controller);
    }
    // Check for controls tag
    QDomElement scriptfiles = controller.firstChildElement("scriptfiles");
    if (scriptfiles.isNull()) {
        scriptfiles = nodeMaker.createElement("scriptfiles");
        controller.appendChild(scriptfiles);
    }
    scriptfiles.appendChild(scriptFile);
}

/* -------- ------------------------------------------------------
   Purpose: Adds an output (LED, etc.) MIDI mapping block to the XML.
   Input:   QDomElement output, QString device
   Output:  -
   -------- ------------------------------------------------------ */
void MidiMapping::addOutput(QDomElement &output, QString device) {
    QDomDocument nodeMaker;
    // Find the controller to attach the XML to...
    QDomElement controller = m_Bindings.firstChildElement("controller");
    while (controller.attribute("id","") != device && !controller.isNull()) {
        controller = controller.nextSiblingElement("controller");
    }
    if (controller.isNull()) {
        // No tag was found - create it
        controller = nodeMaker.createElement("controller");
        controller.setAttribute("id", device);
        m_Bindings.appendChild(controller);
    }

    // Find the outputs block
    QDomElement outputs = controller.firstChildElement("outputs");
    if (outputs.isNull()) {
        outputs = nodeMaker.createElement("outputs");
        controller.appendChild(outputs);
    }
    // attach the output to the outputs block
    outputs.appendChild(output);
}

bool MidiMapping::addInputControl(MidiType midiType, int midiNo, int midiChannel,
                                  QString controlObjectGroup, QString controlObjectKey,
                                  MidiOption midiOption)
{
    //TODO: Check if mapping already exists for this MidiMessage.

    //Add to the input mapping.
    m_inputMapping.insert(MidiMessage(midiType, midiNo, midiChannel),
                          MixxxControl(controlObjectGroup, controlObjectKey,
                                       midiOption));
    return true; //XXX is this right? should this be returning whether the add happened successfully?
}

void MidiMapping::removeInputMapping(MidiType midiType, int midiNo, int midiChannel)
{
    m_inputMapping.remove(MidiMessage(midiType, midiNo, midiChannel));
}

MidiInputMappingTableModel* MidiMapping::getMidiInputMappingTableModel()
{
    return m_pMidiInputMappingTableModel;
}

//Used by MidiObject to query what control matches a given MIDI command.
/*MixxxControl* MidiMapping::getInputMixxxControl(MidiMessage command)
{
    if (!m_inputMapping.contains(command)) {
        qDebug() << "Warning: unbound MIDI command";
        qDebug() << "Midi Type:" << command.getMidiType();
        qDebug() << "Midi No:" << command.getMidiNo();
        qDebug() << "Midi Channel:" << command.getMidiChannel();
        return NULL;
    }

    MixxxControl* control = &(m_inputMapping[command]);
    return control;
    }*/

// BJW: Note: _prevmidivalue is not the previous MIDI value. It's the
// current controller value, scaled to 0-127 but only in the case of pots.
// (See Control*::GetMidiValue())
double MidiMapping::ComputeValue(MidiOption midioption, double _prevmidivalue, double _newmidivalue)
{
    double tempval = 0.;
    double diff = 0.;

    // qDebug() << "ComputeValue: option " << midioption << ", MIDI value " << _newmidivalue << ", current control value " << _prevmidivalue;
    if (midioption == MIDI_OPT_NORMAL) {
        return _newmidivalue;
    }
    else if (midioption == MIDI_OPT_INVERT)
    {
        return 127. - _newmidivalue;
    }
    else if (midioption == MIDI_OPT_ROT64 || midioption == MIDI_OPT_ROT64_INV)
    {
        tempval = _prevmidivalue;
        diff = _newmidivalue - 64.;
        if (diff == -1 || diff == 1)
            diff /= 16;
        else
            diff += (diff > 0 ? -1 : +1);
        if (midioption == MIDI_OPT_ROT64)
            tempval += diff;
        else
            tempval -= diff;
        return (tempval < 0. ? 0. : (tempval > 127. ? 127.0 : tempval));
    }
    else if (midioption == MIDI_OPT_ROT64_FAST)
    {
        tempval = _prevmidivalue;
        diff = _newmidivalue - 64.;
        diff *= 1.5;
        tempval += diff;
        return (tempval < 0. ? 0. : (tempval > 127. ? 127.0 : tempval));
    }
    else if (midioption == MIDI_OPT_DIFF)
    {
        //Interpret 7-bit signed value using two's compliment.
        if (_newmidivalue >= 64.)
            _newmidivalue = _newmidivalue - 128.;
        //Apply sensitivity to signed value. FIXME
       // if(sensitivity > 0)
        //    _newmidivalue = _newmidivalue * ((double)sensitivity / 50.);
        //Apply new value to current value.
        _newmidivalue = _prevmidivalue + _newmidivalue;
    }
    else if (midioption == MIDI_OPT_SELECTKNOB)
    {
        //Interpret 7-bit signed value using two's compliment.
        if (_newmidivalue >= 64.)
            _newmidivalue = _newmidivalue - 128.;
        //Apply sensitivity to signed value. FIXME
        //if(sensitivity > 0)
        //    _newmidivalue = _newmidivalue * ((double)sensitivity / 50.);
        //Since this is a selection knob, we do not want to inherit previous values.
        return _newmidivalue;
    }
    else if (midioption == MIDI_OPT_BUTTON)
    {
        if (_newmidivalue != 0.) {
            _newmidivalue = !_prevmidivalue;
        } else {
            _newmidivalue = _prevmidivalue;
        }
    }
    else if (midioption == MIDI_OPT_SWITCH)
    {
        _newmidivalue = (_newmidivalue != 0);
    }
    else if (midioption == MIDI_OPT_SPREAD64)
    {
       qDebug() << "MIDI_OPT_SPREAD64";
        // BJW: Spread64: Distance away from centre point (aka "relative CC")
        // Uses a similar non-linear scaling formula as ControlTTRotary::getValueFromWidget()
        // but with added sensitivity adjustment. This formula is still experimental.
        /*
         //FIXME
        double distance = _newmidivalue - 64.;
        _newmidivalue = distance * distance * sensitivity / 50000.;
        if (distance < 0.)
            _newmidivalue = -_newmidivalue;
        */

        // qDebug() << "Spread64: in " << distance << "  out " << _newmidivalue;
    }
    else if (midioption == MIDI_OPT_HERC_JOG)
    {
        if (_newmidivalue > 64.) { _newmidivalue -= 128.; }
        _newmidivalue += _prevmidivalue;
        //if (_prevmidivalue != 0.0) { qDebug() << "AAAAAAAAAAAA" << _prevmidivalue; }
    }
    else
    {
        qWarning("Unknown MIDI option %d", midioption);
    }

    return _newmidivalue;
}
