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

#include <QDialogButtonBox>
#include <QMenu>
#include "UnseenFriendSelectionWidget.h"
#include "ui_UnseenFriendSelectionWidget.h"
#include "gui/gxs/GxsIdDetails.h"
#include <retroshare-gui/RsAutoUpdatePage.h>
#include "gui/notifyqt.h"
#include "gui/common/RSTreeWidgetItem.h"
#include "gui/common/StatusDefs.h"
#include "gui/common/PeerDefs.h"
#include "gui/common/GroupDefs.h"
#include "rshare.h"

#include <retroshare/rspeers.h>
#include <retroshare/rsstatus.h>

#include <algorithm>

#include <QMessageBox>
//unseenp2p
#include "gui/gxschats/UnseenGxsSmartlistview.h"
#include "gui/UnseenGxsConversationitemdelegate.h"


#define COLUMN_NAME   0
#define COLUMN_CHECK  0
#define COLUMN_DATA   0
#define COLUMN_COUNT  1

#define IDDIALOG_IDLIST		1

#define ROLE_ID                     Qt::UserRole
#define ROLE_SORT_GROUP             Qt::UserRole + 1
#define ROLE_SORT_STANDARD_GROUP    Qt::UserRole + 2
#define ROLE_SORT_NAME              Qt::UserRole + 3
#define ROLE_SORT_STATE             Qt::UserRole + 4
#define ROLE_FILTER_REASON          Qt::UserRole + 5

#define IMAGE_GROUP16    ":/images/user/group16.png"
#define IMAGE_FRIENDINFO ":/images/peerdetails_16x16.png"

//static bool isSelected(UnseenFriendSelectionWidget::Modus modus, QTreeWidgetItem *item)
static bool isSelected(UnseenFriendSelectionWidget::Modus modus, QTreeWidgetItem *item)
{
	switch (modus) {
    case UnseenFriendSelectionWidget::MODUS_SINGLE:
    case UnseenFriendSelectionWidget::MODUS_MULTI:
		return item->isSelected();
    case UnseenFriendSelectionWidget::MODUS_CHECK:
		return (item->checkState(COLUMN_CHECK) == Qt::Checked);
	}

	return false;
}

static void setSelected(UnseenFriendSelectionWidget::Modus modus, QTreeWidgetItem *item, bool select)
{
	switch (modus) {
    case UnseenFriendSelectionWidget::MODUS_SINGLE:
    case UnseenFriendSelectionWidget::MODUS_MULTI:
		item->setSelected(select);
		break;
    case UnseenFriendSelectionWidget::MODUS_CHECK:
		item->setCheckState(COLUMN_CHECK, select ? Qt::Checked : Qt::Unchecked);
		break;
	}
}

UnseenFriendSelectionWidget::UnseenFriendSelectionWidget(QWidget *parent)
    : RsGxsUpdateBroadcastPage(rsIdentity,parent), ui(new Ui::UnseenFriendSelectionWidget)
{
	ui->setupUi(this);

	mStarted = false;
	mListModus = MODUS_SINGLE;
	mShowTypes = SHOW_GROUP | SHOW_SSL;
	mInGroupItemChanged = false;
	mInGpgItemChanged = false;
	mInSslItemChanged = false;
	mInFillList = false;

	mIdQueue = new TokenQueue(rsIdentity->getTokenService(), this);

    //New GUI
    if (!ui->friendList->model()) {
        smartListModel_ = new UnseenContactSmartListModel("testing", this);
        ui->friendList->setModel(smartListModel_);
        ui->friendList->setItemDelegate(new UnseenGxsConversationItemDelegate());
        ui->friendList->show();
    }

    // New GUI: smartlist selection
    QObject::connect(ui->friendList->selectionModel(),
        SIGNAL(selectionChanged(QItemSelection, QItemSelection)),
        this,
        SLOT(smartListSelectionChanged(QItemSelection, QItemSelection)));

    ui->friendList->setContextMenuPolicy(Qt::CustomContextMenu) ;
    ui->friendList->header()->hide();


	connect(ui->friendList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextMenuRequested(QPoint)));
	connect(ui->friendList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)), this, SLOT(itemDoubleClicked(QTreeWidgetItem*,int)));
	connect(ui->friendList, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(itemChanged(QTreeWidgetItem*,int)));
	connect(ui->friendList, SIGNAL(itemSelectionChanged()), this, SIGNAL(itemSelectionChanged()));
	connect(ui->filterLineEdit, SIGNAL(textChanged(QString)), this, SLOT(filterItems(QString)));

	connect(NotifyQt::getInstance(), SIGNAL(groupsChanged(int)), this, SLOT(groupsChanged(int)));
	connect(NotifyQt::getInstance(), SIGNAL(peerStatusChanged(const QString&,int)), this, SLOT(peerStatusChanged(const QString&,int)));

	mCompareRole = new RSTreeWidgetItemCompareRole;
	mActionSortByState = new QAction(tr("Sort by state"), this);
	mActionSortByState->setCheckable(true);
	connect(mActionSortByState, SIGNAL(toggled(bool)), this, SLOT(sortByState(bool)));
    // old GUI
    //ui->friendList->addContextMenuAction(mActionSortByState);
	mActionFilterConnected = new QAction(tr("Filter only connected"), this);
	mActionFilterConnected->setCheckable(true);
	connect(mActionFilterConnected, SIGNAL(toggled(bool)), this, SLOT(filterConnected(bool)));

    // old GUI
    //ui->friendList->addContextMenuAction(mActionFilterConnected);

	/* initialize list */
