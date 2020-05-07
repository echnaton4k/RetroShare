/****************************************************************
 *  RetroShare is distributed under the following license:
 *
 *  Copyright (C) 2008 Robert Fernie
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

#include <QMenu>
#include <QMessageBox>
#include <QToolButton>

#include "UnseenGxsGroupFrameDialog.h"
#include "ui_UnseenGxsGroupFrameDialog.h"
#include "GxsMessageFrameWidget.h"

#include "gui/settings/rsharesettings.h"
#include "gui/RetroShareLink.h"
#include "gui/gxs/GxsGroupShareKey.h"
#include "gui/common/RSTreeWidget.h"
#include "gui/notifyqt.h"
#include "gui/common/UIStateHelper.h"
#include "GxsCommentDialog.h"

#include "gui/gxschats/UnseenGxsChatLobbyDialog.h"
//unseenp2p
#include "gui/gxs/UnseenGxsGroupDialog.h"
#include "retroshare/rsnotify.h"

//unseenp2p - using SmartListView and SmartListModel
#include "gui/common/GroupTreeWidget.h"
#include "gui/gxschats/UnseenGxsSmartListView.h"
#include "gui/UnseenGxsConversationitemdelegate.h"

#include "retroshare/rspeers.h"

//#define DEBUG_GROUPFRAMEDIALOG 1

/* Images for TreeWidget */
#define IMAGE_SUBSCRIBE      ":/images/edit_add24.png"
#define IMAGE_UNSUBSCRIBE    ":/images/cancel.png"
#define IMAGE_INFO           ":/images/info16.png"
//#define IMAGE_GROUPAUTHD     ":/images/konv_message2.png"
#define IMAGE_COPYLINK       ":/images/copyrslink.png"
#define IMAGE_EDIT           ":/images/edit_16.png"
#define IMAGE_SHARE          ":/images/share-icon-16.png"
#define IMAGE_TABNEW         ":/images/tab-new.png"
#define IMAGE_DELETE         ":/images/delete.png"
#define IMAGE_RETRIEVE       ":/images/edit_add24.png"
#define IMAGE_COMMENT        ""

#define TOKEN_TYPE_GROUP_SUMMARY    1
//#define TOKEN_TYPE_SUBSCRIBE_CHANGE 2
//#define TOKEN_TYPE_CURRENTGROUP     3
#define TOKEN_TYPE_STATISTICS       4
#define TOKEN_TYPE_LAST_MSG_OF_GROUP   5

#define GXSGROUP_NEWGROUPID_2         6

#define MAX_COMMENT_TITLE 32

/*
 * Transformation Notes:
 *   there are still a couple of things that the new groups differ from Old version.
 *   these will need to be addressed in the future.
 *     -> Child TS (for sorting) is not handled by GXS, this will probably have to be done in the GUI.
 *     -> Need to handle IDs properly.
 *     -> Much more to do.
 */

/** Constructor */
UnseenGxsGroupFrameDialog::UnseenGxsGroupFrameDialog(RsGxsIfaceHelper *ifaceImpl, QWidget *parent,bool allow_dist_sync)
: RsGxsUpdateBroadcastPage(ifaceImpl, parent)
{
	/* Invoke the Qt Designer generated object setup routine */
    ui = new Ui::UnseenGxsGroupFrameDialog();
	ui->setupUi(this);

    mInitialized = false;
	mDistSyncAllowed = allow_dist_sync;
    mInFill = false;
	mCountChildMsgs = false;
	mYourGroups = NULL;
	mSubscribedGroups = NULL;
	mPopularGroups = NULL;
	mOtherGroups = NULL;
	mMessageWidget = NULL;

   // QObject::connect(NotifyQt::getInstance(), SIGNAL(notifyReceiveGxsChatTyping2(const QString)), this, SLOT(updateGxsMsgTyping2(const QString)));

    QObject::connect( NotifyQt::getInstance(), SIGNAL(alreadySendChat(const gxsChatId&, const RsGxsChatMsg&, std::string, long long, std::string, bool)), this, SLOT(updateRecentTime(const gxsChatId&, const RsGxsChatMsg&, std::string, long long, std::string, bool)));
    QObject::connect( NotifyQt::getInstance(), SIGNAL(newGxsChatMessageReceive(const gxsChatId&, const RsGxsChatMsg&, std::string, long long, std::string, bool)), this, SLOT(updateNewGxsMsg(const gxsChatId&, const RsGxsChatMsg&, std::string, long long, std::string, bool)));

    connect(ui->filterLineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(filterGxsItems(const QString &)));
    connect(ui->filterLineEdit, SIGNAL(filterChanged(int)), this, SLOT(filterColumnChanged(int)));
    QObject::connect( ui->createGxsGroupChatButton, SIGNAL(clicked()), this, SLOT(newGroup()));

    //QObject::connect(NotifyQt::getInstance(), SIGNAL(notifyReceiveGxsChatTyping(const RsGxsGroupId&, const QString, const RsPeerId&, const RsGxsId&)), this, SLOT(updateGxsMsgTyping(const RsGxsGroupId&, const QString, const RsPeerId&, const RsGxsId&)));

    QObject::connect(NotifyQt::getInstance(), SIGNAL(notifyCreateNewGxsGroup(const QString)), this, SLOT(receiveNotifyCreateNewGxsGroup(const QString)));

    /* add filter actions */
    ui->filterLineEdit->setPlaceholderText("Search ");
    ui->filterLineEdit->showFilterIcon();


//    //MVC GUI for Gxs GroupChat
    if (!ui->unseenGroupTreeWidget->model()) {
        smartListModel_ = new UnseenGxsSmartListModel("testing", this);
        ui->unseenGroupTreeWidget->setModel(smartListModel_);
        ui->unseenGroupTreeWidget->setItemDelegate(new UnseenGxsConversationItemDelegate());
        ui->unseenGroupTreeWidget->show();
    }

    // smartlist selection
    QObject::connect(ui->unseenGroupTreeWidget->selectionModel(),
        SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
        this,
        SLOT(smartListSelectionChanged(QItemSelection, QItemSelection)));

    ui->unseenGroupTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu) ;
    ui->unseenGroupTreeWidget->header()->hide();

    allGxsChatGroupList.clear();
    isRunOnlyOnce = false;
    conversationListMode = GXSCONVERSATION_MODE_WITHOUT_FILTER;
    smartListModel_->setFilterGxsChatGroupListAndMode(allGxsChatGroupList, conversationListMode);

	/* Setup Queue */
	mInterface = ifaceImpl;
	mTokenService = mInterface->getTokenService();
	mTokenQueue = new TokenQueue(mInterface->getTokenService(), this);

	/* Setup UI helper */
	mStateHelper = new UIStateHelper(this);

	mStateHelper->addWidget(TOKEN_TYPE_GROUP_SUMMARY, ui->loadingLabel, UISTATE_LOADING_VISIBLE);

    connect(ui->unseenGroupTreeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(groupTreeCustomPopupMenu(QPoint)));

    /* Set initial size the splitter */
    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setCollapsible(0, false);
    ui->splitter->setCollapsible(1, false);

	QList<int> sizes;
    sizes << 200 << width(); // Qt calculates the right sizes
    ui->splitter->setSizes(sizes);



#ifndef UNFINISHED
	ui->todoPushButton->hide();
#endif
}

UnseenGxsGroupFrameDialog::~UnseenGxsGroupFrameDialog()
{
	// save settings
	processSettings(false);

	delete(mTokenQueue);
	delete(ui);
}

void UnseenGxsGroupFrameDialog::getGroupList(std::map<RsGxsGroupId, RsGroupMetaData> &group_list)
{
	group_list = mCachedGroupMetas ;

	if(group_list.empty())
		requestGroupSummary();
}
void UnseenGxsGroupFrameDialog::initUi()
{
	registerHelpButton(ui->helpButton, getHelpString(),pageName()) ;

	ui->titleBarPixmap->setPixmap(QPixmap(icon(ICON_NAME)));
	ui->titleBarLabel->setText(text(TEXT_NAME));

	/* Initialize group tree */
	QToolButton *newGroupButton = new QToolButton(this);
	newGroupButton->setIcon(QIcon(icon(ICON_NEW)));
	newGroupButton->setToolTip(text(TEXT_NEW));
	connect(newGroupButton, SIGNAL(clicked()), this, SLOT(newGroup()));
   // ui->unseenGroupTreeWidget->addToolButton(newGroupButton);

	/* Create group tree */
	if (text(TEXT_TODO).isEmpty()) {
		ui->todoPushButton->hide();
	}

	// load settings
	mSettingsName = settingsGroupName();
	processSettings(true);

	if (groupFrameSettingsType() != GroupFrameSettings::Nothing) {
		connect(NotifyQt::getInstance(), SIGNAL(settingsChanged()), this, SLOT(settingsChanged()));
		settingsChanged();
	}
}

void UnseenGxsGroupFrameDialog::showEvent(QShowEvent *event)
{
	if (!mInitialized) {
		/* Problem: virtual methods cannot be used in constructor */
		mInitialized = true;

		initUi();
	}

	RsGxsUpdateBroadcastPage::showEvent(event);
}

void UnseenGxsGroupFrameDialog::processSettings(bool load)
{
	if (mSettingsName.isEmpty()) {
		return;
	}

	Settings->beginGroup(mSettingsName);

	if (load) {
		// load settings

		// state of splitter
		ui->splitter->restoreState(Settings->value("Splitter").toByteArray());
	} else {
		// save settings

		// state of splitter
		Settings->setValue("Splitter", ui->splitter->saveState());
	}

    //ui->unseenGroupTreeWidget->processSettings(load);

	Settings->endGroup();
}

void UnseenGxsGroupFrameDialog::settingsChanged()
{
	GroupFrameSettings groupFrameSettings;
	if (Settings->getGroupFrameSettings(groupFrameSettingsType(), groupFrameSettings)) {
		setSingleTab(!groupFrameSettings.mOpenAllInNewTab);
		setHideTabBarWithOneTab(groupFrameSettings.mHideTabBarWithOneTab);
	}
}

void UnseenGxsGroupFrameDialog::setSingleTab(bool singleTab)
{
	if (singleTab) {
		if (!mMessageWidget) {
			mMessageWidget = createMessageWidget(RsGxsGroupId());
			// remove close button of the the first tab
            //ui->messageTabWidget->hideCloseButton(ui->messageTabWidget->indexOf(mMessageWidget));
		}
	} else {
		if (mMessageWidget) {
			delete(mMessageWidget);
			mMessageWidget = NULL;
		}
	}
}

void UnseenGxsGroupFrameDialog::setHideTabBarWithOneTab(bool hideTabBarWithOneTab)
{
    //ui->messageTabWidget->setHideTabBarWithOneTab(hideTabBarWithOneTab);
}

void UnseenGxsGroupFrameDialog::updateDisplay(bool complete)
{
	if (complete || !getGrpIds().empty() || !getGrpIdsMeta().empty()) {
		/* Update group list */
		requestGroupSummary();
	} else {
		/* Update all groups of changed messages */
		std::map<RsGxsGroupId, std::set<RsGxsMessageId> > msgIds;
		getAllMsgIds(msgIds);


        for (auto msgIt = msgIds.begin(); msgIt != msgIds.end(); ++msgIt)
        {
            if(_unseenGxsGroup_infos.find(msgIt->first) != _unseenGxsGroup_infos.end())
                _unseenGxsGroup_infos[msgIt->first].dialog->updateDisplay(false);
            else
            {
                //here is when we receive the new msg but still not have chat window for this
                //so, just show the unread nunmber on the chat list and sort the list
                if(msgIt->second.size() > 0 )
                {
                    //Using token request from this list dialog to get the last msg: cost performance?
                    std::cerr << "we want to show all unread msg for these groups:" << msgIt->first.toStdString() << std::endl;
                    requestLastMsgOfGroup(msgIt->first, msgIt->second);
                }
            }
        }
	}

    updateSearchResults() ;
}

