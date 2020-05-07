/****************************************************************
 *
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2012, RetroShare Team
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 ****************************************************************/

#ifndef UNSEENFRIENDSELECTIONWIDGET_H
#define UNSEENFRIENDSELECTIONWIDGET_H

#include <QWidget>
#include <QDialog>

#include <gui/gxs/RsGxsUpdateBroadcastPage.h>
#include "util/TokenQueue.h"
#include "gui/common/UnseenContactSmartListModel.h"
#include "gui/common/RSTreeWidgetItem.h"
#include "retroshare/rsidentity.h"
#include "retroshare/rsgxschats.h"

//unseenp2p
#include "util/TokenQueue.h"

namespace Ui {
class UnseenFriendSelectionWidget;
}

class QTreeWidgetItem;
class RSTreeWidgetItemCompareRole;

class UnseenGxsSmartListView;

class UIStateHelper;

class UnseenFriendSelectionWidget : public RsGxsUpdateBroadcastPage, public TokenResponse
{
	Q_OBJECT

	Q_PROPERTY(QColor textColorOnline READ textColorOnline WRITE setTextColorOnline)

public:
	enum IdType
	{
		IDTYPE_NONE,
		IDTYPE_GROUP,
		IDTYPE_SSL,
		IDTYPE_GPG,
        IDTYPE_GXS,
        IDTYPE_GXS_CHAT_MEMBER
	};

	enum Modus
	{
		MODUS_SINGLE,
		MODUS_MULTI,
		MODUS_CHECK
	};

    enum ShowFriendListMode
    {
        MODE_CREATE_GROUP,
        MODE_EDIT_GROUP,
        MODE_INVITE_FRIENDS
    };

    enum ShowType {
        SHOW_NONE             = 0,
        SHOW_GROUP            = 1,
        SHOW_GPG              = 2,
        SHOW_SSL              = 4,
        SHOW_NON_FRIEND_GPG   = 8,
        SHOW_GXS              =16,
        SHOW_CONTACTS         =32
    };

    Q_DECLARE_FLAGS(ShowTypes, ShowType)

public:
    explicit UnseenFriendSelectionWidget(QWidget *parent = 0);
    ~UnseenFriendSelectionWidget();

	void setModus(Modus modus);
	void setShowType(ShowTypes types);
	int addColumn(const QString &title);
	void start();

	bool isSortByState();
	bool isFilterConnected();


	int selectedItemCount();
	std::string selectedId(IdType &idType);
    template<class ID_CLASS,UnseenFriendSelectionWidget::IdType TYPE> void selectedIds(std::set<ID_CLASS>& ids, bool onlyDirectSelected)
	{
		std::set<std::string> tmpids ;
        	selectedIds(TYPE, tmpids, onlyDirectSelected);
        	ids.clear() ;
        	for(std::set<std::string>::const_iterator it(tmpids.begin());it!=tmpids.end();++it)
            	ids.insert(ID_CLASS(*it)) ;
	}
    template<class ID_CLASS,UnseenFriendSelectionWidget::IdType TYPE> void setSelectedIds(const std::set<ID_CLASS>& ids, bool add)
	{
        	std::set<std::string> tmpids ;
        	for(typename std::set<ID_CLASS>::const_iterator it(ids.begin());it!=ids.end();++it)
            		tmpids.insert((*it).toStdString()) ;
        	setSelectedIds(TYPE, tmpids, add);
	}

	void itemsFromId(IdType idType, const std::string &id, QList<QTreeWidgetItem*> &items);
	void items(QList<QTreeWidgetItem*> &items, IdType = IDTYPE_NONE);

	IdType idTypeFromItem(QTreeWidgetItem *item);
	std::string idFromItem(QTreeWidgetItem *item);

	QColor textColorOnline() const { return mTextColorOnline; }

	void setTextColorOnline(QColor color) { mTextColorOnline = color; }