//	ui->friendList->setColumnCount(COLUMN_COUNT);
//	ui->friendList->headerItem()->setText(COLUMN_NAME, tr("Name"));
//	ui->friendList->setFilterReasonRole(ROLE_FILTER_REASON);

	/* sort list by name ascending */
    //ui->friendList->sortItems(COLUMN_NAME, Qt::AscendingOrder);
	sortByState(false);

	ui->filterLineEdit->setPlaceholderText(tr("Search Friends"));
	ui->filterLineEdit->showFilterIcon();

	/* Refresh style to have the correct text color */
	Rshare::refreshStyleSheet(this, false);
}

UnseenFriendSelectionWidget::~UnseenFriendSelectionWidget()
{
	delete(mIdQueue);
	delete ui;
}

void UnseenFriendSelectionWidget::changeEvent(QEvent *e)
{
	QWidget::changeEvent(e);
	switch (e->type()) {
	case QEvent::StyleChange:
		fillList();
		break;
	default:
		// remove compiler warnings
		break;
	}
}

void UnseenFriendSelectionWidget::setHeaderText(const QString &text)
{
    // old GUI
    //ui->friendList->headerItem()->setText(COLUMN_NAME, text);
}

void UnseenFriendSelectionWidget::setModus(Modus modus)
{
	mListModus = modus;

	switch (mListModus) {
	case MODUS_SINGLE:
	case MODUS_CHECK:
		ui->friendList->setSelectionMode(QAbstractItemView::SingleSelection);
		break;
	case MODUS_MULTI:
		ui->friendList->setSelectionMode(QAbstractItemView::ExtendedSelection);
		break;
	}

	fillList();
}

void UnseenFriendSelectionWidget::setShowType(ShowTypes types)
{
	mShowTypes = types;

	fillList();
}

int UnseenFriendSelectionWidget::addColumn(const QString &title)
{
    // old GUI
//	int column = ui->friendList->columnCount();
//	ui->friendList->setColumnCount(column + 1);
//	ui->friendList->headerItem()->setText(column, title);
    int column = 2;
    return column;
}

void UnseenFriendSelectionWidget::start()
{
	mStarted = true;
    //secured_fillList();

    updateDisplay(true);
    // old GUI
//	for (int i = 0; i < ui->friendList->columnCount(); ++i) {
//		ui->friendList->resizeColumnToContents(i);
//	}
}

static void initSslItem(QTreeWidgetItem *item, const RsPeerDetails &detail, const std::list<StatusInfo> &statusInfo, QColor textColorOnline)
{
	QString name = PeerDefs::nameWithLocation(detail);
	item->setText(COLUMN_NAME, name);

	int state = RS_STATUS_OFFLINE;
	if (detail.state & RS_PEER_STATE_CONNECTED) {
		std::list<StatusInfo>::const_iterator it;
		for (it = statusInfo.begin(); it != statusInfo.end() ; ++it) {
			if (it->id == detail.id) {
				state = it->status;
				break;
			}
		}
	}

	if (state != (int) RS_STATUS_OFFLINE) {
		item->setTextColor(COLUMN_NAME, textColorOnline);
	}

	item->setIcon(COLUMN_NAME, QIcon(StatusDefs::imageUser(state)));
	item->setData(COLUMN_DATA, ROLE_ID, QString::fromStdString(detail.id.toStdString()));

	item->setData(COLUMN_NAME, ROLE_SORT_GROUP, 1);
	item->setData(COLUMN_NAME, ROLE_SORT_STANDARD_GROUP, 0);
	item->setData(COLUMN_NAME, ROLE_SORT_NAME, name);
	item->setData(COLUMN_NAME, ROLE_SORT_STATE, state);
}

void UnseenFriendSelectionWidget::fillList()
{
	if (!mStarted) {
		return;
	}
	if(!isVisible())
		return ;
	if(RsAutoUpdatePage::eventsLocked())
		return ;

	secured_fillList() ;
}

void UnseenFriendSelectionWidget::loadRequest(const TokenQueue */*queue*/, const TokenRequest &req)
{
	// store all IDs locally, and call fillList() ;

	uint32_t token = req.mToken ;

	RsGxsIdGroup data;
	std::vector<RsGxsIdGroup> datavector;
	std::vector<RsGxsIdGroup>::iterator vit;

	if (!rsIdentity->getGroupData(token, datavector))
	{
        std::cerr << "UnseenFriendSelectionWidget::loadRequest() ERROR. Cannot load data from rsIdentity." << std::endl;
		return ;
	}

	gxsIds.clear() ;

	for(uint32_t i=0;i<datavector.size();++i)
	{
		gxsIds.push_back(datavector[i].mMeta.mGroupId) ;
		//std::cerr << "  got ID = " << datavector[i].mMeta.mGroupId << std::endl;
	}

    //unseenp2p - save the gxsIds to the smartlistmodel
    smartListModel_->setAllIdentites(gxsIds);

    emit ui->friendList->model()->layoutChanged();


	//std::cerr << "Got all " << datavector.size() << " ids from rsIdentity. Calling update of list." << std::endl;
	fillList() ;
}