void UnseenGxsGroupFrameDialog::updateSearchResults()
{
    const std::set<TurtleRequestId>& reqs = getSearchResults();

    for(auto it(reqs.begin());it!=reqs.end();++it)
    {
		std::cerr << "updating search ID " << std::hex << *it << std::dec << std::endl;

        std::map<RsGxsGroupId,RsGxsGroupSummary> group_infos;

        getDistantSearchResults(*it,group_infos) ;

        std::cerr << "retrieved " << std::endl;

        auto it2 = mSearchGroupsItems.find(*it);

        if(mSearchGroupsItems.end() == it2)
        {
            std::cerr << "UnseenGxsGroupFrameDialog::updateSearchResults(): received result notification for req " << std::hex << *it << std::dec << " but no item present!" << std::endl;
            continue ;	// we could create the item just as well but since this situation is not supposed to happen, I prefer to make this a failure case.
        }

        QList<GroupItemInfo> group_items ;

		for(auto it3(group_infos.begin());it3!=group_infos.end();++it3)
			if(mCachedGroupMetas.find(it3->first) == mCachedGroupMetas.end())
			{
				std::cerr << "  adding new group " << it3->first << " "
				          << it3->second.mGroupId << " \""
				          << it3->second.mGroupName << "\"" << std::endl;

                GroupItemInfo i;
				i.id             = QString(it3->second.mGroupId.toStdString().c_str());
				i.name           = QString::fromUtf8(it3->second.mGroupName.c_str());
				i.popularity     = 0; // could be set to the number of hits
				i.lastpost       = QDateTime::fromTime_t(it3->second.mLastMessageTs);
				i.subscribeFlags = 0; // irrelevant here
				i.publishKey     = false ; // IS_GROUP_PUBLISHER(groupInfo.mSubscribeFlags);
				i.adminKey       = false ; // IS_GROUP_ADMIN(groupInfo.mSubscribeFlags);
				i.max_visible_posts = it3->second.mNumberOfMessages;

				group_items.push_back(i);
			}

      //  ui->unseenGroupTreeWidget->fillGroupItems(it2->second, group_items);
    }
}

void UnseenGxsGroupFrameDialog::todo()
{
	QMessageBox::information(this, "Todo", text(TEXT_TODO));
}

void UnseenGxsGroupFrameDialog::removeCurrentSearch()
{
    QAction *action = dynamic_cast<QAction*>(sender()) ;

    if(!action)
        return ;

    TurtleRequestId search_request_id = action->data().toUInt();

    auto it = mSearchGroupsItems.find(search_request_id) ;

    if(it == mSearchGroupsItems.end())
        return ;

//    ui->unseenGroupTreeWidget->removeSearchItem(it->second) ;
    mSearchGroupsItems.erase(it);

    mKnownGroups.erase(search_request_id);
}

void UnseenGxsGroupFrameDialog::removeAllSearches()
{
//    for(auto it(mSearchGroupsItems.begin());it!=mSearchGroupsItems.end();++it)
//        ui->unseenGroupTreeWidget->removeSearchItem(it->second) ;

    mSearchGroupsItems.clear();
    mKnownGroups.clear();
}
void UnseenGxsGroupFrameDialog::groupTreeCustomPopupMenu(QPoint point)
{
	// First separately handle the case of search top level items

	TurtleRequestId search_request_id = 0 ;
    // Then check whether we have a searched item, or a normal group

    QString group_id_s ;

    //QString id = itemIdAt(point);
    //if (id.isEmpty()) return;
    //UnseenGroupItemInfo groupItem = groupItemIdAt(point);
    RsGxsChatGroup groupItem = gxsGroupItemIdAt(point);
    //mGroupId = RsGxsGroupId(id.toStdString());
    mGroupId = groupItem.mMeta.mGroupId;
    bool isAdmin      = IS_GROUP_ADMIN(groupItem.mMeta.mSubscribeFlags);
    bool isPublisher  = IS_GROUP_PUBLISHER(groupItem.mMeta.mSubscribeFlags);
    bool isSubscribed = IS_GROUP_SUBSCRIBED(groupItem.mMeta.mSubscribeFlags);

    QMenu contextMnu(this);
    QAction *action;

    if (isSubscribed && groupItem.type != RsGxsChatGroup::ONE2ONE) {
        QString text = tr("Leave and delete this group");
        if(groupItem.type == RsGxsChatGroup::CHANNEL)
            text = tr("Leave and delete this channel");

        action = contextMnu.addAction(QIcon(IMAGE_UNSUBSCRIBE), text, this, SLOT(unsubscribeGroup()));
        action->setEnabled (!mGroupId.isNull() && isSubscribed);
    }

    contextMnu.addAction(QIcon(icon(ICON_NEW)), text(TEXT_NEW), this, SLOT(newGroup()));

    if(groupItem.type != RsGxsChatGroup::ONE2ONE)
    {
        action = contextMnu.addAction(QIcon(IMAGE_INFO), tr("Show Details"), this, SLOT(showGroupDetails()));
        action->setEnabled (!mGroupId.isNull());

        action = contextMnu.addAction(QIcon(IMAGE_EDIT), tr("Edit Details"), this, SLOT(editGroupDetails()));
        action->setEnabled (!mGroupId.isNull() && isAdmin);
    }

    uint32_t current_store_time = mInterface->getStoragePeriod(mGroupId)/86400 ;
    uint32_t current_sync_time  = mInterface->getSyncPeriod(mGroupId)/86400 ;

    std::cerr << "Got sync=" << current_sync_time << ". store=" << current_store_time << std::endl;
    QAction *actnn = NULL;

    QMenu *ctxMenu2 = contextMnu.addMenu(tr("Synchronise posts of last...")) ;
    actnn = ctxMenu2->addAction(tr(" 5 days"     ),this,SLOT(setSyncPostsDelay())) ; actnn->setData(QVariant(  5)) ; if(current_sync_time ==  5) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 2 weeks"    ),this,SLOT(setSyncPostsDelay())) ; actnn->setData(QVariant( 15)) ; if(current_sync_time == 15) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 1 month"    ),this,SLOT(setSyncPostsDelay())) ; actnn->setData(QVariant( 30)) ; if(current_sync_time == 30) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 3 months"   ),this,SLOT(setSyncPostsDelay())) ; actnn->setData(QVariant( 90)) ; if(current_sync_time == 90) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 6 months"   ),this,SLOT(setSyncPostsDelay())) ; actnn->setData(QVariant(180)) ; if(current_sync_time ==180) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 1 year  "   ),this,SLOT(setSyncPostsDelay())) ; actnn->setData(QVariant(372)) ; if(current_sync_time ==372) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" Indefinitly"),this,SLOT(setSyncPostsDelay())) ; actnn->setData(QVariant(  0)) ; if(current_sync_time ==  0) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}

    ctxMenu2 = contextMnu.addMenu(tr("Store posts for at most...")) ;
    actnn = ctxMenu2->addAction(tr(" 5 days"     ),this,SLOT(setStorePostsDelay())) ; actnn->setData(QVariant(  5)) ; if(current_store_time ==  5) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 2 weeks"    ),this,SLOT(setStorePostsDelay())) ; actnn->setData(QVariant( 15)) ; if(current_store_time == 15) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 1 month"    ),this,SLOT(setStorePostsDelay())) ; actnn->setData(QVariant( 30)) ; if(current_store_time == 30) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 3 months"   ),this,SLOT(setStorePostsDelay())) ; actnn->setData(QVariant( 90)) ; if(current_store_time == 90) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 6 months"   ),this,SLOT(setStorePostsDelay())) ; actnn->setData(QVariant(180)) ; if(current_store_time ==180) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" 1 year  "   ),this,SLOT(setStorePostsDelay())) ; actnn->setData(QVariant(372)) ; if(current_store_time ==372) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}
    actnn = ctxMenu2->addAction(tr(" Indefinitly"),this,SLOT(setStorePostsDelay())) ; actnn->setData(QVariant(  0)) ; if(current_store_time ==  0) { actnn->setEnabled(false);actnn->setIcon(QIcon(":/images/start.png"));}

    if (shareKeyType() && groupItem.type != RsGxsChatGroup::ONE2ONE) {
        action = contextMnu.addAction(QIcon(IMAGE_SHARE), tr("Grant Moderator Permission"), this, SLOT(sharePublishKey()));
        //action->setEnabled(!mGroupId.isNull() && isPublisher); //in this version (0.8.0), just admin can grant and share key
        action->setEnabled(!mGroupId.isNull() && isAdmin);
    }

    contextMnu.addSeparator();

    action = contextMnu.addAction(QIcon(":/images/message-mail-read.png"), tr("Mark all as read"), this, SLOT(markMsgAsRead()));
    action->setEnabled (!mGroupId.isNull() && isSubscribed);

    action = contextMnu.addAction(QIcon(":/images/message-mail.png"), tr("Mark all as unread"), this, SLOT(markMsgAsUnread()));
    action->setEnabled (!mGroupId.isNull() && isSubscribed);

//	/* Add special actions */
    QList<QAction*> actions;
    groupTreeCustomActions(mGroupId, groupItem.mMeta.mSubscribeFlags, actions);
    if (!actions.isEmpty()) {
        contextMnu.addSeparator();
        contextMnu.addActions(actions);
    }

    contextMnu.exec(QCursor::pos());
}

void UnseenGxsGroupFrameDialog::setStorePostsDelay()
{
    QAction *action = dynamic_cast<QAction*>(sender()) ;

    if(!action || mGroupId.isNull())
    {
        std::cerr << "(EE) Cannot find action/group that called me! Group is " << mGroupId << ", action is " << (void*)action << "  " << __PRETTY_FUNCTION__ << std::endl;
        return;
    }

    uint32_t duration = action->data().toUInt() ;

    std::cerr << "Data is " << duration << std::endl;

 	mInterface->setStoragePeriod(mGroupId,duration * 86400) ;

    // If the sync is larger, we reduce it. No need to sync more than we store. The machinery below also takes care of this.
    //
    uint32_t sync_period = mInterface->getSyncPeriod(mGroupId);

    if(duration > 0)      // the >0 test is to discard the indefinitly test. Basically, if we store for less than indefinitly, the sync is reduced accordingly.
    {
        if(sync_period == 0 || sync_period > duration*86400)
        {
			mInterface->setSyncPeriod(mGroupId,duration * 86400) ;

            std::cerr << "(II) auto adjusting sync period to " << duration<< " days as well." << std::endl;
        }
    }
}


