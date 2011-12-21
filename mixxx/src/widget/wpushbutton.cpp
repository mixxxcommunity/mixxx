/***************************************************************************
                          wpushbutton.cpp  -  description
                             -------------------
    begin                : Fri Jun 21 2002
    copyright            : (C) 2002 by Tue & Ken Haste Andersen
    email                : haste@diku.dk
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "wpushbutton.h"
#include "wpixmapstore.h"
#include "controlobject.h"
#include "controlpushbutton.h"
//Added by qt3to4:
#include <QPixmap>
#include <QtDebug>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QApplication>

WPushButton::WPushButton(QWidget * parent) : WWidget(parent)
{
    m_pPixmaps = 0;
    m_pPixmapBack = 0;
    setStates(0);
    //setBackgroundMode(Qt::NoBackground); //obsolete? removal doesn't seem to change anything on the GUI --kousu 2009/03
}

WPushButton::~WPushButton()
{
    for (int i = 0; i < 2*m_iNoStates; i++) {
        WPixmapStore::deletePixmap(m_pPixmaps[i]);
    }

    delete [] m_pPixmaps;

    WPixmapStore::deletePixmap(m_pPixmapBack);
}

void WPushButton::setup(QDomNode node)
{
    // Number of states
    int iNumStates = selectNodeInt(node, "NumberStates");
    setStates(iNumStates);

    m_powerWindowStyle = selectNodeQString(node, "PowerWindowStyle").contains("true", Qt::CaseInsensitive);

    // Set background pixmap if available
    if (!selectNode(node, "BackPath").isNull())
        setPixmapBackground(getPath(selectNodeQString(node, "BackPath")));

    // Load pixmaps for associated states
    QDomNode state = selectNode(node, "State");
    while (!state.isNull())
    {
        if (state.isElement() && state.nodeName() == "State")
        {
            setPixmap(selectNodeInt(state, "Number"), true, getPath(selectNodeQString(state, "Pressed")));
            setPixmap(selectNodeInt(state, "Number"), false, getPath(selectNodeQString(state, "Unpressed")));
        }
        state = state.nextSibling();
    }

    if (selectNodeQString(node, "LeftClickIsPushButton").contains("true", Qt::CaseInsensitive)) {
        m_bLeftClickForcePush = true;
    } else {
        m_bLeftClickForcePush = false;
    }

    if (selectNodeQString(node, "RightClickIsPushButton").contains("true", Qt::CaseInsensitive)) {
        m_bRightClickForcePush = true;
    } else {
        m_bRightClickForcePush = false;
    }

    // BJW: Removed toggle button detection so that buttons that are hardcoded as toggle in the source
    // don't isLeftButtonget overridden if a skin fails to set them to 2-state. Buttons still
    // default to non-toggle otherwise.
}

void WPushButton::setStates(int iStates)
{
    m_iNoStates = iStates;
    m_fValue = 0.;
    m_bPressed = false;

    // If pixmap array is already allocated, delete it
    delete [] m_pPixmaps;
    m_pPixmaps = NULL;

    if (iStates>0)
    {
        m_pPixmaps = new QPixmap*[2*m_iNoStates];
        for (int i=0; i<2*m_iNoStates; ++i)
            m_pPixmaps[i] = 0;
    }
}

void WPushButton::setPixmap(int iState, bool bPressed, const QString &filename)
{
    int pixIdx = (iState*2)+bPressed;
    m_pPixmaps[pixIdx] = WPixmapStore::getPixmap(filename);
    if (!m_pPixmaps[pixIdx])
        qDebug() << "WPushButton: Error loading pixmap:" << filename;

    // Set size of widget equal to pixmap size
    setFixedSize(m_pPixmaps[pixIdx]->size());
}

void WPushButton::setPixmapBackground(const QString &filename)
{
    // Load background pixmap
    m_pPixmapBack = WPixmapStore::getPixmap(filename);
    if (!m_pPixmapBack)
        qDebug() << "WPushButton: Error loading background pixmap:" << filename;
}

void WPushButton::setValue(double v)
{
    m_fValue = v;

    if (m_iNoStates==1)
    {
        if (m_fValue==1.)
            m_bPressed = true;
        else
            m_bPressed = false;
    }

    update();
}

void WPushButton::paintEvent(QPaintEvent *)
{
    if (m_iNoStates>0)
    {
        int idx = (((int)m_fValue%m_iNoStates)*2)+m_bPressed;
        if (m_pPixmaps[idx])
        {
            QPainter p(this);
            if(m_pPixmapBack) p.drawPixmap(0, 0, *m_pPixmapBack);
            p.drawPixmap(0, 0, *m_pPixmaps[idx]);
        }
    }
}

void WPushButton::mousePressEvent(QMouseEvent * e)
{
    Qt::MouseButton klickButton = e->button();

    if (m_powerWindowStyle && m_iNoStates == 2) {
        if (klickButton == Qt::LeftButton) {
            if (m_fValue == 0.0f) {
                m_clickTimer.setSingleShot(true);
                m_clickTimer.start(300);
            }
            m_fValue = 1.0f;
            m_bPressed = true;
            emit(valueChangedLeftDown(1.0f));
            update();
        }
        return;
    }

    if (klickButton == Qt::RightButton) {
        // This is the secondary button function, it does not change m_fValue
        // due the leak of visual feedback we do not allow a toggle function
        if (m_bRightClickForcePush) {
            m_bPressed = true;
            emit(valueChangedRightDown(1.0f));
            update();
        }
        return;
    }

    if (klickButton == Qt::LeftButton) {
        double emitValue;
        if (m_bLeftClickForcePush) {
            // This may a button with different functions on each mouse button
            // m_fValue is changed by a separate feedback connection
            emitValue = 1.0f;
        } else if (m_iNoStates == 1) {
            // This is a Pushbutton
            m_fValue = emitValue = 1.0f;
        } else {
            // Toggle thru the states
            m_fValue = emitValue = (int)(m_fValue+1.)%m_iNoStates;
        }
        m_bPressed = true;
        emit(valueChangedLeftDown(emitValue));
        update();
    }
}

void WPushButton::mouseReleaseEvent(QMouseEvent * e)
{
    Qt::MouseButton klickButton = e->button();

    if (m_powerWindowStyle && m_iNoStates == 2) {
        if (klickButton == Qt::LeftButton) {
            if (    !m_clickTimer.isActive()
                 && !(QApplication::mouseButtons() & Qt::RightButton)
                 && m_bPressed) {
                // Release Button after Timer, but not if right button is clicked
                m_fValue = 0.0f;
                emit(valueChangedLeftUp(0.0f));
            }
            m_bPressed = false;
        }
        else if (klickButton == Qt::RightButton) {
            m_bPressed = false;
        }
        update();
        return;
    }

    if (klickButton == Qt::RightButton) {
        // This is the secondary klickButton function, it does not change m_fValue
        // due the leak of visual feedback we do not allow a toggle function
        if (m_bRightClickForcePush) {
            m_bPressed = false;
            emit(valueChangedRightDown(0.0f));
            update();
        }
        return;
    }

    if (klickButton == Qt::LeftButton) {
        double emitValue;
        if (m_bLeftClickForcePush) {
            // This may a klickButton with different functions on each mouse button
            // m_fValue is changed by a separate feedback connection
            emitValue = 0.0f;
        } else if (m_iNoStates == 1) {
            // This is a Pushbutton
            m_fValue = emitValue = 0.0f;
        } else {
            // Nothing special happens when releasing a toggle button
            emitValue = m_fValue;
        }
        m_bPressed = false;
        emit(valueChangedLeftDown(emitValue));
        update();
    }
}
