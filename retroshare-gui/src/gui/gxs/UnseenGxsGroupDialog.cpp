/*
 * Retroshare Gxs Support
 *
 * Copyright 2012-2013 by Robert Fernie.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License Version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 *
 * Please report all bugs and problems to "retroshare@lunamutt.com".
 *
 */

#include <QMessageBox>

#include "util/misc.h"
#include "util/DateTime.h"
//for unseenp2p

#include "UnseenGxsGroupDialog.h"
#include "gui/common/PeerDefs.h"
#include "retroshare/rsgxsflags.h"

#include <algorithm>

#include <retroshare/rspeers.h>
#include <retroshare/rsgxscircles.h>

#include <gui/settings/rsharesettings.h>

#include <iostream>


#define GXSGROUP_NEWGROUPID         1
#define GXSGROUP_LOADGROUP          2
#define GXSGROUP_INTERNAL_LOADGROUP 3

/** Constructor */
UnseenGxsGroupDialog::UnseenGxsGroupDialog(TokenQueue *tokenExternalQueue, uint32_t enableFlags, uint32_t defaultFlags, QWidget *parent)
    : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint), mTokenService(NULL), mExternalTokenQueue(tokenExternalQueue), mInternalTokenQueue(NULL), mGrpMeta(), mMode(MODE_CREATE), mEnabledFlags(enableFlags), mReadonlyFlags(0), mDefaultsFlags(defaultFlags)
{
	/* Invoke the Qt Designer generated object setup routine */
	ui.setupUi(this);
	
	mInternalTokenQueue = NULL;

    std::set<RsPeerId> friends;
    init(friends);
}

UnseenGxsGroupDialog::UnseenGxsGroupDialog(TokenQueue *tokenExternalQueue, RsTokenService *tokenService, Mode mode, RsGxsGroupId groupId, uint32_t enableFlags, uint32_t defaultFlags, QWidget *parent)
    : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint), mTokenService(NULL), mExternalTokenQueue(tokenExternalQueue), mInternalTokenQueue(NULL), mGrpMeta(), mMode(mode), mEnabledFlags(enableFlags), mReadonlyFlags(0), mDefaultsFlags(defaultFlags)
{
	/* Invoke the Qt Designer generated object setup routine */
	ui.setupUi(this);

	mTokenService = tokenService;
	mInternalTokenQueue = new TokenQueue(tokenService, this);
	mGrpMeta.mGroupId = groupId;

    std::set<RsPeerId> friends;
    init(friends);
}

UnseenGxsGroupDialog::~UnseenGxsGroupDialog()
{
	Settings->saveWidgetInformation(this);
	if (mInternalTokenQueue) {
		delete(mInternalTokenQueue);
	}
}

void UnseenGxsGroupDialog::init(const std::set<RsPeerId>& peer_list)
{
	// connect up the buttons.
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(submitGroup()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(cancelDialog()));
    ui.typeGroup->setChecked(true);
    ui.commentGroupBox->setVisible(false);

    setDefaultOptions();

    connect(ui.typeOne2One, SIGNAL(clicked()), this , SLOT(setDefaultOptions()));
    connect(ui.typeGroup, SIGNAL(clicked()), this , SLOT(setDefaultOptions()));
    connect(ui.typeChannel, SIGNAL(clicked()), this , SLOT(setDefaultOptions()));

    /* initialize key share list */
    ui.keyShareList->setHeaderText(tr("Contacts:"));
    ui.keyShareList->setModus(FriendSelectionWidget::MODUS_CHECK);
    ui.keyShareList->setShowType(FriendSelectionWidget::SHOW_GROUP | FriendSelectionWidget::SHOW_SSL);
    ui.keyShareList->start();
    ui.keyShareList->setSelectedIds<RsPeerId,FriendSelectionWidget::IDTYPE_SSL>(peer_list, false);

	initMode();
	Settings->loadWidgetInformation(this);
}

QIcon UnseenGxsGroupDialog::serviceWindowIcon()
{
	return qApp->windowIcon();
}

void UnseenGxsGroupDialog::showEvent(QShowEvent*)
{
	ui.headerFrame->setHeaderImage(serviceImage());
	setWindowIcon(serviceWindowIcon());

	initUi();
}

