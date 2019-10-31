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

#include <QTimer>
#include <QFileInfo>
#include <QStyle>

#include "rshare.h"
#include "GxsChatPostItem.h"
#include "ui_GxsChatPostItem.h"

#include "FeedHolder.h"
#include "SubFileItem.h"
#include "util/misc.h"
#include "gui/RetroShareLink.h"
#include "util/HandleRichText.h"
#include "util/DateTime.h"
#include "util/stringutil.h"
#include "gui/gxschats/CreateGxsChatMsg.h"

#include <iostream>
#include <cmath>

/****
 * #define DEBUG_ITEM 1
 ****/

GxsChatPostItem::GxsChatPostItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsGroupId &groupId, const RsGxsMessageId &messageId, bool isHome, bool autoUpdate,const std::set<RsGxsMessageId>& older_versions) :
    GxsFeedItem(feedHolder, feedId, groupId, messageId, isHome, rsGxsChats, autoUpdate)
{
    init(messageId,older_versions) ;
}

GxsChatPostItem::GxsChatPostItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsChatMsg& post, bool isHome, bool autoUpdate,const std::set<RsGxsMessageId>& older_versions) :
    GxsFeedItem(feedHolder, feedId, post.mMeta.mGroupId, post.mMeta.mMsgId, isHome, rsGxsChats, autoUpdate)
{
    init(post.mMeta.mMsgId,older_versions) ;
    mPost = post ;
}

void GxsChatPostItem::init(const RsGxsMessageId& messageId,const std::set<RsGxsMessageId>& older_versions)
{
    QVector<RsGxsMessageId> v;
    //bool self = false;

    for(std::set<RsGxsMessageId>::const_iterator it(older_versions.begin());it!=older_versions.end();++it)
        v.push_back(*it) ;

    if(older_versions.find(messageId) == older_versions.end())
        v.push_back(messageId);

    setMessageVersions(v) ;

    setup();

    mLoaded = false ;
}

// This code has been suspended because it adds more complexity than usefulness.
// It was used to load a channel post where the post item is already known.

#ifdef SUSPENDED
GxsChatPostItem::GxsChatPostItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsChatGroup &group, const RsGxsChatMsg &post, bool isHome, bool autoUpdate) :
    GxsFeedItem(feedHolder, feedId, post.mMeta.mGroupId, post.mMeta.mMsgId, isHome, rsGxsChats, autoUpdate)
{
#ifdef DEBUG_ITEM
    std::cerr << "GxsChatPostItem::GxsChatPostItem() Direct Load";
    std::cerr << std::endl;
#endif

    QVector<RsGxsMessageId> v;
    bool self = false;

    for(std::set<RsGxsMessageId>::const_iterator it(post.mOlderVersions.begin());it!=post.mOlderVersions.end();++it)
    {
        if(*it == post.mMeta.mMsgId)
            self = true ;

        v.push_back(*it) ;
    }
    if(!self)
        v.push_back(post.mMeta.mMsgId);

    setMessageVersions(v) ;

    setup();

    setGroup(group, false);

    setPost(post,false);
    mLoaded = false ;
}

GxsChatPostItem::GxsChatPostItem(FeedHolder *feedHolder, uint32_t feedId, const RsGxsChatMsg &post, bool isHome, bool autoUpdate) :
    GxsFeedItem(feedHolder, feedId, post.mMeta.mGroupId, post.mMeta.mMsgId, isHome, rsGxsChats, autoUpdate)
{
#ifdef DEBUG_ITEM
    std::cerr << "GxsChatPostItem::GxsChatPostItem() Direct Load";
    std::cerr << std::endl;
#endif

    setup();

    mLoaded = true ;
    requestGroup();
    setPost(post);
    requestComment();
}
#endif


void GxsChatPostItem::paintEvent(QPaintEvent *e)
{
    /* This method employs a trick to trigger a deferred loading. The post and group is requested only
     * when actually displayed on the screen. */

    if(!mLoaded)
    {
        mLoaded = true ;

        requestGroup();
        requestMessage();
        requestComment();
    }

    GxsFeedItem::paintEvent(e) ;
}

GxsChatPostItem::~GxsChatPostItem()
{
    delete(ui);
}