void UnseenFriendSelectionWidget::secured_fillList()
{
	mInFillList = true;

	// get selected items
    std::set<RsPeerId> sslIdsSelected;
	if (mShowTypes & SHOW_SSL) {
        selectedIds<RsPeerId,IDTYPE_SSL>(sslIdsSelected,true);
	}

    std::set<RsNodeGroupId> groupIdsSelected;
	if (mShowTypes & SHOW_GROUP) {
        selectedIds<RsNodeGroupId,IDTYPE_GROUP>(groupIdsSelected,true);
	}

    std::set<RsPgpId> gpgIdsSelected;
	if (mShowTypes & (SHOW_GPG | SHOW_NON_FRIEND_GPG)) {
        selectedIds<RsPgpId,IDTYPE_GPG>(gpgIdsSelected,true);
	}

    std::set<RsGxsId> gxsIdsSelected;

	if (mShowTypes & SHOW_GXS)
    {
		selectedIds<RsGxsId,IDTYPE_GXS>(gxsIdsSelected,true);
    // old GUI
//        if(!ui->friendList->topLevelItemCount())					// if not loaded yet, use the existing list.
//            gxsIdsSelected = mPreSelectedGxsIds;
    }
		
    std::set<RsGxsId> gxsIdsSelected2;
	if (mShowTypes & SHOW_CONTACTS)
		selectedIds<RsGxsId,IDTYPE_GXS>(gxsIdsSelected2,true);		
	
    // remove old items - old GUI

//	ui->friendList->clear();

	// get existing groups
	std::list<RsGroupInfo> groupInfoList;
	std::list<RsGroupInfo>::iterator groupIt;
	rsPeers->getGroupInfoList(groupInfoList);

    std::list<RsPgpId> gpgIds;
    std::list<RsPgpId>::iterator gpgIt;

	if(mShowTypes & SHOW_NON_FRIEND_GPG)
		rsPeers->getGPGAllList(gpgIds);
	else
		rsPeers->getGPGAcceptedList(gpgIds);

    // add own pgp id to the list
    gpgIds.push_back(rsPeers->getGPGOwnId()) ;

    std::list<RsPeerId> sslIds;
    std::list<RsPeerId>::iterator sslIt;

	if ((mShowTypes & (SHOW_SSL | SHOW_GPG)) == SHOW_SSL) {
		rsPeers->getFriendList(sslIds);
	}

	std::list<StatusInfo> statusInfo;
	std::list<StatusInfo>::iterator statusIt;
	rsStatus->getStatusList(statusInfo);

	std::list<std::string> filledIds; // gpg or ssl id

	// start with groups
	groupIt = groupInfoList.begin();
	while (true) {
		QTreeWidgetItem *groupItem = NULL;
		QTreeWidgetItem *gpgItem = NULL;
        QTreeWidgetItem *gxsItem = NULL;
        RsGroupInfo *groupInfo = NULL;

		if ((mShowTypes & SHOW_GROUP) && groupIt != groupInfoList.end()) 
		{
			groupInfo = &(*groupIt);

			if (groupInfo->peerIds.empty()) {
				// don't show empty groups
				++groupIt;
				continue;
			}

			// add group item
			groupItem = new RSTreeWidgetItem(mCompareRole, IDTYPE_GROUP);

			// Add item to the list
            // old GUI
            //ui->friendList->addTopLevelItem(groupItem);

			groupItem->setFlags(Qt::ItemIsUserCheckable | groupItem->flags());
			groupItem->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicatorWhenChildless);
			groupItem->setTextAlignment(COLUMN_NAME, Qt::AlignLeft | Qt::AlignVCenter);
			groupItem->setIcon(COLUMN_NAME, QIcon(IMAGE_GROUP16));

            groupItem->setData(COLUMN_DATA, ROLE_ID, QString::fromStdString(groupInfo->id.toStdString()));

			groupItem->setExpanded(true);

			QString groupName = GroupDefs::name(*groupInfo);
			groupItem->setText(COLUMN_NAME, groupName);

			groupItem->setData(COLUMN_NAME, ROLE_SORT_GROUP, 0);
			groupItem->setData(COLUMN_NAME, ROLE_SORT_STANDARD_GROUP, (groupInfo->flag & RS_GROUP_FLAG_STANDARD) ? 0 : 1);
			groupItem->setData(COLUMN_NAME, ROLE_SORT_NAME, groupName);
			groupItem->setData(COLUMN_NAME, ROLE_SORT_STATE, 0);

			if (mListModus == MODUS_CHECK) {
				groupItem->setCheckState(0, Qt::Unchecked);
			}

            emit itemAdded(IDTYPE_GROUP, QString::fromStdString(groupInfo->id.toStdString()), groupItem);

			if (std::find(groupIdsSelected.begin(), groupIdsSelected.end(), groupInfo->id) != groupIdsSelected.end()) {
				setSelected(mListModus, groupItem, true);
			}
		}

		if (mShowTypes & (SHOW_GPG | SHOW_NON_FRIEND_GPG)) 
		{
			// iterate through gpg ids
			for (gpgIt = gpgIds.begin(); gpgIt != gpgIds.end(); ++gpgIt) {
				if (groupInfo) {
					// we fill a group, check if gpg id is assigned
					if (std::find(groupInfo->peerIds.begin(), groupInfo->peerIds.end(), *gpgIt) == groupInfo->peerIds.end()) {
						continue;
					}
				} else {
					// we fill the not assigned gpg ids
                    if (std::find(filledIds.begin(), filledIds.end(), (*gpgIt).toStdString()) != filledIds.end()) {
						continue;
					}
				}

				// add equal too, its no problem
                filledIds.push_back((*gpgIt).toStdString());

				RsPeerDetails detail;
                if (!rsPeers->getGPGDetails(*gpgIt, detail)) {
					continue; /* BAD */
				}

				// make a widget per friend
				gpgItem = new RSTreeWidgetItem(mCompareRole, IDTYPE_GPG);

				QString name = QString::fromUtf8(detail.name.c_str());
                gpgItem->setText(COLUMN_NAME, name + " ("+QString::fromStdString( (*gpgIt).toStdString() )+")");

				sslIds.clear();
				rsPeers->getAssociatedSSLIds(*gpgIt, sslIds);

				int state = RS_STATUS_OFFLINE;
				for (statusIt = statusInfo.begin(); statusIt != statusInfo.end() ; ++statusIt) {
					if (std::find(sslIds.begin(), sslIds.end(), statusIt->id) != sslIds.end()) {
						if (statusIt->status != RS_STATUS_OFFLINE) {
							state = RS_STATUS_ONLINE;
							break;
						}
					}
				}

				if (state != (int) RS_STATUS_OFFLINE) {
					gpgItem->setTextColor(COLUMN_NAME, textColorOnline());
				}

				gpgItem->setFlags(Qt::ItemIsUserCheckable | gpgItem->flags());
				gpgItem->setIcon(COLUMN_NAME, QIcon(StatusDefs::imageUser(state)));
				gpgItem->setData(COLUMN_DATA, ROLE_ID, QString::fromStdString(detail.gpg_id.toStdString()));

				gpgItem->setData(COLUMN_NAME, ROLE_SORT_GROUP, 1);
				gpgItem->setData(COLUMN_NAME, ROLE_SORT_STANDARD_GROUP, 0);
				gpgItem->setData(COLUMN_NAME, ROLE_SORT_NAME, name);
				gpgItem->setData(COLUMN_NAME, ROLE_SORT_STATE, state);

				if (mListModus == MODUS_CHECK) {
					gpgItem->setCheckState(0, Qt::Unchecked);
				}

				// add to the list
				if (groupItem) {
					groupItem->addChild(gpgItem);
				} else {
                    // old GUI
                    //ui->friendList->addTopLevelItem(gpgItem);
				}

				gpgItem->setExpanded(true);

                emit itemAdded(IDTYPE_GPG, QString::fromStdString(detail.gpg_id.toStdString()), gpgItem);

				if (mShowTypes & SHOW_SSL) {
					// iterate through associated ssl ids
					for (sslIt = sslIds.begin(); sslIt != sslIds.end(); ++sslIt) {
						RsPeerDetails detail;
						if (!rsPeers->getPeerDetails(*sslIt, detail)) {
							continue; /* BAD */
						}

						// make a widget per friend
						QTreeWidgetItem *item = new RSTreeWidgetItem(mCompareRole, IDTYPE_SSL);

						item->setFlags(Qt::ItemIsUserCheckable | item->flags());
						initSslItem(item, detail, statusInfo, textColorOnline());

						if (mListModus == MODUS_CHECK) {
							item->setCheckState(0, Qt::Unchecked);
						}

						// add to the list
						gpgItem->addChild(item);

                        emit itemAdded(IDTYPE_SSL, QString::fromStdString(detail.id.toStdString()), item);

						if (std::find(sslIdsSelected.begin(), sslIdsSelected.end(), detail.id) != sslIdsSelected.end()) {
							setSelected(mListModus, item, true);
						}
					}
				}

				if (std::find(gpgIdsSelected.begin(), gpgIdsSelected.end(), detail.gpg_id) != gpgIdsSelected.end()) {
					setSelected(mListModus, gpgItem, true);
				}
			}
		} 
		else 
		{
			// iterate through ssl ids
			for (sslIt = sslIds.begin(); sslIt != sslIds.end(); ++sslIt) {
				RsPeerDetails detail;
				if (!rsPeers->getPeerDetails(*sslIt, detail)) {
					continue; /* BAD */
				}

				if (groupInfo) {
					// we fill a group, check if gpg id is assigned
					if (std::find(groupInfo->peerIds.begin(), groupInfo->peerIds.end(), detail.gpg_id) == groupInfo->peerIds.end()) {
						continue;
					}
				} else {
					// we fill the not assigned ssl ids
                    if (std::find(filledIds.begin(), filledIds.end(), (*sslIt).toStdString()) != filledIds.end()) {
						continue;
					}
				}

				// add equal too, its no problem
                filledIds.push_back(detail.id.toStdString());

				// make a widget per friend
				QTreeWidgetItem *item = new RSTreeWidgetItem(mCompareRole, IDTYPE_SSL);

				initSslItem(item, detail, statusInfo, textColorOnline());
				item->setFlags(Qt::ItemIsUserCheckable | item->flags());

				if (mListModus == MODUS_CHECK) {
					item->setCheckState(0, Qt::Unchecked);
				}

				// add to the list
				if (groupItem) {
					groupItem->addChild(item);
				} else {
                    // old GUI
                    //ui->friendList->addTopLevelItem(item);
				}

                emit itemAdded(IDTYPE_SSL, QString::fromStdString(detail.id.toStdString()), item);

				if (std::find(sslIdsSelected.begin(), sslIdsSelected.end(), detail.id) != sslIdsSelected.end()) {
					setSelected(mListModus, item, true);
				}
			}
		}

		if(mShowTypes & SHOW_GXS)
		{
			// iterate through gpg ids
			for (std::vector<RsGxsGroupId>::const_iterator gxsIt = gxsIds.begin(); gxsIt != gxsIds.end(); ++gxsIt)
			{

					// we fill the not assigned gpg ids
				if (std::find(filledIds.begin(), filledIds.end(), (*gxsIt).toStdString()) != filledIds.end()) 
						continue;

				// add equal too, its no problem
				filledIds.push_back((*gxsIt).toStdString());

				RsIdentityDetails detail;
				if (!rsIdentity->getIdDetails(RsGxsId(*gxsIt), detail)) 
					continue; /* BAD */
					
                QList<QIcon> icons ;
                GxsIdDetails::getIcons(detail,icons,GxsIdDetails::ICON_TYPE_AVATAR) ;
                QIcon identicon = icons.front() ;

				// make a widget per friend
				gxsItem = new RSTreeWidgetItem(mCompareRole, IDTYPE_GXS);

				QString name = QString::fromUtf8(detail.mNickname.c_str());
				gxsItem->setText(COLUMN_NAME, name + " ("+QString::fromStdString( (*gxsIt).toStdString() )+")");

				//gxsItem->setTextColor(COLUMN_NAME, textColorOnline());
				gxsItem->setFlags(Qt::ItemIsUserCheckable | gxsItem->flags());
                gxsItem->setIcon(COLUMN_NAME, identicon);
				gxsItem->setData(COLUMN_DATA, ROLE_ID, QString::fromStdString(detail.mId.toStdString()));

				gxsItem->setData(COLUMN_NAME, ROLE_SORT_GROUP, 1);
				gxsItem->setData(COLUMN_NAME, ROLE_SORT_STANDARD_GROUP, 0);
				gxsItem->setData(COLUMN_NAME, ROLE_SORT_NAME, name);

                //TODO: online state for gxs items
				gxsItem->setData(COLUMN_NAME, ROLE_SORT_STATE, 1);

				if (mListModus == MODUS_CHECK) 
					gxsItem->setCheckState(0, Qt::Unchecked);
                // old GUI
                //ui->friendList->addTopLevelItem(gxsItem);

				gxsItem->setExpanded(true);

				emit itemAdded(IDTYPE_GXS, QString::fromStdString(detail.mId.toStdString()), gxsItem);

				if (std::find(gxsIdsSelected.begin(), gxsIdsSelected.end(), detail.mId) != gxsIdsSelected.end()) 
					setSelected(mListModus, gxsItem, true);
			}
		}
		if(mShowTypes & SHOW_CONTACTS)
		{
			// iterate through gpg ids
			for (std::vector<RsGxsGroupId>::const_iterator gxsIt = gxsIds.begin(); gxsIt != gxsIds.end(); ++gxsIt)
			{

					// we fill the not assigned gpg ids
				if (std::find(filledIds.begin(), filledIds.end(), (*gxsIt).toStdString()) != filledIds.end()) 
						continue;

				// add equal too, its no problem
				filledIds.push_back((*gxsIt).toStdString());

				RsIdentityDetails detail;
				if (!rsIdentity->getIdDetails(RsGxsId(*gxsIt), detail)) 
					continue; /* BAD */
					
                QList<QIcon> icons ;
                GxsIdDetails::getIcons(detail,icons,GxsIdDetails::ICON_TYPE_AVATAR) ;
                QIcon identicon = icons.front() ;
                
                if(detail.mFlags & RS_IDENTITY_FLAGS_IS_A_CONTACT)
                {

                  // make a widget per friend
                  gxsItem = new RSTreeWidgetItem(mCompareRole, IDTYPE_GXS);

                  QString name = QString::fromUtf8(detail.mNickname.c_str());
                  gxsItem->setText(COLUMN_NAME, name + " ("+QString::fromStdString( (*gxsIt).toStdString() )+")");

                  //gxsItem->setTextColor(COLUMN_NAME, textColorOnline());
                  gxsItem->setFlags(Qt::ItemIsUserCheckable | gxsItem->flags());
                          gxsItem->setIcon(COLUMN_NAME, identicon);
                  gxsItem->setData(COLUMN_DATA, ROLE_ID, QString::fromStdString(detail.mId.toStdString()));

                  gxsItem->setData(COLUMN_NAME, ROLE_SORT_GROUP, 1);
                  gxsItem->setData(COLUMN_NAME, ROLE_SORT_STANDARD_GROUP, 0);
                  gxsItem->setData(COLUMN_NAME, ROLE_SORT_NAME, name);
                  //TODO: online state for gxs items
                  gxsItem->setData(COLUMN_NAME, ROLE_SORT_STATE, 1);

                  if (mListModus == MODUS_CHECK)
                    gxsItem->setCheckState(0, Qt::Unchecked);
                  // old GUI
                  //ui->friendList->addTopLevelItem(gxsItem);

                  gxsItem->setExpanded(true);

                  emit itemAdded(IDTYPE_GXS, QString::fromStdString(detail.mId.toStdString()), gxsItem);

                  if (std::find(gxsIdsSelected.begin(), gxsIdsSelected.end(), detail.mId) != gxsIdsSelected.end())
                    setSelected(mListModus, gxsItem, true);
				}
			}
		}
		if (groupIt != groupInfoList.end()) {
			++groupIt;
		} else {
			// all done
			break;
		}
	}

	if (ui->filterLineEdit->text().isEmpty() == false) {
		filterItems(ui->filterLineEdit->text());
	}

	ui->friendList->update(); /* update display */

	mInFillList = false;
    // old GUI
    //ui->friendList->resort();
	filterConnected(isFilterConnected());

	emit contentChanged();
}
void UnseenFriendSelectionWidget::updateDisplay(bool)
{
	requestGXSIdList() ;
}
void UnseenFriendSelectionWidget::requestGXSIdList()
{
	if (!mIdQueue)
		return;

	//mStateHelper->setLoading(IDDIALOG_IDLIST, true);
	//mStateHelper->setLoading(IDDIALOG_IDDETAILS, true);
	//mStateHelper->setLoading(IDDIALOG_REPLIST, true);

	mIdQueue->cancelActiveRequestTokens(IDDIALOG_IDLIST);

	RsTokReqOptions opts;
	opts.mReqType = GXS_REQUEST_TYPE_GROUP_DATA;

	uint32_t token;

	mIdQueue->requestGroupInfo(token, RS_TOKREQ_ANSTYPE_DATA, opts, IDDIALOG_IDLIST);
}