void UnseenGxsGroupFrameDialog::setSyncPostsDelay()
{
    QAction *action = dynamic_cast<QAction*>(sender()) ;

    if(!action || mGroupId.isNull())
    {
        std::cerr << "(EE) Cannot find action/group that called me! Group is " << mGroupId << ", action is " << (void*)action << "  " << __PRETTY_FUNCTION__ << std::endl;
        return;
    }

    uint32_t duration = action->data().toUInt() ;

    std::cerr << "Data is " << duration << std::endl;

 	mInterface->setSyncPeriod(mGroupId,duration * 86400) ;

    // If the store is smaller, we increase it accordingly. No need to sync more than we store. The machinery below also takes care of this.
    //
    uint32_t store_period = mInterface->getStoragePeriod(mGroupId);

    if(duration == 0)
 		mInterface->setStoragePeriod(mGroupId,duration * 86400) ;	// indefinite sync => indefinite storage
    else
    {
        if(store_period != 0 && store_period < duration*86400)
        {
 			mInterface->setStoragePeriod(mGroupId,duration * 86400) ;	// indefinite sync => indefinite storage
            std::cerr << "(II) auto adjusting storage period to " << duration<< " days as well." << std::endl;
        }
    }
}

void UnseenGxsGroupFrameDialog::restoreGroupKeys(void)
{
    QMessageBox::warning(this, "UnseenP2P", "ToDo");

#ifdef TOGXS
	mInterface->groupRestoreKeys(mGroupId);
#endif
}

void UnseenGxsGroupFrameDialog::newGroup()
{
    //Need to change new conversation dialog here, add member list into the dialog for choosing
    // Use the CreateLobbyDialog to create the dialog GUI,
    // Copy the CreateLobbyDialog and customize as CreateGxsConversationDialog with 3 options:
    // 1. one2one conversation,
    // 2. group chat conversation:
    //      2.1. private groupchat (member only),
    //      2.2. public groupchat (like forum)
    // 3. Channel conversation:
    //      In channel there are 2 buttons Mute and discuss,
    //          when clicking on discuss, it will open the groupchat
    //          when clicking on Mute -> maybe do not receive any msg

    // In CreateLobbyDialog there is a class named FriendSelectionWidget *keyShareList;
    // we can re-use it to add contact list into the dialog
    UnseenGxsGroupDialog *dialog = createNewGroupDialog(mTokenQueue);
    //GxsGroupDialog *dialog = createNewGroupDialog(mTokenQueue); //old codes
	if (!dialog) {
		return;
	}

	dialog->exec();
	delete(dialog);
}

void UnseenGxsGroupFrameDialog::subscribeGroup()
{
	groupSubscribe(true);
}

void UnseenGxsGroupFrameDialog::unsubscribeGroup()
{
    if ((QMessageBox::question(this, tr("Really leave?"), tr("Do you really want to leave and delete this conversation?"), QMessageBox::Yes|QMessageBox::No, QMessageBox::No))== QMessageBox::Yes)
    {
        groupSubscribe(false);
    }

}

void UnseenGxsGroupFrameDialog::groupSubscribe(bool subscribe)
{
	if (mGroupId.isNull()) {
		return;
	}

    if(subscribe)
    {
        uint32_t token;
        mInterface->subscribeToGroup(token, mGroupId, subscribe);
    }
    else unsubscribeGxsGroupChat(mGroupId);
}

void UnseenGxsGroupFrameDialog::showGroupDetails()
{
	if (mGroupId.isNull()) {
		return;
	}

    RsGxsChatGroup gxsChatGroup = gxsGroupFromList(mGroupId);
    UnseenGxsGroupDialog *dialog = createGroupDialog(mTokenQueue, mInterface->getTokenService(), UnseenGxsGroupDialog::MODE_SHOW,gxsChatGroup.type, mGroupId);
	if (!dialog) {
		return;
	}

	dialog->exec();
	delete(dialog);
}

void UnseenGxsGroupFrameDialog::editGroupDetails()
{
	if (mGroupId.isNull()) {
		return;
	}

    RsGxsChatGroup gxsChatGroup = gxsGroupFromList(mGroupId);
    
    UnseenGxsGroupDialog *dialog = createGroupDialog(mTokenQueue, mInterface->getTokenService(), UnseenGxsGroupDialog::MODE_EDIT, gxsChatGroup.type, mGroupId);
	if (!dialog) {
		return;
	}

	dialog->exec();
	delete(dialog);
}

void UnseenGxsGroupFrameDialog::copyGroupLink()
{
	if (mGroupId.isNull()) {
		return;
	}

	QString name;
	if(!getCurrentGroupName(name)) return;

	RetroShareLink link = RetroShareLink::createGxsGroupLink(getLinkType(), mGroupId, name);

	if (link.valid()) {
		QList<RetroShareLink> urls;
		urls.push_back(link);
		RSLinkClipboard::copyLinks(urls);
	}
}

bool UnseenGxsGroupFrameDialog::getCurrentGroupName(QString& name)
{
    return false;
    //return ui->unseenGroupTreeWidget->getGroupName(QString::fromStdString(mGroupId.toStdString()), name);
}

void UnseenGxsGroupFrameDialog::markMsgAsRead()
{
	GxsMessageFrameWidget *msgWidget = messageWidget(mGroupId, false);
	if (msgWidget) {
		msgWidget->setAllMessagesRead(true);
	}
}

void UnseenGxsGroupFrameDialog::markMsgAsUnread()
{
	GxsMessageFrameWidget *msgWidget = messageWidget(mGroupId, false);
	if (msgWidget) {
		msgWidget->setAllMessagesRead(false);
	}
}

void UnseenGxsGroupFrameDialog::sharePublishKey()
{
	if (mGroupId.isNull()) {
		return;
	}

//	QMessageBox::warning(this, "", "ToDo");

    GroupShareKey shareUi(this, mGroupId, shareKeyType());
    shareUi.exec();
}

void UnseenGxsGroupFrameDialog::loadComment(const RsGxsGroupId &grpId, const QVector<RsGxsMessageId>& msg_versions, const RsGxsMessageId &most_recent_msgId, const QString &title)
{
	RsGxsCommentService *commentService = getCommentService();
	if (!commentService) {
		/* No comment service available */
		return;
	}

	GxsCommentDialog *commentDialog = commentWidget(most_recent_msgId);

	if (!commentDialog) {
		QString comments = title;
		if (title.length() > MAX_COMMENT_TITLE)
		{
			comments.truncate(MAX_COMMENT_TITLE - 3);
			comments += "...";
		}

		commentDialog = new GxsCommentDialog(this, mInterface->getTokenService(), commentService);

		QWidget *commentHeader = createCommentHeaderWidget(grpId, most_recent_msgId);
		if (commentHeader) {
			commentDialog->setCommentHeader(commentHeader);
		}

        std::set<RsGxsMessageId> msgv;
        for(int i=0;i<msg_versions.size();++i)
            msgv.insert(msg_versions[i]);

		commentDialog->commentLoad(grpId, msgv,most_recent_msgId);

//		int index = ui->messageTabWidget->addTab(commentDialog, comments);
//		ui->messageTabWidget->setTabIcon(index, QIcon(IMAGE_COMMENT));
	}

    //ui->messageTabWidget->setCurrentWidget(commentDialog);
}

bool UnseenGxsGroupFrameDialog::navigate(const RsGxsGroupId &groupId, const RsGxsMessageId& msgId)
{
    return false; // unseenp2p
	if (groupId.isNull()) {
		return false;
	}

	if (mStateHelper->isLoading(TOKEN_TYPE_GROUP_SUMMARY)) {
		mNavigatePendingGroupId = groupId;
		mNavigatePendingMsgId = msgId;

		/* No information if group is available */
		return true;
	}

	QString groupIdString = QString::fromStdString(groupId.toStdString());
//    if (ui->unseenGroupTreeWidget->activateId(groupIdString, msgId.isNull()) == NULL) {
//		return false;
//	}

	changedCurrentGroup(groupIdString);

	/* search exisiting tab */
	GxsMessageFrameWidget *msgWidget = messageWidget(mGroupId, false);
	if (!msgWidget) {
		return false;
	}

	if (msgId.isNull()) {
		return true;
	}

	return msgWidget->navigate(msgId);
}

GxsMessageFrameWidget *UnseenGxsGroupFrameDialog::messageWidget(const RsGxsGroupId &groupId, bool ownTab)
{
//	int tabCount = ui->messageTabWidget->count();
//	for (int index = 0; index < tabCount; ++index) {
//		GxsMessageFrameWidget *childWidget = dynamic_cast<GxsMessageFrameWidget*>(ui->messageTabWidget->widget(index));
//		if (ownTab && mMessageWidget && childWidget == mMessageWidget) {
//			continue;
//		}
//		if (childWidget && childWidget->groupId() == groupId) {
//			return childWidget;
//		}
//	}

	return NULL;
}

GxsMessageFrameWidget *UnseenGxsGroupFrameDialog::createMessageWidget(const RsGxsGroupId &groupId)
{
	GxsMessageFrameWidget *msgWidget = createMessageFrameWidget(groupId);
	if (!msgWidget) {
		return NULL;
	}

//	int index = ui->messageTabWidget->addTab(msgWidget, msgWidget->groupName(true));
//	ui->messageTabWidget->setTabIcon(index, msgWidget->groupIcon());

//	connect(msgWidget, SIGNAL(groupChanged(QWidget*)), this, SLOT(messageTabInfoChanged(QWidget*)));
//	connect(msgWidget, SIGNAL(waitingChanged(QWidget*)), this, SLOT(messageTabWaitingChanged(QWidget*)));
//	connect(msgWidget, SIGNAL(loadComment(RsGxsGroupId,QVector<RsGxsMessageId>,RsGxsMessageId,QString)), this, SLOT(loadComment(RsGxsGroupId,QVector<RsGxsMessageId>,RsGxsMessageId,QString)));

	return msgWidget;
}

GxsCommentDialog *UnseenGxsGroupFrameDialog::commentWidget(const RsGxsMessageId& msgId)
{
//	int tabCount = ui->messageTabWidget->count();
//	for (int index = 0; index < tabCount; ++index) {
//		GxsCommentDialog *childWidget = dynamic_cast<GxsCommentDialog*>(ui->messageTabWidget->widget(index));
//		if (childWidget && childWidget->messageId() == msgId) {
//			return childWidget;
//		}
//	}

	return NULL;
}

void UnseenGxsGroupFrameDialog::changedCurrentGroup(const QString &groupId)
{
//	if (mInFill) {
//		return;
//	}

//	if (groupId.isEmpty()) {
//		if (mMessageWidget) {
//			mMessageWidget->setGroupId(RsGxsGroupId());
//			ui->messageTabWidget->setCurrentWidget(mMessageWidget);
//		}
//		return;
//	}

//	mGroupId = RsGxsGroupId(groupId.toStdString());
//	if (mGroupId.isNull()) {
//		return;
//	}

//	/* search exisiting tab */
//	GxsMessageFrameWidget *msgWidget = messageWidget(mGroupId, true);

//	if (!msgWidget) {
//		if (mMessageWidget) {
//			/* not found, use standard tab */
//			msgWidget = mMessageWidget;
//			msgWidget->setGroupId(mGroupId);
//		} else {
//			/* create new tab */
//			msgWidget = createMessageWidget(mGroupId);
//		}
//	}

//	ui->messageTabWidget->setCurrentWidget(msgWidget);
}

void UnseenGxsGroupFrameDialog::groupTreeMiddleButtonClicked(QTreeWidgetItem *item)
{
    //openGroupInNewTab(RsGxsGroupId(ui->unseenGroupTreeWidget->itemId(item).toStdString()));
}

void UnseenGxsGroupFrameDialog::openInNewTab()
{
	openGroupInNewTab(mGroupId);
}

