#include "DecodeTask.h"
#include <vector>
#include <mutex>

#include <rhr/rfbext.h>
#include <rhr/compdecomp.h>
#include <core/messages.h>
#include <PluginUtil/MultiChannelDrawer.h>

#include <osg/Geometry>


using namespace opencover;
using namespace vistle;
using vistle::message::RemoteRenderMessage;


#define CERR std::cerr << "RhrClient:Decode: "


DecodeTask::DecodeTask(std::shared_ptr<const RemoteRenderMessage> msg, std::shared_ptr<std::vector<char>> payload)
: msg(msg)
, payload(payload)
, rgba(NULL)
, depth(NULL)
{}

bool DecodeTask::work() {
    //CERR << "DecodeTask::execute" << std::endl;
    assert(msg->rhr().type == rfbTile);

    if (!depth && !rgba) {
        // dummy task for other node
        //CERR << "DecodeTask: no FB" << std::endl;
        return true;
    }

    const auto &tile = static_cast<const tileMsg &>(msg->rhr());

    if (tile.unzippedsize == 0) {
        CERR << "DecodeTask: no data, unzippedsize=" << tile.unzippedsize << std::endl;
        return false;
    }
    if (!payload) {
        CERR << "DecodeTask: no data, payload is NULL" << std::endl;
        return false;
    }

    int bpp = 0;
    if (tile.format == rfbColorRGBA) {
        if (!rgba)
            return false;
        assert(rgba);
        bpp = 4;
    } else {
        if (!depth)
            return false;
        assert(depth);
        switch (tile.format) {
        case rfbDepthFloat: bpp=4; break;
        case rfbDepth8Bit: bpp=1; break;
        case rfbDepth16Bit: bpp=2; break;
        case rfbDepth24Bit: bpp=4; break;
        case rfbDepth32Bit: bpp=4; break;
        }
    }

    vistle::CompressionParameters param;
    auto &cp = param.rgba;
    auto &dp = param.depth;
    if (tile.compression & rfbTileDepthZfp) {
        dp.depthZfp = true;
    }
    if (tile.compression & rfbTileDepthQuantize) {
        dp.depthQuant = true;
    }
    if (tile.format == rfbDepthFloat) {
        dp.depthFloat = true;
    }
    if (tile.compression == rfbTileJpeg) {
        cp.rgbaJpeg = true;
    }
    param.isDepth = tile.format != rfbColorRGBA;

    auto decompbuf = message::decompressPayload(*msg, *payload);
    if (decompbuf.size() != tile.unzippedsize) {
        CERR << "DecodeTask: invalid data: unzipped size wrong: " << decompbuf.size() << " != " << tile.unzippedsize << std::endl;
    }

    return decompressTile(param.isDepth ? depth : rgba, decompbuf, param, tile.x, tile.y, tile.width, tile.height, tile.totalwidth);
}