void UnseenFriendSelectionWidget::requestIdList()
{
    //Disable by default, will be enable by insertIdDetails()

    if (!mIdQueue)
        return;

    //mStateHelper->setLoading(IDDIALOG_IDLIST, true);

    mIdQueue->cancelActiveRequestTokens(IDDIALOG_IDLIST);

    RsTokReqOptions opts;
    opts.mReqType = GXS_REQUEST_TYPE_GROUP_DATA;

    uint32_t token;

    mIdQueue->requestGroupInfo(token, RS_TOKREQ_ANSTYPE_DATA, opts, IDDIALOG_IDLIST);
}

template<> inline void UnseenFriendSelectionWidget::setSelectedIds<RsGxsId,UnseenFriendSelectionWidget::IDTYPE_GXS>(const std::set<RsGxsId>& ids, bool add)
{
    mPreSelectedGxsIds = ids ;
    requestGXSIdList();
}

void UnseenFriendSelectionWidget::groupsChanged(int /*type*/)
{
	if (mShowTypes & SHOW_GROUP) {
		fillList();
	}
}

void UnseenFriendSelectionWidget::peerStatusChanged(const QString& peerId, int status)
{
	if(!isVisible())
		return ;
	if(RsAutoUpdatePage::eventsLocked())
		return ;

	RsPeerId peerid(peerId.toStdString()) ;
	QString gpgId;
	int gpgStatus = RS_STATUS_OFFLINE;

	if (mShowTypes & (SHOW_GPG | SHOW_NON_FRIEND_GPG)) {
		/* need gpg id and online state */
		RsPeerDetails detail;
        if (rsPeers->getPeerDetails(peerid, detail))
        {
            gpgId = QString::fromStdString(detail.gpg_id.toStdString());

			if (status == (int) RS_STATUS_OFFLINE) {
				/* try other nodes */
                std::list<RsPeerId> sslIds;
				rsPeers->getAssociatedSSLIds(detail.gpg_id, sslIds);

				std::list<StatusInfo> statusInfo;
				std::list<StatusInfo>::iterator statusIt;
				rsStatus->getStatusList(statusInfo);

				for (statusIt = statusInfo.begin(); statusIt != statusInfo.end() ; ++statusIt) {
					if (std::find(sslIds.begin(), sslIds.end(), statusIt->id) != sslIds.end()) {
						if (statusIt->status != RS_STATUS_OFFLINE) {
							gpgStatus = RS_STATUS_ONLINE;
							break;
						}
					}
				}
			} else {
				/* one node is online */
				gpgStatus = RS_STATUS_ONLINE;
			}
		}
	}
    // old GUI
//	QTreeWidgetItemIterator itemIterator(ui->friendList);
//	QTreeWidgetItem *item;
//	while ((item = *itemIterator) != NULL) {
//		++itemIterator;

//		bool bFoundGPG = false;
//		bool bFoundSSL = false;

//		switch (idTypeFromItem(item)) {
//		case IDTYPE_NONE:
//		case IDTYPE_GROUP:
//		case IDTYPE_GXS:
//			break;
//		case IDTYPE_GPG:
//			{
//				if (item->data(COLUMN_DATA, ROLE_ID).toString() == gpgId) {
//					QColor color;
//					if (status != (int) RS_STATUS_OFFLINE) {
//						color = textColorOnline();
//					} else {
//						color = ui->friendList->palette().color(QPalette::Text);
//					}

//					item->setTextColor(COLUMN_NAME, color);
//					item->setIcon(COLUMN_NAME, QIcon(StatusDefs::imageUser(gpgStatus)));

//					item->setData(COLUMN_NAME, ROLE_SORT_STATE, gpgStatus);

//					bFoundGPG = true;
//				}
//			}
//			break;
//		case IDTYPE_SSL:
//			{
//				if (item->data(COLUMN_DATA, ROLE_ID).toString() == peerId) {
//					QColor color;
//					if (status != (int) RS_STATUS_OFFLINE) {
//						color = textColorOnline();
//					} else {
//						color = ui->friendList->palette().color(QPalette::Text);
//					}

//					item->setTextColor(COLUMN_NAME, color);
//					item->setIcon(COLUMN_NAME, QIcon(StatusDefs::imageUser(status)));

//					item->setData(COLUMN_NAME, ROLE_SORT_STATE, status);

//					bFoundSSL = true;
//				}
//			}
//			break;
//		}

//		if (bFoundGPG) {
//			if (mShowTypes & SHOW_GROUP) {
//				// a friend can be assigned to more than one group
//			} else {
//				if (mShowTypes & SHOW_SSL) {
//					// search for ssl id
//				} else {
//					break;
//				}
//			}
//		}
//		if (bFoundSSL) {
//			if (mShowTypes & SHOW_GROUP) {
//				// a friend can be assigned to more than one group
//			} else {
//				break;
//			}
//		}
//	}

//	ui->friendList->resort();
//	filterConnected(isFilterConnected());
}

