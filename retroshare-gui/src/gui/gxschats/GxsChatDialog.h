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

#ifndef GXSCHATDIALOG_H
#define GXSCHATDIALOG_H


#include "gui/gxs/GxsGroupFrameDialog.h"
#include "gui/gxs/UnseenGxsGroupFrameDialog.h"
#define IMAGE_GXSCHANNELS       ":/home/img/Setting/groupchat_tr.svg"      //d

class GxsChatDialog : public UnseenGxsGroupFrameDialog
{
    Q_OBJECT

public:
    /** Default Constructor */
    GxsChatDialog(QWidget *parent = 0);
    /** Default Destructor */
    ~GxsChatDialog();

    virtual QIcon iconPixmap() const { return QIcon(IMAGE_GXSCHANNELS) ; } //MainPage
    virtual QString pageName() const { return tr("New Chats") ; } //MainPage
    virtual QString helpText() const { return ""; } //MainPage

    virtual UserNotify *getUserNotify(QObject *parent);

    void shareOnChannel(const RsGxsGroupId& channel_id, const QList<RetroShareLink>& file_link) ;
protected:
    /* GxsGroupFrameDialog */
    virtual RetroShareLink::enumType getLinkType() { return RetroShareLink::TYPE_CHATS; }
    virtual GroupFrameSettings::Type groupFrameSettingsType() { return GroupFrameSettings::Chats; }
    virtual QString getHelpString() const ;
    virtual void groupInfoToGroupItemInfo(const RsGroupMetaData &groupInfo, GroupItemInfo &groupItemInfo, const RsUserdata *userdata);
    virtual bool getDistantSearchResults(TurtleRequestId id, std::map<RsGxsGroupId,RsGxsGroupSummary>& group_infos);

    virtual TurtleRequestId distantSearch(const QString& search_string) ;
    virtual void checkRequestGroup(const RsGxsGroupId& grpId) ;


private slots:
    void toggleAutoDownload();
    void setDefaultDirectory();
    void setDownloadDirectory();
    void specifyDownloadDirectory();

private:
    /* GxsGroupFrameDialog */
    virtual QString text(TextType type);
    virtual QString icon(IconType type);
    virtual QString settingsGroupName() { return "ChatDialog"; }
    virtual UnseenGxsGroupDialog *createNewGroupDialog(TokenQueue *tokenQueue);
    virtual UnseenGxsGroupDialog *createGroupDialog(TokenQueue *tokenQueue, RsTokenService *tokenService, UnseenGxsGroupDialog::Mode mode,RsGxsChatGroup::ChatType chatType, RsGxsGroupId groupId);
    virtual int shareKeyType();
    virtual GxsMessageFrameWidget *createMessageFrameWidget(const RsGxsGroupId &groupId);
    virtual void groupTreeCustomActions(RsGxsGroupId grpId, int subscribeFlags, QList<QAction*> &actions);
    virtual RsGxsCommentService *getCommentService();
    virtual QWidget *createCommentHeaderWidget(const RsGxsGroupId &grpId, const RsGxsMessageId &msgId);
    virtual uint32_t requestGroupSummaryType() { return GXS_REQUEST_TYPE_GROUP_DATA; } // request complete group data
    virtual void loadGroupSummaryToken(const uint32_t &token, std::list<RsGroupMetaData> &groupInfo, RsUserdata* &userdata);
    virtual void loadGroupSummaryToken2(const uint32_t &token, std::list<RsGxsChatGroup> &groupInfo, RsUserdata* &userdata); //unseenep2p
};


#endif // GXSCHATDIALOG_H