void UnseenGxsGroupFrameDialog::openGroupInNewTab(const RsGxsGroupId &groupId)
{
//	if (groupId.isNull()) {
//		return;
//	}

//	/* search exisiting tab */
//	GxsMessageFrameWidget *msgWidget = messageWidget(groupId, true);
//	if (!msgWidget) {
//		/* not found, create new tab */
//		msgWidget = createMessageWidget(groupId);
//	}

//	ui->messageTabWidget->setCurrentWidget(msgWidget);
}

void UnseenGxsGroupFrameDialog::messageTabCloseRequested(int index)
{
//	QWidget *widget = ui->messageTabWidget->widget(index);
//	if (!widget) {
//		return;
//	}

//	GxsMessageFrameWidget *msgWidget = dynamic_cast<GxsMessageFrameWidget*>(widget);
//	if (msgWidget && msgWidget == mMessageWidget) {
//		/* Don't close single tab */
//		return;
//	}

//	delete(widget);
}

void UnseenGxsGroupFrameDialog::messageTabChanged(int index)
{
//	GxsMessageFrameWidget *msgWidget = dynamic_cast<GxsMessageFrameWidget*>(ui->messageTabWidget->widget(index));
//	if (!msgWidget) {
//		return;
//	}

    //ui->unseenGroupTreeWidget->activateId(QString::fromStdString(msgWidget->groupId().toStdString()), false);
}

void UnseenGxsGroupFrameDialog::messageTabInfoChanged(QWidget *widget)
{
//	int index = ui->messageTabWidget->indexOf(widget);
//	if (index < 0) {
//		return;
//	}

//	GxsMessageFrameWidget *msgWidget = dynamic_cast<GxsMessageFrameWidget*>(ui->messageTabWidget->widget(index));
//	if (!msgWidget) {
//		return;
//	}

//	ui->messageTabWidget->setTabText(index, msgWidget->groupName(true));
//	ui->messageTabWidget->setTabIcon(index, msgWidget->groupIcon());
}

void UnseenGxsGroupFrameDialog::messageTabWaitingChanged(QWidget *widget)
{
//	int index = ui->messageTabWidget->indexOf(widget);
//	if (index < 0) {
//		return;
//	}

//	GxsMessageFrameWidget *msgWidget = dynamic_cast<GxsMessageFrameWidget*>(ui->messageTabWidget->widget(index));
//	if (!msgWidget) {
//		return;
//	}

    //ui->unseenGroupTreeWidget->setWaiting(QString::fromStdString(msgWidget->groupId().toStdString()), msgWidget->isWaiting());
}

///***** INSERT GROUP LISTS *****/
void UnseenGxsGroupFrameDialog::groupInfoToGroupItemInfo(const RsGroupMetaData &groupInfo, GroupItemInfo &groupItemInfo, const RsUserdata */*userdata*/)
{
	groupItemInfo.id = QString::fromStdString(groupInfo.mGroupId.toStdString());
	groupItemInfo.name = QString::fromUtf8(groupInfo.mGroupName.c_str());
	groupItemInfo.popularity = groupInfo.mPop;
	groupItemInfo.lastpost = QDateTime::fromTime_t(groupInfo.mLastPost);
    groupItemInfo.subscribeFlags = groupInfo.mSubscribeFlags;
	groupItemInfo.publishKey = IS_GROUP_PUBLISHER(groupInfo.mSubscribeFlags) ;
	groupItemInfo.adminKey = IS_GROUP_ADMIN(groupInfo.mSubscribeFlags) ;
	groupItemInfo.max_visible_posts = groupInfo.mVisibleMsgCount ;

    //unseenp2p
    groupItemInfo.lastMsgDatetime = groupInfo.mLastPost;
    RsIdentityDetails details ;
    if (rsIdentity->getIdDetails(groupInfo.mAuthorId,details) )
    {
        groupItemInfo.authorOfLastMsg = QString::fromUtf8(details.mNickname.c_str());
    }
    else groupItemInfo.authorOfLastMsg= "";


#if TOGXS
	if (groupInfo.mGroupFlags & RS_DISTRIB_AUTHEN_REQ) {
		groupItemInfo.name += " (" + tr("AUTHD") + ")";
		groupItemInfo.icon = QIcon(IMAGE_GROUPAUTHD);
	}
	else
#endif
	{
		groupItemInfo.icon = QIcon(icon(ICON_DEFAULT));
	}
}

void UnseenGxsGroupFrameDialog::groupInfoToUnseenGroupItemInfo(const RsGroupMetaData &groupInfo, UnseenGroupItemInfo &groupItemInfo, const RsUserdata */*userdata*/)
{
    groupItemInfo.id = QString::fromStdString(groupInfo.mGroupId.toStdString());
    groupItemInfo.gxsGroupId = groupInfo.mGroupId;
    groupItemInfo.name = QString::fromUtf8(groupInfo.mGroupName.c_str());
    groupItemInfo.popularity = groupInfo.mPop;
    groupItemInfo.lastpost = QDateTime::fromTime_t(groupInfo.mLastPost);
    groupItemInfo.subscribeFlags = groupInfo.mSubscribeFlags;
    groupItemInfo.publishKey = IS_GROUP_PUBLISHER(groupInfo.mSubscribeFlags) ;
    groupItemInfo.adminKey = IS_GROUP_ADMIN(groupInfo.mSubscribeFlags) ;
    groupItemInfo.max_visible_posts = groupInfo.mVisibleMsgCount ;

    //unseenp2p
    groupItemInfo.lastMsgDatetime = groupInfo.mLastPost;
    RsIdentityDetails details ;
    if (rsIdentity->getIdDetails(groupInfo.mAuthorId,details) )
    {
        groupItemInfo.authorOfLastMsg = QString::fromUtf8(details.mNickname.c_str());
    }
    else groupItemInfo.authorOfLastMsg= "";


#if TOGXS
    if (groupInfo.mGroupFlags & RS_DISTRIB_AUTHEN_REQ) {
        groupItemInfo.name += " (" + tr("AUTHD") + ")";
        groupItemInfo.icon = QIcon(IMAGE_GROUPAUTHD);
    }
    else
#endif
    {
        groupItemInfo.icon = QIcon(icon(ICON_DEFAULT));
    }
}

void UnseenGxsGroupFrameDialog::groupInfoToUnseenGroupItemInfo2(const RsGxsChatGroup &groupInfo, UnseenGroupItemInfo &groupItemInfo, const RsUserdata */*userdata*/)
{
    groupItemInfo.id = QString::fromStdString(groupInfo.mMeta.mGroupId.toStdString());
    groupItemInfo.gxsGroupId = groupInfo.mMeta.mGroupId;
    groupItemInfo.name = QString::fromUtf8(groupInfo.mMeta.mGroupName.c_str());
    groupItemInfo.popularity = groupInfo.mMeta.mPop;
    groupItemInfo.lastpost = QDateTime::fromTime_t(groupInfo.mMeta.mLastPost);
    groupItemInfo.subscribeFlags = groupInfo.mMeta.mSubscribeFlags;
    groupItemInfo.publishKey = IS_GROUP_PUBLISHER(groupInfo.mMeta.mSubscribeFlags) ;
    groupItemInfo.adminKey = IS_GROUP_ADMIN(groupInfo.mMeta.mSubscribeFlags) ;
    groupItemInfo.max_visible_posts = groupInfo.mMeta.mVisibleMsgCount ;

    //unseenp2p
    groupItemInfo.localMsgInfo = groupInfo.localMsgInfo;            //add later for last message
    groupItemInfo.lastMsgDatetime = groupInfo.mMeta.mLastPost;
    RsIdentityDetails details ;
    if (rsIdentity->getIdDetails(groupInfo.mMeta.mAuthorId,details) )
    {
        groupItemInfo.authorOfLastMsg = QString::fromUtf8(details.mNickname.c_str());
    }
    else groupItemInfo.authorOfLastMsg= "";

    groupItemInfo.members = groupInfo.members;
    groupItemInfo.type = groupInfo.type;

#if TOGXS
    if (groupInfo.mGroupFlags & RS_DISTRIB_AUTHEN_REQ) {
        groupItemInfo.name += " (" + tr("AUTHD") + ")";
        groupItemInfo.icon = QIcon(IMAGE_GROUPAUTHD);
    }
    else
#endif
    {
        groupItemInfo.icon = QIcon(icon(ICON_DEFAULT));
    }
}

void UnseenGxsGroupFrameDialog::UnseenGroupItemInfoToRsGxsChatGroup( RsGxsChatGroup &groupInfo, const UnseenGroupItemInfo &groupItemInfo)
{
    groupInfo.mMeta.mGroupId = groupItemInfo.gxsGroupId;

//    groupItemInfo.id = QString::fromStdString(groupInfo.mMeta.mGroupId.toStdString());
//    groupItemInfo.gxsGroupId = groupInfo.mMeta.mGroupId;
//    groupItemInfo.name = QString::fromUtf8(groupInfo.mMeta.mGroupName.c_str());
//    groupItemInfo.popularity = groupInfo.mMeta.mPop;
//    groupItemInfo.lastpost = QDateTime::fromTime_t(groupInfo.mMeta.mLastPost);
//    groupItemInfo.subscribeFlags = groupInfo.mMeta.mSubscribeFlags;
//    groupItemInfo.publishKey = IS_GROUP_PUBLISHER(groupInfo.mMeta.mSubscribeFlags) ;
//    groupItemInfo.adminKey = IS_GROUP_ADMIN(groupInfo.mMeta.mSubscribeFlags) ;
//    groupItemInfo.max_visible_posts = groupInfo.mMeta.mVisibleMsgCount ;

//    //unseenp2p
//    groupItemInfo.localMsgInfo = groupInfo.localMsgInfo;            //add later for last message
//    groupItemInfo.lastMsgDatetime = groupInfo.mMeta.mLastPost;
//    RsIdentityDetails details ;
//    if (rsIdentity->getIdDetails(groupInfo.mMeta.mAuthorId,details) )
//    {
//        groupItemInfo.authorOfLastMsg = QString::fromUtf8(details.mNickname.c_str());
//    }
//    else groupItemInfo.authorOfLastMsg= "";

//    groupItemInfo.members = groupInfo.members;
//    groupItemInfo.type = groupInfo.type;

//#if TOGXS
//    if (groupInfo.mGroupFlags & RS_DISTRIB_AUTHEN_REQ) {
//        groupItemInfo.name += " (" + tr("AUTHD") + ")";
//        groupItemInfo.icon = QIcon(IMAGE_GROUPAUTHD);
//    }
//    else
//#endif
//    {
//        groupItemInfo.icon = QIcon(icon(ICON_DEFAULT));
//    }
}