void UnseenFriendSelectionWidget::addContextMenuAction(QAction *action)
{
	mContextMenuActions.push_back(action);
}

void UnseenFriendSelectionWidget::contextMenuRequested(const QPoint &/*pos*/)
{
	QMenu *contextMenu = new QMenu(this);

	if (mListModus == MODUS_MULTI) {
		contextMenu->addAction(QIcon(), tr("Mark all"), this, SLOT(selectAll()));
		contextMenu->addAction(QIcon(), tr("Mark none"), this, SLOT(deselectAll()));
	}

	if (!mContextMenuActions.isEmpty()) {
		bool addSeparator = false;
		if (!contextMenu->isEmpty()) {
			// Check for visible action
			foreach (QAction *action, mContextMenuActions) {
				if (action->isVisible()) {
					addSeparator = true;
					break;
				}
			}
		}

		if (addSeparator) {
			contextMenu->addSeparator();
		}

		contextMenu->addActions(mContextMenuActions);
	}
    // old GUI
    //contextMenu = ui->friendList->createStandardContextMenu(contextMenu);

	if (!contextMenu->isEmpty()) {
		contextMenu->exec(QCursor::pos());
	}

	delete contextMenu;
}

void UnseenFriendSelectionWidget::itemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
	if (!item) {
		return;
	}

	emit doubleClicked(idTypeFromItem(item), item->data(COLUMN_DATA, ROLE_ID).toString());
}

