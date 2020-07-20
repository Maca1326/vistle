#ifndef VISTLE_FILEINFO_CRAWLER_H
#define VISTLE_FILEINFO_CRAWLER_H

#include <vistle/core/filequery.h>
#include <vistle/core/messages.h>
#include <vistle/util/buffer.h>

#include "hub.h"

namespace vistle {

class FileInfoCrawler {
public:

    FileInfoCrawler(Hub &hub);
    bool handle(const message::FileQuery &query, const buffer &payload);

private:
    bool sendResponse(const message::FileQuery &query, message::FileQueryResult::Status s, const buffer &payload);

    Hub &m_hub;
};

}
#endif
