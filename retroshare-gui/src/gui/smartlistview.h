/***************************************************************************
 * Copyright (C) 2015-2017 by Savoir-faire Linux                           *
 * Author: Jäger Nicolas <nicolas.jager@savoirfairelinux.com>              *
 * Author: Andreas Traczyk <andreas.traczyk@savoirfairelinux.com>          *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify    *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 3 of the License, or       *
 * (at your option) any later version.                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful,         *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 **************************************************************************/

#pragma once

#include <QTreeView>

class SmartListView : public QTreeView
{
    Q_OBJECT
public:
    explicit SmartListView(QWidget* parent = 0);
    ~SmartListView();

protected:
    void enterEvent(QEvent* event);
    void leaveEvent(QEvent* event);
    void mousePressEvent(QMouseEvent *event);
    bool eventFilter(QObject* watched, QEvent* event);
    void drawRow(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const;

private:
    QModelIndex hoveredRow_;

//signals:
//    void btnAcceptInviteClicked(const QModelIndex& index) const;
//    void btnBlockInviteClicked(const QModelIndex& index) const;
//    void btnIgnoreInviteClicked(const QModelIndex& index) const;
};