void UnseenFriendSelectionWidget::itemChanged(QTreeWidgetItem *item, int column)
{
	if (mInFillList) {
		return;
	}

	if (column != COLUMN_CHECK) {
		emit itemChanged(idTypeFromItem(item), item->data(COLUMN_DATA, ROLE_ID).toString(), item, column);
		return;
	}

	if (mListModus != MODUS_CHECK) {
		return;
	}

	switch (idTypeFromItem(item)) {
	case IDTYPE_NONE:
		break;
	case IDTYPE_GROUP:
		{
			if (mInGroupItemChanged || mInGpgItemChanged || mInSslItemChanged) {
				break;
			}

			mInGroupItemChanged = true;

			bool selected = isSelected(mListModus, item);

			int childCount = item->childCount();
			for (int i = 0; i < childCount; ++i) {
				setSelected(mListModus, item->child(i), selected);
			}

			mInGroupItemChanged = false;
		}
		break;
	case IDTYPE_GPG:
    case IDTYPE_GXS:
        {
			if (mInGpgItemChanged) {
				break;
			}

			mInGpgItemChanged = true;

			if (!mInSslItemChanged) {
				bool selected = isSelected(mListModus, item);

				int childCount = item->childCount();
				for (int i = 0; i < childCount; ++i) {
					setSelected(mListModus, item->child(i), selected);
				}
			}

			if (!mInGroupItemChanged) {
				QTreeWidgetItem *itemParent = item->parent();
				if (itemParent) {
					int childCount = itemParent->childCount();
					bool foundUnselected = false;
					for (int index = 0; index < childCount; ++index) {
						if (!isSelected(mListModus, itemParent->child(index))) {
							foundUnselected = true;
							break;
						}
					}
					setSelected(mListModus, itemParent, !foundUnselected);
				}
			}

			mInGpgItemChanged = false;
		}
		break;
	case IDTYPE_SSL:
		{
			if (mInGroupItemChanged || mInGpgItemChanged || mInSslItemChanged) {
				break;
			}

			mInSslItemChanged = true;

			QTreeWidgetItem *itemParent = item->parent();
			if (itemParent) {
				int childCount = itemParent->childCount();
				bool foundUnselected = false;
				for (int index = 0; index < childCount; ++index) {
					if (!isSelected(mListModus, itemParent->child(index))) {
						foundUnselected = true;
						break;
					}
				}
				setSelected(mListModus, itemParent, !foundUnselected);
			}

			mInSslItemChanged = false;
		}
		break;
	}
}

