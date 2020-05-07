/***************************************************************************
* Copyright (C) 2017-2019 by Savoir-faire Linux                                *
* Author: Anthony L�onard <anthony.leonard@savoirfairelinux.com>          *
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

//#include "gui/common/UnseenGroupTreeWidget.h"
// Qt include
#include <QAbstractItemModel>
#include <QDateTime>
#include <QIcon>
#include "retroshare/rsgxschats.h"
//#include "retroshare/rsmsgs.h"

class UnseenGroupItemInfo
{
public:
    UnseenGroupItemInfo()
      : popularity(0), publishKey(false), adminKey(false)
      , subscribeFlags(0), max_visible_posts(0)
    {}

public:
    QString   id;
    QString   name;
    QString   description;
    int       popularity;
    QDateTime lastpost;
    QIcon     icon;
    bool      publishKey;
    bool      adminKey;
    quint32  subscribeFlags;
    quint32  max_visible_posts ;
    long long lastMsgDatetime;  //unseenp2p: for sorting: lastpost --> QDateTime::currentDateTime().toTime_t()
    QString authorOfLastMsg;    //unseenp2p: for show on the gxs conversation list
    std::set<GxsChatMember> members; //unseenp2p
    RsGxsChatGroup::ChatType type;  //unseenp2p
    /////////////////// all these following using for the gxs conversation list (model-view GUI) //////////
    std::string nickInGroupChat;        // unseenp2p, only in groupchat
    unsigned int UnreadMessagesCount = 0; //unseenp2p, using from the conversationItem
    std::string LastInteractionDate;    // unseenp2p, using from the conversationItem: date for last message
    std::string lastMessage;            //unseenp2p, using from the conversationItem
    bool isOtherLastMsg;            //unseenp2p, using from the conversationItem: true if receiving msg, false if sending msg

    RsGxsGroupId gxsGroupId;
    LocalGroupInfo localMsgInfo;
    /*
     * std::string displayName;            //group name or contact name
    std::string nickInGroupChat;        // only in groupchat
    unsigned int UnreadMessagesCount = 0;
    std::string LastInteractionDate;    // date for last message
    long long lastMsgDatetime;               // QDateTime::currentDateTime().toTime_t()
    std::string lastMessage;
    bool isOtherLastMsg;            // true if receiving msg, false if sending msg
    int contactType;                // 0 - groupchat, 1 - contact chat
    int groupChatType;              // 0 - public, 1: private
    std::string rsPeerIdStr;            // for contact (chatId.toPeerId) and groupchat (chatId.toLobbyId)
    ChatLobbyId chatLobbyId;
    std::string uId;
     */
};

class UnseenGxsSmartListModel : public QAbstractItemModel
{
    Q_OBJECT
public:


    enum Role {
        DisplayName = Qt::UserRole + 1,
        DisplayID,
        Picture,
        Presence,
        URI,
        UnreadMessagesCount,
        LastInteractionDate,
        LastInteraction,
        LastInteractionType,
        ContactType,
        UID,
        ContextMenuOpen
    };

    explicit UnseenGxsSmartListModel(const std::string& accId, QObject *parent = 0, bool contactList = false);

    // QAbstractItemModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    void setAccount(const std::string& accId);

    // hack for context menu highlight retention
    bool isContextMenuOpen{ false };
    std::vector<UnseenGroupItemInfo> getGxsGroupList();
    std::vector<RsGxsChatGroup> getGxsChatGroupList();
    void setGxsGroupList(std::vector<UnseenGroupItemInfo> allList);
    void setFilterGxsGroupListAndMode(std::vector<UnseenGroupItemInfo> allList, uint32_t mode);
    void setFilterGxsChatGroupListAndMode(std::vector<RsGxsChatGroup> allList, uint32_t mode);
    void sortGxsConversationListByRecentTime();
    void setGxsChatGroupList(std::vector<RsGxsChatGroup> allList);

private:
    std::string accId_;
    bool contactList_;
    std::vector<UnseenGroupItemInfo> allGxsGroupList;
    std::vector<UnseenGroupItemInfo> filterGxsGroupList;
    uint32_t conversationMode;
    std::vector<RsGxsChatGroup> allGxsChatGroupList;
    std::vector<RsGxsChatGroup> filteredGxsChatGroupList;
};
