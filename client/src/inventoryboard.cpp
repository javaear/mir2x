/*
 * =====================================================================================
 *
 *       Filename: inventoryboard.cpp
 *        Created: 10/08/2017 19:22:30
 *    Description:
 *
 *        Version: 1.0
 *       Revision: none
 *       Compiler: gcc
 *
 *         Author: ANHONG
 *          Email: anhonghe@gmail.com
 *   Organization: USTC
 *
 * =====================================================================================
 */

#include "pngtexdb.hpp"
#include "sdldevice.hpp"
#include "processrun.hpp"
#include "inventoryboard.hpp"

extern PNGTexDB *g_progUseDB;
extern PNGTexDB *g_itemDB;
extern SDLDevice *g_sdlDevice;

InventoryBoard::InventoryBoard(int nX, int nY, ProcessRun *pRun, Widget *pwidget, bool autoDelete)
    : Widget(DIR_UPLEFT, nX, nY, 0, 0, pwidget, autoDelete)
    , m_opNameBoard
      {
          DIR_UPLEFT,
          132,
          16,
          u8"【背包】",

          1,
          12,
          0,

          colorf::WHITE + 255,
          this,
      }

    , m_wmdAniBoard
      {
          DIR_UPLEFT,
          23,
          14,
          this,
      }

    , m_slider
      {
          DIR_UPLEFT,
          258,
          64,

          291,
          2,

          nullptr,
          this,
      }

    , m_closeButton
      {
          DIR_UPLEFT,
          242,
          422,
          {SYS_TEXNIL, 0X0000001C, 0X0000001D},

          nullptr,
          nullptr,
          [this]()
          {
              show(false);
          },

          0,
          0,
          0,
          0,

          true,
          true,
          this,
      }
    , m_processRun(pRun)
{
    show(false);
    auto texPtr = g_progUseDB->Retrieve(0X0000001B);
    if(!texPtr){
        throw fflerror("no valid inventory frame texture");
    }
    std::tie(m_w, m_h) = SDLDeviceHelper::getTextureSize(texPtr);
}

void InventoryBoard::drawItem(int dstX, int dstY, size_t startRow, bool cursorOn, const PackBin &bin) const
{
    if(true
            && bin
            && bin.x >= 0
            && bin.y >= 0
            && bin.w >  0
            && bin.h >  0){

        if(auto texPtr = g_itemDB->Retrieve(DBCOM_ITEMRECORD(bin.item.itemID).pkgGfxID | 0X01000000)){
            const int startX = dstX + m_invGridX0;
            const int startY = dstY + m_invGridY0 - startRow * SYS_INVGRIDPH;
            const int  viewX = dstX + m_invGridX0;
            const int  viewY = dstY + m_invGridY0;

            const auto [itemPW, itemPH] = SDLDeviceHelper::getTextureSize(texPtr);
            int drawDstX = startX + bin.x * SYS_INVGRIDPW + (bin.w * SYS_INVGRIDPW - itemPW) / 2;
            int drawDstY = startY + bin.y * SYS_INVGRIDPH + (bin.h * SYS_INVGRIDPH - itemPH) / 2;
            int drawSrcX = 0;
            int drawSrcY = 0;
            int drawSrcW = itemPW;
            int drawSrcH = itemPH;

            if(mathf::ROICrop(
                        &drawSrcX, &drawSrcY,
                        &drawSrcW, &drawSrcH,
                        &drawDstX, &drawDstY,

                        drawSrcW,
                        drawSrcH,

                        0, 0, -1, -1,
                        viewX, viewY, SYS_INVGRIDPW * 6, SYS_INVGRIDPH * 8)){
                g_sdlDevice->drawTexture(texPtr, drawDstX, drawDstY, drawSrcX, drawSrcY, drawSrcW, drawSrcH);
            }

            int binGridX = bin.x;
            int binGridY = bin.y;
            int binGridW = bin.w;
            int binGridH = bin.h;

            if(mathf::rectangleOverlapRegion<int>(0, startRow, 6, 8, &binGridX, &binGridY, &binGridW, &binGridH)){
                if(cursorOn){
                    g_sdlDevice->fillRectangle(colorf::WHITE + 64,
                            startX + binGridX * SYS_INVGRIDPW,
                            startY + binGridY * SYS_INVGRIDPH, // startY is for (0, 0), not for (0, startRow)
                            binGridW * SYS_INVGRIDPW,
                            binGridH * SYS_INVGRIDPH);
                }

                if(bin.item.count > 1){
                    const LabelBoard itemCount
                    {
                        DIR_UPLEFT,
                        0, // reset by new width
                        0,
                        to_u8cstr(std::to_string(bin.item.count)),

                        1,
                        10,
                        0,

                        colorf::RGBA(0XFF, 0XFF, 0X00, 0XFF),
                    };
                    itemCount.drawAt(DIR_UPRIGHT, startX + (binGridX + binGridW) * SYS_INVGRIDPW, startY + binGridY * SYS_INVGRIDPH - 2 /* pixel adjust */);
                }
            }
        }
    }
}