void UnseenGxsGroupDialog::setUiText(UiType uiType, const QString &text)
{
	switch (uiType)
	{
	case UITYPE_SERVICE_HEADER:
		setWindowTitle(text);
		ui.headerFrame->setHeaderText(text);
		break;
	case UITYPE_KEY_SHARE_CHECKBOX:
        //ui.pubKeyShare_cb->setText(text);
		break;
	case UITYPE_CONTACTS_DOCK:
	case UITYPE_ADD_ADMINS_CHECKBOX:
		//ui.contactsdockWidget->setWindowTitle(text);
		break;
	case UITYPE_BUTTONBOX_OK:
        //ui.buttonBox->button(QDialogButtonBox::Ok)->setText(text);
		break;
	}
}

void UnseenGxsGroupDialog::setUiToolTip(UiType uiType, const QString &text)
{
	switch (uiType)
	{
	case UITYPE_KEY_SHARE_CHECKBOX:
        //ui.pubKeyShare_cb->setToolTip(text);
		break;
	case UITYPE_ADD_ADMINS_CHECKBOX:
        //ui.addAdmins_cb->setToolTip(text);
		break;
	case UITYPE_BUTTONBOX_OK:
        //ui.buttonBox->button(QDialogButtonBox::Ok)->setToolTip(text);
    default:
		break;
	}
}

void UnseenGxsGroupDialog::initMode()
{
	setAllReadonly();
	switch (mode())
	{
		case MODE_CREATE:
		{
            ui.stackedWidget->setCurrentIndex(0);
			ui.buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
			newGroup();
		}
		break;

		case MODE_SHOW:
		{
			ui.stackedWidget->setCurrentIndex(1);
			mReadonlyFlags = 0xffffffff; // Force all to readonly.
			ui.buttonBox->setStandardButtons(QDialogButtonBox::Close);
			requestGroup(mGrpMeta.mGroupId);
		}
		break;

		case MODE_EDIT:
		{
            ui.stackedWidget->setCurrentIndex(0);
			ui.buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
            //ui.buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Submit Group Changes"));
			requestGroup(mGrpMeta.mGroupId);
		}
		break;
	}
}

void UnseenGxsGroupDialog::clearForm()
{
	ui.groupName->clear();
    //ui.groupDesc->clear();
	ui.groupName->setFocus();
}

void UnseenGxsGroupDialog::setupDefaults()
{
	/* Enable / Show Parts based on Flags */	


	if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PUBLISH_MASK)
	{
		if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PUBLISH_ENCRYPTED)
		{
            //ui.publish_encrypt->setChecked(true);
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PUBLISH_REQUIRED)
		{
            //ui.publish_required->setChecked(true);
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PUBLISH_THREADS)
		{
           //ui.publish_threads->setChecked(true);
		}
		else
		{
			// default
            //ui.publish_open->setChecked(true);
		}
	}

	if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PERSONAL_MASK)
	{
		if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PERSONAL_PGP)
		{
            //ui.personal_pgp->setChecked(true);
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PERSONAL_REQUIRED)
		{
            //ui.personal_required->setChecked(true);
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_PERSONAL_IFNOPUB)
		{
            //ui.personal_ifnopub->setChecked(true);
		}
		else
		{
			// default
            //ui.personal_ifnopub->setChecked(true);
		}
	}

	if (mDefaultsFlags & GXS_GROUP_DEFAULTS_COMMENTS_MASK)
	{
		if (mDefaultsFlags & GXS_GROUP_DEFAULTS_COMMENTS_YES)
		{
			ui.comments_allowed->setChecked(true);
			ui.commentsValueLabel->setText(tr("Allowed"));
		}
		else if (mDefaultsFlags & GXS_GROUP_DEFAULTS_COMMENTS_NO)
		{
			ui.comments_no->setChecked(true);
			ui.commentsValueLabel->setText(tr("Disallowed"));
		}
		else
		{
			// default
			ui.comments_no->setChecked(true);
			ui.commentsValueLabel->setText(tr("Allowed"));
		}
	}
        
        QString antispam_string ;
        if(mDefaultsFlags & GXS_GROUP_DEFAULTS_ANTISPAM_TRACK) antispam_string += tr("Message tracking") ;
	if(mDefaultsFlags & GXS_GROUP_DEFAULTS_ANTISPAM_FAVOR_PGP) antispam_string += (antispam_string.isNull()?"":" and ")+tr("PGP signature required") ;
    
    	ui.antiSpamValueLabel->setText(antispam_string) ;
        
}