void UnseenGxsGroupFrameDialog::addChatPage(UnseenGxsChatLobbyDialog *d)
{

    if(_unseenGxsGroup_infos.find(d->groupId()) == _unseenGxsGroup_infos.end())
    {
        RsGxsGroupId groupId = d->groupId();
        ChatLobbyId id = d->id();
        ChatLobbyInfo linfo;
        ui->stackedWidget->addWidget(d) ;

        //logic of gxs group chat go here: connect GxsChat signals and slots
        connect(d,SIGNAL(gxsGroupLeave(RsGxsGroupId)),this,SLOT(unsubscribeGxsGroupChat(RsGxsGroupId))) ;
//        connect(d,SIGNAL(typingEventReceived(ChatLobbyId)),this,SLOT(updateTypingStatus(ChatLobbyId))) ;
        connect(d,SIGNAL(gxsMessageReceived(RsGxsChatMsg,bool,gxsChatId,QDateTime,QString,QString)),this,SLOT(updateGxsMessageChanged(RsGxsChatMsg,bool,gxsChatId,QDateTime,QString,QString))) ;
//        connect(d,SIGNAL(peerJoined(ChatLobbyId)),this,SLOT(updatePeerEntering(ChatLobbyId))) ;
//        connect(d,SIGNAL(peerLeft(ChatLobbyId)),this,SLOT(updatePeerLeaving(ChatLobbyId))) ;


        _unseenGxsGroup_infos[groupId].dialog = d ;
        _unseenGxsGroup_infos[groupId].last_typing_event = time(NULL) ;

        //get the selected groupchat or contact
        QModelIndexList list = ui->unseenGroupTreeWidget->selectionModel()->selectedIndexes();
        std::string selectedUId;
        foreach (QModelIndex index, list)
        {
            if (index.row()!=-1)
            {
                selectedUId = allGxsChatGroupList.at(index.row()).mMeta.mGroupId.toStdString();
                break;
            }
        }
        //get the groupchat info
        std::string uId = groupId.toStdString(); // std::to_string(groupId);
        RsGxsChatGroup gxsChatGroup;
        if (!isGroupIdInGxsConversationList(uId))
        {
            uint current_time = QDateTime::currentDateTime().toTime_t();
            std::string groupname = "Unknown name";
            //TODO: need to get the group name from the RsGxsChat
            std::list<RsGxsGroupId> groupChatId;
            groupChatId.push_back(groupId);
            std::vector<RsGxsChatGroup> chatsInfo;
            if (rsGxsChats->getChatsInfo(groupChatId, chatsInfo))
            {
                if (chatsInfo.size() > 0)
                {
                    gxsChatGroup = chatsInfo[0];
                    saveGxsGroupChatInfoToModelData(chatsInfo[0], "", 0, current_time, "", true );

                    //after open new window and add the new conversation item, need to sort and update the layout
                    sortGxsConversationListByRecentTime();
                    emit ui->unseenGroupTreeWidget->model()->layoutChanged();
                }
            }

        }
        //Here need to check whether this groupchat this member create or receive auto-accept group invite
        // If this is this member create so no need to clearSelection and re-select, if not, no need to do these following
        if (IS_GROUP_ADMIN(gxsChatGroup.mMeta.mSubscribeFlags))
        {
            ui->unseenGroupTreeWidget->selectionModel()->clearSelection();
            // need to re-select the conversation item when we have new chat only
            int seletedrow = getIndexFromUId(uId);
            QModelIndex idx = ui->unseenGroupTreeWidget->model()->index(seletedrow, 0);
            ui->unseenGroupTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
            emit ui->unseenGroupTreeWidget->model()->layoutChanged();
        }
        else
        {
            //if receive the auto-accept group-invite, so after sort conversation items, the selection will go wrong,
            // need to save the selected item and re-select again
            ui->unseenGroupTreeWidget->selectionModel()->clearSelection();
            int seletedrow = getIndexFromUId(selectedUId);
            QModelIndex idx = ui->unseenGroupTreeWidget->model()->index(seletedrow, 0);
            ui->unseenGroupTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
            emit ui->unseenGroupTreeWidget->model()->layoutChanged();
        }

    }
}

void UnseenGxsGroupFrameDialog::insertGroupsData2(const std::map<RsGxsGroupId,RsGxsChatGroup> &groupList, const RsUserdata *userdata)
{
    if (!mInitialized) {
        return;
    }

    mInFill = true;
    std::cerr << std::endl;
    allGxsChatGroupList.clear();
    for (auto it = groupList.begin(); it != groupList.end(); ++it)
    {
        /* sort it into Publish (Own), Subscribed, Popular and Other */
        uint32_t flags = it->second.mMeta.mSubscribeFlags;

        RsGxsChatGroup groupChat = it->second;
        bool isAdmin      = IS_GROUP_ADMIN(it->second.mMeta.mSubscribeFlags);
        bool isPublisher  = IS_GROUP_PUBLISHER(it->second.mMeta.mSubscribeFlags);
        bool isSubscribed = IS_GROUP_SUBSCRIBED(it->second.mMeta.mSubscribeFlags);

        if( !isAdmin && isSubscribed)
        {
            std::cerr << " This group is publisher, not admin:" << it->second.mMeta.mGroupName << std::endl;
            //need to create local nodegroupId for all private and one2one chat so that it can sync properly
            if(it->second.type == RsGxsChatGroup::ONE2ONE ||  (it->second.type == RsGxsChatGroup::GROUPCHAT  && it->second.mMeta.mCircleType == GXS_CIRCLE_TYPE_YOUR_FRIENDS_ONLY))
            {
                RsGroupInfo groupInfo2;
                //if we can not find the group name with the mGroupId, so we need to create a local group node with name = mGroupId
                if(!rsPeers->getGroupInfoByName(it->second.mMeta.mGroupId.toStdString(), groupInfo2))
                {
                    RsGroupInfo groupInfo;
                    //groupInfo.id = RsNodeGroupId(it->second.mMeta.mGroupId);
                    groupInfo.id = RsNodeGroupId(it->second.mMeta.mInternalCircle); //.clear(); // RS will generate an ID
                    groupInfo.name = it->second.mMeta.mGroupId.toStdString();
                    groupInfo.type =it->second.type == RsGxsChatGroup::ONE2ONE? RsGroupInfo::ONE2ONE : RsGroupInfo::GROUPCHAT;

                    //create local groupnode and add this group to the groupnode list in the local here:
                    if(!rsPeers->addGroupWithId(groupInfo, true))
                    {
                        std::cerr << " Can not create local group node for this group chat on member side." << std::endl;
                        return;
                    }
                    std::set<RsPgpId> pgpIds;
                    //get all PgpId from the member list of this group
                    for(std::set<GxsChatMember>::iterator it2 = it->second.members.begin(); it2 != it->second.members.end(); ++it2)
                    {
                        RsPeerDetails detail;
                        if (rsPeers->getPeerDetails((*it2).chatPeerId, detail))
                        {
                            pgpIds.insert(detail.gpg_id);
                        }
                    }
                    std::set<RsPgpId>::iterator it;
                    for (it = pgpIds.begin(); it != pgpIds.end(); ++it) {
                        rsPeers->assignPeerToGroup(groupInfo.id, *it, true);
                    }
                }

            }
        }


        //after customizing the RsGxsChatGroup we can to add it into the allGxsChatGroupList
        if (isSubscribed)
        {
             allGxsChatGroupList.push_back(it->second);
        }

    }
    // We can update to MVC GUI here from the all list, need to check whenever will update the GUI,
    // if not it will refresh the list very frequently and work wrong (for ex. clear the unread number)!
    //if(!isRunOnlyOnce)
    {
        updateDisplay(false);
        sortGxsConversationListByRecentTime();
        smartListModel_->setGxsChatGroupList(allGxsChatGroupList);
        emit ui->unseenGroupTreeWidget->model()->layoutChanged();
        isRunOnlyOnce= true;
    }
#ifdef DEBUG_GROUPFRAMEDIALOG
    std::cerr << " Show all Gxs Group Chat : " << std::endl;
    for (std::vector<UnseenGroupItemInfo>::iterator it2 = allGxsChatGroupList.begin(); it2!= allGxsChatGroupList.end(); ++it2)
    {
        std::cerr << " GxsChat Id: " << (*it2).id.toStdString() << " : " << (*it2).name.toStdString() << std::endl;
        std::set<GxsChatMember> list = (*it2).members;
        std::cerr << " Member: " << std::endl;
        for(std::set<GxsChatMember>::iterator it3=list.begin(); it3!= list.end(); ++it3)
        {
            std::cerr << (*it3).nickname << ", " << std::endl;;
        }
    }
#endif
    //update the groupname and member list if the groupchat already opened: need to check the changes ?!!!
    for (auto it = allGxsChatGroupList.begin(); it != allGxsChatGroupList.end(); ++it)
    {
       //need to check the changes ?!!!
       if(_unseenGxsGroup_infos.find((*it).mMeta.mGroupId) != _unseenGxsGroup_infos.end())
        {
           std::cerr << " There is change in the group: " << (*it).mMeta.mGroupName << std::endl;
           _unseenGxsGroup_infos[(*it).mMeta.mGroupId].dialog->updateTitle(QString::fromStdString((*it).mMeta.mGroupName));
           _unseenGxsGroup_infos[(*it).mMeta.mGroupId].dialog->updateParticipantsList();
           bool isPublisher  = IS_GROUP_PUBLISHER((*it).mMeta.mSubscribeFlags);
           if( isPublisher && (*it).type == RsGxsChatGroup::CHANNEL)
           {
                _unseenGxsGroup_infos[(*it).mMeta.mGroupId].dialog->showChatTextInbox(true);
           }
       }

    }

}

void UnseenGxsGroupFrameDialog::updateMessageSummaryList(RsGxsGroupId groupId)
{
    if (!mInitialized) {
        return;
    }

    if (groupId.isNull())
    {
        //maybe we can scan all gxschat list here?
        //requestGroupStatistics(groupId);

    } else {
        requestGroupStatistics(groupId);
    }
}

/*********************** **** **** **** ***********************/
/** Request / Response of Data ********************************/
/*********************** **** **** **** ***********************/

void UnseenGxsGroupFrameDialog::requestLastMsgOfGroup(RsGxsGroupId groupId, const std::set<RsGxsMessageId> &msgIds)
{
    mStateHelper->setLoading(TOKEN_TYPE_LAST_MSG_OF_GROUP, true);

#ifdef DEBUG_GROUPFRAMEDIALOG
    std::cerr << "UnseenGxsGroupFrameDialog::requestLastMsgOfGroup()";
    std::cerr << std::endl;
#endif

    //mTokenQueue->cancelActiveRequestTokens(TOKEN_TYPE_LAST_MSG_OF_GROUP);

    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_MSG_DATA;

    uint32_t token;
    GxsMsgReq requestMsgIds;
    requestMsgIds[groupId] = msgIds;
    //mTokenQueue->requestMsgInfo(token, RS_TOKREQ_ANSTYPE_DATA, opts, requestMsgIds, mTokenTypePosts);

    //uint32_t token;
    mTokenQueue->requestMsgInfo(token, RS_TOKREQ_ANSTYPE_DATA, opts, requestMsgIds, TOKEN_TYPE_LAST_MSG_OF_GROUP);
}

void UnseenGxsGroupFrameDialog::loadPosts(const uint32_t &token)
{
    std::vector<RsGxsChatMsg> msgs;
    rsGxsChats->getPostData(token, msgs);
    long long mPublishTs = QDateTime::currentSecsSinceEpoch();
    for(auto chatMsg = msgs.begin(); chatMsg != msgs.end(); ++chatMsg)
    {
         updateRecentTimeAndUnreadNumber((*chatMsg).mMeta.mGroupId, *chatMsg, "", mPublishTs, (*chatMsg).mMsg, true, 1, false  );
    }

    sortGxsConversationListByRecentTime();
    smartListModel_->setGxsChatGroupList(allGxsChatGroupList);
    emit ui->unseenGroupTreeWidget->model()->layoutChanged();
}