void UnseenFriendSelectionWidget::filterItems(const QString& text)
{
    // old GUI
//	ui->friendList->filterItems(COLUMN_NAME, text);
//	ui->friendList->resort();
	filterConnected(isFilterConnected());
}

int UnseenFriendSelectionWidget::selectedItemCount()
{
    // old GUI
    //return ui->friendList->selectedItems().count();
}

std::string UnseenFriendSelectionWidget::selectedId(IdType &idType)
{
    return "";
    // old GUI
//	QTreeWidgetItem *item = ui->friendList->currentItem();
//	if (!item) {
//		idType = IDTYPE_NONE;
//		return "";
//	}

//	idType = idTypeFromItem(item);
//	return idFromItem(item);
}

void UnseenFriendSelectionWidget::selectedIds(IdType idType, std::set<std::string> &ids, bool onlyDirectSelected)
{
    //OLD GUI
//	QTreeWidgetItemIterator itemIterator(ui->friendList);
//	QTreeWidgetItem *item;
//	while ((item = *itemIterator) != NULL) {
//		++itemIterator;

//		std::string id;

//		switch (idTypeFromItem(item)) {
//		case IDTYPE_NONE:
//			break;
//		case IDTYPE_GROUP:
//			if (idType == IDTYPE_GROUP) {
//				if (isSelected(mListModus, item)) {
//					id = idFromItem(item);
//				}
//			}
//			break;
//		case IDTYPE_GPG:
//        case IDTYPE_GXS:
//            if (idType == IDTYPE_GPG || idType == IDTYPE_GXS)
//            {
//				if (isSelected(mListModus, item)) {
//					id = idFromItem(item);
//				} else {
//					if (!onlyDirectSelected) {
//						QTreeWidgetItem *itemParent = item;
//						while ((itemParent = itemParent->parent()) != NULL) {
//							if (isSelected(mListModus, itemParent)) {
//								id = idFromItem(item);
//								break;
//							}
//						}
//					}
//				}
//			}
//			break;
//		case IDTYPE_SSL:
//			if (idType == IDTYPE_SSL) {
//				if (isSelected(mListModus, item)) {
//					id = idFromItem(item);
//				} else {
//					if (!onlyDirectSelected) {
//						QTreeWidgetItem *itemParent = item;
//						while ((itemParent = itemParent->parent()) != NULL) {
//							if (isSelected(mListModus, itemParent)) {
//								id = idFromItem(item);
//								break;
//							}
//						}
//					}
//				}
//			}
//			break;
//		}
//		if (!id.empty() && std::find(ids.begin(), ids.end(), id) == ids.end()) {
//            ids.insert(id);
//		}
//	}
}

void UnseenFriendSelectionWidget::deselectAll()
{
//	for(QTreeWidgetItemIterator itemIterator(ui->friendList);*itemIterator!=NULL;++itemIterator)
//		setSelected(mListModus, *itemIterator, false);
}

void UnseenFriendSelectionWidget::selectAll()
{
//	for(QTreeWidgetItemIterator itemIterator(ui->friendList);*itemIterator!=NULL;++itemIterator)
//		setSelected(mListModus, *itemIterator, true);
}

void UnseenFriendSelectionWidget::setSelectedIds(IdType idType, const std::set<std::string> &ids, bool add)
{
//	QTreeWidgetItemIterator itemIterator(ui->friendList);
//	QTreeWidgetItem *item;
//	while ((item = *itemIterator) != NULL) {
//		++itemIterator;

//		std::string id = idFromItem(item);
//		IdType itemType = idTypeFromItem(item);

//		switch (itemType) {
//		case IDTYPE_NONE:
//			break;
//		case IDTYPE_GROUP:
//		case IDTYPE_GPG:
//		case IDTYPE_SSL:
//        case IDTYPE_GXS:
//            if (idType == itemType) {
//                if (ids.find(id) != ids.end()) {
//					setSelected(mListModus, item, true);
//					break;
//				}
//			}
//			if (!add) {
//				setSelected(mListModus, item, false);
//			}
//			break;
//		}
//	}
}