void InventoryBoard::update(double fUpdateTime)
{
    m_wmdAniBoard.update(fUpdateTime);
}

void InventoryBoard::drawEx(int dstX, int dstY, int, int, int, int) const
{
    if(auto pTexture = g_progUseDB->Retrieve(0X0000001B)){
        g_sdlDevice->drawTexture(pTexture, dstX, dstY);
    }

    const auto myHeroPtr = m_processRun->getMyHero();
    if(!myHeroPtr){
        return;
    }

    const auto startRow = getStartRow();
    const auto [mousePX, mousePY] = SDLDeviceHelper::getMousePLoc();
    const auto &packBinListCRef = myHeroPtr->getInvPack().getPackBinList();
    const auto cursorOnIndex = getPackBinIndex(mousePX, mousePY);
    for(int i = 0; i < to_d(packBinListCRef.size()); ++i){
        drawItem(dstX, dstY, startRow, (i == cursorOnIndex), packBinListCRef.at(i));
    }

    drawGold();
    m_opNameBoard.draw();
    m_wmdAniBoard.draw();
    m_slider     .draw();
    m_closeButton.draw();

    if(cursorOnIndex >= 0){
        drawItemHoverText(packBinListCRef.at(cursorOnIndex));
    }
}

bool InventoryBoard::processEvent(const SDL_Event &event, bool valid)
{
    if(!valid){
        focus(false);
        return false;
    }

    if(!show()){
        return false;
    }

    if(m_closeButton.processEvent(event, valid)){
        return true;
    }

    if(m_slider.processEvent(event, valid)){
        return true;
    }

    switch(event.type){
        case SDL_MOUSEMOTION:
            {
                if((event.motion.state & SDL_BUTTON_LMASK) && (in(event.motion.x, event.motion.y) || focus())){
                    const auto [rendererW, rendererH] = g_sdlDevice->getRendererSize();
                    const int maxX = rendererW - w();
                    const int maxY = rendererH - h();

                    const int newX = std::max<int>(0, std::min<int>(maxX, x() + event.motion.xrel));
                    const int newY = std::max<int>(0, std::min<int>(maxY, y() + event.motion.yrel));
                    moveBy(newX - x(), newY - y());
                    return focusConsume(this, true);
                }
                return focusConsume(this, false);
            }
        case SDL_MOUSEBUTTONDOWN:
            {
                auto myHeroPtr = m_processRun->getMyHero();
                auto &invPackRef = myHeroPtr->getInvPack();
                auto lastGrabbedItem = invPackRef.getGrabbedItem();

                switch(event.button.button){
                    case SDL_BUTTON_LEFT:
                        {
                            if(in(event.button.x, event.button.y)){
                                if(const int selectedPackIndex = getPackBinIndex(event.button.x, event.button.y); selectedPackIndex >= 0){
                                    auto selectedPackBin = invPackRef.getPackBinList().at(selectedPackIndex);
                                    invPackRef.setGrabbedItem(selectedPackBin.item);
                                    invPackRef.remove(selectedPackBin.item);
                                    if(lastGrabbedItem){
                                        // when swapping
                                        // prefer to use current location to store
                                        invPackRef.add(lastGrabbedItem, selectedPackBin.x, selectedPackBin.y);
                                    }
                                }
                                else if(lastGrabbedItem){
                                    const auto [gridX, gridY] = getInvGrid(event.button.x, event.button.y);
                                    const auto [gridW, gridH] = InvPack::getPackBinSize(lastGrabbedItem.itemID);
                                    const auto startGridX = gridX - gridW / 2; // can give an invalid (x, y)
                                    const auto startGridY = gridY - gridH / 2;
                                    invPackRef.add(lastGrabbedItem, startGridX, startGridY);
                                    invPackRef.setGrabbedItem({});
                                }
                                return focusConsume(this, true);
                            }
                            return focusConsume(this, false);
                        }
                    case SDL_BUTTON_RIGHT:
                        {
                            if(in(event.button.x, event.button.y)){
                                if(const int selectedPackIndex = getPackBinIndex(event.button.x, event.button.y); selectedPackIndex >= 0){
                                    packBinConsume(invPackRef.getPackBinList().at(selectedPackIndex));
                                }
                                return focusConsume(this, true);
                            }
                            return focusConsume(this, false);
                        }
                    default:
                        {
                            return focusConsume(this, false);
                        }
                }
            }
        case SDL_MOUSEWHEEL:
            {
                const auto [mousePX, mousePY] = SDLDeviceHelper::getMousePLoc();
                if(mathf::pointInRectangle<int>(mousePX, mousePY, x() + m_invGridX0, y() + m_invGridY0, 6 * SYS_INVGRIDPW, 8 * SYS_INVGRIDPH)){
                    const auto rowCount = getRowCount();
                    if(rowCount > 8){
                        m_slider.addValue((event.wheel.y > 0 ? -1.0 : 1.0) / (rowCount - 8));
                    }
                    return focusConsume(this, true);
                }
                return false;
            }
        default:
            {
                return focusConsume(this, false);
            }
    }
}