void UnseenGxsGroupFrameDialog::requestGroupSummary()
{
	mStateHelper->setLoading(TOKEN_TYPE_GROUP_SUMMARY, true);

#ifdef DEBUG_GROUPFRAMEDIALOG
    std::cerr << "UnseenGxsGroupFrameDialog::requestGroupSummary()";
	std::cerr << std::endl;
#endif

	mTokenQueue->cancelActiveRequestTokens(TOKEN_TYPE_GROUP_SUMMARY);

	RsTokReqOptions opts;
	opts.mReqType = requestGroupSummaryType();

	uint32_t token;
	mTokenQueue->requestGroupInfo(token, RS_TOKREQ_ANSTYPE_SUMMARY, opts, TOKEN_TYPE_GROUP_SUMMARY);
}

void UnseenGxsGroupFrameDialog::loadGroupSummaryToken(const uint32_t &token, std::list<RsGroupMetaData> &groupInfo, RsUserdata *&/*userdata*/)
{
	/* Default implementation for request type GXS_REQUEST_TYPE_GROUP_META */
	mInterface->getGroupSummary(token, groupInfo);
}
//unseenp2p
void UnseenGxsGroupFrameDialog::loadGroupSummaryToken2(const uint32_t &token, std::list<RsGxsChatGroup> &groupInfo, RsUserdata *&/*userdata*/)
{
    /* Default implementation for request type GXS_REQUEST_TYPE_GROUP_META */
    std::list<RsGroupMetaData> groupMetaInfo;
    for( std::list<RsGxsChatGroup>::iterator it=groupInfo.begin(); it!= groupInfo.end(); ++it)
        groupMetaInfo.push_back((*it).mMeta);
    mInterface->getGroupSummary(token, groupMetaInfo);
}

void UnseenGxsGroupFrameDialog::loadGroupSummary(const uint32_t &token)
{
#ifdef DEBUG_GROUPFRAMEDIALOG
    std::cerr << "UnseenGxsGroupFrameDialog::loadGroupSummary()";
	std::cerr << std::endl;
#endif

    //std::list<RsGroupMetaData> groupInfo;
    std::list<RsGxsChatGroup> groupInfo2;

	RsUserdata *userdata = NULL;
    //loadGroupSummaryToken(token, groupInfo, userdata);
    loadGroupSummaryToken2(token, groupInfo2, userdata);

    //here we can sync the std::vector<RsGxsChatGroup> allGxsChatGroupList with this groupInfo2?
    //check if the new info in the groupInfo2, if the


	mCachedGroupMetas.clear();
    mCachedChatGroupData.clear();
    //for(auto it(groupInfo.begin());it!=groupInfo.end();++it)
    //  mCachedGroupMetas[(*it).mGroupId] = *it;
    for(auto it(groupInfo2.begin());it!=groupInfo2.end();++it)
        mCachedChatGroupData[(*it).mMeta.mGroupId] = *it;


    //insertGroupsData(mCachedGroupMetas, userdata);
    insertGroupsData2(mCachedChatGroupData, userdata);
    updateSearchResults();

	mStateHelper->setLoading(TOKEN_TYPE_GROUP_SUMMARY, false);

	if (userdata) {
		delete(userdata);
	}

	if (!mNavigatePendingGroupId.isNull()) {
		/* Navigate pending */
		navigate(mNavigatePendingGroupId, mNavigatePendingMsgId);

		mNavigatePendingGroupId.clear();
		mNavigatePendingMsgId.clear();
	}
}

/*********************** **** **** **** ***********************/
/*********************** **** **** **** ***********************/

//void UnseenGxsGroupFrameDialog::acknowledgeSubscribeChange(const uint32_t &token)
//{
//#ifdef DEBUG_GROUPFRAMEDIALOG
//	std::cerr << "UnseenGxsGroupFrameDialog::acknowledgeSubscribeChange()";
//	std::cerr << std::endl;
//#endif

//	RsGxsGroupId groupId;
//	mInterface->acknowledgeGrp(token, groupId);

//	fillComplete();
//}

/*********************** **** **** **** ***********************/
/*********************** **** **** **** ***********************/

//void UnseenGxsGroupFrameDialog::requestGroupSummary_CurrentGroup(const RsGxsGroupId &groupId)
//{
//	RsTokReqOptions opts;
//	opts.mReqType = GXS_REQUEST_TYPE_GROUP_META;

//	std::list<std::string> grpIds;
//	grpIds.push_back(groupId);

//	std::cerr << "UnseenGxsGroupFrameDialog::requestGroupSummary_CurrentGroup(" << groupId << ")";
//	std::cerr << std::endl;

//	uint32_t token;
//	mInteface->requestGroupInfo(token, RS_TOKREQ_ANSTYPE_SUMMARY, opts, grpIds, TOKEN_TYPE_CURRENTGROUP);
//}

//void UnseenGxsGroupFrameDialog::loadGroupSummary_CurrentGroup(const uint32_t &token)
//{
//	std::cerr << "UnseenGxsGroupFrameDialog::loadGroupSummary_CurrentGroup()";
//	std::cerr << std::endl;

//	std::list<RsGroupMetaData> groupInfo;
//	rsGxsForums->getGroupSummary(token, groupInfo);

//	if (groupInfo.size() == 1)
//	{
//		RsGroupMetaData fi = groupInfo.front();
//		mSubscribeFlags = fi.mSubscribeFlags;
//	}
//	else
//	{
//		resetData();
//		std::cerr << "UnseenGxsGroupFrameDialog::loadGroupSummary_CurrentGroup() ERROR Invalid Number of Groups...";
//		std::cerr << std::endl;
//	}

//	setValid(true);
//}

/*********************** **** **** **** ***********************/
/*********************** **** **** **** ***********************/

void UnseenGxsGroupFrameDialog::requestGroupStatistics(const RsGxsGroupId &groupId)
{
	uint32_t token;
	mTokenService->requestGroupStatistic(token, groupId);
	mTokenQueue->queueRequest(token, 0, RS_TOKREQ_ANSTYPE_ACK, TOKEN_TYPE_STATISTICS);
}

void UnseenGxsGroupFrameDialog::loadGroupStatistics(const uint32_t &token)
{
	GxsGroupStatistic stats;
	mInterface->getGroupStatistic(token, stats);

    //UnseenGroupItemInfo item = groupItemIdAt(QString::fromStdString(stats.mGrpId.toStdString()));
    RsGxsChatGroup groupItem = gxsGroupItemIdAt(stats.mGrpId);
    if (groupItem.mMeta.mGroupId.toStdString().length() == 0) {
		return;
	}

    //Need to update the unread number here on the gxschat list, and the last message?


    //ui->unseenGroupTreeWidget->setUnreadCount(item, mCountChildMsgs ? (stats.mNumThreadMsgsUnread + stats.mNumChildMsgsUnread) : stats.mNumThreadMsgsUnread);
}

/*********************** **** **** **** ***********************/
/*********************** **** **** **** ***********************/

void UnseenGxsGroupFrameDialog::loadRequest(const TokenQueue *queue, const TokenRequest &req)
{
#ifdef DEBUG_GROUPFRAMEDIALOG
    std::cerr << "UnseenGxsGroupFrameDialog::loadRequest() UserType: " << req.mUserType;
	std::cerr << std::endl;
#endif

	if (queue == mTokenQueue)
	{
		/* now switch on req */
		switch(req.mUserType)
		{
		case TOKEN_TYPE_GROUP_SUMMARY:
			loadGroupSummary(req.mToken);
			break;

//		case TOKEN_TYPE_SUBSCRIBE_CHANGE:
//			acknowledgeSubscribeChange(req.mToken);
//			break;

//		case TOKEN_TYPE_CURRENTGROUP:
//			loadGroupSummary_CurrentGroup(req.mToken);
//			break;

		case TOKEN_TYPE_STATISTICS:
			loadGroupStatistics(req.mToken);
			break;
        case TOKEN_TYPE_LAST_MSG_OF_GROUP:
            loadPosts(req.mToken);
            break;
        case GXSGROUP_NEWGROUPID_2:
            std::cerr << "UnseenGxsGroupFrameDialog::createGroup ok!!! () UserType: " << req.mUserType;
            break;

		default:
            std::cerr << "UnseenGxsGroupFrameDialog::loadRequest() ERROR: INVALID TYPE";
			std::cerr << std::endl;
		}
	}
}

//TurtleRequestId UnseenGxsGroupFrameDialog::distantSearch(const QString& search_string)   // this should be overloaded in the child class
//{
//    std::cerr << "Searching for \"" << search_string.toStdString() << "\". Function is not overloaded, so nothing will happen." << std::endl;
//    return 0;
//}

void UnseenGxsGroupFrameDialog::searchNetwork(const QString& search_string)
{
    if(search_string.isNull())
        return ;

    //uint32_t request_id = distantSearch(search_string);

    //if(request_id == 0)
        return ;

   // mSearchGroupsItems[request_id] = ui->unseenGroupTreeWidget->addSearchItem(tr("Search for")+ " \"" + search_string + "\"",(uint32_t)request_id,QIcon(icon(ICON_SEARCH)));
}

void UnseenGxsGroupFrameDialog::distantRequestGroupData()
{
    QAction *action = dynamic_cast<QAction*>(sender()) ;

    if(!action)
        return ;

    RsGxsGroupId group_id(action->data().toString().toStdString());

    if(group_id.isNull())
    {
        std::cerr << "Cannot retrieve group! Group id is null!" << std::endl;
    }

	std::cerr << "Explicit request for group " << group_id << std::endl;
    checkRequestGroup(group_id) ;
}

void UnseenGxsGroupFrameDialog::setCurrentChatPage(UnseenGxsChatLobbyDialog *d)
{
    ui->stackedWidget->setCurrentWidget(d) ;

    if (d) {
        //re-select the left conversation item
        std::string uId = std::to_string(d->chatId().toLobbyId());
        //int seletedrow = rsMsgs->getIndexFromUId(uId);
//        if (seletedrow >= 0)
//        {
//            ui->unseenGroupTreeWidget->selectionModel()->clearSelection();
//            emit ui->unseenGroupTreeWidget->model()->layoutChanged();

//            //update again after clear seletion
//            QModelIndex idx = ui->unseenGroupTreeWidget->model()->index(seletedrow, 0);
//            ui->unseenGroupTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
//            emit ui->unseenGroupTreeWidget->model()->layoutChanged();
//        }
    }
}

void UnseenGxsGroupFrameDialog::smartListSelectionChanged(const QItemSelection  &selected, const QItemSelection  &deselected)
{
    Q_UNUSED(deselected);
    QModelIndexList indices = selected.indexes();

    if (indices.isEmpty()) {
        return;
    }

    auto selectedIndex = indices.at(0);

    if (not selectedIndex.isValid()) {
        return;
    }

    selectConversation(selectedIndex);
}

