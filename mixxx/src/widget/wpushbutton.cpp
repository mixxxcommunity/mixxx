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

    m_bLeftClickForcePush = false;
    if (selectNodeQString(node, "LeftClickIsPushButton").contains("true", Qt::CaseInsensitive))
        m_bLeftClickForcePush = true;

    m_bRightClickForcePush = false;
    if (selectNodeQString(node, "RightClickIsPushButton").contains("true", Qt::CaseInsensitive))
        m_bRightClickForcePush = true;

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
    bool leftClick = e->button() == Qt::LeftButton;
    bool rightClick = e->button() == Qt::RightButton;

    // The value to emit.
    double emitValue = m_fValue;

    if (m_powerWindowStyle && m_iNoStates == 2) {
        if (m_fValue == 0.0f) {
            m_clickTimer.setSingleShot(true);
            m_clickTimer.start(300);
        }
        m_fValue = emitValue = 1.0f;
    } else if (m_iNoStates == 1) {
        // Calculate new state if it is a one state button
        m_fValue = emitValue = (m_fValue == 0.0f) ? 1.0f : 0.0f;
    } else if (leftClick) {
        // Update state on press if it is a n-state button and not a pushbutton
        if (m_bLeftClickForcePush) {
            emitValue = 1.0f;
        } else {
            m_fValue = emitValue = (int)(m_fValue+1.)%m_iNoStates;
        }
    }

    // Do not allow right-clicks to change the state of the button. This is how
    // Mixxx <1.8.0 worked so keep it that way. For a multi-state button, really
    // only one click type (left/right) should be able to change the state. One
    // problem with this is that you can get the button out of sync with its
    // underlying control. For example the PFL buttons on Jus's skins could get
    // out of sync with the button state. rryan 9/2010

    // else if (rightClick) {
    //     if (m_bRightClickForcePush) {
    //         emitValue = 1.0f;
    //     } else {
    //         m_fValue = emitValue = (int)(m_fValue+1.)%m_iNoStates;
    //     }
    // }

    m_bPressed = true;

    if (leftClick) {
        emit(valueChangedLeftDown(emitValue));
    } else if (rightClick) {
        emit(valueChangedRightDown(emitValue));
    }

    update();
}

void WPushButton::mouseReleaseEvent(QMouseEvent * e)
{
    bool leftClick = e->button() == Qt::LeftButton;
    bool rightClick = e->button() == Qt::RightButton;

    // The value to emit
    double emitValue = m_fValue;

    if (m_powerWindowStyle && m_iNoStates == 2) {
        if (    !m_clickTimer.isActive()
             && !(QApplication::mouseButtons() & Qt::RightButton)
             && m_bPressed) {
            // Release Button after Timer, but not if right button is clicked
            m_fValue = emitValue = 0.0f;
        }
    } else if (m_iNoStates==1) // && e->button()==Qt::LeftButton)
    {   // Update state if it is a one state button.
        m_fValue = emitValue = (m_fValue == 0.0f) ? 1.0f : 0.0f;
    } else if ((leftClick && m_bLeftClickForcePush) || (rightClick && m_bRightClickForcePush)) {
        emitValue = 0.0f;
    }

    m_bPressed = false;

    if (leftClick) {
        emit(valueChangedLeftUp(emitValue));
    } else if (rightClick) {
        emit(valueChangedRightUp(emitValue));
    }

    update();
}