bool GxsChatPostItem::isUnread() const
{
    return IS_MSG_UNREAD(mPost.mMeta.mMsgStatus) ;
}

void GxsChatPostItem::setup()
{
    /* Invoke the Qt Designer generated object setup routine */
    ui = new Ui::GxsChatPostItem;
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose, true);

    mInFill = false;
    mCloseOnRead = false;

    /* clear ui */
    ui->titleLabel->setText(tr("Loading"));
    ui->subjectLabel->clear();
    ui->datetimelabel->clear();
    ui->filelabel->clear();
    ui->newCommentLabel->hide();
    ui->commLabel->hide();

    /* general ones */
    connect(ui->expandButton, SIGNAL(clicked()), this, SLOT(toggle()));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(removeItem()));

    /* specific */
    connect(ui->readAndClearButton, SIGNAL(clicked()), this, SLOT(readAndClearItem()));
    connect(ui->unsubscribeButton, SIGNAL(clicked()), this, SLOT(unsubscribeChannel()));

    connect(ui->downloadButton, SIGNAL(clicked()), this, SLOT(download()));
    // HACK FOR NOW.
    connect(ui->commentButton, SIGNAL(clicked()), this, SLOT(loadComments()));

    connect(ui->playButton, SIGNAL(clicked()), this, SLOT(play(void)));
    connect(ui->editButton, SIGNAL(clicked()), this, SLOT(edit(void)));
    connect(ui->copyLinkButton, SIGNAL(clicked()), this, SLOT(copyMessageLink()));

    connect(ui->readButton, SIGNAL(toggled(bool)), this, SLOT(readToggled(bool)));

    // hide voting buttons, backend is not implemented yet
    ui->voteUpButton->hide();
    ui->voteDownButton->hide();
    //connect(ui-> voteUpButton, SIGNAL(clicked()), this, SLOT(makeUpVote()));
    //connect(ui->voteDownButton, SIGNAL(clicked()), this, SLOT(makeDownVote()));

    ui->scoreLabel->hide();

    ui->downloadButton->hide();
    ui->playButton->hide();
    ui->warn_image_label->hide();
    ui->warning_label->hide();

    ui->commentButton->hide();
    ui->titleLabel->setMinimumWidth(100);
    ui->subjectLabel->setMinimumWidth(100);
    ui->warning_label->setMinimumWidth(100);

    ui->mainFrame->setProperty("new", false);
    ui->mainFrame->style()->unpolish(ui->mainFrame);
    ui->mainFrame->style()->polish(  ui->mainFrame);

    ui->expandFrame->hide();

}

bool GxsChatPostItem::setGroup(const RsGxsChatGroup &group, bool doFill)
{
    if (groupId() != group.mMeta.mGroupId) {
        std::cerr << "GxsChatPostItem::setGroup() - Wrong id, cannot set post";
        std::cerr << std::endl;
        return false;
    }

    mGroup = group;

    // If not publisher, hide the edit button. Without the publish key, there's no way to edit a message.
#ifdef DEBUG_ITEM
    std::cerr << "Group subscribe flags = " << std::hex << mGroup.mMeta.mSubscribeFlags << std::dec << std::endl ;
#endif
    if( !IS_GROUP_PUBLISHER(mGroup.mMeta.mSubscribeFlags) )
        ui->editButton->hide() ;

    if (doFill) {
        fill();
    }

    return true;
}

bool GxsChatPostItem::setPost(const RsGxsChatMsg &post, bool doFill)
{
    if (groupId() != post.mMeta.mGroupId || messageId() != post.mMeta.mMsgId) {
        std::cerr << "GxsChatPostItem::setPost() - Wrong id, cannot set post";
        std::cerr << std::endl;
        return false;
    }

    mPost = post;

    if (doFill) {
        fill();
    }

    updateItem();

    return true;
}

QString GxsChatPostItem::getTitleLabel()
{
    return QString::fromUtf8(mPost.mMeta.mMsgName.c_str());
}

QString GxsChatPostItem::getMsgLabel()
{
    //return RsHtml().formatText(NULL, QString::fromUtf8(mPost.mMsg.c_str()), RSHTML_FORMATTEXT_EMBED_SMILEYS | RSHTML_FORMATTEXT_EMBED_LINKS);
    // Disabled, because emoticon replacement kills performance.
    return QString::fromUtf8(mPost.mMsg.c_str());
}

