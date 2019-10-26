#ifndef P3_GXSCHATS_SERVICE_HEADER
#define P3_GXSCHATS_SERVICE_HEADER

#include "retroshare/rsgxschats.h"
#include "services/p3gxscommon.h"
#include "gxs/rsgenexchange.h"
#include "gxs/gxstokenqueue.h"

#include "util/rstickevent.h"

#include <map>
#include <string>

class SSGxsChatGroup
{
    public:
    SSGxsChatGroup(): mAutoDownload(false), mDownloadDirectory("") {}
    bool load(const std::string &input);
    std::string save() const;

    bool mAutoDownload;
    std::string mDownloadDirectory;
};

class p3GxsChats: public RsGenExchange, public RsGxsChats,
    public GxsTokenQueue, public p3Config,
    public RsTickEvent	/* only needed for testing - remove after */
{
public:
    p3GxsChats( RsGeneralDataService* gds, RsNetworkExchangeService* nes,
                   RsGixs* gixs );
    virtual RsServiceInfo getServiceInfo();

    virtual void service_tick();

protected:


    virtual RsSerialiser* setupSerialiser();                            // @see p3Config::setupSerialiser()
    virtual bool saveList(bool &cleanup, std::list<RsItem *>&saveList); // @see p3Config::saveList(bool &cleanup, std::list<RsItem *>&)
    virtual bool loadList(std::list<RsItem *>& loadList);               // @see p3Config::loadList(std::list<RsItem *>&)

    virtual TurtleRequestId turtleGroupRequest(const RsGxsGroupId& group_id);
    virtual TurtleRequestId turtleSearchRequest(const std::string& match_string);
    virtual bool retrieveDistantSearchResults(TurtleRequestId req, std::map<RsGxsGroupId, RsGxsGroupSummary> &results) ;
    virtual bool clearDistantSearchResults(TurtleRequestId req);
    virtual bool retrieveDistantGroup(const RsGxsGroupId& group_id,RsGxsChatGroup& distant_group);

    // Overloaded to cache new groups.
    // no tokens... should be cached.
    virtual bool setChannelAutoDownload(const RsGxsGroupId &groupId, bool enabled);
    virtual	bool getChannelAutoDownload(const RsGxsGroupId &groupid, bool& enabled);
    virtual bool setChannelDownloadDirectory(const RsGxsGroupId &groupId, const std::string& directory);
    virtual bool getChannelDownloadDirectory(const RsGxsGroupId &groupId, std::string& directory);



virtual RsGenExchange::ServiceCreate_Return service_CreateGroup(RsGxsGrpItem* grpItem, RsTlvSecurityKeySet& keySet);

virtual void notifyChanges(std::vector<RsGxsNotify*>& changes);

        // Overloaded from RsTickEvent.
virtual void handle_event(uint32_t event_type, const std::string &elabel);

public:

virtual bool getGroupData(const uint32_t &token, std::vector<RsGxsChatGroup> &groups);
virtual bool getPostData(const uint32_t &token, std::vector<RsGxsChatMsg> &posts, std::vector<RsGxsComment> &cmts);
virtual bool getPostData(const uint32_t &token, std::vector<RsGxsChatMsg> &posts) {	std::vector<RsGxsComment> cmts; return getPostData( token, posts, cmts);}
//Not currently used
//virtual bool getRelatedPosts(const uint32_t &token, std::vector<RsGxsChannelPost> &posts);

        //////////////////////////////////////////////////////////////////////////////

//virtual bool setMessageStatus(const std::string &msgId, const uint32_t status, const uint32_t statusMask);
//virtual bool setGroupSubscribeFlags(const std::string &groupId, uint32_t subscribeFlags, uint32_t subscribeMask);

//virtual bool groupRestoreKeys(const std::string &groupId);
    virtual bool groupShareKeys(
            const RsGxsGroupId &groupId, const std::set<RsPeerId>& peers);

virtual bool createGroup(uint32_t &token, RsGxsChatGroup &group);
virtual bool createPost(uint32_t &token, RsGxsChatMsg &post);

virtual bool updateGroup(uint32_t &token, RsGxsChatGroup &group);



    /// @see RsGxsChannels::turtleSearchRequest
    virtual bool turtleSearchRequest(const std::string& matchString,
            const std::function<void (const RsGxsGroupSummary&)>& multiCallback,
            rstime_t maxWait = 300 );

    /**
     * Receive results from turtle search @see RsGenExchange @see RsNxsObserver
     * @see p3turtle::handleSearchResult
     */
    void receiveDistantSearchResults( TurtleRequestId id,
                                      const RsGxsGroupId& grpId );

    // Overloaded from RsGxsIface.
virtual bool subscribeToGroup(uint32_t &token, const RsGxsGroupId &groupId, bool subscribe);

    // Set Statuses.
virtual void setMessageProcessedStatus(uint32_t& token, const RsGxsGrpMsgIdPair& msgId, bool processed);
virtual void setMessageReadStatus(uint32_t& token, const RsGxsGrpMsgIdPair& msgId, bool read);

    // File Interface
    virtual bool ExtraFileHash(const std::string& path);
virtual bool ExtraFileRemove(const RsFileHash &hash);


    /// Implementation of @see RsGxsChannels::getChatsSummaries
    virtual bool getChatsSummaries(std::list<RsGroupMetaData>& channels);

    /// Implementation of @see RsGxsChannels::getChannelsInfo
    virtual bool getChatsInfo(
            const std::list<RsGxsGroupId>& chanIds,
            std::vector<RsGxsChatGroup>& channelsInfo );

    /// Implementation of @see RsGxsChannels::getChannelContent
    virtual bool getChatsContent(
            const std::list<RsGxsGroupId>& chanIds,
            std::vector<RsGxsChatMsg>& posts );

protected:
    // Overloaded from GxsTokenQueue for Request callbacks.
    virtual void handleResponse(uint32_t token, uint32_t req_type);


private:

static uint32_t chatsAuthenPolicy();

    // Handle Processing.
    void request_AllSubscribedGroups();
    void request_SpecificSubscribedGroups(const std::list<RsGxsGroupId> &groups);
    void load_SubscribedGroups(const uint32_t &token);

    void request_SpecificUnprocessedPosts(std::list<std::pair<RsGxsGroupId, RsGxsMessageId> > &ids);
    void load_SpecificUnprocessedPosts(const uint32_t &token);

    void request_GroupUnprocessedPosts(const std::list<RsGxsGroupId> &grouplist);
    void load_GroupUnprocessedPosts(const uint32_t &token);

    void handleUnprocessedPost(const RsGxsChatMsg &msg);

    // Local Cache of Subscribed Groups. and AutoDownload Flag.
    void updateSubscribedGroup(const RsGroupMetaData &group);
    void clearUnsubscribedGroup(const RsGxsGroupId &id);

    bool setAutoDownload(const RsGxsGroupId &groupId, bool enabled);
    bool autoDownloadEnabled(const RsGxsGroupId &groupId, bool &enabled);



    std::map<RsGxsGroupId, RsGroupMetaData> mSubscribedGroups;


// DUMMY DATA,
virtual bool generateDummyData();

std::string genRandomId();

void 	dummy_tick();

bool generatePost(uint32_t &token, const RsGxsGroupId &grpId);

bool generateGroup(uint32_t &token, std::string groupName);

    class ChatDummyRef
    {
        public:
        ChatDummyRef() { return; }
        ChatDummyRef(const RsGxsGroupId &grpId, const RsGxsMessageId &threadId, const RsGxsMessageId &msgId)
        :mGroupId(grpId), mThreadId(threadId), mMsgId(msgId) { return; }

        RsGxsGroupId mGroupId;
        RsGxsMessageId mThreadId;
        RsGxsMessageId mMsgId;
    };

    uint32_t mGenToken;
    bool mGenActive;
    int mGenCount;
    std::vector<ChatDummyRef> mGenRefs;
    RsGxsMessageId mGenThreadId;

    std::map<RsGxsGroupId,rstime_t> mKnownChats;

    /** Store search callbacks with timeout*/
    std::map<
        TurtleRequestId,
        std::pair<
            std::function<void (const RsGxsGroupSummary&)>,
            std::chrono::system_clock::time_point >
     > mSearchCallbacksMap;
    RsMutex mSearchCallbacksMapMutex;

    /// Cleanup mSearchCallbacksMap
    void cleanTimedOutSearches();
};

#endif // P3GXSCHATS_H