void UnseenGxsGroupDialog::setupVisibility()
{
    ui.groupName->setVisible(true);
    ui.distribGroupBox->setVisible(true);
    ui.typeGroup->setVisible(true);
    ui.commentGroupBox->setVisible(false);
    ui.label_2->setVisible(true);
    ui.keyShareList->setVisible(true);

    ui.label->setVisible(true);

//	ui.extraFrame->setVisible(mEnabledFlags & GXS_GROUP_FLAGS_EXTRA);
}

void UnseenGxsGroupDialog::setAllReadonly()
{
	uint32_t origReadonlyFlags = mReadonlyFlags;
	mReadonlyFlags = 0xffffffff;

	setupReadonly();

	mReadonlyFlags = origReadonlyFlags;
}

void UnseenGxsGroupDialog::setupReadonly()
{

}

void UnseenGxsGroupDialog::newGroup()
{
	setupDefaults();
	setupVisibility();
	setupReadonly();
	clearForm();
}

void UnseenGxsGroupDialog::updateFromExistingMeta(const QString &description)
{
    std::cerr << "void GxsGroupDialog::updateFromExistingMeta()";
    std::cerr << std::endl;

    std::cerr << "void GxsGroupDialog::updateFromExistingMeta() mGrpMeta.mCircleType: ";
    std::cerr << mGrpMeta.mCircleType << " Internal: " << mGrpMeta.mInternalCircle;
    std::cerr << " External: " << mGrpMeta.mCircleId;
    std::cerr << std::endl;

    setupDefaults();
    setupVisibility();
    setupReadonly();
    clearForm();
    setGroupSignFlags(mGrpMeta.mSignFlags) ;

    /* setup name */
    ui.groupName->setText(QString::fromUtf8(mGrpMeta.mGroupName.c_str()));

    /* Show Mode */
    ui.nameline->setText(QString::fromUtf8(mGrpMeta.mGroupName.c_str()));
    ui.popline->setText(QString::number( mGrpMeta.mPop)) ;
    ui.postsline->setText(QString::number(mGrpMeta.mVisibleMsgCount));
    if(mGrpMeta.mLastPost==0)
        ui.lastpostline->setText(tr("Never"));
    else
        ui.lastpostline->setText(DateTime::formatLongDateTime(mGrpMeta.mLastPost));
    ui.authorLabel->setId(mGrpMeta.mAuthorId);
    ui.IDline->setText(QString::fromStdString(mGrpMeta.mGroupId.toStdString()));
    ui.descriptiontextEdit->setPlainText(description);

    switch (mode())
    {
    case MODE_CREATE:{
    }
        break;
    case MODE_SHOW:{
        ui.headerFrame->setHeaderText(QString::fromUtf8(mGrpMeta.mGroupName.c_str()));
        if (!mPicture.isNull())
            ui.headerFrame->setHeaderImage(mPicture);
    }
        break;
    case MODE_EDIT:{
    }
        break;
    }
    /* set description */
//    ui.groupDesc->setPlainText(description);
    QString distribution_string = "[Unknown]";
    ui.distributionValueLabel->setText(distribution_string) ;

    setDefaultOptions();
}

void UnseenGxsGroupDialog::submitGroup()
{
	std::cerr << "GxsGroupDialog::submitGroup()";
	std::cerr << std::endl;

	/* switch depending on mode */
	switch (mode())
	{
		case MODE_CREATE:
		{
			/* just close if down */
			createGroup();
		}
		break;

		case MODE_SHOW:
		{
			/* just close if down */
			cancelDialog();
		}
		break;

		case MODE_EDIT:
		{
			editGroup();
		}
		break;
	}
}