QString GxsChatPostItem::groupName()
{
    return QString::fromUtf8(mGroup.mMeta.mGroupName.c_str());
}

void GxsChatPostItem::loadComments()
{
    QString title = QString::fromUtf8(mPost.mMeta.mMsgName.c_str());
    comments(title);
}

void GxsChatPostItem::loadGroup(const uint32_t &token)
{
#ifdef DEBUG_ITEM
    std::cerr << "GxsChatGroupItem::loadGroup()";
    std::cerr << std::endl;
#endif

    std::vector<RsGxsChatGroup> groups;
    if (!rsGxsChats->getGroupData(token, groups))
    {
        std::cerr << "GxsChatGroupItem::loadGroup() ERROR getting data";
        std::cerr << std::endl;
        return;
    }

    if (groups.size() != 1)
    {
        std::cerr << "GxsChatGroupItem::loadGroup() Wrong number of Items";
        std::cerr << std::endl;
        return;
    }

    setGroup(groups[0]);
}

void GxsChatPostItem::loadMessage(const uint32_t &token)
{
#ifdef DEBUG_ITEM
    std::cerr << "GxsChatPostItem::loadMessage()";
    std::cerr << std::endl;
#endif

    std::vector<RsGxsChatMsg> posts;
    std::vector<RsGxsComment> cmts;
    if (!rsGxsChats->getPostData(token, posts, cmts))
    {
        std::cerr << "GxsChatPostItem::loadMessage() ERROR getting data";
        std::cerr << std::endl;
        return;
    }

    if (posts.size() == 1)
    {
        setPost(posts[0]);
    }
    else if (cmts.size() == 1)
    {
        RsGxsComment cmt = cmts[0];

        ui->newCommentLabel->show();
        ui->commLabel->show();
        ui->commLabel->setText(QString::fromUtf8(cmt.mComment.c_str()));

        //Change this item to be uploaded with thread element.
        setMessageId(cmt.mMeta.mThreadId);
        requestMessage();
    }
    else
    {
        std::cerr << "GxsChatPostItem::loadMessage() Wrong number of Items. Remove It.";
        std::cerr << std::endl;
        removeItem();
        return;
    }
}

void GxsChatPostItem::loadComment(const uint32_t &token)
{
#ifdef DEBUG_ITEM
    std::cerr << "GxsChatPostItem::loadComment()";
    std::cerr << std::endl;
#endif

//    std::vector<RsGxsComment> cmts;
//    if (!rsGxsChats->getRelatedComments(token, cmts))
//    {
//        std::cerr << "GxsChatPostItem::loadComment() ERROR getting data";
//        std::cerr << std::endl;
//        return;
//    }

//    size_t comNb = cmts.size();
//    QString sComButText = tr("Comment");
//    if (comNb == 1) {
//        sComButText = sComButText.append("(1)");
//    } else if (comNb > 1) {
//        sComButText = tr("Comments ").append("(%1)").arg(comNb);
//    }
//    ui->commentButton->setText(sComButText);
}