std::string InventoryBoard::getGoldStr() const
{
    return str_ksep([this]() -> int
    {
        if(auto p = m_processRun->getMyHero()){
            return p->getGold();
        }
        return 0;
    }(), ',');
}

size_t InventoryBoard::getRowCount() const
{
    const auto &packBinList = m_processRun->getMyHero()->getInvPack().getPackBinList();
    if(packBinList.empty()){
        return 0;
    }

    size_t rowCount = 0;
    for(const auto &bin: packBinList){
        rowCount = std::max<size_t>(rowCount, bin.y + bin.h);
    }
    return rowCount;
}

size_t InventoryBoard::getStartRow() const
{
    const size_t rowGfxCount = 8;
    const size_t rowCount = getRowCount();

    if(rowCount <= rowGfxCount){
        return 0;
    }
    return std::lround((rowCount - rowGfxCount) * m_slider.getValue());
}

void InventoryBoard::drawGold() const
{
    const LabelBoard goldBoard
    {
        DIR_UPLEFT,
        0, // reset by new width
        0,
        to_u8cstr(getGoldStr()),

        1,
        12,
        0,

        colorf::RGBA(0XFF, 0XFF, 0X00, 0XFF),
    };
    goldBoard.drawAt(DIR_NONE, x() + 106, y() + 409);
}

int InventoryBoard::getPackBinIndex(int locPX, int locPY) const
{
    const auto [gridX, gridY] = getInvGrid(locPX, locPY);
    if(gridX < 0 || gridY < 0){
        return -1;
    }

    const auto startRow = getStartRow();
    const auto myHeroPtr = m_processRun->getMyHero();
    const auto &packBinListCRef = myHeroPtr->getInvPack().getPackBinList();
    for(int i = 0; i < to_d(packBinListCRef.size()); ++i){
        const auto &binCRef = packBinListCRef.at(i);
        if(mathf::pointInRectangle<int>(gridX, gridY, binCRef.x, binCRef.y - startRow, binCRef.w, binCRef.h)){
            return i;
        }
    }
    return -1;
}

std::tuple<int, int> InventoryBoard::getInvGrid(int locPX, int locPY) const
{
    const int gridPX0 = m_invGridX0 + x();
    const int gridPY0 = m_invGridY0 + y();

    if(!mathf::pointInRectangle<int>(locPX, locPY, gridPX0, gridPY0, 6 * SYS_INVGRIDPW, 8 * SYS_INVGRIDPH)){
        return {-1, -1};
    }

    return
    {
        (locPX - gridPX0) / SYS_INVGRIDPW,
        (locPY - gridPY0) / SYS_INVGRIDPH,
    };
}

void InventoryBoard::drawItemHoverText(const PackBin &bin) const
{
    const auto &ir = DBCOM_ITEMRECORD(bin.item.itemID);
    const auto hoverText = str_printf
    (
        u8R"###( <layout>                  )###""\n"
        u8R"###(     <par>【名称】%s</par> )###""\n"
        u8R"###(     <par>【描述】%s</par> )###""\n"
        u8R"###( </layout>                 )###""\n",

        ir.name,
        str_haschar(ir.description) ? ir.description : u8"游戏处于开发阶段，此物品暂无描述。"
    );

    LayoutBoard hoverTextBoard
    {
        DIR_UPLEFT,
        0,
        0,
        200,

        false,
        {0, 0, 0, 0},

        false,

        1,
        12,
        0,
        colorf::WHITE + 255,
        0,

        LALIGN_JUSTIFY,
    };

    hoverTextBoard.loadXML(to_cstr(hoverText));
    const auto [mousePX, mousePY] = SDLDeviceHelper::getMousePLoc();
    const auto textBoxW = std::max<int>(hoverTextBoard.w(), 200) + 20;
    const auto textBoxH = hoverTextBoard.h() + 20;

    g_sdlDevice->fillRectangle(colorf::RGBA(0, 0,   0, 200), mousePX, mousePY, textBoxW, textBoxH, 5);
    g_sdlDevice->drawRectangle(colorf::RGBA(0, 0, 255, 255), mousePX, mousePY, textBoxW, textBoxH, 5);
    hoverTextBoard.drawAt(DIR_UPLEFT, mousePX + 10, mousePY + 10);
}

void InventoryBoard::packBinConsume(const PackBin &bin)
{
    const auto &ir = DBCOM_ITEMRECORD(bin.item.itemID);
    if(false
            || to_u8sv(ir.type) == u8"恢复药水"
            || to_u8sv(ir.type) == u8"强化药水"
            || to_u8sv(ir.type) == u8"技能书"){
        m_processRun->requestConsumeItem(bin.item.itemID, bin.item.seqID, 1);
    }
}