void UnseenGxsGroupDialog::editGroup()
{
	std::cerr << "GxsGroupDialog::editGroup()" << std::endl;

	RsGroupMetaData newMeta;
	newMeta.mGroupId = mGrpMeta.mGroupId;

	if(!prepareGroupMetaData(newMeta))
	{
		/* error message */
        QMessageBox::warning(this, "UnseenP2P", tr("Failed to Prepare Group MetaData - please Review"), QMessageBox::Ok, QMessageBox::Ok);
		return; //Don't add  a empty name!!
	}

	std::cerr << "GxsGroupDialog::editGroup() calling service_EditGroup";
	std::cerr << std::endl;

	uint32_t token;
	if (service_EditGroup(token, newMeta))
	{
		// get the Queue to handle response.
		if(mExternalTokenQueue != NULL)
			mExternalTokenQueue->queueRequest(token, TOKENREQ_GROUPINFO, RS_TOKREQ_ANSTYPE_ACK, GXSGROUP_NEWGROUPID);
	}
	else
	{
		std::cerr << "GxsGroupDialog::editGroup() ERROR";
		std::cerr << std::endl;
	}

	close();
}

bool UnseenGxsGroupDialog::prepareGroupMetaData(RsGroupMetaData &meta)
{
	std::cerr << "GxsGroupDialog::prepareGroupMetaData()";
	std::cerr << std::endl;

    QString name;
    if(chatType!= RsGxsChatGroup::ONE2ONE)
    {
        name = getName();
    }
    else
    {
        RsPeerDetails detail;
        for(std::set<RsPeerId>::const_iterator it(mShareFriends.begin());it!=mShareFriends.end();++it)
        {
            if (rsPeers->getPeerDetails( *it, detail))
            {
                name = QString::fromStdString(detail.name);
                break;
                //QMessageBox::warning(this, "UnseenP2P", tr("You want to chat with ") + name, QMessageBox::Ok, QMessageBox::Ok);
            }
        }
    }
	uint32_t flags = GXS_SERV::FLAG_PRIVACY_PUBLIC;

    if(chatType!= RsGxsChatGroup::ONE2ONE && name.isEmpty()) {
		std::cerr << "GxsGroupDialog::prepareGroupMetaData()";
		std::cerr << " Invalid GroupName";
		std::cerr << std::endl;
		return false;
	}

	// Fill in the MetaData as best we can.
	meta.mGroupName = std::string(name.toUtf8());

	meta.mGroupFlags = flags;
	meta.mSignFlags = getGroupSignFlags();

//	if (!setCircleParameters(meta)){
//		std::cerr << "GxsGroupDialog::prepareGroupMetaData()";
//		std::cerr << " Invalid Circles";
//		std::cerr << std::endl;
//		return false;
//	}

	std::cerr << "void GxsGroupDialog::prepareGroupMetaData() meta.mCircleType: ";
	std::cerr << meta.mCircleType << " Internal: " << meta.mInternalCircle;
	std::cerr << " External: " << meta.mCircleId;
	std::cerr << std::endl;

	return true;
}