void GxsChatPostItem::fill()
{
    /* fill in */

    if (isLoading()) {
        /* Wait for all requests */
        return;
    }

#ifdef DEBUG_ITEM
    std::cerr << "GxsChatPostItem::fill()";
    std::cerr << std::endl;
#endif

    mInFill = true;

    QString title;

    if(mPost.mThumbnail.mData != NULL)
    {
        QPixmap thumbnail;
        thumbnail.loadFromData(mPost.mThumbnail.mData, mPost.mThumbnail.mSize, "PNG");
        // Wiping data - as its been passed to thumbnail.
        ui->logoLabel->setPixmap(thumbnail);
    }

    if (!mIsHome)
    {
        if (mCloseOnRead && !IS_MSG_NEW(mPost.mMeta.mMsgStatus)) {
            removeItem();
        }

        title = tr("Channel Feed") + ": ";
        RetroShareLink link = RetroShareLink::createGxsGroupLink(RetroShareLink::TYPE_CHANNEL, mPost.mMeta.mGroupId, groupName());
        title += link.toHtml();
        ui->titleLabel->setText(title);

        RetroShareLink msgLink = RetroShareLink::createGxsMessageLink(RetroShareLink::TYPE_CHANNEL, mPost.mMeta.mGroupId, mPost.mMeta.mMsgId, messageName());
        ui->subjectLabel->setText(msgLink.toHtml());

        if (IS_GROUP_SUBSCRIBED(mGroup.mMeta.mSubscribeFlags) || IS_GROUP_ADMIN(mGroup.mMeta.mSubscribeFlags))
        {
            ui->unsubscribeButton->setEnabled(true);
        }
        else
        {
            ui->unsubscribeButton->setEnabled(false);
        }
        ui->readButton->hide();
        ui->newLabel->hide();
        ui->copyLinkButton->hide();

        if (IS_MSG_NEW(mPost.mMeta.mMsgStatus)) {
            mCloseOnRead = true;
        }
    }
    else
    {
        /* subject */
        ui->titleLabel->setText(QString::fromUtf8(mPost.mMeta.mMsgName.c_str()));

        //uint32_t autorized_lines = (int)floor((ui->logoLabel->height() - ui->titleLabel->height() - ui->buttonHLayout->sizeHint().height())/QFontMetricsF(ui->subjectLabel->font()).height());

        // fill first 4 lines of message. (csoler) Disabled the replacement of smileys and links, because the cost is too crazy
        //ui->subjectLabel->setText(RsHtml().formatText(NULL, RsStringUtil::CopyLines(QString::fromUtf8(mPost.mMsg.c_str()), autorized_lines), RSHTML_FORMATTEXT_EMBED_SMILEYS | RSHTML_FORMATTEXT_EMBED_LINKS));

        ui->subjectLabel->hide();
        //ui->subjectLabel->setText(RsStringUtil::CopyLines(QString::fromUtf8(mPost.mMsg.c_str()), 2)) ;

        //QString score = QString::number(post.mTopScore);
        // scoreLabel->setText(score);

        /* disable buttons: deletion facility not enabled with cache services yet */
        ui->clearButton->setEnabled(false);
        ui->unsubscribeButton->setEnabled(false);
        ui->clearButton->hide();
        ui->readAndClearButton->hide();
        ui->unsubscribeButton->hide();
        ui->copyLinkButton->show();

        if (IS_GROUP_SUBSCRIBED(mGroup.mMeta.mSubscribeFlags) || IS_GROUP_ADMIN(mGroup.mMeta.mSubscribeFlags))
        {
            ui->readButton->setVisible(true);

            setReadStatus(IS_MSG_NEW(mPost.mMeta.mMsgStatus), IS_MSG_UNREAD(mPost.mMeta.mMsgStatus) || IS_MSG_NEW(mPost.mMeta.mMsgStatus));
        }
        else
        {
            ui->readButton->setVisible(false);
            ui->newLabel->setVisible(false);
        }

        mCloseOnRead = false;
    }

    // differences between Feed or Top of Comment.
    if (mFeedHolder)
    {
        if (mIsHome) {
            //ui->commentButton->show();
            ui->commentButton->hide();
        } else if (ui->commentButton->icon().isNull()){
            //Icon is seted if a comment received.
            ui->commentButton->hide();
        }

// THIS CODE IS doesn't compile - disabling until fixed.
#if 0
        if (post.mComments)
        {
            QString commentText = QString::number(post.mComments);
            commentText += " ";
            commentText += tr("Comments");
            ui->commentButton->setText(commentText);
        }
        else
        {
            ui->commentButton->setText(tr("Comment"));
        }
#endif

    }
    else
    {
        ui->commentButton->hide();
    }

    // disable voting buttons - if they have already voted.
    /*if (post.mMeta.mMsgStatus & GXS_SERV::GXS_MSG_STATUS_VOTE_MASK)
    {
        voteUpButton->setEnabled(false);
        voteDownButton->setEnabled(false);
    }*/

    ui->msgFrame->setVisible(!mPost.mMsg.empty());
    if (wasExpanded() || ui->expandFrame->isVisible()) {
        fillExpandFrame();
    }

    ui->datetimelabel->setText(DateTime::formatLongDateTime(mPost.mMeta.mPublishTs));

//    if ( (mPost.mCount != 0) || (mPost.mSize != 0) ) {
//        ui->filelabel->setVisible(true);
//        ui->filelabel->setText(QString("(%1 %2) %3").arg(mPost.mCount).arg(tr("Files")).arg(misc::friendlyUnit(mPost.mSize)));
//    } else {
//        ui->filelabel->setVisible(false);
//    }
    ui->filelabel->setVisible(false);

    if (mFileItems.empty() == false) {
        std::list<SubFileItem *>::iterator it;
        for(it = mFileItems.begin(); it != mFileItems.end(); ++it)
        {
            delete(*it);
        }
        mFileItems.clear();
    }

    std::list<RsGxsFile>::const_iterator it;
    for(it = mPost.mFiles.begin(); it != mPost.mFiles.end(); ++it)
    {
        /* add file */
        std::string path;
        SubFileItem *fi = new SubFileItem(it->mHash, it->mName, path, it->mSize, SFI_STATE_REMOTE | SFI_TYPE_CHANNEL, RsPeerId());
        mFileItems.push_back(fi);

        /* check if the file is a media file */
        if (!misc::isPreviewable(QFileInfo(QString::fromUtf8(it->mName.c_str())).suffix()))
        {
        fi->mediatype();
                /* check if the file is not a media file and change text */
        ui->playButton->setText(tr("Open"));
        ui->playButton->setToolTip(tr("Open File"));
    } else {
        ui->playButton->setText(tr("Play"));
        ui->playButton->setToolTip(tr("Play Media"));
    }

        QLayout *layout = ui->expandFrame->layout();
        layout->addWidget(fi);
    }

    expand(true);
    mInFill = false;
}