	// Add QAction to context menu (action won't be deleted)
	void addContextMenuAction(QAction *action);


    virtual void updateDisplay(bool complete);

    //úneenp2p
    void setSelectedContacts(const std::set<GxsChatMember> list);
    void getSelectedContacts(std::set<GxsChatMember> &list);
    void getAllContacts(std::set<GxsChatMember> &list);
    void setGxsGroupId(const RsGxsGroupId _groupChatId);
    void setModeOfFriendList(ShowFriendListMode showMode);

protected:
	void changeEvent(QEvent *e);

	virtual void loadRequest(const TokenQueue *queue,const TokenRequest& req);


signals:
	void itemAdded(int idType, const QString &id, QTreeWidgetItem *item);
	void contentChanged();
	void doubleClicked(int idType, const QString &id);
	void itemChanged(int idType, const QString &id, QTreeWidgetItem *item, int column);
	void itemSelectionChanged();

public slots:
	void sortByState(bool sort);
	void filterConnected(bool filter);

private slots:
	void groupsChanged(int type);
	void peerStatusChanged(const QString& peerId, int status);
	void filterItems(const QString &text);
	void contextMenuRequested(const QPoint &pos);
	void itemDoubleClicked(QTreeWidgetItem *item, int column);
	void itemChanged(QTreeWidgetItem *item, int column);
	void selectAll() ;
	void deselectAll() ;

    void smartListSelectionChanged(const QItemSelection  &selected, const QItemSelection  &deselected);

private:
	void fillList();
	void secured_fillList();

    void selectedIds(IdType idType, std::set<std::string> &ids, bool onlyDirectSelected);
    void setSelectedIds(IdType idType, const std::set<std::string> &ids, bool add);

	void requestGXSIdList() ;

    //New GUI
    void selectConversation(const QModelIndex& index);

    void updateLineEditFromList();

private:
	bool mStarted;
	RSTreeWidgetItemCompareRole *mCompareRole;
	Modus mListModus;
	ShowTypes mShowTypes;
	bool mInGroupItemChanged;
	bool mInGpgItemChanged;
	bool mInSslItemChanged;
	bool mInFillList;
	QAction *mActionSortByState;
	QAction *mActionFilterConnected;

	/* Color definitions (for standard see qss.default) */
	QColor mTextColorOnline;

    Ui::UnseenFriendSelectionWidget *ui;

    friend class UnseenFriendSelectionDialog ;

	std::vector<RsGxsGroupId> gxsIds ;
	TokenQueue *mIdQueue ;
	QList<QAction*> mContextMenuActions;

    std::set<RsGxsId> mPreSelectedGxsIds; // because loading of GxsIds is asynchroneous we keep selected Ids from the client in a list here and use it to initialize after loading them.

    UnseenContactSmartListModel* smartListModel_; //unseenp2p - use the MVC GUI for the gxs chat

    ////////////////////////////////////////////////////
    //From IdDialog
    //TokenQueue *mIdQueue;
    TokenQueue *mCircleQueue;

    UIStateHelper *mStateHelper;

    QTreeWidgetItem *contactsItem;
    QTreeWidgetItem *allItem;
    QTreeWidgetItem *ownItem;
    QTreeWidgetItem *mExternalBelongingCircleItem;
    QTreeWidgetItem *mExternalOtherCircleItem;
    RsGxsUpdateBroadcastBase *mCirclesBroadcastBase ;

    //std::map<uint32_t, CircleUpdateOrder> mCircleUpdates ;

    RsGxsGroupId mId;
    RsGxsGroupId mIdToNavigate;
    int filter;

    //unseenp2p
    QString stringList;
    std::set<GxsChatMember> selectedList;
    RsGxsGroupId groupChatId;
    ShowFriendListMode showMode;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(UnseenFriendSelectionWidget::ShowTypes)

#endif // FRIENDSELECTIONWIDGET_H