void UnseenGxsGroupFrameDialog::selectConversation(const QModelIndex& index)
{

    //How to get the ConversationModel and get the index of it
    if (!index.isValid()) return;

    //uint32_t conversationMode = getConversationListMode();
    //std::vector<UnseenGroupItemInfo> list = smartListModel_->getGxsGroupList();
    std::vector<RsGxsChatGroup> gxslist = smartListModel_->getGxsChatGroupList();

    //if (list.size() == 0 || index.row() >= static_cast<int>(list.size())) return;
    if (gxslist.size() == 0 || index.row() >= static_cast<int>(gxslist.size())) return;

    //UnseenGroupItemInfo gxsGroupItem = list.at(index.row());

    RsGxsChatGroup gxsChatGroupItem = gxslist.at(index.row());

    //std::cerr << " gxsGroupItem info, name : " << gxsGroupItem.name.toStdString() << std::endl;

    //mGroupId = RsGxsGroupId(gxsGroupItem.id.toStdString());

    mGroupId = gxsChatGroupItem.mMeta.mGroupId; // RsGxsGroupId(gxsGroupItem.id.toStdString());

    gxsChatId gxschatId(mGroupId);
    gxschatId.gxsChatType = gxsChatGroupItem.type;

    showGxsGroupChatMVC(gxschatId);

    //check if this is the filtered search mode, just return to normal mode
    if (getConversationListMode() == GXSCONVERSATION_MODE_WITH_SEARCH_FILTER)
    {
        if (!ui->filterLineEdit->text().isEmpty())
        {
            ui->filterLineEdit->setText("");
        }
    }

    //Need to check the unread msg, if this item has unread number, so call these functions
    //if (gxsGroupItem.UnreadMessagesCount > 0)
    if (gxsChatGroupItem.localMsgInfo.unreadMsgIds.size() > 0)
    {
        //reset the unread number as 0 when user click on the conversation item on the left side

        updateRecentTimeAndUnreadNumber(gxsChatGroupItem.mMeta.mGroupId, RsGxsChatMsg(), "", 0, gxsChatGroupItem.localMsgInfo.msg, false, 1, true);

        smartListModel_->setFilterGxsChatGroupListAndMode(allGxsChatGroupList, GXSCONVERSATION_MODE_WITHOUT_FILTER);

        //to mark it as read
        std::set<RsGxsMessageId>::iterator lastId = gxsChatGroupItem.localMsgInfo.unreadMsgIds.end();
        uint32_t token;
        RsGxsGrpMsgIdPair msgPair = std::make_pair(gxsChatGroupItem.mMeta.mGroupId, *lastId);

        rsGxsChats->setMessageReadStatus(token, msgPair, gxsChatGroupItem.localMsgInfo.msg , true);

    }

    emit ui->unseenGroupTreeWidget->model()->layoutChanged();
    //focus on the input box:


}

QString UnseenGxsGroupFrameDialog::itemIdAt(QPoint &point)
{
    QModelIndex itemIndex = ui->unseenGroupTreeWidget->indexAt(point);

    std::vector<UnseenGroupItemInfo> iteminfoList = smartListModel_->getGxsGroupList();

    if (itemIndex.row() < iteminfoList.size())
    {
        UnseenGroupItemInfo itemInfo = iteminfoList.at(itemIndex.row());
        return  itemInfo.id;
    }
    else return "";

}

UnseenGroupItemInfo UnseenGxsGroupFrameDialog::groupItemIdAt(QPoint &point)
{
    QModelIndex itemIndex = ui->unseenGroupTreeWidget->indexAt(point);

    std::vector<UnseenGroupItemInfo> iteminfoList = smartListModel_->getGxsGroupList();

    if (itemIndex.row() < iteminfoList.size())
    {
        return iteminfoList.at(itemIndex.row());
    }
    else return UnseenGroupItemInfo();

}

RsGxsChatGroup UnseenGxsGroupFrameDialog::gxsGroupItemIdAt(QPoint &point)
{
    QModelIndex itemIndex = ui->unseenGroupTreeWidget->indexAt(point);

    std::vector<RsGxsChatGroup> iteminfoList = smartListModel_->getGxsChatGroupList();

    if (itemIndex.row() < iteminfoList.size())
    {
        return iteminfoList.at(itemIndex.row());
    }
    else return RsGxsChatGroup();

}

UnseenGroupItemInfo UnseenGxsGroupFrameDialog::groupItemIdAt(QString groupId)
{
    std::vector<UnseenGroupItemInfo> iteminfoList = smartListModel_->getGxsGroupList();

    for (std::vector<UnseenGroupItemInfo>::iterator it = iteminfoList.begin(); it != iteminfoList.end(); ++it)
    {
        if (it->id == groupId)
        {
            return *it;
        }
    }
    return UnseenGroupItemInfo();
}

RsGxsChatGroup UnseenGxsGroupFrameDialog::gxsGroupItemIdAt(RsGxsGroupId groupId)
{
    std::vector<RsGxsChatGroup> iteminfoList = smartListModel_->getGxsChatGroupList();

    for (std::vector<RsGxsChatGroup>::iterator it = iteminfoList.begin(); it != iteminfoList.end(); ++it)
    {
        if (it->mMeta.mGroupId == groupId)
        {
            return *it;
        }
    }
    return RsGxsChatGroup();
}

RsGxsChatGroup UnseenGxsGroupFrameDialog::gxsGroupFromList(RsGxsGroupId groupId)
{
    for (std::vector<RsGxsChatGroup>::iterator it = allGxsChatGroupList.begin(); it != allGxsChatGroupList.end(); ++it)
    {
        if (it->mMeta.mGroupId == groupId)
        {
            return *it;
        }
    }

    return RsGxsChatGroup();
}


void UnseenGxsGroupFrameDialog::sortGxsConversationListByRecentTime()
{
    std::sort(allGxsChatGroupList.begin(), allGxsChatGroupList.end(),
              [] (RsGxsChatGroup const& a, RsGxsChatGroup const& b)
    { return a.localMsgInfo.update_ts > b.localMsgInfo.update_ts; });

}

void UnseenGxsGroupFrameDialog::showGxsGroupChatMVC(gxsChatId chatId)
{

    //ChatLobbyId lobbyId = QString::fromStdString(chatIdStr).toULongLong();
    RsGxsGroupId groupId = chatId.toGxsGroupId();
    if(_unseenGxsGroup_infos.find(groupId) == _unseenGxsGroup_infos.end())
    {
        ChatDialog::chatFriend(chatId,true) ;
    }
    else
    {
        ui->stackedWidget->setCurrentWidget(_unseenGxsGroup_infos[groupId].dialog) ;
    }

    //unseenp2p
    if (UnseenGxsChatLobbyDialog *cld = dynamic_cast<UnseenGxsChatLobbyDialog*>(ChatDialog::getExistingChat(chatId))) {
        cld->updateParticipantsList();
    }


}

std::string UnseenGxsGroupFrameDialog::getSelectedId()
{
    QModelIndexList list = ui->unseenGroupTreeWidget->selectionModel()->selectedIndexes();
    std::string selectedUId = "";
    foreach (QModelIndex index, list)
    {
        if (index.row()!=-1)
        {
            if (getConversationListMode() == GXSCONVERSATION_MODE_WITH_SEARCH_FILTER)
            {
                 if(index.row() < static_cast<int>(filteredGxsChatGroupList.size()))
                     selectedUId = filteredGxsChatGroupList.at(index.row()).mMeta.mGroupId.toStdString();
            }
            else if (conversationListMode == GXSCONVERSATION_MODE_WITHOUT_FILTER)
            {
                if(index.row() < static_cast<int>(allGxsChatGroupList.size()))
                     selectedUId = allGxsChatGroupList.at(index.row()).mMeta.mGroupId.toStdString();

            }

            break;
        }
    }
    return selectedUId;
}

//update recent time for every chat item and sort by recent time WHEN SENDING MSG!
void UnseenGxsGroupFrameDialog::updateRecentTime(const gxsChatId & gxschatId, const RsGxsChatMsg& gxsChatMsg, std::string nickInGroupChat, long long current_time, std::string textmsg, bool isSend)
{

       //Need to get the selected item with saving uId before sorting and updating the layout
       //so that we use uId to re-select the item with saved uId

        // Even when user in search mode, we still can get the right selectedUId
       std::string selectedUId = getSelectedId();

       //update both new last msg and last msg datetime to the conversation list
        updateRecentTimeAndUnreadNumber(gxschatId.toGxsGroupId(), gxsChatMsg, nickInGroupChat, current_time, textmsg, !isSend, 0, false );

        // if this is we send the msg, just sort the list and select the first one after sorting
        sortGxsConversationListByRecentTime();
        smartListModel_->setGxsChatGroupList(allGxsChatGroupList);
        emit ui->unseenGroupTreeWidget->model()->layoutChanged();

        //check if this is the filtered search mode, just return to normal mode
        if (getConversationListMode() == GXSCONVERSATION_MODE_WITH_SEARCH_FILTER)
        {
            if (!ui->filterLineEdit->text().isEmpty())
            {
                ui->filterLineEdit->setText("");
            }
        }

        //re-select the chat item again, the old selected one was saved to selectedIndex
        // at first find the index of the uId, then re-select
        int seletedrow = 0; //getIndexFromUId(selectedUId);
        QModelIndex idx = ui->unseenGroupTreeWidget->model()->index(seletedrow, 0);
        if (idx.isValid())
        {
            // For both case of sending or receiving, just clear the selection and re-select chat item
            ui->unseenGroupTreeWidget->selectionModel()->clearSelection();

            ui->unseenGroupTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
            emit ui->unseenGroupTreeWidget->model()->layoutChanged();

        }

}

void UnseenGxsGroupFrameDialog::updateNewGxsMsg(const gxsChatId & gxsChatId, const RsGxsChatMsg &gxsChatMsg, std::string nickInGroupChat, long long current_time, std::string textmsg, bool isSend)
{
    //THe same for the updateNewGxsMsg:

    //Need to get the selected item with saving uId before sorting and updating the layout
    //so that we use uId to re-select the item with saved uId

     // Even when user in search mode, we still can get the right selectedUId
    std::string selectedUId = getSelectedId();

     //receive new msg, need to update unread msg to model data, increase 1
     updateRecentTimeAndUnreadNumber(gxsChatId.toGxsGroupId(), gxsChatMsg, nickInGroupChat, current_time,textmsg, true, 1, false);

     //after update the unread msg, need to sort the list and update the unread notification on the item
     sortGxsConversationListByRecentTime();

     smartListModel_->setGxsChatGroupList(allGxsChatGroupList);
     //reselect the seletion (because the list already sorted, the selection is still keeping the index
     ui->unseenGroupTreeWidget->selectionModel()->clearSelection();

     //re-select the chat item again, the old selected one was saved to selectedIndex
     // at first find the index of the uId, then re-select

     int seletedrow = getIndexFromUId(selectedUId);
     if (seletedrow >= 0)
     {
         QModelIndex idx = ui->unseenGroupTreeWidget->model()->index(seletedrow, 0);
         ui->unseenGroupTreeWidget->selectionModel()->select(idx, QItemSelectionModel::Select);
         emit ui->unseenGroupTreeWidget->model()->layoutChanged();
     }

     //check if this is a current chat window, so update new msg as read
     if (gxsChatId.toGxsGroupId().toStdString() == selectedUId)
     {
         updateRecentTimeAndUnreadNumber(gxsChatId.toGxsGroupId(), RsGxsChatMsg(), nickInGroupChat, current_time, textmsg, true, 1, true);

         //after update the unread msg, need to sort the list and update the unread notification on the item
         smartListModel_->setGxsChatGroupList(allGxsChatGroupList);
         emit ui->unseenGroupTreeWidget->model()->layoutChanged();
         //to mark it as read
         uint32_t token;
         RsGxsGrpMsgIdPair msgPair = std::make_pair(gxsChatId.toGxsGroupId(),gxsChatMsg.mMeta.mMsgId);
         rsGxsChats->setMessageReadStatus(token, msgPair, textmsg, true);

     }
}