void GxsChatPostItem::fillExpandFrame()
{
    ui->msgLabel->setText(RsHtml().formatText(NULL, QString::fromUtf8(mPost.mMsg.c_str()), RSHTML_FORMATTEXT_EMBED_SMILEYS | RSHTML_FORMATTEXT_EMBED_LINKS));
}

QString GxsChatPostItem::messageName()
{
    return QString::fromUtf8(mPost.mMeta.mMsgName.c_str());
}

void GxsChatPostItem::setReadStatus(bool isNew, bool isUnread)
{
    if (isUnread)
    {
        ui->readButton->setChecked(true);
        ui->readButton->setIcon(QIcon(":/images/message-state-unread.png"));
    }
    else
    {
        ui->readButton->setChecked(false);
        ui->readButton->setIcon(QIcon(":/images/message-state-read.png"));
    }

    ui->newLabel->setVisible(isNew);

    ui->mainFrame->setProperty("new", isNew);
    ui->mainFrame->style()->unpolish(ui->mainFrame);
    ui->mainFrame->style()->polish(  ui->mainFrame);
}

void GxsChatPostItem::setFileCleanUpWarning(uint32_t time_left)
{
    int hours = (int)time_left/3600;
    int minutes = (time_left - hours*3600)%60;

    ui->warning_label->setText(tr("Warning! You have less than %1 hours and %2 minute before this file is deleted Consider saving it.").arg(
            QString::number(hours)).arg(QString::number(minutes)));

    QFont warnFont = ui->warning_label->font();
    warnFont.setBold(true);
    ui->warning_label->setFont(warnFont);

    ui->warn_image_label->setVisible(true);
    ui->warning_label->setVisible(true);
}

void GxsChatPostItem::updateItem()
{
    /* fill in */

#ifdef DEBUG_ITEM
    std::cerr << "GxsChatPostItem::updateItem()";
    std::cerr << std::endl;
#endif

    int msec_rate = 10000;

    int downloadCount = 0;
    int downloadStartable = 0;
    int playCount = 0;
    int playStartable = 0;
    bool startable;
    bool loopAgain = false;

    /* Very slow Tick to check when all files are downloaded */
    std::list<SubFileItem *>::iterator it;
    for(it = mFileItems.begin(); it != mFileItems.end(); ++it)
    {
        SubFileItem *item = *it;

        if (item->isDownloadable(startable)) {
            ++downloadCount;
            if (startable) {
                ++downloadStartable;
            }
        }
        if (item->isPlayable(startable)) {
            ++playCount;
            if (startable) {
                ++playStartable;
            }
        }

        if (!item->done())
        {
            /* loop again */
            loopAgain = true;
        }
    }

    if (downloadCount) {
        ui->downloadButton->show();

        if (downloadStartable) {
            ui->downloadButton->setEnabled(true);
        } else {
            ui->downloadButton->setEnabled(false);
        }
    } else {
        ui->downloadButton->hide();
    }
    if (playCount) {
        /* one file is playable */
        ui->playButton->show();

        if (playStartable == 1) {
            ui->playButton->setEnabled(true);
        } else {
            ui->playButton->setEnabled(false);
        }
    } else {
        ui->playButton->hide();
    }

    if (loopAgain) {
        QTimer::singleShot( msec_rate, this, SLOT(updateItem(void)));
    }

    // HACK TO DISPLAY COMMENT BUTTON FOR NOW.
    //downloadButton->show();
    //downloadButton->setEnabled(true);
}

