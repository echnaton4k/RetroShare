
/*
 * Retroshare Gxs Feed Item
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

#ifndef GXSCHATPOSTITEM
#define GXSCHATPOSTITEM


#include <QMetaType>

#include <retroshare/rsgxschats.h>
#include "gui/gxs/GxsFeedItem.h"

namespace Ui {
class GxsChatPostItem;
}

class FeedHolder;
class SubFileItem;

class GxsChatPostItem : public GxsFeedItem
{
    Q_OBJECT

public:
    // This one is used in NewFeed for incoming channel posts. Only the group and msg ids are known at this point.
    // It can be used for all apparences of channel posts. But in rder to merge comments from the previous versions of the post, the list of
    // previous posts should be supplied. It's optional. If not supplied only the comments of the new version will be displayed.

    GxsChatPostItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsGroupId &groupId, const RsGxsMessageId &messageId, bool isHome, bool autoUpdate, const std::set<RsGxsMessageId>& older_versions = std::set<RsGxsMessageId>());

    // This method can be called when additional information is known about the post. In this case, the widget will be initialized with some
    // minimap information from the post and completed when the use displays it, which shouldn't cost anything more.

    GxsChatPostItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsChatMsg& post, bool isHome, bool autoUpdate, const std::set<RsGxsMessageId>& older_versions = std::set<RsGxsMessageId>());

    //GxsChannelPostItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsChannelGroup &group, const RsGxsChannelPost &post, bool isHome, bool autoUpdate);
    //GxsChannelPostItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsChannelPost &post, bool isHome, bool autoUpdate);
    virtual ~GxsChatPostItem();

    bool setGroup(const RsGxsChatGroup &group, bool doFill = true);
    bool setPost(const RsGxsChatMsg &post, bool doFill = true);

    void setFileCleanUpWarning(uint32_t time_left);

    QString getTitleLabel();
    QString getMsgLabel();
    const std::list<SubFileItem *> &getFileItems() {return mFileItems; }

    bool isUnread() const ;

protected:
    void init(const RsGxsMessageId& messageId,const std::set<RsGxsMessageId>& older_versions);

    /* FeedItem */
    virtual void doExpand(bool open);
    virtual void expandFill(bool first);

    // This does nothing except triggering the loading of the post data and comments. This function is mainly used to detect
    // when the post is actually made visible.

    virtual void paintEvent(QPaintEvent *);

    /* GxsGroupFeedItem */
    virtual QString groupName();
    virtual void loadGroup(const uint32_t &token);
    virtual RetroShareLink::enumType getLinkType() { return RetroShareLink::TYPE_CHATS; }

    /* GxsFeedItem */
    virtual QString messageName();
    virtual void loadMessage(const uint32_t &token);
    virtual void loadComment(const uint32_t &token);

private slots:
    /* default stuff */
    void toggle();
    void readAndClearItem();
    void download();
    void play();
    void edit();
    void loadComments();

    void readToggled(bool checked);

    void unsubscribeChannel();
    void updateItem();

    void makeUpVote();
    void makeDownVote();

signals:
    void vote(const RsGxsGrpMsgIdPair& msgId, bool up);

private:
    void setup();
    void fill();
    void fillExpandFrame();
    void setReadStatus(bool isNew, bool isUnread);

private:
    bool mInFill;
    bool mCloseOnRead;
    bool mLoaded;

    RsGxsChatGroup mGroup;
    RsGxsChatMsg mPost;

    std::list<SubFileItem*> mFileItems;

    /** Qt Designer generated object */
    Ui::GxsChatPostItem *ui;
};

Q_DECLARE_METATYPE(RsGxsChatMsg)


#endif // GXSCHATPOSTITEM