void UnseenGxsGroupFrameDialog::unsubscribeGxsGroupChat(RsGxsGroupId id)
{

    // close the tab.
    std::map<RsGxsGroupId,UnseenGxsChatLobbyInfoStruct>::iterator it = _unseenGxsGroup_infos.find(id) ;

    if(it != _unseenGxsGroup_infos.end())
    {
//        if (myGxsChatUserNotify){
//            myGxsChatUserNotify->gxsChatCleared(gxsChatId(id), "");
//        }

        ui->stackedWidget->removeWidget(it->second.dialog) ;
        _unseenGxsGroup_infos.erase(it) ;
    }
    //remove item from conversations list, using the MVC now
    ChatDialog::closeChat(gxsChatId(id));

    uint32_t token;
    mInterface->subscribeToGroup(token, id, false);

    //leave and delete private group/one2one: remove the local group node with the name = groupId
    RsGroupInfo groupInfo;
    if(rsPeers->getGroupInfoByName(id.toStdString(), groupInfo))
    {
        rsPeers->removeGroup(groupInfo.id);
    }

    removeGxsChatGroup(id);
    smartListModel_->setGxsChatGroupList(allGxsChatGroupList);
    emit ui->unseenGroupTreeWidget->model()->layoutChanged();

}

//update recent time for every chat item and sort by recent time WHEN RECEIVING MSG!
void UnseenGxsGroupFrameDialog::updateGxsMessageChanged(RsGxsChatMsg gxsChatMsg, bool incoming, RsGxsGroupId id, QDateTime time, QString senderName, QString msg)
{

    if (myGxsChatUserNotify)
    {
        if (incoming)
        {
            myGxsChatUserNotify->gxsChatNewMessage(gxsChatMsg, gxsChatId(id), time, senderName, msg);
        }
        //myGxsChatUserNotify->chatLobbyNewMessage(id, time, senderName, msg);
    }
}

UserNotify *UnseenGxsGroupFrameDialog::getUserNotify(QObject *parent)
{
    if (!myGxsChatUserNotify)
    {
        myGxsChatUserNotify = new GxsChatUserNotify(rsGxsChats, parent);
        connect(myGxsChatUserNotify, SIGNAL(countChanged(RsGxsChatMsg, gxsChatId, unsigned int)), this, SLOT(updateGxsMsgNotify(RsGxsChatMsg, gxsChatId, unsigned int)));
    }
    return myGxsChatUserNotify;
}


void UnseenGxsGroupFrameDialog::updateGxsMsgNotify(RsGxsChatMsg gxsChatMsg, gxsChatId id, unsigned int count)
{
    UnseenGxsChatLobbyDialog *dialog=NULL;
    dialog=_unseenGxsGroup_infos[id.toGxsGroupId()].dialog;
    if(!dialog) return;

    QToolButton* notifyButton=dialog->getChatWidget()->getNotifyButton();
    if (!notifyButton) return;
    dialog->getChatWidget()->setGxsNotify(myGxsChatUserNotify);
    if (count>0){
        notifyButton->setVisible(true);
        //notifyButton->setIcon(_lobby_infos[id].default_icon);
        notifyButton->setToolTip(QString("(%1)").arg(count));
    } else {
        notifyButton->setVisible(false);
        RsGxsGrpMsgIdPair msgPair = std::make_pair(id.toGxsGroupId(), gxsChatMsg.mMeta.mMsgId);
        uint32_t token;
        rsGxsChats->setMessageReadStatus(token, msgPair, gxsChatMsg.mMsg, true);
    }
}


bool UnseenGxsGroupFrameDialog::isGroupIdInGxsConversationList(std::string uId)
{
    bool foundUIdInList = false;
    for (unsigned int i = 0; i < allGxsChatGroupList.size(); i++ )
    {
        if (allGxsChatGroupList[i].mMeta.mGroupId.toStdString() == uId)
        {
            foundUIdInList = true;
            break;
        }
    }
    return foundUIdInList;
}

void UnseenGxsGroupFrameDialog::saveGxsGroupChatInfoToModelData(const RsGxsChatGroup gxsGroupInfo, std::string nickInGroupChat, unsigned int UnreadMessagesCount, unsigned int lastMsgDatetime, std::string lastMessage, bool isOtherLastMsg )
{
    allGxsChatGroupList.push_back(gxsGroupInfo);

    smartListModel_->setGxsChatGroupList(allGxsChatGroupList);
    emit ui->unseenGroupTreeWidget->model()->layoutChanged();

}

int UnseenGxsGroupFrameDialog::getIndexFromUId(std::string uId)
{
    int index = -1;
    for (unsigned int i = 0; i < allGxsChatGroupList.size(); i++ )
    {
        if (allGxsChatGroupList[i].mMeta.mGroupId.toStdString() == uId)
        {
            index = static_cast<int>(i);
            break;
        }
    }
    return index;
}

void UnseenGxsGroupFrameDialog::updateRecentTimeAndUnreadNumber(const RsGxsGroupId &groupId, const RsGxsChatMsg& gxsChatMsg, std::string nickInGroupChat, long long lastMsgDatetime, std::string textmsg, bool isOtherMsg, unsigned int unreadNumber, bool isReset)
{
    bool isRead = true;
    if (isReset)
    {
    }
    else
    {
        if( isOtherMsg)     //when receive new msg, need to insert into the unreadMsgIds
        {
            isRead = false;
        }
    }

    uint32_t token;
    RsGxsGrpMsgIdPair msgPair = std::make_pair(groupId,gxsChatMsg.mMeta.mMsgId);
    if(!isOtherMsg)
    {
        //this is when we send chat
        rsGxsChats->setMessageReadStatus(token, msgPair, textmsg, false);

    }
    uint32_t token2;
    rsGxsChats->setLocalMessageStatus(msgPair, textmsg);
    rsGxsChats->setMessageReadStatus(token2, msgPair, textmsg, isRead);

    //still need to update in the allGxsChatGroupList for GUI, sorting,... for example
    for (unsigned int i = 0; i < allGxsChatGroupList.size(); i++ )
    {
        if (allGxsChatGroupList[i].mMeta.mGroupId == groupId)
        {
            //isReset = true, that mean clear all read number (clear unreadMsgIds), set as already read, there is no update last msg, last date
            //only isReset= true: when user click on the groupItem with unread number > 0, else: isReset = false
            // isReset = false, we have 2 option: + when user sendMsg: isOtherMsg = false,
            //                                    + when user receive Msg: isOtherMsg = true
            bool isRead = true;
            if (isReset)
            {
                allGxsChatGroupList[i].localMsgInfo.unreadMsgIds.clear();
            }
            else
            {
                if( isOtherMsg)     //when receive new msg, need to insert into the unreadMsgIds
                {
                    allGxsChatGroupList[i].localMsgInfo.unreadMsgIds.insert(gxsChatMsg.mMeta.mMsgId);
                }

                allGxsChatGroupList[i].localMsgInfo.update_ts = lastMsgDatetime;
                allGxsChatGroupList[i].localMsgInfo.msg = textmsg;

            }
            break;
        }
    }
}

void UnseenGxsGroupFrameDialog::setConversationListMode(uint32_t mode)
{
    conversationListMode = mode;
    if (conversationListMode == GXSCONVERSATION_MODE_WITHOUT_FILTER)
    {
        smartListModel_->setFilterGxsChatGroupListAndMode(allGxsChatGroupList, conversationListMode);
    }
    else if (conversationListMode == GXSCONVERSATION_MODE_WITH_SEARCH_FILTER)
        smartListModel_->setFilterGxsChatGroupListAndMode(filteredGxsChatGroupList, conversationListMode);
}

uint32_t UnseenGxsGroupFrameDialog::getConversationListMode()
{
    return conversationListMode;
}

std::vector<RsGxsChatGroup> UnseenGxsGroupFrameDialog::getSearchFilteredGxsChatGroupList()
{
     return filteredGxsChatGroupList;
}

void UnseenGxsGroupFrameDialog::setSearchFilter(const std::string &filtertext)
{
    filter_text = filtertext;

    filteredGxsChatGroupList.clear();
    for (const RsGxsChatGroup& item : allGxsChatGroupList)
    {
        std::string nameforsearch = item.mMeta.mGroupName;

        std::transform(nameforsearch.begin(), nameforsearch.end(), nameforsearch.begin(),
            [](unsigned char c){ return std::tolower(c); });
        std::string filtertext2 = filtertext;
        std::transform(filtertext2.begin(), filtertext2.end(), filtertext2.begin(),
            [](unsigned char c){ return std::tolower(c); });

        if (std::size_t found = nameforsearch.find(filtertext2) !=std::string::npos)
        {
            filteredGxsChatGroupList.push_back(item);
        }
   }

}

void UnseenGxsGroupFrameDialog::filterColumnChanged(int)
{
    filterGxsItems(ui->filterLineEdit->text());
}

void UnseenGxsGroupFrameDialog::filterGxsItems(const QString &text)
{
    if (text.isEmpty())
    {
        setConversationListMode(GXSCONVERSATION_MODE_WITHOUT_FILTER);
    }
    else
    {
        std::string filter = text.toStdString();
        setSearchFilter(filter);
        setConversationListMode(GXSCONVERSATION_MODE_WITH_SEARCH_FILTER);
        //after filtering the list, need to clear the selection so that user can choose one (if there is only one)
        ui->unseenGroupTreeWidget->selectionModel()->clearSelection();
    }
    emit ui->unseenGroupTreeWidget->model()->layoutChanged();
}

void UnseenGxsGroupFrameDialog::removeGxsChatGroup(RsGxsGroupId groupId)
{
    std::vector<RsGxsChatGroup>::iterator it2;

    for(it2 = allGxsChatGroupList.begin(); it2 != allGxsChatGroupList.end();)
    {

       if((*it2).mMeta.mGroupId == groupId)
       {
          it2 = allGxsChatGroupList.erase(it2);
          break;
       }
       else
       {
          ++it2;
       }
    }

}

void UnseenGxsGroupFrameDialog::receiveNotifyCreateNewGxsGroup(const QString groupId)
{
    RsGxsGroupId groupGxsId = RsGxsGroupId(groupId.toStdString());

    RsGxsChatGroup gxsChatGroup = getGxsChatGroup(groupGxsId);

    gxsChatId gxs_ChatId(groupGxsId);
    gxs_ChatId.gxsChatType = gxsChatGroup.type;
    //only open the gxsgroup chat when this user is admin! (open when this user create the group):
    if(IS_GROUP_ADMIN(gxsChatGroup.mMeta.mSubscribeFlags))
    {
        ChatDialog::chatFriend(gxsChatId(groupGxsId), true);
        //showGxsGroupChatMVC(gxsChatId(groupGxsId));
    }

//        ChatDialog::chatFriend(gxsChatId(groupGxsId), true);

//    //unseenp2p
//    if (UnseenGxsChatLobbyDialog *cld = dynamic_cast<UnseenGxsChatLobbyDialog*>(ChatDialog::getExistingChat(gxsChatId(groupGxsId)))) {
//        cld->updateParticipantsList();
//    }

}

RsGxsChatGroup UnseenGxsGroupFrameDialog::getGxsChatGroup(RsGxsGroupId groupId)
{
    std::list<RsGxsGroupId> groupIdList;
    groupIdList.push_back(groupId);
    std::vector<RsGxsChatGroup> chatInfos;

    if (rsGxsChats->getChatsInfo(groupIdList, chatInfos))
    {
        if(chatInfos.size() > 0)
        {
            return chatInfos[0];
        }
    }
    return RsGxsChatGroup();
}