void GxsChatPostItem::doExpand(bool open)
{
    if (mFeedHolder)
    {
        mFeedHolder->lockLayout(this, true);
    }

    if (open)
    {
        ui->expandFrame->show();
        ui->expandButton->setIcon(QIcon(QString(":/images/edit_remove24.png")));
        ui->expandButton->setToolTip(tr("Hide"));

        readToggled(false);
    }
    else
    {
        ui->expandFrame->hide();
        ui->expandButton->setIcon(QIcon(QString(":/images/edit_add24.png")));
        ui->expandButton->setToolTip(tr("Expand"));
    }

    emit sizeChanged(this);

    if (mFeedHolder)
    {
        mFeedHolder->lockLayout(this, false);
    }
}

void GxsChatPostItem::expandFill(bool first)
{
    GxsFeedItem::expandFill(first);

    if (first) {
        fillExpandFrame();
    }
}

void GxsChatPostItem::toggle()
{
    expand(ui->expandFrame->isHidden());
}

/*********** SPECIFIC FUNCTIONS ***********************/

void GxsChatPostItem::readAndClearItem()
{
#ifdef DEBUG_ITEM
    std::cerr << "GxsChatPostItem::readAndClearItem()";
    std::cerr << std::endl;
#endif

    readToggled(false);
    removeItem();
}

void GxsChatPostItem::unsubscribeChannel()
{
#ifdef DEBUG_ITEM
    std::cerr << "GxsChatPostItem::unsubscribeChannel()";
    std::cerr << std::endl;
#endif

    unsubscribe();
}

void GxsChatPostItem::download()
{
    std::list<SubFileItem *>::iterator it;
    for(it = mFileItems.begin(); it != mFileItems.end(); ++it)
    {
        (*it)->download();
    }

    updateItem();
}

void GxsChatPostItem::edit()
{
    CreateGxsChatMsg *msgDialog = new CreateGxsChatMsg(mGroup.mMeta.mGroupId,mPost.mMeta.mMsgId);
    msgDialog->show();
}

void GxsChatPostItem::play()
{
    std::list<SubFileItem *>::iterator it;
    for(it = mFileItems.begin(); it != mFileItems.end(); ++it)
    {
        bool startable;
        if ((*it)->isPlayable(startable) && startable) {
            (*it)->play();
        }
    }
}

void GxsChatPostItem::readToggled(bool checked)
{
    if (mInFill) {
        return;
    }

    mCloseOnRead = false;

    RsGxsGrpMsgIdPair msgPair = std::make_pair(groupId(), messageId());

    uint32_t token;
    rsGxsChats->setMessageReadStatus(token, msgPair, !checked);

    setReadStatus(false, checked);
}

void GxsChatPostItem::makeDownVote()
{
    RsGxsGrpMsgIdPair msgId;
    msgId.first = mPost.mMeta.mGroupId;
    msgId.second = mPost.mMeta.mMsgId;

    ui->voteUpButton->setEnabled(false);
    ui->voteDownButton->setEnabled(false);

    emit vote(msgId, false);
}

void GxsChatPostItem::makeUpVote()
{
    RsGxsGrpMsgIdPair msgId;
    msgId.first = mPost.mMeta.mGroupId;
    msgId.second = mPost.mMeta.mMsgId;

    ui->voteUpButton->setEnabled(false);
    ui->voteDownButton->setEnabled(false);

    emit vote(msgId, true);
}