void UnseenFriendSelectionWidget::itemsFromId(IdType idType, const std::string &id, QList<QTreeWidgetItem*> &items)
{
//	QTreeWidgetItemIterator itemIterator(ui->friendList);
//	QTreeWidgetItem *item;
//	while ((item = *itemIterator) != NULL) {
//		++itemIterator;

//		if (idType == idTypeFromItem(item) && idFromItem(item) == id) {
//			items.push_back(item);
//		}
//	}
}

void UnseenFriendSelectionWidget::items(QList<QTreeWidgetItem*> &_items, IdType idType)
{
//	QTreeWidgetItemIterator itemIterator(ui->friendList);
//	QTreeWidgetItem *item;
//	while ((item = *itemIterator) != NULL) {
//		++itemIterator;

//		if (idType == IDTYPE_NONE || idType == idTypeFromItem(item)) {
//			_items.push_back(item);
//		}
//	}
}

UnseenFriendSelectionWidget::IdType UnseenFriendSelectionWidget::idTypeFromItem(QTreeWidgetItem *item)
{
	if (!item) {
		return IDTYPE_NONE;
	}

	return (IdType) item->type();
}

std::string UnseenFriendSelectionWidget::idFromItem(QTreeWidgetItem *item)
{
	if (!item) {
		return "";
	}

	return item->data(COLUMN_DATA, ROLE_ID).toString().toStdString();
}

void UnseenFriendSelectionWidget::sortByState(bool sort)
{
	mCompareRole->setRole(COLUMN_NAME, ROLE_SORT_GROUP);
	mCompareRole->addRole(COLUMN_NAME, ROLE_SORT_STANDARD_GROUP);

	if (sort) {
		mCompareRole->addRole(COLUMN_NAME, ROLE_SORT_STATE);
		mCompareRole->addRole(COLUMN_NAME, ROLE_SORT_NAME);
	} else {
		mCompareRole->addRole(COLUMN_NAME, ROLE_SORT_NAME);
		mCompareRole->addRole(COLUMN_NAME, ROLE_SORT_STATE);
	}

	mActionSortByState->setChecked(sort);
    // Old GUI
    //ui->friendList->resort();
	filterConnected(isFilterConnected());
}

bool UnseenFriendSelectionWidget::isSortByState()
{
	return mActionSortByState->isChecked();
}

void UnseenFriendSelectionWidget::filterConnected(bool filter)
{
//	ui->friendList->filterMinValItems(COLUMN_NAME, filter ? RS_STATUS_AWAY : RS_STATUS_OFFLINE, ROLE_SORT_STATE);

//	mActionFilterConnected->setChecked(filter);

//	ui->friendList->resort();
}

bool UnseenFriendSelectionWidget::isFilterConnected()
{
	return mActionFilterConnected->isChecked();
}

//For New GUI
void UnseenFriendSelectionWidget::smartListSelectionChanged(const QItemSelection  &selected, const QItemSelection  &deselected)
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

void UnseenFriendSelectionWidget::selectConversation(const QModelIndex& index)
{

    //How to get the ConversationModel and get the index of it
    if (!index.isValid()) return;

    std::vector<RsGxsGroupId> list = smartListModel_->getAllIdentities();

    if (list.size() <= index.row()) return;

    RsGxsGroupId contactId = list.at(index.row());
    RsIdentityDetails detail;
    if (!rsIdentity->getIdDetails(RsGxsId(contactId), detail))
    {
        return ;
    }

    QString name = QString::fromStdString(detail.mNickname.c_str());
     QMessageBox::warning(this, "UnseenP2P", tr("You want to add ") + name + tr("to this group"), QMessageBox::Ok, QMessageBox::Ok);
    //MessagesDialog:


//    UnseenGroupItemInfo gxsGroupItem = list.at(index.row());

//    std::cerr << " gxsGroupItem info, name : " << gxsGroupItem.name.toStdString() << std::endl;

//    mGroupId = RsGxsGroupId(gxsGroupItem.id.toStdString());

//    showGxsGroupChatMVC(gxsChatId(mGroupId));

}

//QString UnseenFriendSelectionWidget::itemIdAt(QPoint &point)
//{
//    QModelIndex itemIndex = ui->unseenGroupTreeWidget->indexAt(point);

//    std::vector<UnseenGroupItemInfo> iteminfoList = smartListModel_->getGxsGroupList();

//    if (itemIndex.row() < iteminfoList.size())
//    {
//        UnseenGroupItemInfo itemInfo = iteminfoList.at(itemIndex.row());
//        return  itemInfo.id;
//    }
//    else return "";

//}

//UnseenGroupItemInfo UnseenFriendSelectionWidget::groupItemIdAt(QPoint &point)
//{
//    QModelIndex itemIndex = ui->unseenGroupTreeWidget->indexAt(point);

//    std::vector<UnseenGroupItemInfo> iteminfoList = smartListModel_->getGxsGroupList();

//    if (itemIndex.row() < iteminfoList.size())
//    {
//        return iteminfoList.at(itemIndex.row());
//    }
//    else return UnseenGroupItemInfo();

//}

//UnseenGroupItemInfo UnseenFriendSelectionWidget::groupItemIdAt(QString groupId)
//{
//    std::vector<UnseenGroupItemInfo> iteminfoList = smartListModel_->getGxsGroupList();

//    for (std::vector<UnseenGroupItemInfo>::iterator it = iteminfoList.begin(); it != iteminfoList.end(); ++it)
//    if (it->id == groupId)
//    {
//        return *it;
//    }
//    else return UnseenGroupItemInfo();
//}

//void UnseenFriendSelectionWidget::sortGxsConversationListByRecentTime()
//{
//    smartListModel_->sortGxsConversationListByRecentTime();
//}