void UnseenGxsGroupDialog::createGroup()
{
	std::cerr << "GxsGroupDialog::createGroup()";
	std::cerr << std::endl;

    if (ui.typeOne2One->isChecked())
    {
        chatType = RsGxsChatGroup::ONE2ONE;
    }
    else if (ui.typeGroup->isChecked())
    {
        chatType = RsGxsChatGroup::GROUPCHAT;
    }
    else if (ui.typeChannel->isChecked())
    {
        chatType = RsGxsChatGroup::CHANNEL;
    }


    ui.keyShareList->selectedIds<RsPeerId,FriendSelectionWidget::IDTYPE_SSL>(mShareFriends, false);

	/* Check name */
	QString name = getName();
    if(name.isEmpty() && !ui.typeOne2One->isChecked())
	{
		/* error message */
        QMessageBox::warning(this, "UnseenP2P", tr("Please add a Name"), QMessageBox::Ok, QMessageBox::Ok);
		return; //Don't add  a empty name!!
	}

    if (mShareFriends.empty())
    {
        QMessageBox::warning(this, "UnseenP2P", tr("Please choose a contact for chat"), QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    if (ui.typeOne2One->isChecked())
    {
        // when choosing the one2one chat, the gxs name will be the name of choosing contact
        if (mShareFriends.size() >= 2)
        {
            QMessageBox::warning(this, "UnseenP2P", tr("Please choose only one contact for chat"), QMessageBox::Ok, QMessageBox::Ok);
            return;
        }
    }

	uint32_t token;
	RsGroupMetaData meta;
	if (!prepareGroupMetaData(meta))
	{
		/* error message */
        QMessageBox::warning(this, "UnseenP2P", tr("Failed to Prepare Group MetaData - please Review"), QMessageBox::Ok, QMessageBox::Ok);
		return; //Don't add with invalid circle.
	}

	if (service_CreateGroup(token, meta))
	{
		// get the Queue to handle response.
		if(mExternalTokenQueue != NULL)
			mExternalTokenQueue->queueRequest(token, TOKENREQ_GROUPINFO, RS_TOKREQ_ANSTYPE_ACK, GXSGROUP_NEWGROUPID);
	}
	close();
}
	
uint32_t UnseenGxsGroupDialog::getGroupSignFlags()
{
    /* grab from the ui options -> */
    uint32_t signFlags = 0;

    return signFlags;
}

void UnseenGxsGroupDialog::setGroupSignFlags(uint32_t signFlags)
{
                
        QString antispam_string ;
        if(signFlags & GXS_SERV::FLAG_AUTHOR_AUTHENTICATION_TRACK_MESSAGES) antispam_string += tr("Message tracking") ;
	if(signFlags & GXS_SERV::FLAG_AUTHOR_AUTHENTICATION_GPG_KNOWN)      antispam_string += (antispam_string.isNull()?"":" and ")+tr("PGP signature from known ID required") ;
    	else
	if(signFlags & GXS_SERV::FLAG_AUTHOR_AUTHENTICATION_GPG)            antispam_string += (antispam_string.isNull()?"":" and ")+tr("PGP signature required") ;
    
    ui.antiSpamValueLabel->setText(antispam_string) ;

	/* guess at comments */
	if ((signFlags & GXS_SERV::FLAG_GROUP_SIGN_PUBLISH_THREADHEAD) &&
	    (signFlags & GXS_SERV::FLAG_AUTHOR_AUTHENTICATION_IFNOPUBSIGN))
	{
        	// (cyril) very weird piece of code. Need to clear this up.
        
		ui.comments_allowed->setChecked(true);
        	ui.commentsValueLabel->setText("Allowed") ;
	}
	else
	{
		ui.comments_no->setChecked(true);
        	ui.commentsValueLabel->setText("Allowed") ;
	}
}

/**** Above logic is flawed, and will be removed shortly
 *
 *
 ****/

void UnseenGxsGroupDialog::setDefaultOptions()
{

    if (ui.typeOne2One->isChecked())
    {
        // Need to create one2one gxs chat, hide comment elements
        ui.commentGroupBox->setVisible(false);
        ui.label_2->setText("Select one friend with which you want to chat (one2one):");
        ui.label->setVisible(false);
        ui.groupName->setVisible(false);
    }
    else if (ui.typeGroup->isChecked())
    {
        // Need to create gxs group chat, hide comment elements
        ui.commentGroupBox->setVisible(false);
        ui.groupTypeComboBox->setCurrentIndex(0);
        ui.label_2->setText("Select the friends with which you want to group chat: ");
        ui.label->setVisible(true);
        ui.groupName->setVisible(true);
    }
    else if (ui.typeChannel->isChecked())
    {
        // Create channel, show the comment group elements
        ui.commentGroupBox->setVisible(true);
        ui.label_2->setText("Select the friends with which you want to make channel: ");
        ui.label->setVisible(true);
        ui.groupName->setVisible(true);
    }
}

bool UnseenGxsGroupDialog::setCircleParameters(RsGroupMetaData &meta)
{
    meta.mCircleType = GXS_CIRCLE_TYPE_PUBLIC;
    meta.mCircleId.clear();
    meta.mOriginator.clear();
    meta.mInternalCircle.clear();

    return true;
}

void UnseenGxsGroupDialog::cancelDialog()
{
	std::cerr << "GxsGroupDialog::cancelDialog() Should Close!";
	std::cerr << std::endl;

	close();
}

void UnseenGxsGroupDialog::addGroupLogo()
{
	QPixmap img = misc::getOpenThumbnailedPicture(this, tr("Load Group Logo"), 64, 64);
	
	if (img.isNull())
		return;

	setLogo(img);
}

QPixmap UnseenGxsGroupDialog::getLogo()
{
	return mPicture;
}

void UnseenGxsGroupDialog::setLogo(const QPixmap &pixmap)
{
	mPicture = pixmap;

	// to show the selected
//	ui.groupLogo->setPixmap(mPicture);
}

QString UnseenGxsGroupDialog::getName()
{
	return misc::removeNewLine(ui.groupName->text());
}

QString UnseenGxsGroupDialog::getDescription()
{
    return ui.groupName->text();
}

void UnseenGxsGroupDialog::getSelectedModerators(std::set<RsGxsId>& ids)
{
//	ui.adminsList->selectedIds<RsGxsId,FriendSelectionWidget::IDTYPE_GXS>(ids, true);
}

void UnseenGxsGroupDialog::setSelectedModerators(const std::set<RsGxsId>& ids)
{
//	ui.adminsList->setSelectedIds<RsGxsId,FriendSelectionWidget::IDTYPE_GXS>(ids, false);

	QString moderatorsListString ;
    RsIdentityDetails det;

    for(auto it(ids.begin());it!=ids.end();++it)
    {
		rsIdentity->getIdDetails(*it,det);

        if(!moderatorsListString.isNull())
            moderatorsListString += ", " ;

        moderatorsListString += det.mNickname.empty()?("[Unknown]"):QString::fromStdString(det.mNickname) ;
    }

	ui.moderatorsLabel->setText(moderatorsListString);
}

/***********************************************************************************
  Share Lists.
 ***********************************************************************************/

void UnseenGxsGroupDialog::sendShareList(std::string /*groupId*/)
{
	close();
}

void UnseenGxsGroupDialog::setAdminsList()
{
//	if (ui.addAdmins_cb->isChecked())
//    {
//		//this->resize(this->size().width() + ui.contactsdockWidget->size().width(), this->size().height());
//		ui.adminsList->show();
//	}
//    else
//    {  // hide share widget
//		ui.adminsList->hide();
//		//this->resize(this->size().width() - ui.contactsdockWidget->size().width(), this->size().height());
//	}
}

void UnseenGxsGroupDialog::setShareList()
{
//	if (ui.pubKeyShare_cb->isChecked()) {
//		QMessageBox::warning(this, "", "ToDo");
//		ui.pubKeyShare_cb->setChecked(false);
//	}
//	if (ui.pubKeyShare_cb->isChecked()){
//		this->resize(this->size().width() + ui.contactsdockWidget->size().width(), this->size().height());
//		ui.contactsdockWidget->show();
//	} else {  // hide share widget
//		ui.contactsdockWidget->hide();
//		this->resize(this->size().width() - ui.contactsdockWidget->size().width(), this->size().height());
//	}
}

/***********************************************************************************
  Loading Group.
 ***********************************************************************************/

void UnseenGxsGroupDialog::requestGroup(const RsGxsGroupId &groupId)
{
	RsTokReqOptions opts;
	opts.mReqType = GXS_REQUEST_TYPE_GROUP_DATA;

	std::list<RsGxsGroupId> groupIds;
	groupIds.push_back(groupId);

	std::cerr << "GxsGroupDialog::requestGroup() Requesting Group Summary(" << groupId << ")";
	std::cerr << std::endl;

	uint32_t token;
	if (mInternalTokenQueue)
		mInternalTokenQueue->requestGroupInfo(token, RS_TOKREQ_ANSTYPE_DATA, opts, groupIds, GXSGROUP_INTERNAL_LOADGROUP) ;
}

void UnseenGxsGroupDialog::loadGroup(uint32_t token)
{
	std::cerr << "GxsGroupDialog::loadGroup(" << token << ")";
	std::cerr << std::endl;

	QString description;
	if (service_loadGroup(token, mMode, mGrpMeta, description))
	{
		updateFromExistingMeta(description);
	}
}

void UnseenGxsGroupDialog::loadRequest(const TokenQueue *queue, const TokenRequest &req)
{
	std::cerr << "GxsGroupDialog::loadRequest() UserType: " << req.mUserType;
	std::cerr << std::endl;

	if (queue == mInternalTokenQueue)
	{
		/* now switch on req */
		switch(req.mUserType)
		{
			case GXSGROUP_INTERNAL_LOADGROUP:
				loadGroup(req.mToken);
				break;
			default:
				std::cerr << "GxsGroupDialog::loadGroup() UNKNOWN UserType ";
				std::cerr << std::endl;
				break;
		}
	}
}

void UnseenGxsGroupDialog::getShareFriends(std::set<RsPeerId> &shareList)
{
    shareList = mShareFriends;
}

RsGxsChatGroup::ChatType UnseenGxsGroupDialog::getChatType()
{
    return chatType;
}

